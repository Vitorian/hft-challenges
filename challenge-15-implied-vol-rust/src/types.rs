// Implied Volatility types — do NOT modify.

#[derive(Clone, Copy)]
pub struct OptionContract {
    pub market_price: f64,
    pub spot: f64,
    pub strike: f64,
    pub rate: f64,
    pub time_to_expiry: f64,
    pub is_call: bool,
}

#[derive(Clone, Copy, Default)]
pub struct VolResult {
    pub implied_vol: f64,
    pub converged: bool,
}

pub trait ImpliedVolSolver {
    fn new() -> Self;
    fn build(&mut self) {}
    fn solve_batch(&self, contracts: &[OptionContract], out: &mut [VolResult]);
}

// Math helpers — safe wrappers around libc erfc (same as C++ std::erfc)
extern "C" { fn erfc(x: f64) -> f64; }

/// Standard normal CDF
pub fn norm_cdf(x: f64) -> f64 {
    0.5 * unsafe { erfc(-x * std::f64::consts::FRAC_1_SQRT_2) }
}

/// Standard normal PDF
pub fn norm_pdf(x: f64) -> f64 {
    (1.0 / (2.0 * std::f64::consts::PI).sqrt()) * (-0.5 * x * x).exp()
}
