# Challenge 15: Implied Volatility (Rust)

## Problem

Solve for implied volatility using Black-Scholes. Batch solve implied vol via Newton-Raphson.

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — Trait and types (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)

## Limits

- Time: 60 seconds
- Memory: 512 MB
- Daily submissions: 5

## Validation

Every result reported `converged` is repriced with Black-Scholes (the formula
in the skeleton) and must come within 1e-6 of the market price; a single false
claim fails the run. At most 1% of a batch may be reported not converged.

The checks run on the timed batch (after every timed call) and on a held-out
batch of 100,000 contracts; a failed check fails the run. The certified run
does the same with batches from private seeds.
