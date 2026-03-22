#include "solution.h"
#include <cstring>

namespace hftu {

ImpliedBook::ImpliedBook() = default;

void ImpliedBook::build(std::span<const int> weights, int depth) {
    num_legs_ = std::min((int)weights.size(), MAX_LEGS);
    depth_ = std::min(depth, MAX_DEPTH);
    for (int i = 0; i < num_legs_; i++)
        weights_[i] = weights[i];
    std::memset(books_, 0, sizeof(books_));
    std::memset(book_sizes_, 0, sizeof(book_sizes_));
}

// Naive: full recomputation on every update.
// For N legs with D levels each, generates D^N combinations.
void ImpliedBook::recompute(Level* out_bids, int& nbids, Level* out_asks, int& nasks) {
    // Temporary storage for all combinations
    struct Combo { int64_t price; int64_t quantity; };
    static thread_local std::vector<Combo> bid_combos, ask_combos;
    bid_combos.clear();
    ask_combos.clear();

    // For implied BID: we want to BUY the synthetic.
    // For leg with weight > 0: take from outright ASKS (we buy the leg)
    // For leg with weight < 0: take from outright BIDS (we sell the leg)
    // Implied price = sum(weight_i * leg_price_i)
    // Implied qty = min(leg_qty_i / |weight_i|)

    // Recursive enumeration of all level combinations
    struct State { int64_t price; int64_t qty; };

    auto enumerate = [&](bool is_bid, auto& combos) {
        // Start with leg 0, enumerate all combinations
        std::vector<State> current = {State{0, INT64_MAX}};
        std::vector<State> next;

        for (int leg = 0; leg < num_legs_; leg++) {
            next.clear();
            int w = weights_[leg];
            // For implied bid: positive weight -> buy -> use ask side
            //                  negative weight -> sell -> use bid side
            // For implied ask: positive weight -> sell -> use bid side
            //                  negative weight -> buy -> use ask side
            int side;
            if (is_bid)
                side = (w > 0) ? 1 : 0;  // ask if buying, bid if selling
            else
                side = (w > 0) ? 0 : 1;  // bid if selling, ask if buying

            int nlevels = book_sizes_[leg][side];
            if (nlevels == 0) {
                current.clear();
                break;
            }

            for (auto& st : current) {
                for (int li = 0; li < nlevels; li++) {
                    auto& lv = books_[leg][side][li];
                    int64_t implied_price = st.price + w * lv.price;
                    int64_t implied_qty = std::min(st.qty, lv.quantity / std::abs(w));
                    if (implied_qty > 0)
                        next.push_back(State{implied_price, implied_qty});
                }
            }
            std::swap(current, next);
        }

        for (auto& st : current)
            combos.push_back(Combo{st.price, st.qty});
    };

    enumerate(true, bid_combos);
    enumerate(false, ask_combos);

    // Sort bids descending by price, asks ascending
    std::sort(bid_combos.begin(), bid_combos.end(),
              [](auto& a, auto& b) { return a.price > b.price; });
    std::sort(ask_combos.begin(), ask_combos.end(),
              [](auto& a, auto& b) { return a.price < b.price; });

    // Aggregate same-price levels and take top depth_
    auto aggregate = [&](std::vector<Combo>& combos, Level* out, int& count) {
        count = 0;
        int64_t last_price = INT64_MIN;
        for (auto& c : combos) {
            if (count > 0 && c.price == last_price) {
                out[count - 1].quantity += c.quantity;
            } else {
                if (count >= depth_) break;
                out[count++] = Level{c.price, c.quantity};
                last_price = c.price;
            }
        }
    };

    aggregate(bid_combos, out_bids, nbids);
    aggregate(ask_combos, out_asks, nasks);
}

std::pair<int,int> ImpliedBook::on_update(const BookUpdate& update,
                                           Level* out_bids, Level* out_asks) {
    int leg = update.leg_index;
    int side = update.side;

    if (leg >= num_legs_) return {0, 0};

    // Apply update to outright book
    int& sz = book_sizes_[leg][side];
    int pos = update.position;

    switch (update.action) {
        case 0: // add
            if (sz < MAX_DEPTH && pos <= sz) {
                for (int i = sz; i > pos; i--)
                    books_[leg][side][i] = books_[leg][side][i - 1];
                books_[leg][side][pos] = update.level;
                sz++;
            }
            break;
        case 1: // modify
            if (pos < sz)
                books_[leg][side][pos] = update.level;
            break;
        case 2: // delete
            if (pos < sz) {
                for (int i = pos; i < sz - 1; i++)
                    books_[leg][side][i] = books_[leg][side][i + 1];
                sz--;
            }
            break;
    }

    // Full recomputation
    int nbids = 0, nasks = 0;
    recompute(out_bids, nbids, out_asks, nasks);
    return {nbids, nasks};
}

} // namespace hftu
