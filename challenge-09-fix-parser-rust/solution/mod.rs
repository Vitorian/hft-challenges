// Challenge 09: FIX Parser — Skeleton (Rust)
// Naive byte-by-byte parsing. You can do much better!

use crate::types::{FixParser, ParsedOrder, SymbolEntry};
use std::collections::HashMap;

pub struct MyFixParser {
    symbols: HashMap<String, u32>,
}

impl FixParser for MyFixParser {
    fn new() -> Self { Self { symbols: HashMap::new() } }

    fn build(&mut self, entries: &[SymbolEntry]) {
        for e in entries {
            let s = String::from_utf8(e.symbol.to_vec()).unwrap();
            self.symbols.insert(s, e.id);
        }
    }

    fn parse_batch(&mut self, data: &[u8]) -> Vec<ParsedOrder> {
        let mut results = Vec::new();
        let mut pos = 0;
        while pos < data.len() {
            let mut order = ParsedOrder::default();
            order.valid = true;
            let mut checksum: u32 = 0;
            let msg_start = pos;

            // Parse tag=value\x01 pairs until tag 10 (checksum)
            loop {
                if pos >= data.len() { break; }
                let (tag, val_start, val_end, next) = parse_field(data, pos);
                // Accumulate checksum (all bytes including SOH up to tag 10)
                if tag != 10 {
                    for i in pos..next {
                        checksum = checksum.wrapping_add(data[i] as u32);
                    }
                }
                match tag {
                    35 => order.msg_type = data[val_start],
                    54 => order.side = (data[val_start] - b'0') as i8,
                    55 => {
                        let sym = String::from_utf8(data[val_start..val_end].to_vec()).unwrap_or_default();
                        order.symbol_id = self.symbols.get(&sym).copied().unwrap_or(0);
                    }
                    52 => order.timestamp = parse_int(data, val_start, val_end),
                    44 => order.price = parse_fixed_price(data, val_start, val_end),
                    38 => order.quantity = parse_int(data, val_start, val_end),
                    10 => {
                        let expected = parse_int(data, val_start, val_end) as u32;
                        if (checksum % 256) != expected {
                            order.valid = false;
                        }
                        pos = next;
                        break;
                    }
                    _ => {}
                }
                pos = next;
            }
            let _ = msg_start; // suppress warning
            results.push(order);
        }
        results
    }
}

fn parse_field(data: &[u8], start: usize) -> (u32, usize, usize, usize) {
    let mut i = start;
    let mut tag: u32 = 0;
    while i < data.len() && data[i] != b'=' {
        tag = tag * 10 + (data[i] - b'0') as u32;
        i += 1;
    }
    i += 1; // skip '='
    let val_start = i;
    while i < data.len() && data[i] != 1 { // SOH = 0x01
        i += 1;
    }
    let val_end = i;
    (tag, val_start, val_end, i + 1)
}

fn parse_int(data: &[u8], start: usize, end: usize) -> i64 {
    let mut val: i64 = 0;
    let mut neg = false;
    let mut i = start;
    if i < end && data[i] == b'-' { neg = true; i += 1; }
    while i < end {
        val = val * 10 + (data[i] - b'0') as i64;
        i += 1;
    }
    if neg { -val } else { val }
}

fn parse_fixed_price(data: &[u8], start: usize, end: usize) -> i64 {
    // Parse price with up to 8 decimal places -> fixed point * 10^8
    let mut integer: i64 = 0;
    let mut frac: i64 = 0;
    let mut frac_digits = 0;
    let mut in_frac = false;
    for i in start..end {
        if data[i] == b'.' {
            in_frac = true;
        } else if in_frac {
            frac = frac * 10 + (data[i] - b'0') as i64;
            frac_digits += 1;
        } else {
            integer = integer * 10 + (data[i] - b'0') as i64;
        }
    }
    while frac_digits < 8 { frac *= 10; frac_digits += 1; }
    integer * 100_000_000 + frac
}
