// FIX Parser types — do NOT modify.

#[derive(Clone, Copy, Default)]
pub struct ParsedOrder {
    pub msg_type: u8,      // b'D', b'8', b'F', b'G'
    pub side: i8,          // 1=Buy, 2=Sell
    pub symbol_id: u32,    // from build() mapping
    pub timestamp: i64,    // tag 52: nanoseconds
    pub price: i64,        // price * 100_000_000 (8 decimal places)
    pub quantity: i64,
    pub valid: bool,       // false if checksum failed
}

pub struct SymbolEntry<'a> {
    pub symbol: &'a [u8],
    pub id: u32,
}

pub trait FixParser {
    fn new() -> Self;
    /// Receive symbol -> ID mapping. NOT timed.
    fn build(&mut self, entries: &[SymbolEntry]);
    /// Parse concatenated FIX messages, return parsed orders.
    fn parse_batch(&mut self, data: &[u8]) -> Vec<ParsedOrder>;
}
