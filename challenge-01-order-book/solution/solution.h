#pragma once
// Challenge 01: Order Book
// Edit this file and solution.cpp to implement your solution.

#include <cstdint>
#include <map>
#include <unordered_map>

namespace hftu {

class OrderBook {
public:
    // Add a new order. side: 0=buy(bid), 1=sell(ask)
    void add_order(uint64_t id, int side, int64_t price, int64_t quantity);

    // Cancel an order by ID. No-op if ID doesn't exist.
    void cancel_order(uint64_t id);

    // Return highest bid price, or 0 if no bids.
    int64_t best_bid() const;

    // Return lowest ask price, or 0 if no asks.
    int64_t best_ask() const;

private:
    // Replace these with your own data structures
    struct Order {
        int side;
        int64_t price;
        int64_t quantity;
    };

    std::unordered_map<uint64_t, Order> orders_;
    std::map<int64_t, int64_t, std::greater<>> bids_; // price -> total qty, descending
    std::map<int64_t, int64_t> asks_;                  // price -> total qty, ascending
};

} // namespace hftu
