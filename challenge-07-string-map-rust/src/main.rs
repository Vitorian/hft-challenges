// Benchmark harness for Challenge 07: String Map (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::StringMap;

fn generate_keys(count: usize, max_len: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    let mut keys = Vec::with_capacity(count);
    for _ in 0..count {
        let len = 1 + rng.next_range(max_len as u64) as usize;
        let mut key = Vec::with_capacity(len);
        for _ in 0..len {
            key.push(b'A' + rng.next_range(26) as u8);
        }
        keys.push(key);
    }
    keys
}

fn main() {
    let mut harness = hftu_bench::Harness::new();

    harness.add_benchmark("BM_Solution", 200_000, |iterations| {
        let keys = generate_keys(100_000, 16, 0xDEAD);
        let mut total: u64 = 0;

        for _ in 0..iterations {
            let mut sm = solution::MyStringMap::new();
            let start = cycle_start();
            for (j, key) in keys.iter().enumerate() {
                sm.insert(key, j as u32);
            }
            for key in &keys {
                black_box(sm.find(key));
            }
            clobber();
            let end = cycle_end();
            total += end - start;
        }
        total
    }, 0);

    std::process::exit(harness.run());
}
