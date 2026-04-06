#include "solution.h"

namespace hftu {

void ImpliedVolSolver::solve_batch(const OptionContract* contracts,
                                    VolResult* out, int count) {
    constexpr double TOL = 1e-6;
    constexpr int MAX_ITER = 50;

    for (int i = 0; i < count; i++) {
        const auto& c = contracts[i];
        double vol = 0.2;  // initial guess
        bool converged = false;

        for (int iter = 0; iter < MAX_ITER; iter++) {
            double price = bs_price(c.spot, c.strike, c.rate,
                                    c.time_to_expiry, vol, c.is_call);
            double diff = price - c.market_price;

            if (std::abs(diff) < TOL) {
                converged = true;
                break;
            }

            double vega = bs_vega(c.spot, c.strike, c.rate,
                                  c.time_to_expiry, vol);

            if (vega < 1e-12) break;  // degenerate case

            vol -= diff / vega;

            // Clamp to reasonable range
            if (vol < 0.001) vol = 0.001;
            if (vol > 5.0) vol = 5.0;
        }

        out[i].implied_vol = vol;
        out[i].converged = converged;
    }
}

} // namespace hftu
