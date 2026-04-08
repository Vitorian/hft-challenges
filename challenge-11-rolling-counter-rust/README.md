# Challenge 11: Rolling Counter (Rust)

## Problem

Maintain a sliding window count of events.

- **`update(time_ns)`** — Advance clock (monotonically increasing)
- **`add_event(count)`** — Add events at the current time
- **`count()`** — Return total events in the rolling window

## Constraints

- Window size: 1 second (1,000,000,000 ns)
- Precision: 1 millisecond (1,000,000 ns) — approximate counting is OK
- Time steps: ~1ms with jitter
- 1,000,000 operations per benchmark iteration

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — The `RollingCounter` trait (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)

## Limits

- Time: 60 seconds
- Memory: 512 MB
- Daily submissions: 5
