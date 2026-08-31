# HFT University — Microbenchmark Challenges (C++ & Rust)

High-performance coding challenges with certified benchmark results.

## How It Works

1. **Subscribe** at [hftuniversity.com](https://hftuniversity.com/pricing) ($5/month)
2. **Create a private repo** and copy this template into it:
   ```bash
   git clone https://github.com/Vitorian/hft-challenges.git my-challenges
   cd my-challenges
   git remote set-url origin git@github.com:YOUR_USER/my-challenges.git
   git push -u origin main
   ```
3. **Set up** your repo at [hftuniversity.com/challenges/setup](https://hftuniversity.com/challenges/setup)
4. **Solve** challenges by editing files in each challenge's `solution/` directory
5. **Test locally** — the benchmark harness is self-contained, no external dependencies
6. **Submit** on the challenge page — our server builds and benchmarks your code on controlled hardware

## Challenge Index

| # | Challenge | C++ | Rust |
|---|-----------|-----|------|
| 01 | Order Book | `challenge-01-order-book` | `challenge-01-order-book-rust` |
| 02 | Multi-Symbol Order Book | `challenge-02-multi-book` | `challenge-02-multi-book-rust` |
| 03 | Event Scheduler | `challenge-03-event-scheduler` | `challenge-03-event-scheduler-rust` |
| 04 | Event Scheduler (No Cancel) | `challenge-04-event-scheduler` | `challenge-04-event-scheduler-rust` |
| 05 | SPSC Ring Buffer | `challenge-05-ring-buffer` | — |
| 06 | Seqlock | `challenge-06-seqlock` | — |
| 07 | String Map | `challenge-07-string-map` | `challenge-07-string-map-rust` |
| 08 | Ticker Lookup | `challenge-08-ticker-lookup` | `challenge-08-ticker-lookup-rust` |
| 09 | FIX Parser | `challenge-09-fix-parser` | `challenge-09-fix-parser-rust` |
| 10 | FIX Encoder | `challenge-10-fix-builder` | `challenge-10-fix-builder-rust` |
| 11 | Rolling Counter | `challenge-11-rolling-counter` | `challenge-11-rolling-counter-rust` |
| 12 | Feed Arbiter | `challenge-12-feed-arbiter` | `challenge-12-feed-arbiter-rust` |
| 13 | Implied Book | `challenge-13-implied-book` | `challenge-13-implied-book-rust` |
| 14 | Build Optimization (flags only) | `challenge-14-build-optimization` | — |
| 15 | Implied Volatility | `challenge-15-implied-vol` | `challenge-15-implied-vol-rust` |
| 16 | Monte Carlo Pricer | `challenge-16-monte-carlo` | `challenge-16-monte-carlo-rust` |
| 17 | Cross-CCX Ring Buffer | `challenge-17-cross-ccx-ring` | — |
| 18 | Build Optimization II (flags only) | `challenge-18-build-optimization-2` | — |
| 19 | Memory Triathlon | `challenge-19-memory-triathlon` | — |
| 20 | Fast Integer-to-String | `challenge-20-int-to-string` | — |

Challenges 05, 06 and 17 are multi-threaded and are not ported to Rust (they would
require `unsafe`, which is not allowed in Rust solutions). Full problem statements,
scoring goals and badge thresholds are on each challenge's page at
[hftuniversity.com/challenges](https://hftuniversity.com/challenges).

## Building Locally — C++

Requirements: GCC 13+ or Clang 17+, CMake 3.20+. The benchmark harness
(`common/benchmark_harness.h`) is a standalone header with no external dependencies.

```bash
cd challenge-01-order-book
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmark
```

## Building Locally — Rust

Requirements: a recent Rust toolchain (rustup stable is fine). Rust challenges use the
zero-dependency `common/hftu-bench` crate via a path dependency.

```bash
cd challenge-01-order-book-rust
cargo run --release
```

The binary is named `benchmark` (`cargo build --release` puts it at
`target/release/benchmark`).

> Local runs are for iterating on your solution. Local numbers are **not** comparable to
> certified scores — the server runs a private variant of each benchmark with fixed
> iteration counts and different seeds on isolated CPU cores.

## Rules

- Edit **only** files inside `solution/` directories
- **C++**: code is built with C++23 and `-O2 -march=native` (see `common/challenge.cmake`).
  No inline assembly, no compiler-specific intrinsics (unless a challenge states otherwise)
- **Rust**: solutions must be 100% safe Rust — `unsafe`, `std::process`, `std::net`,
  `std::fs`, `asm!` and FFI are rejected by the submission scanner
- [Pre-installed libraries](https://hftuniversity.com/challenges/libraries) (Boost, Abseil,
  TBB, etc.) are available for C++ — see the full list on the website
- Time limits, memory limits and daily submission limits are per-challenge — see the
  challenge's page on the website

## Getting New Challenges

Run the bundled sync script from your repo root:

```bash
./sync.sh
```

It fetches the latest scaffolding from this repo (harness, CMake files, benchmark
drivers, new challenge directories) while **never touching your `solution/` files**,
then stages the changes for you to review and commit. Don't use a raw
`git pull upstream main` — it will conflict against every solution you've edited.
