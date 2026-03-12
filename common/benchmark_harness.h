#pragma once
// HFT University benchmark harness — standalone, no external dependencies.
// Do NOT modify this file — it will be overwritten during certified runs.
//
// TSC measurement follows Intel's "How to Benchmark Code Execution Times"
// (Intel white paper 324264-001) and AMD's CPUID specification:
//   START: CPUID (serialize) -> RDTSC (read counter)
//   END:   RDTSCP (read counter + serialize) -> CPUID (full drain)

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include <algorithm>
#include <functional>

namespace hftu {

// ---------------------------------------------------------------------------
// TSC cycle measurement
// ---------------------------------------------------------------------------

// Start measurement: CPUID serializes the pipeline, then RDTSC reads the counter.
inline uint64_t cycle_start() {
    uint32_t lo, hi;
    asm volatile(
        "cpuid\n\t"
        "rdtsc"
        : "=a"(lo), "=d"(hi)
        : "a"(0)
        : "rbx", "rcx"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// End measurement: RDTSCP reads TSC and serializes, then CPUID drains.
inline uint64_t cycle_end() {
    uint32_t lo, hi, aux;
    asm volatile(
        "rdtscp\n\t"
        "mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        "cpuid"
        : "=r"(lo), "=r"(hi), "=c"(aux)
        :
        : "rax", "rbx", "rdx"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// ---------------------------------------------------------------------------
// Compiler barriers
// ---------------------------------------------------------------------------

// Prevent compiler from optimizing away a value
template <typename T>
inline void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

template <typename T>
inline void do_not_optimize(T& value) {
    asm volatile("" : "+r,m"(value) : : "memory");
}

// Prevent compiler from reordering memory operations across this point
inline void clobber() {
    asm volatile("" : : : "memory");
}

// ---------------------------------------------------------------------------
// Deterministic RNG
// ---------------------------------------------------------------------------

inline std::mt19937_64& rng() {
    static std::mt19937_64 gen(42);
    return gen;
}

// ---------------------------------------------------------------------------
// Benchmark runner
// ---------------------------------------------------------------------------

struct BenchmarkResult {
    const char* name;
    uint64_t ops_per_iteration;
    uint64_t iterations;
    uint64_t total_cycles;
};

// A benchmark function: given iteration count, returns total_cycles
using BenchmarkFn = std::function<uint64_t(int iterations)>;

struct BenchmarkDef {
    const char* name;
    uint64_t ops_per_iteration;
    BenchmarkFn fn;
};

inline std::vector<BenchmarkDef>& benchmark_registry() {
    static std::vector<BenchmarkDef> reg;
    return reg;
}

struct RegisterBenchmark {
    RegisterBenchmark(const char* name, uint64_t ops, BenchmarkFn fn) {
        benchmark_registry().push_back({name, ops, std::move(fn)});
    }
};

// Calibrate iteration count using TSC cycles.
// We want enough iterations for stable measurement — target ~1 billion cycles total.
inline int calibrate(const BenchmarkFn& fn) {
    // Warmup
    fn(1);

    // Time a single iteration in cycles
    uint64_t single_cycles = fn(1);

    // Target ~1 billion cycles total
    constexpr uint64_t TARGET_CYCLES = 1'000'000'000ULL;
    int n = static_cast<int>(TARGET_CYCLES / std::max(single_cycles, uint64_t(1)));
    n = std::max(n, 3);
    n = std::min(n, 1000);
    return n;
}

inline void run_benchmarks() {
    auto& reg = benchmark_registry();

    std::vector<BenchmarkResult> results;
    results.reserve(reg.size());

    for (auto& def : reg) {
        int iters = calibrate(def.fn);
        uint64_t cycles = def.fn(iters);
        results.push_back({def.name, def.ops_per_iteration,
                          static_cast<uint64_t>(iters), cycles});
    }

    // Output JSON
    std::printf("{\n  \"benchmarks\": [\n");
    for (size_t i = 0; i < results.size(); ++i) {
        auto& r = results[i];
        double cycles_per_op = static_cast<double>(r.total_cycles) /
            (static_cast<double>(r.iterations) * static_cast<double>(r.ops_per_iteration));

        std::printf("    {\n");
        std::printf("      \"name\": \"%s\",\n", r.name);
        std::printf("      \"iterations\": %lu,\n", r.iterations);
        std::printf("      \"ops_per_iteration\": %lu,\n", r.ops_per_iteration);
        std::printf("      \"total_cycles\": %lu,\n", r.total_cycles);
        std::printf("      \"cycles_per_op\": %.2f\n", cycles_per_op);
        std::printf("    }%s\n", (i + 1 < results.size()) ? "," : "");
    }
    std::printf("  ]\n}\n");
}

} // namespace hftu
