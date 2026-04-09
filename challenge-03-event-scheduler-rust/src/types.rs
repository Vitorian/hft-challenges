// Event Scheduler (with Cancel) trait — do NOT modify.

pub trait EventScheduler {
    fn new() -> Self;

    /// Schedule a one-shot event. If event_id already exists, replace it.
    fn schedule(&mut self, event_id: u64, time_us: i64);

    /// Cancel a pending event. Returns true if found and cancelled.
    fn cancel(&mut self, event_id: u64) -> bool;

    /// Advance clock. Fire all events with time <= new_time_us.
    /// Calls fire_fn(event_id, scheduled_time) for each.
    /// Returns number of events fired.
    fn advance(&mut self, new_time_us: i64, fire_fn: &mut dyn FnMut(u64, i64)) -> u32;

    /// Number of pending events.
    fn size(&self) -> u64;

    /// Time of next event, or i64::MAX if empty.
    fn next_event_time(&self) -> i64;
}
