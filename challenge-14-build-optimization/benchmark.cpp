#include "../common/benchmark_harness.h"
#include <cstdint>
#include <cmath>
#include <vector>
#include <random>

namespace {

// ---- Types and implementation (frozen — cannot be modified) ----

struct alignas(16) MarketTick {
    double   price;        // 8   trade price
    int32_t  quantity;     // 4   trade size
    uint8_t  flags;        // 1   0 = end-of-burst sentinel
    uint8_t  side;         // 1   0=buy, 1=sell
    uint16_t symbol_id;    // 2   instrument index
};
static_assert(sizeof(MarketTick) == 16);

struct BurstResult {
    double  total_notional;
    int64_t net_volume;
};

__attribute__((noinline))
BurstResult process_burst(const MarketTick* t) {
    double  total_notional = 0.0;
    int64_t net_volume     = 0;

    while (t->flags) {
        total_notional += t->price;
        int32_t mask = -static_cast<int32_t>(t->side);  // 0 or -1
        net_volume += (t->quantity ^ mask) - mask;       // branchless negate
        ++t;
    }

    return {total_notional, net_volume};
}

// ---- Data generation ----

std::vector<MarketTick> generate_burst(int n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<MarketTick> ticks(n + 1);

    for (int i = 0; i < n; i++) {
        ticks[i].price     = 100.0 + (rng() % 10000) * 0.01;
        ticks[i].quantity  = 1 + (rng() % 500);
        ticks[i].flags     = 1;
        ticks[i].side      = rng() % 2;
        ticks[i].symbol_id = rng() % 8000;
    }
    ticks[n] = {};  // sentinel (flags = 0)
    return ticks;
}

static auto burst = generate_burst(200000, 0xBEEF01);

// ---- Validation ----

hftu::RegisterValidation val_correctness("validate_burst", []() {
    auto test = generate_burst(1000, 0xCAFE);
    auto result = process_burst(test.data());

    double expected_notional = 0.0;
    int64_t expected_vol = 0;
    for (int i = 0; i < 1000; i++) {
        expected_notional += test[i].price;
        int32_t mask = -static_cast<int32_t>(test[i].side);
        expected_vol += (test[i].quantity ^ mask) - mask;
    }

    if (std::abs(result.total_notional - expected_notional) > 1e-6) {
        return hftu::check_failed("validate_burst",
            "total_notional mismatch");
    }
    if (result.net_volume != expected_vol) {
        return hftu::check_failed("validate_burst",
            "net_volume mismatch");
    }

    return true;
});

// ---- Benchmark ----

static hftu::RegisterBenchmark reg_burst("burst_processing", (burst.size() - 1) / 1000,
    [](int iters) -> uint64_t {
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            // Warm the cache — untimed
            auto warmup = process_burst(burst.data());
            hftu::do_not_optimize(warmup);
            hftu::clobber();

            // Measured run
            auto start = hftu::cycle_start();
            auto result = process_burst(burst.data());
            hftu::do_not_optimize(result);
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    }, 1);

} // anon namespace

int main() {
    return hftu::run_benchmarks();
}
