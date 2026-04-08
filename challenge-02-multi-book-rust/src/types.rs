// Shared types and venue interface for Challenge 02: Multi-Symbol Order Book
// Do NOT modify this file — it will be overwritten during certified runs.

pub const NUM_SYMBOLS: usize = 200;

/// Market data level
#[derive(Clone, Copy, Default)]
pub struct TopLevel {
    pub price: i64,
    pub qty: i64,
    pub count: i32,
}

/// Queue position for one of our orders
#[derive(Clone, Copy, Default)]
pub struct QueuePosition {
    pub index: i32,      // orders ahead of us (-1 = not in book yet)
    pub qty_ahead: i64,  // total qty of orders ahead
}

/// Venue interface — provided by the benchmark harness.
/// The user's MultiOrderBook calls these to interact with the exchange.
pub trait Venue {
    /// Send our order to the exchange. Returns exchange-assigned order ID.
    fn send_order(&mut self, our_id: u64, symbol: u16, side: i32,
                  price: i64, qty: i64) -> u64;

    /// Modify our order. Returns exchange ID:
    ///   - Same ID if qty decreased only (keeps queue position)
    ///   - New ID if price changed or qty increased (loses position)
    fn modify_order(&mut self, exchange_id: u64, new_price: i64, new_qty: i64) -> u64;

    /// Cancel our order on the exchange.
    fn cancel_order(&mut self, exchange_id: u64);
}

/// Trait for the multi-symbol order book.
pub trait MultiOrderBook {
    /// Create a new order book connected to a venue.
    /// The venue is passed as a Box so the book owns it.
    fn new(venue: Box<dyn Venue>) -> Self;

    // === Our order management ===
    fn send_order(&mut self, our_id: u64, symbol: u16, side: i32, price: i64, qty: i64);
    fn modify_our_order(&mut self, our_id: u64, new_price: i64, new_qty: i64);
    fn cancel_our_order(&mut self, our_id: u64);

    // === Exchange feed ===
    fn add_order(&mut self, exchange_id: u64, symbol: u16, side: i32, price: i64, qty: i64);
    fn modify_order(&mut self, exchange_id: u64, new_qty: i64);
    fn cancel_order(&mut self, exchange_id: u64);

    // === Queries ===
    fn best_bid(&self, symbol: u16) -> TopLevel;
    fn best_ask(&self, symbol: u16) -> TopLevel;
    /// Write up to n best levels into out[]. Returns levels written.
    fn get_top_levels(&self, symbol: u16, side: i32, n: usize, out: &mut [TopLevel]) -> usize;
    /// Total qty within `depth` ticks of best price.
    fn volume_near_best(&self, symbol: u16, side: i32, depth: i64) -> i64;
    /// Queue position for one of our orders.
    fn get_queue_position(&self, our_id: u64) -> QueuePosition;
}
