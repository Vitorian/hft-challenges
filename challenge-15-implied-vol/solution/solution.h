#pragma once
#include "../types.h"
#include <cmath>

namespace hftu {

class ImpliedVolSolver {
public:
    ImpliedVolSolver() = default;

    // Pre-compute anything you need. NOT timed.
    void build() {}

    // Solve a batch of options.
    void solve_batch(const OptionContract* contracts, VolResult* out, int count);

private:
    // Black-Scholes price for a given vol
    static double bs_price(double spot, double strike, double rate,
                           double T, double vol, bool is_call) {
        double d1 = (std::log(spot / strike) + (rate + 0.5 * vol * vol) * T)
                     / (vol * std::sqrt(T));
        double d2 = d1 - vol * std::sqrt(T);
        double nd1 = norm_cdf(d1);
        double nd2 = norm_cdf(d2);

        if (is_call)
            return spot * nd1 - strike * std::exp(-rate * T) * nd2;
        else
            return strike * std::exp(-rate * T) * (1.0 - nd2) - spot * (1.0 - nd1);
    }

    // Vega: dPrice/dVol
    static double bs_vega(double spot, double strike, double rate,
                          double T, double vol) {
        double d1 = (std::log(spot / strike) + (rate + 0.5 * vol * vol) * T)
                     / (vol * std::sqrt(T));
        return spot * std::sqrt(T) * norm_pdf(d1);
    }

    static double norm_cdf(double x) {
        return 0.5 * std::erfc(-x * M_SQRT1_2);
    }

    static double norm_pdf(double x) {
        return (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * x * x);
    }
};

} // namespace hftu
