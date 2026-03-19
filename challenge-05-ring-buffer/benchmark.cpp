// Benchmark harness for Challenge 05: Ring Buffer (SPSC)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// Measures end-to-end latency: the time between producer push and consumer pop.
// Uses RDTSC with LFENCE for ordering (no CPUID serialization overhead).

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <thread>
#include <atomic>

namespace {

// LFENCE + RDTSC: ordered but lightweight (~20 cycles vs ~100 for CPUID)
inline uint64_t rdtsc_fenced() {
    uint32_t lo, hi;
    asm volatile(
        "lfence\n\t"
        "rdtsc"
        : "=a"(lo), "=d"(hi)
        :
        : "memory"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

uint64_t run_latency(size_t capacity, size_t total_ops) {
    hftu::RingBuffer rb(capacity);
    uint64_t total_latency = 0;
    std::atomic<bool> consumer_ready{false};

    std::thread consumer([&]() {
        hftu::pin_to_isolated(1);
        consumer_ready.store(true, std::memory_order_release);
        int64_t val;
        size_t count = 0;
        uint64_t sum = 0;
        while (count < total_ops) {
            if (rb.pop(val)) {
                uint64_t now = rdtsc_fenced();
                sum += now - static_cast<uint64_t>(val);
                ++count;
            }
        }
        total_latency = sum;
    });

    while (!consumer_ready.load(std::memory_order_acquire)) {}

    std::thread producer([&]() {
        hftu::pin_to_isolated(0);
        for (size_t i = 0; i < total_ops; ++i) {
            auto ts = static_cast<int64_t>(rdtsc_fenced());
            while (!rb.push(ts)) {}
        }
    });

    producer.join();
    consumer.join();
    return total_latency;
}

} // namespace

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 1'000'000,
    [](int iterations) -> uint64_t {
        uint64_t total_cycles = 0;
        for (int i = 0; i < iterations; ++i) {
            total_cycles += run_latency(1024, 1'000'000);
        }
        return total_cycles;
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
