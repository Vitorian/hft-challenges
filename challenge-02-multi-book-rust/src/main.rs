// Benchmark harness for Challenge 02: Multi-Symbol Order Book (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::*;

// ---------------------------------------------------------------------------
// Venue implementation — deterministic exchange simulator
// ---------------------------------------------------------------------------

struct VenueImpl {
    next_eid: u64,
    our_by_eid: std::collections::HashMap<u64, (i64, i64)>, // eid -> (price, qty)
}

impl VenueImpl {
    fn new(start_eid: u64) -> Self {
        Self { next_eid: start_eid, our_by_eid: std::collections::HashMap::new() }
    }
    fn peek_next_eid(&self) -> u64 { self.next_eid }
}

impl Venue for VenueImpl {
    fn send_order(&mut self, _our_id: u64, _symbol: u16, _side: i32,
                  price: i64, qty: i64) -> u64 {
        let eid = self.next_eid;
        self.next_eid += 1;
        self.our_by_eid.insert(eid, (price, qty));
        eid
    }

    fn modify_order(&mut self, exchange_id: u64, new_price: i64, new_qty: i64) -> u64 {
        let Some(&(price, qty)) = self.our_by_eid.get(&exchange_id) else {
            return exchange_id;
        };
        if new_price != price || new_qty > qty {
            self.our_by_eid.remove(&exchange_id);
            let new_eid = self.next_eid;
            self.next_eid += 1;
            self.our_by_eid.insert(new_eid, (new_price, new_qty));
            new_eid
        } else {
            self.our_by_eid.get_mut(&exchange_id).unwrap().1 = new_qty;
            exchange_id
        }
    }

    fn cancel_order(&mut self, exchange_id: u64) {
        self.our_by_eid.remove(&exchange_id);
    }
}

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

#[derive(Clone, Copy)]
enum OpType {
    Add, Cancel, Modify,
    SendOur, ModifyOur, CancelOur,
    BestBid, BestAsk, TopLevels, VolumeNearBest, QueuePosition_,
}

#[derive(Clone, Copy)]
struct Operation {
    op: OpType,
    id: u64,
    symbol: u16,
    side: i8,
    price: i64,
    qty: i64,
    aux: i64,  // depth for VolumeNearBest, n for TopLevels
}

fn execute_op(book: &mut impl MultiOrderBook, op: &Operation) {
    match op.op {
        OpType::Add => book.add_order(op.id, op.symbol, op.side as i32, op.price, op.qty),
        OpType::Cancel => book.cancel_order(op.id),
        OpType::Modify => book.modify_order(op.id, op.qty),
        OpType::SendOur => book.send_order(op.id, op.symbol, op.side as i32, op.price, op.qty),
        OpType::ModifyOur => book.modify_our_order(op.id, op.price, op.qty),
        OpType::CancelOur => book.cancel_our_order(op.id),
        OpType::BestBid => { black_box(book.best_bid(op.symbol)); }
        OpType::BestAsk => { black_box(book.best_ask(op.symbol)); }
        OpType::TopLevels => {
            let mut out = [TopLevel::default(); 5];
            black_box(book.get_top_levels(op.symbol, op.side as i32, op.aux as usize, &mut out));
        }
        OpType::VolumeNearBest => {
            black_box(book.volume_near_best(op.symbol, op.side as i32, op.aux));
        }
        OpType::QueuePosition_ => {
            black_box(book.get_queue_position(op.id));
        }
    }
}

// ---------------------------------------------------------------------------
// Workload generation
// ---------------------------------------------------------------------------

fn symbol_mid_price(sym: u16, max_price: i64) -> i64 {
    1000 + (sym as i64) * (max_price - 2000) / 200
}

fn pick_symbol(rng: &mut Rng, num_symbols: usize, num_hot: usize) -> u16 {
    if rng.next_range(100) < 90 {
        rng.next_range(num_hot as u64) as u16
    } else {
        (num_hot as u64 + rng.next_range((num_symbols - num_hot) as u64)) as u16
    }
}

fn pick_price(rng: &mut Rng, symbol: u16, max_price: i64) -> i64 {
    let mid = symbol_mid_price(symbol, max_price);
    if rng.next_range(100) < 80 {
        let offset = rng.next_i64_range(-100, 100);
        (mid + offset).clamp(1, max_price)
    } else {
        rng.next_i64_range(1, max_price)
    }
}

struct Workload {
    setup: Vec<Operation>,
    timed: Vec<Operation>,
    venue_start_eid: u64,
}

