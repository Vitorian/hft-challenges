// Benchmark harness for Challenge 11: Rolling Counter (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::RollingCounter;

fn main() {
    let mut harness = hftu_bench::Harness::new();

    // Standard: 1s window, 1ms precision, ~1000 events/sec
    harness.add_benchmark("BM_Solution", 1_000_000, |iterations| {
        let mut total: u64 = 0;

        for _ in 0..iterations {
            let mut rc = solution::MyRollingCounter::new(1_000_000_000, 1_000_000);
            let mut rng = Rng::new(0xABCD);
            let mut t: i64 = 0;

            let start = cycle_start();
            for _ in 0..1_000_000 {
                t += 1_000_000 + rng.next_range(500_000) as i64;
                rc.update(t);
                rc.add_event(rng.next_range(6) as usize);
                black_box(rc.count());
            }
            clobber();
            let end = cycle_end();
            total += end - start;
        }
        total
    }, 0);

    std::process::exit(harness.run());
}
