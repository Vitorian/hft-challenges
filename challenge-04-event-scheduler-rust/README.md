# Challenge 04: Event Scheduler — No Cancel (Rust)

## Problem

Implement an event scheduler that supports schedule and advance (no cancel).

- **`schedule(event_idx, time_ns)`** — Schedule an event to fire at a given time
- **`advance(new_time_ns, fire_fn)`** — Fire all events with time <= new_time_ns
- **`size()`** — Number of pending events
- **`next_event_time()`** — Time of next event, or i64::MAX if empty

## Constraints

- Events are identified by index (usize), not pointers
- Time is monotonically increasing in nanoseconds
- Up to 500,000 prefilled events
- P99 latency is the scored metric

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — The `EventScheduler` trait (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)

## Limits

- Time: 60 seconds
- Memory: 512 MB
- Daily submissions: 5
