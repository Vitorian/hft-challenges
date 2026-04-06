#pragma once
// Venue interface and shared types for Challenge 02: Multi-Symbol Order Book
// Do NOT modify this file — it will be overwritten during certified runs.

#include <cstdint>

namespace hftu {

// Market data level
struct TopLevel {
    int64_t price = 0;
    int64_t qty = 0;
    int32_t count = 0;
};

// Queue position for one of our orders
struct QueuePosition {
    int32_t index = -1;     // orders ahead of us (-1 = not in book yet)
    int64_t qty_ahead = 0;  // total qty of orders ahead
};

// Venue interface — provided by the benchmark harness.
// The user's MultiOrderBook calls these to interact with the exchange.
class Venue {
public:
    virtual ~Venue() = default;

    // Send our order to the exchange. Returns exchange-assigned order ID.
    // The order will later appear in the feed via add_order().
    virtual uint64_t send_order(uint64_t our_id, uint16_t symbol, int side,
                                int64_t price, int64_t qty) = 0;

    // Modify our order. Returns exchange ID:
    //   - Same ID if qty decreased only (keeps queue position)
    //   - New ID if price changed or qty increased (loses position;
    //     feed will show cancel(old_id) + add(new_id))
    virtual uint64_t modify_order(uint64_t exchange_id,
                                  int64_t new_price, int64_t new_qty) = 0;

    // Cancel our order on the exchange.
    virtual void cancel_order(uint64_t exchange_id) = 0;
};

constexpr uint16_t NUM_SYMBOLS = 200;

} // namespace hftu
