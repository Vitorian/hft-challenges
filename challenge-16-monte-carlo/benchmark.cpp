#include "common/benchmark_harness.h"
#include "solution/solution.h"
#include "types.h"

#include <random>
#include <vector>
#include <cmath>

namespace {

std::vector<hftu::MCOption> generate_options(int count, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<hftu::MCOption> opts(count);

    std::uniform_real_distribution<double> spot_dist(80.0, 120.0);
    std::uniform_real_distribution<double> moneyness_dist(0.85, 1.15);
    std::uniform_real_distribution<double> rate_dist(0.02, 0.05);
    std::uniform_real_distribution<double> vol_dist(0.10, 0.60);
    std::uniform_real_distribution<double> tte_dist(0.25, 2.0);
    std::uniform_int_distribution<int> type_dist(0, 3);
    std::uniform_int_distribution<int> call_dist(0, 1);

    for (int i = 0; i < count; i++) {
        double spot = spot_dist(rng);
        double strike = spot * moneyness_dist(rng);
        int type = type_dist(rng);

        opts[i].spot = spot;
        opts[i].strike = strike;
        opts[i].rate = rate_dist(rng);
        opts[i].vol = vol_dist(rng);
        opts[i].time_to_expiry = tte_dist(rng);
        opts[i].type = type;
        opts[i].is_call = call_dist(rng) == 1;

        // Barrier: 10-20% away from spot
        if (type == 2) opts[i].barrier = spot * 1.15;      // up-and-out
        else if (type == 3) opts[i].barrier = spot * 0.85;  // down-and-out
        else opts[i].barrier = 0.0;
    }
    return opts;
}

// 200 options, 10K paths, 100 steps → targets ~20s with naive implementation
constexpr int NUM_OPTIONS = 200;
constexpr int PATHS = 10000;
constexpr int STEPS = 100;

static auto options = generate_options(NUM_OPTIONS, 0xABCD01);

static hftu::RegisterBenchmark reg_solution("BM_Solution", NUM_OPTIONS,
    [](int iters) -> uint64_t {
        std::vector<hftu::MCResult> results(NUM_OPTIONS);
        hftu::MonteCarloPricer pricer;
        pricer.build(PATHS, STEPS);
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            auto start = hftu::cycle_start();
            pricer.price_batch(options.data(), results.data(), NUM_OPTIONS);
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    });

} // anon namespace

int main() {
    hftu::run_benchmarks();
    return 0;
}
