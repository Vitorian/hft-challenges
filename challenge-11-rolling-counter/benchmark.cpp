// Benchmark harness for Challenge 11: Rolling Counter
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

// Standard: 1s window, 1ms precision, ~1000 events/sec
static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 1'000'000,
    [](int iterations) -> uint64_t {
        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) {
            hftu::RollingCounter rc(1'000'000'000LL, 1'000'000LL); // 1s window, 1ms precision
            std::mt19937_64 gen(0xABCD);
            std::uniform_int_distribution<size_t> ev_dist(0, 5);
            std::uniform_int_distribution<int64_t> jitter(0, 500'000);

            int64_t t = 0;
            uint64_t start = hftu::cycle_start();
            for (size_t j = 0; j < 1'000'000; ++j) {
                t += 1'000'000LL + jitter(gen); // ~1ms steps
                rc.update(t);
                rc.addEvent(ev_dist(gen));
                size_t c = rc.count();
                hftu::do_not_optimize(c);
            }
            hftu::clobber();
            uint64_t end = hftu::cycle_end();
            total += (end - start);
        }
        return total;
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
