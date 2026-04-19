#pragma once
// HFT University benchmark harness — standalone, no external dependencies.
// Do NOT modify this file — it will be overwritten during certified runs.
//
// TSC measurement follows Intel's "How to Benchmark Code Execution Times"
// (Intel white paper 324264-001) and AMD's CPUID specification:
//   START: CPUID (serialize) -> RDTSC (read counter)
//   END:   RDTSCP (read counter + serialize) -> CPUID (full drain)

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
namespace hftu {

// ---------------------------------------------------------------------------
// CPU affinity — no-op stubs for local development.
// The certified benchmark on the server uses its own pinning infrastructure.
// ---------------------------------------------------------------------------

inline bool pin_thread(int) { return false; }
inline int pin_to_isolated(int) { return -1; }

// ---------------------------------------------------------------------------
// TSC cycle measurement
// On x86: CPUID+RDTSC/RDTSCP+CPUID per Intel white paper 324264-001.
// On other platforms: fallback to clock_gettime (nanoseconds, not cycles).
// Scores from non-x86 platforms are NOT comparable to certified scores.
// ---------------------------------------------------------------------------

#if defined(__x86_64__) || defined(_M_X64)

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

#elif defined(__aarch64__) || defined(_M_ARM64)

// ARM64: use the Performance Monitors cycle counter (PMCCNTR_EL0).
// Falls back to clock_gettime if the counter is not accessible.
#include <time.h>

inline uint64_t _arm64_rdtsc() {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

inline uint64_t cycle_start() { return _arm64_rdtsc(); }
inline uint64_t cycle_end()   { return _arm64_rdtsc(); }

#else

// Generic fallback: nanosecond wall clock. Not cycle-accurate.
#include <time.h>

inline uint64_t cycle_start() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_nsec;
}

inline uint64_t cycle_end() { return cycle_start(); }

#endif

// ---------------------------------------------------------------------------
// Compiler barriers
// ---------------------------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)

template <typename T>
inline void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

template <typename T>
inline void do_not_optimize(T& value) {
    asm volatile("" : "+r,m"(value) : : "memory");
}

inline void clobber() {
    asm volatile("" : : : "memory");
}

#endif

// ---------------------------------------------------------------------------
// Deterministic RNG
// ---------------------------------------------------------------------------

inline std::mt19937_64& rng() {
    static std::mt19937_64 gen(42);
    return gen;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

using ValidateFn = std::function<bool()>;

struct ValidateDef {
    const char* name;
    ValidateFn fn;
};

inline std::vector<ValidateDef>& validate_registry() {
    static std::vector<ValidateDef> reg;
    return reg;
}

struct RegisterValidation {
    RegisterValidation(const char* name, ValidateFn fn) {
        validate_registry().push_back({name, std::move(fn)});
    }
};

// Check helper: prints FAIL message and returns false
inline bool check_failed(const char* test, const char* msg) {
    std::fprintf(stderr, "FAIL [%s]: %s\n", test, msg);
    return false;
}

// Run all validations. Returns true if all pass.
inline bool run_validations() {
    auto& reg = validate_registry();
    if (reg.empty()) return true;

    std::fprintf(stderr, "Running %zu validation(s)...\n", reg.size());
    bool all_pass = true;
    for (auto& v : reg) {
        bool ok = v.fn();
        std::fprintf(stderr, "  %s: %s\n", v.name, ok ? "PASS" : "FAIL");
        if (!ok) all_pass = false;
    }
    return all_pass;
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

using BenchmarkFn = std::function<uint64_t(int iterations)>;

struct BenchmarkDef {
    const char* name;
    uint64_t ops_per_iteration;
    BenchmarkFn fn;
    int fixed_iterations;  // 0 = auto-calibrate
};

inline std::vector<BenchmarkDef>& benchmark_registry() {
    static std::vector<BenchmarkDef> reg;
    return reg;
}

struct RegisterBenchmark {
    RegisterBenchmark(const char* name, uint64_t ops, BenchmarkFn fn, int fixed_iters = 0) {
        benchmark_registry().push_back({name, ops, std::move(fn), fixed_iters});
    }
};

inline int calibrate(const BenchmarkFn& fn) {
    fn(1);  // warmup
    uint64_t single_cycles = fn(1);

    constexpr uint64_t TARGET_CYCLES = 1'000'000'000ULL;
    int n = static_cast<int>(TARGET_CYCLES / std::max(single_cycles, uint64_t(1)));
    n = std::max(n, 3);
    n = std::min(n, 1000);
    return n;
}

inline int run_benchmarks() {
    // Validate first — refuse to benchmark incorrect solutions
    if (!run_validations()) {
        std::printf("{\"error\": \"Validation failed\", \"benchmarks\": []}\n");
        return 1;
    }

    auto& reg = benchmark_registry();

    std::vector<BenchmarkResult> results;
    results.reserve(reg.size());

    for (auto& def : reg) {
        int iters = def.fixed_iterations > 0 ? def.fixed_iterations : calibrate(def.fn);
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
        std::printf("      \"iterations\": %" PRIu64 ",\n", r.iterations);
        std::printf("      \"ops_per_iteration\": %" PRIu64 ",\n", r.ops_per_iteration);
        std::printf("      \"total_cycles\": %" PRIu64 ",\n", r.total_cycles);
        std::printf("      \"cycles_per_op\": %.2f\n", cycles_per_op);
        std::printf("    }%s\n", (i + 1 < results.size()) ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

} // namespace hftu
