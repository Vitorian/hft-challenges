// Event Scheduler (No Cancel) trait — your solution must implement this.
// Do NOT modify this file — it will be overwritten during certified runs.

/// Event scheduler with no cancel support.
/// Events are identified by index (usize) into an external pool.
pub trait EventScheduler {
    fn new() -> Self;

    /// Schedule an event to fire at the given time.
    fn schedule(&mut self, event_idx: usize, time_ns: i64);

    /// Advance time. Fires all events with time <= new_time_ns.
    /// Calls `fire_fn(event_idx, time_ns)` for each fired event.
    /// Returns the number of events fired.
    fn advance(&mut self, new_time_ns: i64, fire_fn: &mut dyn FnMut(usize, i64)) -> u32;

    /// Number of pending events.
    fn size(&self) -> u64;

    /// Time of the next event, or i64::MAX if empty.
    fn next_event_time(&self) -> i64;
}
