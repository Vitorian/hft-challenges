#pragma once
#include "../types.h"
#include <cmath>
#include <random>
#include <vector>

namespace hftu {

class MonteCarloPricer {
public:
    MonteCarloPricer() = default;

    // Configure simulation parameters. NOT timed.
    void build(int paths_per_option, int steps_per_path) {
        paths_ = paths_per_option;
        steps_ = steps_per_path;
    }

    // Price a batch of options. out[] pre-allocated.
    void price_batch(const MCOption* options, MCResult* out, int count);

private:
    int paths_ = 10000;
    int steps_ = 100;
};

} // namespace hftu
