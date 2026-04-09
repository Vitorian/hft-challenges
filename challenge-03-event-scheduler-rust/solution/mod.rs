// Challenge 03: Event Scheduler (with Cancel) — Skeleton (Rust)
// Naive BTreeMap + HashMap. You can do much better!

use crate::types::EventScheduler;
use std::collections::{BTreeMap, HashMap};

pub struct MyEventScheduler {
    timeline: BTreeMap<i64, Vec<u64>>,
    lookup: HashMap<u64, i64>,  // event_id -> time
}

impl EventScheduler for MyEventScheduler {
    fn new() -> Self {
        Self { timeline: BTreeMap::new(), lookup: HashMap::new() }
    }

    fn schedule(&mut self, event_id: u64, time_us: i64) {
        // If already scheduled, cancel first
        if let Some(&old_time) = self.lookup.get(&event_id) {
            if let Some(events) = self.timeline.get_mut(&old_time) {
                events.retain(|&id| id != event_id);
                if events.is_empty() { self.timeline.remove(&old_time); }
            }
        }
        self.timeline.entry(time_us).or_insert_with(Vec::new).push(event_id);
        self.lookup.insert(event_id, time_us);
    }

    fn cancel(&mut self, event_id: u64) -> bool {
        if let Some(time) = self.lookup.remove(&event_id) {
            if let Some(events) = self.timeline.get_mut(&time) {
                events.retain(|&id| id != event_id);
                if events.is_empty() { self.timeline.remove(&time); }
            }
            true
        } else {
            false
        }
    }

    fn advance(&mut self, new_time_us: i64, fire_fn: &mut dyn FnMut(u64, i64)) -> u32 {
        let mut fired: u32 = 0;
        while let Some((&time, _)) = self.timeline.iter().next() {
            if time > new_time_us { break; }
            let events = self.timeline.remove(&time).unwrap();
            for id in events {
                self.lookup.remove(&id);
                fire_fn(id, time);
                fired += 1;
            }
        }
        fired
    }

    fn size(&self) -> u64 { self.lookup.len() as u64 }

    fn next_event_time(&self) -> i64 {
        self.timeline.keys().next().copied().unwrap_or(i64::MAX)
    }
}
