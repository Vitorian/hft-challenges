// Benchmark harness for Challenge 06: Seqlock
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <thread>
#include <atomic>

namespace {

// Writer on one core, reader on another.
// Returns total cycles measured on the reader side.
uint64_t run_concurrent(size_t total_ops, int write_pct) {
    hftu::Seqlock sl;
    uint64_t reader_cycles = 0;

    std::thread writer([&]() {
        hftu::pin_to_isolated(0);
        hftu::Payload p{};
        size_t writes = (total_ops * write_pct) / 100;
        for (size_t i = 0; i < writes; ++i) {
            p.a = static_cast<int64_t>(i);
            p.b = p.a * 2;
            p.c = p.a * 3;
            p.d = p.a * 4;
            sl.write(p);
        }
    });

    std::thread reader([&]() {
        hftu::pin_to_isolated(1);
        uint64_t start = hftu::cycle_start();
        size_t reads = (total_ops * (100 - write_pct)) / 100;
        for (size_t i = 0; i < reads; ++i) {
            auto r = sl.read();
            hftu::do_not_optimize(r);
        }
        hftu::clobber();
        uint64_t end = hftu::cycle_end();
        reader_cycles = end - start;
    });

    writer.join();
    reader.join();
    return reader_cycles;
}

} // namespace

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 500'000,
    [](int iterations) -> uint64_t {
        uint64_t total_cycles = 0;
        for (int i = 0; i < iterations; ++i) {
            total_cycles += run_concurrent(1'000'000, 50);  // 50/50 read/write
        }
        return total_cycles;
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
