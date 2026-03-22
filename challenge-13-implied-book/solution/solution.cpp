#include "solution.h"

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

// Sweep through levels consuming quantity, like aggressing the book.
// For implied bid: buy legs with w>0 (use ask side), sell legs with w<0 (use bid side).
// For implied ask: sell legs with w>0 (use bid side), buy legs with w<0 (use ask side).
int ImpliedBook::sweep(bool is_bid, Level* out) {
    // Determine which side to read from for each leg
    int sides[MAX_LEGS];
    for (int i = 0; i < num_legs_; i++) {
        int w = weights_[i];
        if (is_bid)
            sides[i] = (w > 0) ? 0 : 1;  // bid: sell legs w>0 (use bid), buy legs w<0 (use ask)
        else
            sides[i] = (w > 0) ? 1 : 0;  // ask: buy legs w>0 (use ask), sell legs w<0 (use bid)
    }

    // Check all legs have at least one level
    for (int i = 0; i < num_legs_; i++) {
        if (book_sizes_[i][sides[i]] == 0)
            return 0;
    }

    // Working copy of quantities remaining at each level
    int64_t remaining[MAX_LEGS][MAX_DEPTH];
    int pos[MAX_LEGS];  // current level index per leg

    for (int i = 0; i < num_legs_; i++) {
        pos[i] = 0;
        int s = sides[i];
        for (int j = 0; j < book_sizes_[i][s]; j++)
            remaining[i][j] = books_[i][s][j].quantity;
    }

    int count = 0;
    while (count < depth_) {
        // Check all legs have remaining levels
        bool valid = true;
        for (int i = 0; i < num_legs_; i++) {
            if (pos[i] >= book_sizes_[i][sides[i]]) {
                valid = false;
                break;
            }
        }
        if (!valid) break;

        // Compute implied price at current positions
        int64_t price = 0;
        for (int i = 0; i < num_legs_; i++)
            price += weights_[i] * books_[i][sides[i]][pos[i]].price;

        // Find min available quantity (adjusted for weight)
        int64_t min_qty = INT64_MAX;
        for (int i = 0; i < num_legs_; i++) {
            int64_t avail = remaining[i][pos[i]] / std::abs(weights_[i]);
            if (avail < min_qty)
                min_qty = avail;
        }

        if (min_qty <= 0) break;

        // Emit this level
        out[count++] = Level{price, min_qty};

        // Consume quantity from each leg
        for (int i = 0; i < num_legs_; i++) {
            remaining[i][pos[i]] -= min_qty * std::abs(weights_[i]);
            if (remaining[i][pos[i]] <= 0)
                pos[i]++;
        }
    }

    return count;
}

std::pair<int,int> ImpliedBook::on_update(const BookUpdate& update,
                                           Level* out_bids, Level* out_asks) {
    int leg = update.leg_index;
    int side = update.side;

    if (leg >= num_legs_) return {0, 0};

    // Apply update to outright book
    int& sz = book_sizes_[leg][side];
    int64_t price = update.level.price;

    switch (update.action) {
        case 0: { // add — insert at sorted position
            if (sz >= MAX_DEPTH) break;
            int pos = 0;
            if (side == 0) { // bid: descending
                while (pos < sz && books_[leg][side][pos].price > price) pos++;
            } else { // ask: ascending
                while (pos < sz && books_[leg][side][pos].price < price) pos++;
            }
            for (int i = sz; i > pos; i--)
                books_[leg][side][i] = books_[leg][side][i - 1];
            books_[leg][side][pos] = update.level;
            sz++;
            break;
        }
        case 1: { // modify — find by price
            for (int i = 0; i < sz; i++) {
                if (books_[leg][side][i].price == price) {
                    books_[leg][side][i].quantity = update.level.quantity;
                    break;
                }
            }
            break;
        }
        case 2: { // delete — find by price and remove
            for (int i = 0; i < sz; i++) {
                if (books_[leg][side][i].price == price) {
                    for (int j = i; j < sz - 1; j++)
                        books_[leg][side][j] = books_[leg][side][j + 1];
                    sz--;
                    break;
                }
            }
            break;
        }
    }

    // Recompute implied book via sweep
    int nbids = sweep(true, out_bids);
    int nasks = sweep(false, out_asks);
    return {nbids, nasks};
}

} // namespace hftu
