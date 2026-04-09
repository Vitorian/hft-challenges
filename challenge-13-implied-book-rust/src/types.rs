// Implied Book types — do NOT modify.

#[derive(Clone, Copy, Default)]
pub struct Level {
    pub price: i64,
    pub quantity: i64,
}

#[derive(Clone, Copy)]
pub struct BookUpdate {
    pub leg_index: u16,
    pub side: i8,       // 0=bid, 1=ask
    pub action: u8,     // 0=add, 1=modify, 2=delete
    pub level: Level,
}

pub trait ImpliedBook {
    fn new() -> Self;

    /// Define the synthetic instrument.
    /// weights: per-leg multiplier (e.g. [+1, -2, +1] for butterfly)
    /// depth: number of output levels per side
    /// NOT timed.
    fn build(&mut self, weights: &[i32], depth: usize);

    /// Apply an update to one outright book.
    /// Returns (num_bids, num_asks) written to out_bids/out_asks.
    fn on_update(&mut self, update: &BookUpdate,
                 out_bids: &mut [Level], out_asks: &mut [Level]) -> (usize, usize);
}
