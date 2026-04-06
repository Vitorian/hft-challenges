#include "solution.h"
#include <algorithm>
#include <numeric>

namespace hftu {

void MonteCarloPricer::price_batch(const MCOption* options, MCResult* out, int count) {
    std::mt19937_64 rng(42);
    std::normal_distribution<double> norm(0.0, 1.0);

    for (int i = 0; i < count; i++) {
        const auto& opt = options[i];
        double dt = opt.time_to_expiry / steps_;
        double drift = (opt.rate - 0.5 * opt.vol * opt.vol) * dt;
        double diffusion = opt.vol * std::sqrt(dt);
        double discount = std::exp(-opt.rate * opt.time_to_expiry);

        double sum_payoff = 0.0;
        double sum_payoff_sq = 0.0;

        for (int p = 0; p < paths_; p++) {
            double S = opt.spot;
            double avg = 0.0;
            bool knocked_out = false;

            for (int t = 0; t < steps_; t++) {
                double z = norm(rng);
                S *= std::exp(drift + diffusion * z);
                avg += S;

                // Barrier check
                if (opt.type == 2 && S >= opt.barrier) knocked_out = true;
                if (opt.type == 3 && S <= opt.barrier) knocked_out = true;
            }

            double payoff = 0.0;
            if (!knocked_out) {
                double final_price = S;
                if (opt.type == 1) {
                    // Asian: use average price
                    final_price = avg / steps_;
                }

                if (opt.is_call)
                    payoff = std::max(0.0, final_price - opt.strike);
                else
                    payoff = std::max(0.0, opt.strike - final_price);
            }

            payoff *= discount;
            sum_payoff += payoff;
            sum_payoff_sq += payoff * payoff;
        }

        double mean = sum_payoff / paths_;
        double variance = (sum_payoff_sq / paths_) - mean * mean;
        double std_err = std::sqrt(variance / paths_);

        out[i].price = mean;
        out[i].std_error = std_err;
    }
}

} // namespace hftu
