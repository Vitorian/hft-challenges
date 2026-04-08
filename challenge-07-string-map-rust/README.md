# Challenge 07: String Map (Rust)

## Problem

Implement a string-keyed hash map that supports insert and lookup.

- **`insert(key, value)`** — Insert a key-value pair (keys are unique)
- **`find(key)`** — Look up a key, return `Some(value)` or `None`

## Constraints

- Keys are byte slices, max 16 bytes
- Keys contain only uppercase ASCII (A-Z)
- Up to 100,000 entries
- Mixed workload: insert all, then lookup all

## Files

- `solution/mod.rs` — Your implementation (edit this)
- `src/types.rs` — The `StringMap` trait (do NOT modify)
- `src/main.rs` — Benchmark harness (do NOT modify)

## Limits

- Time: 60 seconds
- Memory: 512 MB
- Daily submissions: 5
