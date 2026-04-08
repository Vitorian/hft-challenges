// Challenge 07: String Map — Skeleton Implementation (Rust)
// Edit this file to implement your solution. You can do much better!

use crate::types::StringMap;
use std::collections::HashMap;

pub struct MyStringMap {
    map: HashMap<String, u32>,
}

impl StringMap for MyStringMap {
    fn new() -> Self {
        Self { map: HashMap::new() }
    }

    fn insert(&mut self, key: &[u8], value: u32) {
        let s = String::from_utf8(key.to_vec()).unwrap();
        self.map.insert(s, value);
    }

    fn find(&self, key: &[u8]) -> Option<u32> {
        let s = String::from_utf8(key.to_vec()).unwrap();
        self.map.get(&s).copied()
    }
}
