// Challenge 10: FIX Encoder — Skeleton (Rust)
// Naive string formatting. You can do much better!

use crate::types::{FixBuilder, OrderFields, TemplateField};

pub struct MyFixBuilder {
    template: Vec<(i32, Vec<u8>)>,
    buffer: Vec<u8>,
}

impl FixBuilder for MyFixBuilder {
    fn new() -> Self {
        Self { template: Vec::new(), buffer: Vec::new() }
    }

    fn build(&mut self, fields: &[TemplateField]) {
        self.template = fields.iter().map(|f| (f.tag, f.value.clone())).collect();
    }

    fn encode(&mut self, order: &OrderFields) -> &[u8] {
        self.buffer.clear();

        for (tag, default_val) in &self.template {
            let tag_str = format!("{}", tag);
            self.buffer.extend_from_slice(tag_str.as_bytes());
            self.buffer.push(b'=');

            match *tag {
                52 => self.buffer.extend_from_slice(format!("{}", order.timestamp).as_bytes()),
                34 => self.buffer.extend_from_slice(format!("{}", order.seq_num).as_bytes()),
                11 => self.buffer.extend_from_slice(format!("{}", order.cl_ord_id).as_bytes()),
                44 => {
                    let int_part = order.price / 100_000_000;
                    let frac_part = (order.price % 100_000_000).abs();
                    self.buffer.extend_from_slice(format!("{}.{:08}", int_part, frac_part).as_bytes());
                }
                38 => self.buffer.extend_from_slice(format!("{}", order.quantity).as_bytes()),
                54 => self.buffer.push(b'0' + order.side as u8),
                _ => self.buffer.extend_from_slice(default_val),
            }
            self.buffer.push(1); // SOH
        }

        // Checksum (tag 10)
        let sum: u32 = self.buffer.iter().map(|&b| b as u32).sum();
        let cksum = format!("10={:03}\x01", sum % 256);
        self.buffer.extend_from_slice(cksum.as_bytes());

        &self.buffer
    }
}
