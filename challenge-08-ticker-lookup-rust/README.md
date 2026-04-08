# Challenge 08: Ticker Lookup (Rust)

## Problem

Build a lookup table for ticker symbols. `build()` is called once (not timed), then `find()` is benchmarked.

- **`build(entries)`** — Receive all entries at once (not timed)
- **`find(symbol)`** — Look up a symbol, return `Some(value)` or `None`

## Constraints

- Symbols are byte slices, max 6 bytes, uppercase ASCII (A-Z)
- Up to 50,000 entries
- Only `find()` is timed — spend as long as you want in `build()`

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — The `TickerLookup` trait (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)

## Limits

- Time: 60 seconds
- Memory: 512 MB
- Daily submissions: 5
