# Challenge 13: Implied Book (Rust)

Rust version of the challenge, with the same contract, workloads and scoring as the C++
version (`challenge-13-implied-book`). See `src/types.rs` for the trait definition and
`solution/mod.rs` for the reference behaviour.

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — Trait and types (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)

## Benchmarks

- **butterfly_3leg**: weights [+1, -2, +1], 5 levels (this one is the score)
- **spread_2leg**: weights [+1, -1], 5 levels
- **strip_3leg**: weights [+1, +1, +1], 5 levels

## Limits

- Time: 60 seconds
- Memory: 2048 MB
- Daily submissions: 5