fn generate_workload(seed: u64) -> Workload {
    let mut rng = Rng::new(seed);
    let num_symbols = 200usize;
    let num_hot = 50usize;
    let setup_orders = 500_000usize;
    let timed_ops = 100_000usize;
    let max_our_orders = 50;
    let our_order_latency = 10;
    let max_price: i64 = 100_000;

    let mut setup = Vec::with_capacity(setup_orders);
    let mut next_eid: u64 = 1;
    let mut next_our_id: u64 = 1;

    struct ActiveOrder { eid: u64, symbol: u16, side: i8, price: i64, qty: i64 }
    let mut active: Vec<ActiveOrder> = Vec::with_capacity(setup_orders);

    struct OurOrder { our_id: u64, exchange_id: u64, symbol: u16, side: i8, price: i64, qty: i64 }
    let mut our_active: Vec<OurOrder> = Vec::new();

    struct PendingEvent { trigger_at: usize, op: Operation }
    let mut pending: Vec<PendingEvent> = Vec::new();

    // Setup phase
    for _ in 0..setup_orders {
        let sym = pick_symbol(&mut rng, num_symbols, num_hot);
        let side = rng.next_range(2) as i8;
        let price = pick_price(&mut rng, sym, max_price);
        let qty = rng.next_i64_range(1, 10_000);
        let eid = next_eid; next_eid += 1;
        setup.push(Operation { op: OpType::Add, id: eid, symbol: sym, side, price, qty, aux: 0 });
        active.push(ActiveOrder { eid, symbol: sym, side, price, qty });
    }
    let venue_start_eid = next_eid;

    // Timed phase
    let mut timed = Vec::with_capacity(timed_ops * 2);
    let mut op_idx: usize = 0;

    let drain_pending = |pending: &mut Vec<PendingEvent>, timed: &mut Vec<Operation>, op_idx: &mut usize| {
        pending.sort_by(|a, b| b.trigger_at.cmp(&a.trigger_at)); // sort desc so we can pop
        while let Some(last) = pending.last() {
            if last.trigger_at <= *op_idx {
                timed.push(pending.pop().unwrap().op);
                *op_idx += 1;
            } else {
                break;
            }
        }
    };

    for _ in 0..timed_ops {
        drain_pending(&mut pending, &mut timed, &mut op_idx);

        let r = rng.next_range(100) as i32;

        if r < 35 || active.len() < 100 {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            let side = rng.next_range(2) as i8;
            let price = pick_price(&mut rng, sym, max_price);
            let qty = rng.next_i64_range(1, 10_000);
            let eid = next_eid; next_eid += 1;
            timed.push(Operation { op: OpType::Add, id: eid, symbol: sym, side, price, qty, aux: 0 });
            active.push(ActiveOrder { eid, symbol: sym, side, price, qty });
        } else if r < 55 && active.len() > 1000 {
            let idx = rng.next_range(active.len() as u64) as usize;
            let eid = active[idx].eid;
            active.swap_remove(idx);
            timed.push(Operation { op: OpType::Cancel, id: eid, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 });
        } else if r < 65 && active.len() > 100 {
            let idx = rng.next_range(active.len() as u64) as usize;
            let new_qty = (active[idx].qty * rng.next_i64_range(30, 90) / 100).max(1);
            active[idx].qty = new_qty;
            timed.push(Operation { op: OpType::Modify, id: active[idx].eid, symbol: 0, side: 0, price: 0, qty: new_qty, aux: 0 });
        } else if r < 70 && (our_active.len() as i32) < max_our_orders {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            let side = rng.next_range(2) as i8;
            let price = pick_price(&mut rng, sym, max_price);
            let qty = rng.next_i64_range(1, 10_000);
            let oid = next_our_id; next_our_id += 1;
            let eid = next_eid; next_eid += 1;
            timed.push(Operation { op: OpType::SendOur, id: oid, symbol: sym, side, price, qty, aux: 0 });
            let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
            pending.push(PendingEvent { trigger_at: op_idx + delay + 1,
                op: Operation { op: OpType::Add, id: eid, symbol: sym, side, price, qty, aux: 0 } });
            our_active.push(OurOrder { our_id: oid, exchange_id: eid, symbol: sym, side, price, qty });
            active.push(ActiveOrder { eid, symbol: sym, side, price, qty });
        } else if r < 73 && !our_active.is_empty() {
            let idx = rng.next_range(our_active.len() as u64) as usize;
            if rng.next_range(100) < 60 {
                let new_qty = (our_active[idx].qty * rng.next_i64_range(30, 90) / 100).max(1);
                let price = our_active[idx].price;
                timed.push(Operation { op: OpType::ModifyOur, id: our_active[idx].our_id, symbol: 0, side: 0, price, qty: new_qty, aux: 0 });
                let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
                pending.push(PendingEvent { trigger_at: op_idx + delay + 1,
                    op: Operation { op: OpType::Modify, id: our_active[idx].exchange_id, symbol: 0, side: 0, price: 0, qty: new_qty, aux: 0 } });
                our_active[idx].qty = new_qty;
            } else {
                let new_price = pick_price(&mut rng, our_active[idx].symbol, max_price);
                let new_qty = our_active[idx].qty;
                let old_eid = our_active[idx].exchange_id;
                let new_eid = next_eid; next_eid += 1;
                timed.push(Operation { op: OpType::ModifyOur, id: our_active[idx].our_id, symbol: 0, side: 0, price: new_price, qty: new_qty, aux: 0 });
                let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
                let when = op_idx + delay + 1;
                pending.push(PendingEvent { trigger_at: when,
                    op: Operation { op: OpType::Cancel, id: old_eid, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 } });
                pending.push(PendingEvent { trigger_at: when + 1,
                    op: Operation { op: OpType::Add, id: new_eid, symbol: our_active[idx].symbol, side: our_active[idx].side, price: new_price, qty: new_qty, aux: 0 } });
                our_active[idx].exchange_id = new_eid;
                our_active[idx].price = new_price;
            }
        } else if r < 75 && !our_active.is_empty() {
            let idx = rng.next_range(our_active.len() as u64) as usize;
            let oo = &our_active[idx];
            timed.push(Operation { op: OpType::CancelOur, id: oo.our_id, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 });
            let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
            pending.push(PendingEvent { trigger_at: op_idx + delay + 1,
                op: Operation { op: OpType::Cancel, id: oo.exchange_id, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 } });
            if let Some(pos) = active.iter().position(|a| a.eid == oo.exchange_id) {
                active.swap_remove(pos);
            }
            our_active.swap_remove(idx);
        } else if r < 80 {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            timed.push(Operation { op: OpType::BestBid, id: 0, symbol: sym, side: 0, price: 0, qty: 0, aux: 0 });
        } else if r < 85 {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            timed.push(Operation { op: OpType::BestAsk, id: 0, symbol: sym, side: 0, price: 0, qty: 0, aux: 0 });
        } else if r < 90 {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            let side = rng.next_range(2) as i8;
            let n = rng.next_i64_range(1, 5);
            timed.push(Operation { op: OpType::TopLevels, id: 0, symbol: sym, side, price: 0, qty: 0, aux: n });
        } else if r < 95 {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            let side = rng.next_range(2) as i8;
            let depth = rng.next_i64_range(5, 50);
            timed.push(Operation { op: OpType::VolumeNearBest, id: 0, symbol: sym, side, price: 0, qty: 0, aux: depth });
        } else if !our_active.is_empty() {
            let idx = rng.next_range(our_active.len() as u64) as usize;
            timed.push(Operation { op: OpType::QueuePosition_, id: our_active[idx].our_id, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 });
        } else {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            timed.push(Operation { op: OpType::BestBid, id: 0, symbol: sym, side: 0, price: 0, qty: 0, aux: 0 });
        }

        op_idx = timed.len();
    }

    // Drain remaining pending
    pending.sort_by_key(|e| e.trigger_at);
    for e in pending {
        timed.push(e.op);
    }

    Workload { setup, timed, venue_start_eid }
}

fn main() {
    let mut harness = hftu_bench::Harness::new();

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let wl = generate_workload(0x02_CAFE_BABE);
        let mut total_cycles: u64 = 0;

        for _ in 0..iterations {
            let venue = VenueImpl::new(wl.venue_start_eid);
            let mut book = solution::MyMultiOrderBook::new(Box::new(venue));

            // Setup (not timed)
            for op in &wl.setup {
                execute_op(&mut book, op);
            }

            // Timed — per-operation measurement
            for op in &wl.timed {
                let t0 = cycle_start();
                execute_op(&mut book, op);
                clobber();
                let t1 = cycle_end();
                total_cycles += t1 - t0;
            }
        }

        total_cycles
    }, 0);

    std::process::exit(harness.run());
}
