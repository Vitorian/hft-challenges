// Challenge 11: Rolling Counter — Skeleton Implementation (Rust)
// Edit this file to implement your solution. You can do much better!

use crate::types::RollingCounter;
use std::collections::VecDeque;

pub struct MyRollingCounter {
    interval_ns: i64,
    current_time: i64,
    total: usize,
    events: VecDeque<(i64, usize)>,
}

impl RollingCounter for MyRollingCounter {
    fn new(interval_ns: i64, _precision_ns: i64) -> Self {
        Self {
            interval_ns,
            current_time: 0,
            total: 0,
            events: VecDeque::new(),
        }
    }

    fn update(&mut self, time_ns: i64) {
        self.current_time = time_ns;
        let cutoff = time_ns - self.interval_ns;
        while let Some(&(t, c)) = self.events.front() {
            if t <= cutoff {
                self.total -= c;
                self.events.pop_front();
            } else {
                break;
            }
        }
    }

    fn add_event(&mut self, count: usize) {
        self.events.push_back((self.current_time, count));
        self.total += count;
    }

    fn count(&self) -> usize {
        self.total
    }
}
