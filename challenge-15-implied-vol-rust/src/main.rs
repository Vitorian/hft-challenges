mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, check_failed, clobber, cycle_start, cycle_end, Rng};
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

// ---------------------------------------------------------------------------
// Correctness validation
// ---------------------------------------------------------------------------
// Rule: the resulting sigma must produce a Black-Scholes price within 1e-6 of the
// market price, and converged must be true only for valid results.
//   1. Truthful flag. A result reported converged must reprice within the
//      tolerance. A single false claim fails the run.
//   2. Coverage. At most MAX_UNSOLVED_PCT of a batch may be reported not
//      converged (the naive skeleton leaves a fraction of a percent).
// Checked on the timed batch, on a held-out batch, and on the output of every
// timed call. The certified run does the same with batches from private seeds.

const TOLERANCE: f64 = 1e-6;
const TOLERANCE_SLACK: f64 = 1e-9; // equivalent pricing formulas differ by rounding
const MAX_UNSOLVED_PCT: f64 = 1.0;

/// Black-Scholes price, the same formula as bs_price() in the public skeleton.
fn reference_price(c: &OptionContract, vol: f64) -> f64 {
    let sd = vol * c.time_to_expiry.sqrt();
    let d1 = ((c.spot / c.strike).ln() + (c.rate + 0.5 * vol * vol) * c.time_to_expiry) / sd;
    let d2 = d1 - sd;
    let discounted = c.strike * (-c.rate * c.time_to_expiry).exp();
    if c.is_call {
        c.spot * types::norm_cdf(d1) - discounted * types::norm_cdf(d2)
    } else {
        discounted * (1.0 - types::norm_cdf(d2)) - c.spot * (1.0 - types::norm_cdf(d1))
    }
}

/// Poisoned output: an entry the solution never writes reads as a converged NaN.
fn poisoned(n: usize) -> Vec<VolResult> {
    vec![VolResult { implied_vol: f64::NAN, converged: true }; n]
}

fn check_results(test: &str, batch: &[OptionContract], results: &[VolResult], quiet: bool) -> bool {
    let (mut unsolved, mut wrong, mut worst_index) = (0usize, 0usize, 0usize);
    let (mut worst_error, mut worst_price) = (0.0_f64, f64::NAN);
    for (i, (c, r)) in batch.iter().zip(results).enumerate() {
        if !r.converged {
            unsolved += 1;
            continue;
        }
        let mut price = f64::NAN;
        let mut error = f64::INFINITY;
        if r.implied_vol.is_finite() && r.implied_vol > 0.0 {
            price = reference_price(c, r.implied_vol);
            error = (price - c.market_price).abs();
        }
        if !(error <= TOLERANCE + TOLERANCE_SLACK) {
            wrong += 1;
            if !(error <= worst_error) {
                worst_error = error;
                worst_index = i;
                worst_price = price;
            }
        }
    }

    let mut ok = true;
    if wrong > 0 {
        let c = &batch[worst_index];
        check_failed(test, &format!(
            "{} of {} results are reported converged but do not reprice within 1e-6 \
             of the market price (worst error {:.3e})", wrong, batch.len(), worst_error));
        check_failed(test, &format!(
            "worst case: {} S={:.6} K={:.6} r={:.6} T={:.6}, market price {:.8}, \
             implied_vol {:.8e} reprices to {:.8}",
            if c.is_call { "call" } else { "put" }, c.spot, c.strike, c.rate, c.time_to_expiry,
            c.market_price, results[worst_index].implied_vol, worst_price));
        ok = false;
    }
    if unsolved as f64 * 100.0 > MAX_UNSOLVED_PCT * batch.len() as f64 {
        check_failed(test, &format!(
            "{} of {} results are reported not converged (at most {:.0}% may be left unsolved)",
            unsolved, batch.len(), MAX_UNSOLVED_PCT));
        ok = false;
    }
    if ok && !quiet {
        eprintln!("  {}: {} results checked, {} reported not converged", test, batch.len(), unsolved);
    }
    ok
}

fn solve_and_check(test: &str, batch: &[OptionContract]) -> bool {
    let mut results = poisoned(batch.len());
    let mut solver = solution::MySolver::new();
    solver.build();
    solver.solve_batch(batch, &mut results);
    check_results(test, batch, &results, false)
}

fn main() {
    const SCORED_SEED: u64 = 0xBEEF;
    const HELDOUT_SEED: u64 = 0x5EED15;
    const SCORED_COUNT: usize = 10_000;
    const HELDOUT_COUNT: usize = 100_000;

    let mut harness = hftu_bench::Harness::new();
    harness.add_validation("accuracy", || {
        solve_and_check("accuracy", &generate_contracts(SCORED_COUNT, SCORED_SEED))
    });
    harness.add_validation("accuracy_heldout", || {
        solve_and_check("accuracy_heldout", &generate_contracts(HELDOUT_COUNT, HELDOUT_SEED))
    });

    harness.add_benchmark("BM_Solution", SCORED_COUNT as u64, |iterations| {
        let contracts = generate_contracts(SCORED_COUNT, SCORED_SEED);
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut results = poisoned(contracts.len());
            let mut solver = solution::MySolver::new();
            solver.build();
            let start = cycle_start();
            solver.solve_batch(&contracts, &mut results);
            clobber();
            let end = cycle_end();
            total += end - start;
            black_box(&results);
            if !check_results("accuracy_timed", &contracts, &results, true) {
                println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
                std::process::exit(1);
            }
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
