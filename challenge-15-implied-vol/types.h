#pragma once
#include <cstdint>

namespace hftu {

struct OptionContract {
    double market_price;
    double spot;
    double strike;
    double rate;
    double time_to_expiry;  // years
    bool   is_call;
};

struct VolResult {
    double implied_vol;
    bool   converged;
};

} // namespace hftu
