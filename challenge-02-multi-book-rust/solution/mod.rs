// Challenge 02: Multi-Symbol Order Book — Skeleton Implementation (Rust)
// Edit this file to implement your solution. You can do much better!

use crate::types::*;
use std::collections::{BTreeMap, HashMap};

pub struct MyMultiOrderBook {
    venue: Box<dyn Venue>,
    orders: HashMap<u64, Order>,        // exchange_id -> order
    our_orders: HashMap<u64, u64>,       // our_id -> exchange_id
    books: Vec<SymbolBook>,
}

struct Order {
    symbol: u16,
    side: i8,
    price: i64,
    qty: i64,
}

struct Level {
    total_qty: i64,
    count: i32,
    queue: Vec<(u64, i64)>,  // (exchange_id, qty) in FIFO order
}

struct SymbolBook {
    bids: BTreeMap<i64, Level>,  // last() = best bid
    asks: BTreeMap<i64, Level>,  // first() = best ask
}

impl MultiOrderBook for MyMultiOrderBook {
    fn new(venue: Box<dyn Venue>) -> Self {
        let mut books = Vec::with_capacity(NUM_SYMBOLS);
        for _ in 0..NUM_SYMBOLS {
            books.push(SymbolBook {
                bids: BTreeMap::new(),
                asks: BTreeMap::new(),
            });
        }
        Self {
            venue,
            orders: HashMap::new(),
            our_orders: HashMap::new(),
            books,
        }
    }

    // === Our order management ===

    fn send_order(&mut self, our_id: u64, symbol: u16, side: i32, price: i64, qty: i64) {
        let exchange_id = self.venue.send_order(our_id, symbol, side, price, qty);
        self.our_orders.insert(our_id, exchange_id);
    }

    fn modify_our_order(&mut self, our_id: u64, new_price: i64, new_qty: i64) {
        let Some(&exchange_id) = self.our_orders.get(&our_id) else { return };
        let new_eid = self.venue.modify_order(exchange_id, new_price, new_qty);
        self.our_orders.insert(our_id, new_eid);
    }

    fn cancel_our_order(&mut self, our_id: u64) {
        let Some(exchange_id) = self.our_orders.remove(&our_id) else { return };
        self.venue.cancel_order(exchange_id);
    }

    // === Exchange feed ===

    fn add_order(&mut self, exchange_id: u64, symbol: u16, side: i32, price: i64, qty: i64) {
        self.orders.insert(exchange_id, Order { symbol, side: side as i8, price, qty });
        let book = &mut self.books[symbol as usize];
        let levels = if side == 0 { &mut book.bids } else { &mut book.asks };
        let level = levels.entry(price).or_insert_with(|| Level {
            total_qty: 0, count: 0, queue: Vec::new(),
        });
        level.queue.push((exchange_id, qty));
        level.total_qty += qty;
        level.count += 1;
    }

    fn modify_order(&mut self, exchange_id: u64, new_qty: i64) {
        let Some(order) = self.orders.get_mut(&exchange_id) else { return };
        let old_qty = order.qty;
        order.qty = new_qty;

        let book = &mut self.books[order.symbol as usize];
        let levels = if order.side == 0 { &mut book.bids } else { &mut book.asks };
        let Some(level) = levels.get_mut(&order.price) else { return };
        level.total_qty += new_qty - old_qty;
        for (eid, qty) in level.queue.iter_mut() {
            if *eid == exchange_id {
                *qty = new_qty;
                break;
            }
        }
    }

    fn cancel_order(&mut self, exchange_id: u64) {
        let Some(order) = self.orders.remove(&exchange_id) else { return };
        let book = &mut self.books[order.symbol as usize];
        let levels = if order.side == 0 { &mut book.bids } else { &mut book.asks };
        let Some(level) = levels.get_mut(&order.price) else { return };
        if let Some(pos) = level.queue.iter().position(|(eid, _)| *eid == exchange_id) {
            let (_, qty) = level.queue.remove(pos);
            level.total_qty -= qty;
            level.count -= 1;
        }
        if level.count == 0 {
            levels.remove(&order.price);
        }
    }

    // === Queries ===

    fn best_bid(&self, symbol: u16) -> TopLevel {
        let bids = &self.books[symbol as usize].bids;
        match bids.iter().next_back() {
            Some((&price, level)) => TopLevel { price, qty: level.total_qty, count: level.count },
            None => TopLevel::default(),
        }
    }

    fn best_ask(&self, symbol: u16) -> TopLevel {
        let asks = &self.books[symbol as usize].asks;
        match asks.iter().next() {
            Some((&price, level)) => TopLevel { price, qty: level.total_qty, count: level.count },
            None => TopLevel::default(),
        }
    }

    fn get_top_levels(&self, symbol: u16, side: i32, n: usize, out: &mut [TopLevel]) -> usize {
        let mut written = 0;
        if side == 0 {
            for (&price, level) in self.books[symbol as usize].bids.iter().rev() {
                if written >= n { break; }
                out[written] = TopLevel { price, qty: level.total_qty, count: level.count };
                written += 1;
            }
        } else {
            for (&price, level) in self.books[symbol as usize].asks.iter() {
                if written >= n { break; }
                out[written] = TopLevel { price, qty: level.total_qty, count: level.count };
                written += 1;
            }
        }
        written
    }

    fn volume_near_best(&self, symbol: u16, side: i32, depth: i64) -> i64 {
        let mut total: i64 = 0;
        if side == 0 {
            let bids = &self.books[symbol as usize].bids;
            if let Some((&best, _)) = bids.iter().next_back() {
                let min_price = best - depth + 1;
                for (&price, level) in bids.iter().rev() {
                    if price < min_price { break; }
                    total += level.total_qty;
                }
            }
        } else {
            let asks = &self.books[symbol as usize].asks;
            if let Some((&best, _)) = asks.iter().next() {
                let max_price = best + depth - 1;
                for (&price, level) in asks.iter() {
                    if price > max_price { break; }
                    total += level.total_qty;
                }
            }
        }
        total
    }

    fn get_queue_position(&self, our_id: u64) -> QueuePosition {
        let Some(&exchange_id) = self.our_orders.get(&our_id) else {
            return QueuePosition { index: -1, qty_ahead: 0 };
        };
        let Some(order) = self.orders.get(&exchange_id) else {
            return QueuePosition { index: -1, qty_ahead: 0 };
        };
        let book = &self.books[order.symbol as usize];
        let levels = if order.side == 0 { &book.bids } else { &book.asks };
        let Some(level) = levels.get(&order.price) else {
            return QueuePosition { index: -1, qty_ahead: 0 };
        };
        let mut index: i32 = 0;
        let mut qty_ahead: i64 = 0;
        for &(eid, qty) in &level.queue {
            if eid == exchange_id {
                return QueuePosition { index, qty_ahead };
            }
            index += 1;
            qty_ahead += qty;
        }
        QueuePosition { index: -1, qty_ahead: 0 }
    }
}
