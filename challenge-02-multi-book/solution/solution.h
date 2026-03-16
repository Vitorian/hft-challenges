#pragma once
// Challenge 02: Multi-Symbol Order Book
// Edit this file and solution.cpp to implement your solution.

#include "../venue.h"
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>

namespace hftu {

class MultiOrderBook {
public:
    explicit MultiOrderBook(Venue& venue);
    ~MultiOrderBook();

    // === Our order management ===
    // Called by the strategy. Forward to venue and track the mapping.
    void send_order(uint64_t our_id, uint16_t symbol, int side,
                    int64_t price, int64_t qty);
    void modify_our_order(uint64_t our_id, int64_t new_price, int64_t new_qty);
    void cancel_our_order(uint64_t our_id);

    // === Exchange feed ===
    // All orders (everyone's, including ours when they appear).
    void add_order(uint64_t exchange_id, uint16_t symbol, int side,
                   int64_t price, int64_t qty);
    // Qty-down only — order keeps its queue position.
    void modify_order(uint64_t exchange_id, int64_t new_qty);
    // Remove order from book.
    void cancel_order(uint64_t exchange_id);

    // === Queries ===
    TopLevel best_bid(uint16_t symbol) const;
    TopLevel best_ask(uint16_t symbol) const;
    // Write up to n best levels into out[]. Returns levels written.
    int get_top_levels(uint16_t symbol, int side, int n, TopLevel* out) const;
    // Total qty within `depth` ticks of best price.
    int64_t volume_near_best(uint16_t symbol, int side, int64_t depth) const;
    // Queue position for one of our orders.
    QueuePosition get_queue_position(uint64_t our_id) const;

private:
    Venue& venue_;

    struct Order {
        uint16_t symbol;
        int8_t side;
        int64_t price;
        int64_t qty;
    };

    // Per-level FIFO queue
    struct Level {
        int64_t total_qty = 0;
        int32_t count = 0;
        std::list<std::pair<uint64_t, int64_t>> queue; // (exchange_id, qty) in FIFO order
    };

    struct SymbolBook {
        std::map<int64_t, Level> bids; // rbegin() = best bid
        std::map<int64_t, Level> asks; // begin() = best ask
    };

    std::unordered_map<uint64_t, Order> orders_;        // exchange_id -> order
    std::unordered_map<uint64_t, uint64_t> our_orders_; // our_id -> exchange_id
    SymbolBook books_[NUM_SYMBOLS];
};

} // namespace hftu
