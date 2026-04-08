// Benchmark harness for Challenge 08: Ticker Lookup (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{TickerEntry, TickerLookup};

fn generate_symbols(count: usize, max_len: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    let mut syms = Vec::with_capacity(count);
    for _ in 0..count {
        let len = 1 + rng.next_range(max_len as u64) as usize;
        let mut s = Vec::with_capacity(len);
        for _ in 0..len {
            s.push(b'A' + rng.next_range(26) as u8);
        }
        syms.push(s);
    }
    syms
}

fn main() {
    let mut harness = hftu_bench::Harness::new();

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let symbols = generate_symbols(50_000, 6, 0xBEEF);
        let entries: Vec<TickerEntry> = symbols.iter().enumerate()
            .map(|(i, s)| TickerEntry { symbol: s, value: i as u32 })
            .collect();

        let mut total: u64 = 0;

        for _ in 0..iterations {
            let mut tl = solution::MyTickerLookup::new();
            tl.build(&entries);

            let start = cycle_start();
            for sym in &symbols {
                black_box(tl.find(sym));
            }
            clobber();
            let end = cycle_end();
            total += end - start;
        }
        total
    }, 0);

    std::process::exit(harness.run());
}
