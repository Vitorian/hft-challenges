// Rolling Counter trait — your solution must implement this.
// Do NOT modify this file — it will be overwritten during certified runs.

pub trait RollingCounter {
    /// Create a new rolling counter.
    /// interval_ns: window size in nanoseconds
    /// precision_ns: acceptable error window in nanoseconds
    fn new(interval_ns: i64, precision_ns: i64) -> Self;

    /// Advance clock. time_ns is guaranteed monotonically increasing.
    fn update(&mut self, time_ns: i64);

    /// Add count events at the current time.
    fn add_event(&mut self, count: usize);

    /// Return total events in the rolling window.
    fn count(&self) -> usize;
}
