// Monte Carlo Pricer types — do NOT modify.

#[derive(Clone, Copy)]
pub struct MCOption {
    pub spot: f64,
    pub strike: f64,
    pub rate: f64,
    pub vol: f64,
    pub time_to_expiry: f64,
    pub barrier: f64,       // 0 = not applicable
    pub option_type: i32,   // 0=European, 1=Asian, 2=BarrierUpOut, 3=BarrierDownOut
    pub is_call: bool,
}

#[derive(Clone, Copy, Default)]
pub struct MCResult {
    pub price: f64,
    pub std_error: f64,
}

pub trait MonteCarloPricer {
    fn new() -> Self;
    fn build(&mut self, paths_per_option: i32, steps_per_path: i32);
    fn price_batch(&self, options: &[MCOption], out: &mut [MCResult]);
}
