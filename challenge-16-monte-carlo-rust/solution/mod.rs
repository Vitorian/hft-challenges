// Challenge 16: Monte Carlo Pricer — Skeleton (Rust)
// Basic path simulation. You can do much better!

use crate::types::{MCOption, MCResult, MonteCarloPricer};
use hftu_bench::Rng;

pub struct MyPricer {
    paths: i32,
    steps: i32,
}

impl MonteCarloPricer for MyPricer {
    fn new() -> Self { Self { paths: 10000, steps: 100 } }

    fn build(&mut self, paths_per_option: i32, steps_per_path: i32) {
        self.paths = paths_per_option;
        self.steps = steps_per_path;
    }

    fn price_batch(&self, options: &[MCOption], out: &mut [MCResult]) {
        for (i, opt) in options.iter().enumerate() {
            out[i] = price_one(opt, self.paths, self.steps);
        }
    }
}

fn price_one(opt: &MCOption, paths: i32, steps: i32) -> MCResult {
    let dt = opt.time_to_expiry / steps as f64;
    let drift = (opt.rate - 0.5 * opt.vol * opt.vol) * dt;
    let diffusion = opt.vol * dt.sqrt();
    let mut rng = Rng::new(42);
    let mut sum = 0.0_f64;
    let mut sum_sq = 0.0_f64;

    for _ in 0..paths {
        let mut s = opt.spot;
        let mut s_sum = 0.0_f64;
        let mut breached = false;

        for _ in 0..steps {
            let z = box_muller(&mut rng);
            s *= (drift + diffusion * z).exp();
            s_sum += s;
            if opt.option_type == 2 && s >= opt.barrier { breached = true; }
            if opt.option_type == 3 && s <= opt.barrier { breached = true; }
        }

        let payoff = match opt.option_type {
            0 => european_payoff(s, opt.strike, opt.is_call),
            1 => european_payoff(s_sum / steps as f64, opt.strike, opt.is_call),
            2 | 3 => if breached { 0.0 } else { european_payoff(s, opt.strike, opt.is_call) },
            _ => 0.0,
        };
        sum += payoff;
        sum_sq += payoff * payoff;
    }

    let n = paths as f64;
    let mean = sum / n;
    let variance = (sum_sq / n - mean * mean) / n;
    let discount = (-opt.rate * opt.time_to_expiry).exp();
    MCResult {
        price: discount * mean,
        std_error: discount * variance.sqrt(),
    }
}

fn european_payoff(s: f64, k: f64, is_call: bool) -> f64 {
    if is_call { (s - k).max(0.0) } else { (k - s).max(0.0) }
}

fn box_muller(rng: &mut Rng) -> f64 {
    let u1 = (rng.next_u64() as f64 + 1.0) / (u64::MAX as f64 + 1.0);
    let u2 = (rng.next_u64() as f64 + 1.0) / (u64::MAX as f64 + 1.0);
    (-2.0 * u1.ln()).sqrt() * (2.0 * std::f64::consts::PI * u2).cos()
}
