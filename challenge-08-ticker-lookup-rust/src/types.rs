// Ticker Lookup trait — your solution must implement this.
// Do NOT modify this file — it will be overwritten during certified runs.

pub struct TickerEntry<'a> {
    pub symbol: &'a [u8],  // max 6 bytes
    pub value: u32,
}

pub trait TickerLookup {
    fn new() -> Self;

    /// Receive all entries at once. Called exactly once. NOT timed.
    fn build(&mut self, entries: &[TickerEntry]);

    /// Look up a symbol. Returns Some(value) if found, None otherwise.
    fn find(&self, symbol: &[u8]) -> Option<u32>;
}
