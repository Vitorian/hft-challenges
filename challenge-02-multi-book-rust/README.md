# Challenge 02: Multi-Symbol Order Book (Rust)

## Problem

Build a multi-symbol order book with queue position tracking and venue interaction.

### Features

- **200 symbols** with hot/cold traffic distribution (50 symbols generate 90% of traffic)
- **Venue interaction** — send/modify/cancel our orders through a venue, track exchange IDs
- **Queue position** — track FIFO position and qty ahead for our orders
- **Modify semantics** — qty-down keeps queue position, price change or qty-up loses it
- **Level queries** — best bid/ask with qty+count, top N levels, volume near best

## Constraints

- Prices are integers in the range `[1, 100_000]`
- Quantities are integers in the range `[1, 10_000]`
- Up to 200 symbols, 1M+ active orders
- Side is `0` for buy (bid), `1` for sell (ask)

## What We Benchmark

- Mixed workload: 35% add, 20% cancel, 10% modify, 10% best_bid/ask, 5% send_our, 5% top_levels, 5% volume_near_best, 5% queue_position, 3% modify_our, 2% cancel_our
- 500,000 orders pre-filled before timing
- Measured in x86-64 CPU cycles per operation via `rdtscp` (lower is better)

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — The `MultiOrderBook` trait and shared types (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)
- `Cargo.toml` — Build config (do NOT modify)

## Limits

- Time: 120 seconds
- Memory: 1024 MB
- Daily submissions: 10
