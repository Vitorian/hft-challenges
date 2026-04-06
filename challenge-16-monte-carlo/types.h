#pragma once
#include <cstdint>

namespace hftu {

struct MCOption {
    double spot;
    double strike;
    double rate;
    double vol;
    double time_to_expiry;  // years
    double barrier;         // for barrier options (0 = not applicable)
    int    type;            // 0=European, 1=Asian, 2=BarrierUpOut, 3=BarrierDownOut
    bool   is_call;
};

struct MCResult {
    double price;
    double std_error;
};

} // namespace hftu
