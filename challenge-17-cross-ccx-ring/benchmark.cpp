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

inline uint64_t rdtsc_fenced() {
#if defined(__x86_64__) || defined(_M_X64)
    uint32_t lo, hi;
    asm volatile(
        "lfence\n\t"
        "rdtsc"
        : "=a"(lo), "=d"(hi)
        :
        : "memory"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    asm volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_nsec;
#endif
}

uint64_t run_latency(size_t capacity, size_t total_ops) {
    hftu::RingBuffer rb(capacity);
    uint64_t total_latency = 0;
    std::atomic<bool> consumer_ready{false};

    std::thread consumer([&]() {
        hftu::pin_to_isolated(5);  // cross-CCX: different L3 from producer
        consumer_ready.store(true, std::memory_order_release);
        hftu::Message msg{};
        size_t count = 0;
        uint64_t sum = 0;
        while (count < total_ops) {
            if (rb.pop(msg)) {
                uint64_t now = rdtsc_fenced();
                sum += now - msg.timestamp;
                ++count;
            }
        }
        total_latency = sum;
    });

    while (!consumer_ready.load(std::memory_order_acquire)) {}

    std::thread producer([&]() {
        hftu::pin_to_isolated(0);
        hftu::Message msg{};
        for (size_t i = 0; i < total_ops; ++i) {
            msg.timestamp = rdtsc_fenced();
            msg.sequence = i;
            msg.symbol_id = static_cast<uint32_t>(i & 0xFFF);
            msg.side = static_cast<uint16_t>(i & 1);
            msg.price = static_cast<int64_t>(i * 100 + 1);
            msg.quantity = static_cast<int64_t>((i & 0xFF) + 1);
            msg.order_id = static_cast<int64_t>(i);
            while (!rb.push(msg)) {}
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
