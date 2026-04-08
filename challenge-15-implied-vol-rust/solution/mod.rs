// Challenge 15: Implied Volatility — Skeleton (Rust)
// Basic Newton-Raphson on Black-Scholes. You can do much better!

use crate::types::{ImpliedVolSolver, OptionContract, VolResult, norm_cdf, norm_pdf};

pub struct MySolver;

impl ImpliedVolSolver for MySolver {
    fn new() -> Self { Self }

    fn solve_batch(&self, contracts: &[OptionContract], out: &mut [VolResult]) {
        for (i, c) in contracts.iter().enumerate() {
            out[i] = solve_one(c);
        }
    }
}

fn solve_one(c: &OptionContract) -> VolResult {
    let mut vol = 0.2_f64;
    for _ in 0..100 {
        let price = bs_price(c.spot, c.strike, c.rate, c.time_to_expiry, vol, c.is_call);
        let vega = bs_vega(c.spot, c.strike, c.rate, c.time_to_expiry, vol);
        if vega.abs() < 1e-20 { break; }
        let diff = price - c.market_price;
        if diff.abs() < 1e-10 {
            return VolResult { implied_vol: vol, converged: true };
        }
        vol -= diff / vega;
        if vol <= 0.0 { vol = 0.001; }
    }
    VolResult { implied_vol: vol, converged: false }
}

fn bs_price(s: f64, k: f64, r: f64, t: f64, v: f64, is_call: bool) -> f64 {
    let d1 = ((s / k).ln() + (r + 0.5 * v * v) * t) / (v * t.sqrt());
    let d2 = d1 - v * t.sqrt();
    if is_call {
        s * norm_cdf(d1) - k * (-r * t).exp() * norm_cdf(d2)
    } else {
        k * (-r * t).exp() * (1.0 - norm_cdf(d2)) - s * (1.0 - norm_cdf(d1))
    }
}

fn bs_vega(s: f64, k: f64, r: f64, t: f64, v: f64) -> f64 {
    let d1 = ((s / k).ln() + (r + 0.5 * v * v) * t) / (v * t.sqrt());
    s * t.sqrt() * norm_pdf(d1)
}
