#include "../common/benchmark_harness.h"
#include "hotpath.h"
#include <cstdint>
#include <vector>
#include <random>

namespace {

std::vector<uint64_t> generate_input(int n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<uint64_t> v(n);
    for (int i = 0; i < n; ++i) v[i] = rng();
    return v;
}

static auto input = generate_input(200000, 0xBEEF18);

// ---- Hot loop (frozen) ----
__attribute__((noinline))
uint64_t process_burst(const uint64_t* in, size_t n) {
    uint64_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc = mix(acc, in[i]);
    }
    return acc;
}

hftu::RegisterValidation val_correctness("validate_burst", []() {
    auto test = generate_input(1000, 0xCAFE18);
    uint64_t got = process_burst(test.data(), test.size());

    uint64_t expected = 0;
    for (uint64_t v : test) expected = mix(expected, v);

    if (got != expected) {
        return hftu::check_failed("validate_burst", "result mismatch");
    }
    return true;
});

static hftu::RegisterBenchmark reg_burst("burst_processing", input.size(),
    [](int iters) -> uint64_t {
        uint64_t total = 0;

        for (int i = 0; i < iters; ++i) {
            auto warm = process_burst(input.data(), input.size());
            hftu::do_not_optimize(warm);
            hftu::clobber();

            auto start = hftu::cycle_start();
            auto result = process_burst(input.data(), input.size());
            hftu::do_not_optimize(result);
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    }, 1);

} // anon namespace

int main() { return hftu::run_benchmarks(); }
