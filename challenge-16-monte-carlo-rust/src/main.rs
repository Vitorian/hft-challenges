mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{MCOption, MCResult, MonteCarloPricer};

fn generate_options(count: usize, seed: u64) -> Vec<MCOption> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let spot = 50.0 + rng.next_range(200) as f64;
        let strike = spot * (0.8 + rng.next_range(40) as f64 / 100.0);
        let option_type = rng.next_range(4) as i32;
        let barrier = if option_type >= 2 {
            if option_type == 2 { spot * 1.3 } else { spot * 0.7 }
        } else { 0.0 };
        MCOption {
            spot, strike, rate: 0.05, vol: 0.2, time_to_expiry: 1.0,
            barrier, option_type, is_call: rng.next_range(2) == 0,
        }
    }).collect()
}

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 100, |iterations| {
        let options = generate_options(100, 0xCAFE);
        let mut results = vec![MCResult::default(); options.len()];
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut pricer = solution::MyPricer::new();
            pricer.build(10_000, 100);
            let start = cycle_start();
            pricer.price_batch(&options, &mut results);
            clobber();
            let end = cycle_end();
            total += end - start;
            black_box(&results);
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
