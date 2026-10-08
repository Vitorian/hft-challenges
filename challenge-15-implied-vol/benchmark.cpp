#include "common/benchmark_harness.h"
#include "solution/solution.h"
#include "types.h"

#include <random>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

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

// ---------------------------------------------------------------------------
// Correctness validation
// ---------------------------------------------------------------------------
// Rule: the returned sigma must produce a Black-Scholes price within 1e-6 of
// the market price, and converged must be true for valid results.
//
//   1. Truthful flag. A result reported converged must reprice within the
//      tolerance. A single false claim fails the run.
//   2. Coverage. At most MAX_UNSOLVED_PCT of a batch may be reported not
//      converged. The naive skeleton leaves about 0.46% unsolved (its Newton
//      iteration stalls on low-vega contracts and says so).
// Both run on the benchmark workload and on a second batch from another seed.
// The certified run does the same with a private seed, so an answer tuned to
// these batches does not pass there.

constexpr double TOLERANCE = 1e-6;
constexpr double TOLERANCE_SLACK = 1e-9;   // equivalent pricing formulas differ by rounding
constexpr double MAX_UNSOLVED_PCT = 1.0;
constexpr uint64_t HELDOUT_SEED = 0x5EED15;

// Black-Scholes price, the same formula as bs_price() in the skeleton.
double reference_price(const hftu::OptionContract& c, double vol) {
    const double sd = vol * std::sqrt(c.time_to_expiry);
    const double d1 = (std::log(c.spot / c.strike) + (c.rate + 0.5 * vol * vol) * c.time_to_expiry) / sd;
    const double d2 = d1 - sd;
    const double nd1 = 0.5 * std::erfc(-d1 * M_SQRT1_2);
    const double nd2 = 0.5 * std::erfc(-d2 * M_SQRT1_2);
    const double discounted = c.strike * std::exp(-c.rate * c.time_to_expiry);
    return c.is_call ? c.spot * nd1 - discounted * nd2
                     : discounted * (1.0 - nd2) - c.spot * (1.0 - nd1);
}

bool validate_batch(const char* test, const std::vector<hftu::OptionContract>& batch) {
    // Poisoned output: an entry the solution never writes reads as a converged NaN.
    std::vector<hftu::VolResult> results(batch.size());
    for (auto& r : results) {
        r.implied_vol = std::numeric_limits<double>::quiet_NaN();
        r.converged = true;
    }

    hftu::ImpliedVolSolver solver;
    solver.build();
    solver.solve_batch(batch.data(), results.data(), static_cast<int>(batch.size()));

    size_t unsolved = 0, wrong = 0, worst_index = 0;
    double worst_error = 0.0, worst_price = 0.0;
    for (size_t i = 0; i < batch.size(); i++) {
        unsigned char flag;                      // solutions may write the flag as raw bytes
        std::memcpy(&flag, &results[i].converged, 1);
        if (flag == 0) { ++unsolved; continue; }
        const double vol = results[i].implied_vol;
        double price = std::numeric_limits<double>::quiet_NaN();
        double error = std::numeric_limits<double>::infinity();
        if (std::isfinite(vol) && vol > 0.0) {
            price = reference_price(batch[i], vol);
            error = std::abs(price - batch[i].market_price);
        }
        if (!(error <= TOLERANCE + TOLERANCE_SLACK)) {
            ++wrong;
            if (!(error <= worst_error)) { worst_error = error; worst_index = i; worst_price = price; }
        }
    }

    bool ok = true;
    char msg[512];
    if (wrong > 0) {
        const auto& c = batch[worst_index];
        std::snprintf(msg, sizeof msg,
                      "%zu of %zu results are reported converged but do not reprice within 1e-6 "
                      "of the market price (worst error %.3g)", wrong, batch.size(), worst_error);
        hftu::check_failed(test, msg);
        std::snprintf(msg, sizeof msg,
                      "worst case: %s S=%.6f K=%.6f r=%.6f T=%.6f, market price %.8f, "
                      "implied_vol %.8g reprices to %.8f",
                      c.is_call ? "call" : "put", c.spot, c.strike, c.rate, c.time_to_expiry,
                      c.market_price, results[worst_index].implied_vol, worst_price);
        hftu::check_failed(test, msg);
        ok = false;
    }
    if (static_cast<double>(unsolved) * 100.0 > MAX_UNSOLVED_PCT * static_cast<double>(batch.size())) {
        std::snprintf(msg, sizeof msg,
                      "%zu of %zu results are reported not converged (at most %.0f%% may be left unsolved)",
                      unsolved, batch.size(), MAX_UNSOLVED_PCT);
        hftu::check_failed(test, msg);
        ok = false;
    }
    if (ok)
        std::fprintf(stderr, "  %s: %zu results checked, %zu reported not converged\n",
                     test, batch.size(), unsolved);
    return ok;
}

static hftu::RegisterValidation val_accuracy("accuracy", []() -> bool {
    return validate_batch("accuracy", contracts);
});

static hftu::RegisterValidation val_accuracy_heldout("accuracy_heldout", []() -> bool {
    return validate_batch("accuracy_heldout", generate_contracts(100000, HELDOUT_SEED));
});

} // anon namespace

int main() {
    return hftu::run_benchmarks();
}
