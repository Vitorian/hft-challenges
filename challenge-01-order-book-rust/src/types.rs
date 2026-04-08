// Order book trait — your solution must implement this.
// Do NOT modify this file — it will be overwritten during certified runs.

pub trait OrderBook {
    /// Create a new empty order book.
    fn new() -> Self;

    /// Add a new order. side: 0=buy(bid), 1=sell(ask).
    fn add_order(&mut self, id: u64, side: i32, price: i64, quantity: i64);

    /// Cancel an order by ID. No-op if ID doesn't exist.
    fn cancel_order(&mut self, id: u64);

    /// Return highest bid price, or 0 if no bids.
    fn best_bid(&self) -> i64;

    /// Return lowest ask price, or 0 if no asks.
    fn best_ask(&self) -> i64;
}
