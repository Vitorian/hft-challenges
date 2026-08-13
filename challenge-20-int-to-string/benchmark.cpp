// Benchmark harness for Challenge 20: Fast Integer-to-String
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "common/hftu_xoshiro.h"
#include "solution/solution.h"

#include <vector>

namespace {

// Digit-length-uniform workload: digit count k drawn uniformly from 1..20,
// then a value uniform within that decimal range. Every output length is
// equally likely — a solution that is only fast for short numbers won't win.
std::vector<uint64_t> make_values(uint64_t seed, size_t n) {
    static constexpr uint64_t POW10[20] = {
        1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL,
        100000ULL, 1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL,
        10000000000ULL, 100000000000ULL, 1000000000000ULL,
        10000000000000ULL, 100000000000000ULL, 1000000000000000ULL,
        10000000000000000ULL, 100000000000000000ULL,
        1000000000000000000ULL, 10000000000000000000ULL};

    hftu::Rng rng(seed);
    std::vector<uint64_t> v(n);
    for (size_t i = 0; i < n; ++i) {
        const int k = static_cast<int>(rng.next_range(20)) + 1; // digits 1..20
        uint64_t lo, span;
        if (k == 1) {
            lo = 0; span = 10;
        } else if (k == 20) {
            lo = POW10[19]; span = UINT64_MAX - POW10[19] + 1;
        } else {
            lo = POW10[k - 1]; span = POW10[k] - lo;
        }
        v[i] = lo + rng.next_range(span);
    }
    return v;
}

} // namespace

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 1'000'000,
    [](int iterations) -> uint64_t {
        const std::vector<uint64_t> values = make_values(0x175A57, 1'000'000);
        // Conversions run in batches of 16, each into its own 32-byte slot —
        // like a FIX encoder emitting the numeric fields of one message.
        // Independent conversions may overlap in the pipeline; the score is
        // still average cycles per conversion.
        constexpr size_t kBatch = 16;
        static_assert(1'000'000 % kBatch == 0);
        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) {
            alignas(64) char out[kBatch * 32];
            char* escaped = out;
            hftu::do_not_optimize(escaped); // out's address escapes: stores are real
            uint64_t acc = 0;
            uint64_t start = hftu::cycle_start();
            for (size_t j = 0; j < values.size(); j += kBatch) {
                for (size_t k = 0; k < kBatch; ++k)
                    acc += hftu::u64_to_chars(values[j + k], out + k * 32);
                hftu::clobber();
            }
            hftu::do_not_optimize(acc);
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
