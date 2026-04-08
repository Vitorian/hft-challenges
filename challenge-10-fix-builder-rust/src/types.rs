// FIX Encoder types — do NOT modify.

pub struct TemplateField {
    pub tag: i32,
    pub value: Vec<u8>,
}

#[derive(Clone, Copy)]
pub struct OrderFields {
    pub timestamp: i64,     // tag 52
    pub seq_num: u64,       // tag 34
    pub cl_ord_id: u64,     // tag 11
    pub price: i64,         // tag 44: fixed-point * 10^8
    pub quantity: i64,      // tag 38
    pub side: i8,           // tag 54: 1=Buy, 2=Sell
}

pub trait FixBuilder {
    fn new() -> Self;
    /// Receive template as tag-value pairs. NOT timed.
    fn build(&mut self, fields: &[TemplateField]);
    /// Produce a valid FIX message. Returned slice valid until next encode().
    fn encode(&mut self, order: &OrderFields) -> &[u8];
}
