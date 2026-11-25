// Benchmark harness for Challenge 02: Multi-Symbol Order Book (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::*;
use std::cell::Cell;
use std::collections::HashMap;
use std::rc::Rc;

// ---------------------------------------------------------------------------
// Venue implementation — deterministic exchange simulator
// ---------------------------------------------------------------------------

struct VenueImpl {
    // The id the venue gives the next order. The harness keeps a handle and
    // sets it to the id the workload pre-allocated (op.aux) before each
    // SendOur / re-quoting ModifyOur, like the C++ harness's reset_next_eid.
    next_eid: Rc<Cell<u64>>,
    our_by_eid: std::collections::HashMap<u64, (i64, i64)>, // eid -> (price, qty)
}

impl VenueImpl {
    fn new(start_eid: u64) -> Self {
        Self { next_eid: Rc::new(Cell::new(start_eid)), our_by_eid: std::collections::HashMap::new() }
    }
    fn take_id(&mut self) -> u64 {
        let eid = self.next_eid.get();
        self.next_eid.set(eid + 1);
        eid
    }
}

impl Venue for VenueImpl {
    fn send_order(&mut self, _our_id: u64, _symbol: u16, _side: i32,
                  price: i64, qty: i64) -> u64 {
        let eid = self.take_id();
        self.our_by_eid.insert(eid, (price, qty));
        eid
    }

