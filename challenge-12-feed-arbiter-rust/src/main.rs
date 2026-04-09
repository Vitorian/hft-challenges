mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{FeedArbiter, Message};

fn generate_messages(count: usize, seed: u64) -> Vec<Message> {
    let mut rng = Rng::new(seed);
    let mut msgs: Vec<Message> = (1..=count as u64).map(|seq| {
        Message {
            sequence: seq,
            timestamp: 1_000_000_000 + seq * 1000,
            price: rng.next_i64_range(100_00000000, 999_00000000),
            order_id: rng.next_range(1_000_000) as u32,
            quantity: rng.next_i64_range(1, 10_000) as i32,
            symbol_id: rng.next_range(200) as u16,
            msg_type: [b'A', b'X', b'E', b'P'][rng.next_range(4) as usize],
            side: rng.next_range(2) as u8,
            _pad: [0; 8],
        }
    }).collect();
    // Shuffle to create out-of-order delivery
    for i in (1..msgs.len()).rev() {
        let j = rng.next_range((i + 1) as u64) as usize;
        msgs.swap(i, j);
    }
    // Add ~10% duplicates
    let dup_count = count / 10;
    for _ in 0..dup_count {
        let idx = rng.next_range(msgs.len() as u64) as usize;
        msgs.push(msgs[idx]);
    }
    msgs
}

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let msgs = generate_messages(100_000, 0xBEEF);
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut arb = solution::MyFeedArbiter::new();
            arb.build(100_000);
            let mut out = vec![Message::default(); 100_000];
            let start = cycle_start();
            for msg in &msgs {
                let n = arb.on_message(msg, &mut out);
                black_box(n);
            }
            clobber();
            let end = cycle_end();
            total += end - start;
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
