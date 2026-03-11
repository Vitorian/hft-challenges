#pragma once
// Common benchmark utilities for HFT University challenges.
// Do NOT modify this file — it will be overwritten during certified runs.

#include <benchmark/benchmark.h>
#include <random>
#include <cstdint>

namespace hftu {

// Deterministic RNG for reproducible benchmarks
inline std::mt19937_64& rng() {
    static std::mt19937_64 gen(42);
    return gen;
}

// Prevent compiler from optimizing away a value
template <typename T>
inline void do_not_optimize(T&& value) {
    benchmark::DoNotOptimize(std::forward<T>(value));
}

// Prevent compiler from reordering across this point
inline void clobber() {
    benchmark::ClobberMemory();
}

} // namespace hftu
