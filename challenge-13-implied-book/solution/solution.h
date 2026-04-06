#pragma once
#include <cstdint>
#include <cstddef>
#include <span>
#include <utility>
#include <algorithm>
#include <climits>
#include <cstring>

namespace hftu {

struct Level {
    int64_t price;
    int64_t quantity;
};

struct BookUpdate {
    uint16_t leg_index;   // which outright book changed
    int8_t   side;        // 0=bid, 1=ask
    uint8_t  action;      // 0=add, 1=modify, 2=delete
    Level    level;        // price + quantity (position derived from price)
};

class ImpliedBook {
public:
    ImpliedBook();

    /// Define the synthetic instrument.
    /// weights: per-leg multiplier (e.g. {+1, -2, +1} for butterfly)
    /// depth: number of output levels per side
    /// NOT timed.
    void build(std::span<const int> weights, int depth);

    /// Apply an update to one of the outright books.
    /// Returns {num_bids, num_asks} written to out_bids/out_asks.
    /// out_bids and out_asks are pre-allocated with `depth` capacity.
    std::pair<int,int> on_update(const BookUpdate& update,
                                 Level* out_bids, Level* out_asks);

private:
    static constexpr int MAX_LEGS = 8;
    static constexpr int MAX_DEPTH = 10;

    int num_legs_ = 0;
    int depth_ = 0;
    int weights_[MAX_LEGS] = {};

    // Outright books: [leg][side][level]
    Level books_[MAX_LEGS][2][MAX_DEPTH] = {};
    int   book_sizes_[MAX_LEGS][2] = {};

    int sweep(bool is_bid, Level* out);
};

} // namespace hftu
