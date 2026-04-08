// Challenge 04: Event Scheduler (No Cancel) — Skeleton Implementation (Rust)
// Edit this file to implement your solution. You can do much better!

use crate::types::EventScheduler;
use std::collections::BTreeMap;

pub struct MyEventScheduler {
    map: BTreeMap<i64, Vec<usize>>,
    size: u64,
}

impl EventScheduler for MyEventScheduler {
    fn new() -> Self {
        Self { map: BTreeMap::new(), size: 0 }
    }

    fn schedule(&mut self, event_idx: usize, time_ns: i64) {
        self.map.entry(time_ns).or_insert_with(Vec::new).push(event_idx);
        self.size += 1;
    }

    fn advance(&mut self, new_time_ns: i64, fire_fn: &mut dyn FnMut(usize, i64)) -> u32 {
        let mut fired: u32 = 0;
        while let Some((&time, _)) = self.map.iter().next() {
            if time > new_time_ns { break; }
            let events = self.map.remove(&time).unwrap();
            for idx in events {
                fire_fn(idx, time);
                fired += 1;
                self.size -= 1;
            }
        }
        fired
    }

    fn size(&self) -> u64 { self.size }

    fn next_event_time(&self) -> i64 {
        self.map.keys().next().copied().unwrap_or(i64::MAX)
    }
}
