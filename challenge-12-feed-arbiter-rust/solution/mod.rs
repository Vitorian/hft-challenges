// Challenge 12: Feed Arbiter — Skeleton (Rust)
// Naive BTreeMap buffer. You can do much better!

use crate::types::{FeedArbiter, Message};
use std::collections::BTreeMap;

pub struct MyFeedArbiter {
    next_expected: u64,
    buffer: BTreeMap<u64, Message>,
}

impl FeedArbiter for MyFeedArbiter {
    fn new() -> Self {
        Self { next_expected: 1, buffer: BTreeMap::new() }
    }

    fn build(&mut self, _max_sequence: u64) {
        self.next_expected = 1;
        self.buffer.clear();
    }

    fn on_message(&mut self, msg: &Message, out: &mut [Message]) -> usize {
        let seq = msg.sequence;

        // Duplicate or old — ignore
        if seq < self.next_expected || self.buffer.contains_key(&seq) {
            return 0;
        }

        // In order — emit directly
        if seq == self.next_expected {
            out[0] = *msg;
            self.next_expected += 1;

            // Flush buffered
            let mut count = 1;
            while let Some((&next_seq, _)) = self.buffer.iter().next() {
                if next_seq != self.next_expected { break; }
                out[count] = self.buffer.remove(&next_seq).unwrap();
                self.next_expected += 1;
                count += 1;
            }
            return count;
        }

        // Out of order — buffer
        self.buffer.insert(seq, *msg);
        0
    }
}