    fn modify_order(&mut self, exchange_id: u64, new_price: i64, new_qty: i64) -> u64 {
        let Some(&(price, qty)) = self.our_by_eid.get(&exchange_id) else {
            return exchange_id;
        };
        if new_price != price || new_qty > qty {
            self.our_by_eid.remove(&exchange_id);
            let new_eid = self.take_id();
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
    aux: i64,  // depth for VolumeNearBest, n for TopLevels, the id the venue assigns for SendOur / re-quoting ModifyOur
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
// Reference book for the correctness check. The certified benchmark checks
// the same things against the same kind of book, on a seed of its own.
// ---------------------------------------------------------------------------

#[derive(Default)]
struct RefLevel { total_qty: i64, count: i32, queue: Vec<(u64, i64)> } // (eid, qty), FIFO

struct RefBook {
    venue: VenueImpl,
    orders: HashMap<u64, (u16, i8, i64, i64)>, // eid -> (symbol, side, price, qty)
    our: HashMap<u64, u64>,                    // our_id -> eid
    books: Vec<[std::collections::BTreeMap<i64, RefLevel>; 2]>,
}

impl RefBook {
    fn new(venue: VenueImpl) -> Self {
        Self { venue, orders: HashMap::new(), our: HashMap::new(),
               books: (0..NUM_SYMBOLS).map(|_| [Default::default(), Default::default()]).collect() }
    }

    fn exec(&mut self, op: &Operation) {
        match op.op {
            OpType::Add => {
                self.orders.insert(op.id, (op.symbol, op.side, op.price, op.qty));
                let lv = self.books[op.symbol as usize][op.side as usize].entry(op.price).or_default();
                lv.queue.push((op.id, op.qty)); lv.total_qty += op.qty; lv.count += 1;
            }
            OpType::Cancel => {
                let Some((sym, side, price, _)) = self.orders.remove(&op.id) else { return };
                let levels = &mut self.books[sym as usize][side as usize];
                if let Some(lv) = levels.get_mut(&price) {
                    if let Some(i) = lv.queue.iter().position(|e| e.0 == op.id) {
                        lv.total_qty -= lv.queue[i].1; lv.count -= 1; lv.queue.remove(i);
                    }
                    if lv.count == 0 { levels.remove(&price); }
                }
            }
            OpType::Modify => {
                let Some(o) = self.orders.get_mut(&op.id) else { return };
                if let Some(lv) = self.books[o.0 as usize][o.1 as usize].get_mut(&o.2) {
                    if let Some(e) = lv.queue.iter_mut().find(|e| e.0 == op.id) {
                        lv.total_qty += op.qty - e.1; e.1 = op.qty;
                    }
                }
                o.3 = op.qty;
            }
            OpType::SendOur => {
                let eid = self.venue.send_order(op.id, op.symbol, op.side as i32, op.price, op.qty);
                self.our.insert(op.id, eid);
            }
            OpType::ModifyOur => {
                if let Some(&eid) = self.our.get(&op.id) {
                    let new_eid = self.venue.modify_order(eid, op.price, op.qty);
                    self.our.insert(op.id, new_eid);
                }
            }
            OpType::CancelOur => {
                if let Some(eid) = self.our.remove(&op.id) { self.venue.cancel_order(eid); }
            }
            _ => {}
        }
    }

    // Every level of (symbol, side), best first.
    fn levels(&self, sym: u16, side: usize) -> Vec<TopLevel> {
        let m = &self.books[sym as usize][side];
        let f = |(p, l): (&i64, &RefLevel)| TopLevel { price: *p, qty: l.total_qty, count: l.count };
        if side == 0 { m.iter().rev().map(f).collect() } else { m.iter().map(f).collect() }
    }
    fn best(&self, sym: u16, side: usize) -> TopLevel {
        self.levels(sym, side).first().copied().unwrap_or_default()
    }
    fn queue_position(&self, our_id: u64) -> QueuePosition {
        let none = QueuePosition { index: -1, qty_ahead: 0 };
        let Some(eid) = self.our.get(&our_id) else { return none };
        let Some(&(sym, side, price, _)) = self.orders.get(eid) else { return none };
        let Some(lv) = self.books[sym as usize][side as usize].get(&price) else { return none };
        let mut qa = 0;
        for (i, e) in lv.queue.iter().enumerate() {
            if e.0 == *eid { return QueuePosition { index: i as i32, qty_ahead: qa }; }
            qa += e.1;
        }
        none
    }
}

// Your book and the reference replay the same workload; every best_bid /
// best_ask (price, qty, count) and get_queue_position answer is compared,
// then every level of every symbol at the end.
fn check_correctness() -> bool {
    let wl = generate_workload(0x02_DEAD_BEEF);
    let venue = VenueImpl::new(wl.venue_start_eid);
    let next_eid = Rc::clone(&venue.next_eid);
    let mut book = solution::MyMultiOrderBook::new(Box::new(venue));
    let ref_venue = VenueImpl::new(wl.venue_start_eid);
    let ref_next_eid = Rc::clone(&ref_venue.next_eid);
    let mut r = RefBook::new(ref_venue);
    let fail = |m: String| hftu_bench::check_failed("correctness", &m);

    for op in &wl.setup { execute_op(&mut book, op); r.exec(op); }
    let mut checks = 0;
    for op in &wl.timed {
        if matches!(op.op, OpType::SendOur | OpType::ModifyOur) && op.aux != 0 {
            next_eid.set(op.aux as u64);
            ref_next_eid.set(op.aux as u64);
        }
        execute_op(&mut book, op);
        r.exec(op);
        match op.op {
            OpType::BestBid | OpType::BestAsk => {
                let (name, side) = if matches!(op.op, OpType::BestBid) { ("best_bid", 0) } else { ("best_ask", 1) };
                let u = if side == 0 { book.best_bid(op.symbol) } else { book.best_ask(op.symbol) };
                let e = r.best(op.symbol, side);
                if u.price != e.price || u.qty != e.qty || u.count != e.count {
                    return fail(format!("{}(sym={}), query {}: got {{p={},q={},c={}}} expected {{p={},q={},c={}}}",
                        name, op.symbol, checks, u.price, u.qty, u.count, e.price, e.qty, e.count));
                }
                checks += 1;
            }
            OpType::QueuePosition_ => {
                let (u, e) = (book.get_queue_position(op.id), r.queue_position(op.id));
                if u.index != e.index || u.qty_ahead != e.qty_ahead {
                    return fail(format!("queue_position(our_id={}), query {}: got {{idx={},qa={}}} expected {{idx={},qa={}}}",
                        op.id, checks, u.index, u.qty_ahead, e.index, e.qty_ahead));
                }
                checks += 1;
            }
            _ => {}
        }
    }
    const MAX_LEVELS: usize = 16384;
    let mut got = vec![TopLevel::default(); MAX_LEVELS];
    for sym in 0..NUM_SYMBOLS as u16 {
        for side in 0..2usize {
            let want = r.levels(sym, side);
            let n = book.get_top_levels(sym, side as i32, MAX_LEVELS, &mut got).min(MAX_LEVELS);
            if n != want.len() {
                return fail(format!("final book sym={} side={}: get_top_levels returned {} levels, expected {}", sym, side, n, want.len()));
            }
            for (i, (g, w)) in got[..n].iter().zip(&want).enumerate() {
                if g.price != w.price || g.qty != w.qty || g.count != w.count {
                    return fail(format!("final book sym={} side={} level {}: got {{p={},q={},c={}}} expected {{p={},q={},c={}}}",
                        sym, side, i, g.price, g.qty, g.count, w.price, w.qty, w.count));
                }
            }
        }
    }
    eprintln!("  {} query checks passed, final book matches", checks);
    true
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

// Exchange ids as a real venue issues them (the certified benchmark uses the
// same scheme): one counter per matching engine numbers every new order of
// every instrument on it, so ids are large (here a random start between 2^40
// and 2^46) and the 200 symbols you follow see gaps between them (mean gap
// drawn per seed between 8 and 64). A few orders entered long ago rest with
// ids 2^20 to 2^30 below the counter. Ids never repeat in a run. Do not rely
// on any of these numbers: an id is an opaque 64-bit value.
struct IdSource {
    rng: Rng,
    base: u64,
    next: u64,
    gap_p: f64,
    old_used: std::collections::HashSet<u64>,
}

impl IdSource {
    fn new(seed: u64) -> Self {
        let mut rng = Rng::new(seed ^ 0x5851_F42D_4C95_7F2D);
        let base = (1u64 << 40) + rng.next_range((1u64 << 46) - (1u64 << 40) + 1);
        let mean = 8.0 + unit(&mut rng) * 56.0;
        Self { rng, base, next: base, gap_p: 1.0 / mean, old_used: Default::default() }
    }
    fn fresh(&mut self) -> u64 {
        let id = self.next;
        self.next += 1 + geometric(&mut self.rng, self.gap_p);
        id
    }
    fn old(&mut self) -> u64 {
        loop {
            let id = self.base - ((1u64 << 20) + self.rng.next_range((1u64 << 30) - (1u64 << 20) + 1));
            if self.old_used.insert(id) { return id; }
        }
    }
    fn chance(&mut self, p: f64) -> bool { unit(&mut self.rng) < p }
}

// Uniform in [0, 1).
fn unit(rng: &mut Rng) -> f64 { (rng.next_u64() >> 11) as f64 * (1.0 / (1u64 << 53) as f64) }

// Failures before the first success, success probability p (mean (1-p)/p).
fn geometric(rng: &mut Rng, p: f64) -> u64 {
    ((1.0 - unit(rng)).ln() / (1.0 - p).ln()).floor() as u64
}

fn generate_workload(seed: u64) -> Workload {
    let mut rng = Rng::new(seed);
    let num_symbols = 200usize;
    let num_hot = 50usize;
    let setup_orders = 500_000usize;
    let timed_ops = 100_000usize;
    let max_our_orders = 50;
    let our_order_latency = 10;
    let max_price: i64 = 1_000_000; // the contract: prices up to 1,000,000

    let mut setup = Vec::with_capacity(setup_orders);
    let mut ids = IdSource::new(seed);
    // Half the feed cancels hit a young order (age in feed adds, mean 300),
    // as on a real feed where most orders die within a second.
    let mut cancel_rng = Rng::new(seed ^ 0x2545_F491_4F6C_DD1D);
    let mut next_our_id: u64 = 1;

    #[allow(dead_code)]
    // is_our: one of our orders; the feed never cancels or modifies it on its own.
    struct ActiveOrder { eid: u64, symbol: u16, side: i8, price: i64, qty: i64, is_our: bool }
    let mut active: Vec<ActiveOrder> = Vec::with_capacity(setup_orders);
    // eid -> index in `active`, and every feed add in order (young cancels)
    let mut apos: HashMap<u64, usize> = HashMap::with_capacity(setup_orders + timed_ops);
    let mut recent: Vec<u64> = Vec::with_capacity(setup_orders + timed_ops);
    fn active_push(active: &mut Vec<ActiveOrder>, apos: &mut HashMap<u64, usize>, ao: ActiveOrder) {
        apos.insert(ao.eid, active.len());
        active.push(ao);
    }
    fn active_remove(active: &mut Vec<ActiveOrder>, apos: &mut HashMap<u64, usize>, k: usize) {
        apos.remove(&active[k].eid);
        active.swap_remove(k);
        if k < active.len() { apos.insert(active[k].eid, k); }
    }

    // min_trigger: the feed events of one of our orders arrive in the order
    // they were caused (a cancel never lands before the add it cancels).
    struct OurOrder { our_id: u64, exchange_id: u64, symbol: u16, side: i8, price: i64, qty: i64, min_trigger: usize }
    let mut our_active: Vec<OurOrder> = Vec::new();

    struct PendingEvent { trigger_at: usize, op: Operation }
    let mut pending: Vec<PendingEvent> = Vec::new();

    // Setup phase
    for _ in 0..setup_orders {
        let sym = pick_symbol(&mut rng, num_symbols, num_hot);
        let side = rng.next_range(2) as i8;
        let price = pick_price(&mut rng, sym, max_price);
        let qty = rng.next_i64_range(1, 10_000);
        let eid = if ids.chance(0.04) { ids.old() } else { ids.fresh() };
        setup.push(Operation { op: OpType::Add, id: eid, symbol: sym, side, price, qty, aux: 0 });
        active_push(&mut active, &mut apos, ActiveOrder { eid, symbol: sym, side, price, qty, is_our: false });
        recent.push(eid);
    }
    let venue_start_eid = ids.next;

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
            let eid = if ids.chance(0.002) { ids.old() } else { ids.fresh() };
            timed.push(Operation { op: OpType::Add, id: eid, symbol: sym, side, price, qty, aux: 0 });
            active_push(&mut active, &mut apos, ActiveOrder { eid, symbol: sym, side, price, qty, is_our: false });
            recent.push(eid);
        } else if r < 55 && active.len() > 1000 {
            let mut idx = rng.next_range(active.len() as u64) as usize;
            if unit(&mut cancel_rng) < 0.5 {
                let age = geometric(&mut cancel_rng, 1.0 / 300.0) as usize;
                if age < recent.len() {
                    if let Some(&k) = apos.get(&recent[recent.len() - 1 - age]) { idx = k; }
                }
            }
            while active[idx].is_our { idx = (idx + 1) % active.len(); }
            let eid = active[idx].eid;
            active_remove(&mut active, &mut apos, idx);
            timed.push(Operation { op: OpType::Cancel, id: eid, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 });
        } else if r < 65 && active.len() > 100 {
            let mut idx = rng.next_range(active.len() as u64) as usize;
            while active[idx].is_our { idx = (idx + 1) % active.len(); }
            let new_qty = (active[idx].qty * rng.next_i64_range(30, 90) / 100).max(1);
            active[idx].qty = new_qty;
            timed.push(Operation { op: OpType::Modify, id: active[idx].eid, symbol: 0, side: 0, price: 0, qty: new_qty, aux: 0 });
        } else if r < 70 && (our_active.len() as i32) < max_our_orders {
            let sym = pick_symbol(&mut rng, num_symbols, num_hot);
            let side = rng.next_range(2) as i8;
            let price = pick_price(&mut rng, sym, max_price);
            let qty = rng.next_i64_range(1, 10_000);
            let oid = next_our_id; next_our_id += 1;
            let eid = ids.fresh();
            timed.push(Operation { op: OpType::SendOur, id: oid, symbol: sym, side, price, qty, aux: eid as i64 });
            let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
            let when = op_idx + delay + 1;
            pending.push(PendingEvent { trigger_at: when,
                op: Operation { op: OpType::Add, id: eid, symbol: sym, side, price, qty, aux: 0 } });
            our_active.push(OurOrder { our_id: oid, exchange_id: eid, symbol: sym, side, price, qty, min_trigger: when + 1 });
            active_push(&mut active, &mut apos, ActiveOrder { eid, symbol: sym, side, price, qty, is_our: true });
        } else if r < 73 && !our_active.is_empty() {
            let idx = rng.next_range(our_active.len() as u64) as usize;
            if rng.next_range(100) < 60 {
                let new_qty = (our_active[idx].qty * rng.next_i64_range(30, 90) / 100).max(1);
                let price = our_active[idx].price;
                timed.push(Operation { op: OpType::ModifyOur, id: our_active[idx].our_id, symbol: 0, side: 0, price, qty: new_qty, aux: 0 });
                let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
                let when = (op_idx + delay + 1).max(our_active[idx].min_trigger);
                pending.push(PendingEvent { trigger_at: when,
                    op: Operation { op: OpType::Modify, id: our_active[idx].exchange_id, symbol: 0, side: 0, price: 0, qty: new_qty, aux: 0 } });
                our_active[idx].qty = new_qty;
                our_active[idx].min_trigger = when + 1;
            } else {
                let mut new_price = pick_price(&mut rng, our_active[idx].symbol, max_price);
                // A re-quote must move: the venue answers a same-price modify
                // with the old exchange id, so the new id would never arrive.
                if new_price == our_active[idx].price {
                    new_price = if new_price > 1 { new_price - 1 } else { new_price + 1 };
                }
                let new_qty = our_active[idx].qty;
                let old_eid = our_active[idx].exchange_id;
                let new_eid = ids.fresh();
                timed.push(Operation { op: OpType::ModifyOur, id: our_active[idx].our_id, symbol: 0, side: 0, price: new_price, qty: new_qty, aux: new_eid as i64 });
                let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
                let when = (op_idx + delay + 1).max(our_active[idx].min_trigger);
                our_active[idx].min_trigger = when + 2;
                pending.push(PendingEvent { trigger_at: when,
                    op: Operation { op: OpType::Cancel, id: old_eid, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 } });
                pending.push(PendingEvent { trigger_at: when + 1,
                    op: Operation { op: OpType::Add, id: new_eid, symbol: our_active[idx].symbol, side: our_active[idx].side, price: new_price, qty: new_qty, aux: 0 } });
                if let Some(k) = apos.remove(&old_eid) {
                    active[k].eid = new_eid;
                    active[k].price = new_price;
                    apos.insert(new_eid, k);
                }
                our_active[idx].exchange_id = new_eid;
                our_active[idx].price = new_price;
            }
        } else if r < 75 && !our_active.is_empty() {
            let idx = rng.next_range(our_active.len() as u64) as usize;
            let oo = &our_active[idx];
            timed.push(Operation { op: OpType::CancelOur, id: oo.our_id, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 });
            let delay = rng.next_i64_range(3, our_order_latency as i64) as usize;
            pending.push(PendingEvent { trigger_at: (op_idx + delay + 1).max(oo.min_trigger),
                op: Operation { op: OpType::Cancel, id: oo.exchange_id, symbol: 0, side: 0, price: 0, qty: 0, aux: 0 } });
            if let Some(&pos) = apos.get(&oo.exchange_id) {
                active_remove(&mut active, &mut apos, pos);
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

    harness.add_validation("correctness", check_correctness);

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let wl = generate_workload(0x02_CAFE_BABE);
        let mut total_cycles: u64 = 0;

        for _ in 0..iterations {
            let venue = VenueImpl::new(wl.venue_start_eid);
            let next_eid = Rc::clone(&venue.next_eid);
            let mut book = solution::MyMultiOrderBook::new(Box::new(venue));

            // Setup (not timed)
            for op in &wl.setup {
                execute_op(&mut book, op);
            }

            // Timed — per-operation measurement
            for op in &wl.timed {
                // Outside the measurement: the id the venue will assign.
                if matches!(op.op, OpType::SendOur | OpType::ModifyOur) && op.aux != 0 {
                    next_eid.set(op.aux as u64);
                }
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
