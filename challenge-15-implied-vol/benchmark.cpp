#include "common/benchmark_harness.h"
#include "solution/solution.h"
#include "types.h"

#include <random>
#include <vector>
#include <cmath>

namespace {

std::vector<hftu::OptionContract> generate_contracts(int count, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<hftu::OptionContract> contracts(count);

    std::uniform_real_distribution<double> spot_dist(50.0, 200.0);
    std::uniform_real_distribution<double> moneyness_dist(0.8, 1.2);
    std::uniform_real_distribution<double> rate_dist(0.01, 0.06);
    std::uniform_real_distribution<double> tte_dist(0.01, 2.0);
    std::uniform_real_distribution<double> vol_dist(0.05, 0.80);
    std::uniform_int_distribution<int> type_dist(0, 1);

    for (int i = 0; i < count; i++) {
        double spot = spot_dist(rng);
        double strike = spot * moneyness_dist(rng);
        double rate = rate_dist(rng);
        double T = tte_dist(rng);
        double true_vol = vol_dist(rng);
        bool is_call = type_dist(rng) == 1;

        // Compute the "market price" from the true vol using Black-Scholes
        double d1 = (std::log(spot / strike) + (rate + 0.5 * true_vol * true_vol) * T)
                     / (true_vol * std::sqrt(T));
        double d2 = d1 - true_vol * std::sqrt(T);
        double nd1 = 0.5 * std::erfc(-d1 * M_SQRT1_2);
        double nd2 = 0.5 * std::erfc(-d2 * M_SQRT1_2);

        double price;
        if (is_call)
            price = spot * nd1 - strike * std::exp(-rate * T) * nd2;
        else
            price = strike * std::exp(-rate * T) * (1.0 - nd2) - spot * (1.0 - nd1);

        // Ensure price is positive and meaningful
        if (price < 0.01) price = 0.01;

        contracts[i] = {price, spot, strike, rate, T, is_call};
    }
    return contracts;
}

static auto contracts = generate_contracts(100000, 0x1ABF01);

static hftu::RegisterBenchmark reg_solution("BM_Solution", contracts.size(),
    [](int iters) -> uint64_t {
        std::vector<hftu::VolResult> results(contracts.size());
        hftu::ImpliedVolSolver solver;
        solver.build();
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            auto start = hftu::cycle_start();
            solver.solve_batch(contracts.data(), results.data(), contracts.size());
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
