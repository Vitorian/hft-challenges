# HFT University — C++ Microbenchmark Challenges

High-performance C++ coding challenges with certified benchmark results.

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
5. **Test locally** using CMake + Google Benchmark
6. **Submit** on the challenge page — our server builds and benchmarks your code on controlled hardware

## Building Locally

Requirements: GCC 13+ or Clang 17+, CMake 3.20+, Google Benchmark

```bash
cd challenge-01-order-book
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmark
```

## Rules

- Edit **only** files inside `solution/` directories
- Your code must compile with `-std=c++20 -O2`
- No inline assembly, no compiler-specific intrinsics (unless stated otherwise)
- Standard library only (no Boost, no external dependencies)
- Time limit and memory limit are per-challenge (see each challenge's README)

## Getting New Challenges

When new challenges are published, pull from upstream:

```bash
git remote add upstream https://github.com/Vitorian/hft-challenges.git
git pull upstream main
```
