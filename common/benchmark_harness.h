#pragma once
// Common benchmark utilities for HFT University challenges.
// Do NOT modify this file — it will be overwritten during certified runs.
//
// TSC measurement follows Intel's "How to Benchmark Code Execution Times"
// (Intel white paper 324264-001) and AMD's CPUID specification:
//   START: CPUID (serialize) -> RDTSC (read counter)
//   END:   RDTSCP (read counter + serialize) -> CPUID (full drain)

#include <benchmark/benchmark.h>
#include <random>
#include <cstdint>

namespace hftu {

// Start measurement: CPUID serializes the pipeline, then RDTSC reads the counter.
// CPUID is a full serializing instruction — all prior instructions complete
// before it returns, and no subsequent instructions begin until it finishes.
inline uint64_t cycle_start() {
    uint32_t lo, hi;
    asm volatile(
        "cpuid\n\t"
        "rdtsc"
        : "=a"(lo), "=d"(hi)
        : "a"(0)           // CPUID leaf 0
        : "rbx", "rcx"     // CPUID clobbers ebx, ecx
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// End measurement: RDTSCP reads TSC and serializes (waits for prior instructions),
// then CPUID drains the pipeline so no later instructions leak into the window.
inline uint64_t cycle_end() {
    uint32_t lo, hi, aux;
    asm volatile(
        "rdtscp\n\t"
        "mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        "cpuid"
        : "=r"(lo), "=r"(hi), "=c"(aux)
        :
        : "rax", "rbx", "rdx"  // CPUID clobbers eax, ebx, ecx, edx
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

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
