// Benchmark harness for Challenge 20: Fast Integer-to-String
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "common/hftu_xoshiro.h"
#include "solution/solution.h"

#include <vector>

namespace {

// Workload: the "execution-report field mix". Each conversion is one numeric
// field of a synthetic execution report, drawn i.i.d.:
//
//   30%  quantity    log-uniform over [0, 10^6)        -> 1-6 digits
//   25%  price       log-uniform over [10^4, 10^8)     -> 5-8 digits
//                    (a $1-$10,000 price scaled by 10^4, ITCH-style)
//   25%  order id    log-uniform over [10^9, 10^19)    -> 10-19 digits
//   10%  seq number  uniform over [1, 10^10]           -> mostly 10 digits
//   10%  timestamp   uniform ns-since-midnight over the
//                    09:30-16:00 session                -> always 14 digits
//
// "log-uniform" = digit count first (uniform over the range), then a value
// uniform within that decimal decade — every length in the field's range is
// equally likely. Field weights follow the numeric-field census of a FIX
// ExecutionReport. Full rationale, sources, and the resulting per-digit
// table are in the README.
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
        const uint64_t roll = rng.next_range(100);
        uint64_t lo, span;
        if (roll < 30) {        // quantity: digits 1..6 (0 allowed: LeavesQty)
            const int k = static_cast<int>(rng.next_range(6)) + 1;
            lo = (k == 1) ? 0 : POW10[k - 1];
            span = POW10[k] - lo;
        } else if (roll < 55) { // price: digits 5..8
            const int k = static_cast<int>(rng.next_range(4)) + 5;
            lo = POW10[k - 1]; span = POW10[k] - lo;
        } else if (roll < 80) { // order/exec id: digits 10..19
            const int k = static_cast<int>(rng.next_range(10)) + 10;
            lo = POW10[k - 1]; span = POW10[k] - lo;
        } else if (roll < 90) { // sequence number: uniform [1, 10^10]
            lo = 1; span = 10'000'000'000ULL;
        } else {                // timestamp: ns since midnight, 09:30-16:00
            lo = 34'200'000'000'000ULL; span = 23'400'000'000'000ULL;
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
