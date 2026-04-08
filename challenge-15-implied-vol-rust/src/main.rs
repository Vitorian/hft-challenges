mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{ImpliedVolSolver, OptionContract, VolResult};

fn generate_contracts(count: usize, seed: u64) -> Vec<OptionContract> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let spot = 50.0 + rng.next_range(200) as f64;
        let strike = spot * (0.8 + rng.next_range(40) as f64 / 100.0);
        let vol = 0.1 + rng.next_range(50) as f64 / 100.0;
        let t = 0.1 + rng.next_range(20) as f64 / 10.0;
        let r = 0.01 + rng.next_range(10) as f64 / 100.0;
        let is_call = rng.next_range(2) == 0;
        let d1 = ((spot / strike).ln() + (r + 0.5 * vol * vol) * t) / (vol * t.sqrt());
        let d2 = d1 - vol * t.sqrt();
        let price = if is_call {
            spot * types::norm_cdf(d1) - strike * (-r * t).exp() * types::norm_cdf(d2)
        } else {
            strike * (-r * t).exp() * (1.0 - types::norm_cdf(d2)) - spot * (1.0 - types::norm_cdf(d1))
        };
        OptionContract { market_price: price, spot, strike, rate: r, time_to_expiry: t, is_call }
    }).collect()
}

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 10_000, |iterations| {
        let contracts = generate_contracts(10_000, 0xBEEF);
        let mut results = vec![VolResult::default(); contracts.len()];
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let solver = solution::MySolver::new();
            let start = cycle_start();
            solver.solve_batch(&contracts, &mut results);
            clobber();
            let end = cycle_end();
            total += end - start;
            black_box(&results);
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
