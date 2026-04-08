# Challenge 01: Order Book (Rust)

## Problem

Implement a limit order book that supports:

- **`add_order(id, side, price, quantity)`** — Insert a new order
- **`cancel_order(id)`** — Remove an order by ID
- **`best_bid()`** — Return the highest bid price (or 0 if empty)
- **`best_ask()`** — Return the lowest ask price (or 0 if empty)

## Constraints

- Prices are integers in the range `[1, 1_000_000]`
- Quantities are integers in the range `[1, 10_000]`
- Order IDs are unique `u64` values
- Side is `0` for buy (bid), `1` for sell (ask)
- Up to 1,000,000 orders may be active at once

## What We Benchmark

- Mixed workload: 60% adds, 20% cancels, 10% best_bid, 10% best_ask
- Measured in x86-64 CPU cycles per operation via `rdtscp` (lower is better)

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — The `OrderBook` trait (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)
- `Cargo.toml` — Build config (do NOT modify)

## Limits

- Time: 60 seconds
- Memory: 512 MB
- Daily submissions: 5
