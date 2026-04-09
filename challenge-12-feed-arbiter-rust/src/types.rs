// Feed Arbiter types — do NOT modify.

#[derive(Clone, Copy, Default)]
#[repr(C)]
pub struct Message {
    pub sequence: u64,
    pub timestamp: u64,
    pub price: i64,
    pub order_id: u32,
    pub quantity: i32,
    pub symbol_id: u16,
    pub msg_type: u8,
    pub side: u8,
    pub _pad: [u8; 8],
}

pub trait FeedArbiter {
    fn new() -> Self;

    /// Called once with the maximum expected sequence number. NOT timed.
    fn build(&mut self, max_sequence: u64);

    /// A message arrived. Deduplicate and emit in sequence order.
    /// Returns the number of in-order messages written to `out`.
    fn on_message(&mut self, msg: &Message, out: &mut [Message]) -> usize;
}
