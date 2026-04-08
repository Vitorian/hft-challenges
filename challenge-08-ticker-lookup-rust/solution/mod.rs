// Challenge 08: Ticker Lookup — Skeleton Implementation (Rust)
// Edit this file to implement your solution. You can do much better!

use crate::types::{TickerEntry, TickerLookup};
use std::collections::HashMap;

pub struct MyTickerLookup {
    map: HashMap<String, u32>,
}

impl TickerLookup for MyTickerLookup {
    fn new() -> Self {
        Self { map: HashMap::new() }
    }

    fn build(&mut self, entries: &[TickerEntry]) {
        for e in entries {
            let s = String::from_utf8(e.symbol.to_vec()).unwrap();
            self.map.insert(s, e.value);
        }
    }

    fn find(&self, symbol: &[u8]) -> Option<u32> {
        let s = String::from_utf8(symbol.to_vec()).unwrap();
        self.map.get(&s).copied()
    }
}
