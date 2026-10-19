// Challenge 13: Implied Book — Skeleton (Rust)
// Naive full recalculation on every update. You can do much better!
//
// Same contract as the C++ challenge-13-implied-book skeleton.

use crate::types::{ImpliedBook, BookUpdate, Level};

const MAX_LEGS: usize = 8;
const MAX_DEPTH: usize = 10;

pub struct MyImpliedBook {
    num_legs: usize,
    depth: usize,
    weights: [i32; MAX_LEGS],
    books: [[[Level; MAX_DEPTH]; 2]; MAX_LEGS],   // [leg][side][level]
    book_sizes: [[usize; 2]; MAX_LEGS],
}

impl ImpliedBook for MyImpliedBook {
    fn new() -> Self {
        Self {
            num_legs: 0,
            depth: 0,
            weights: [0; MAX_LEGS],
            books: [[[Level::default(); MAX_DEPTH]; 2]; MAX_LEGS],
            book_sizes: [[0; 2]; MAX_LEGS],
        }
    }

    fn build(&mut self, weights: &[i32], depth: usize) {
        self.num_legs = weights.len().min(MAX_LEGS);
        self.depth = depth.min(MAX_DEPTH);
        for i in 0..self.num_legs {
            self.weights[i] = weights[i];
        }
        self.books = [[[Level::default(); MAX_DEPTH]; 2]; MAX_LEGS];
        self.book_sizes = [[0; 2]; MAX_LEGS];
    }

    fn on_update(&mut self, update: &BookUpdate,
                 out_bids: &mut [Level], out_asks: &mut [Level]) -> (usize, usize) {
        let leg = update.leg_index as usize;
        let side = update.side as usize;
        if leg >= self.num_legs {
            return (0, 0);
        }

        // Apply update to outright book
        let sz = self.book_sizes[leg][side];
        let price = update.level.price;
        let book = &mut self.books[leg][side];
        match update.action {
            0 => { // add — insert at sorted position
                if sz < MAX_DEPTH {
                    let mut pos = 0;
                    if side == 0 { // bid: descending
                        while pos < sz && book[pos].price > price { pos += 1; }
                    } else {       // ask: ascending
                        while pos < sz && book[pos].price < price { pos += 1; }
                    }
                    let mut i = sz;
                    while i > pos {
                        book[i] = book[i - 1];
                        i -= 1;
                    }
                    book[pos] = update.level;
                    self.book_sizes[leg][side] = sz + 1;
                }
            }
            1 => { // modify — find by price
                for i in 0..sz {
                    if book[i].price == price {
                        book[i].quantity = update.level.quantity;
                        break;
                    }
                }
            }
            2 => { // delete — find by price and remove
                for i in 0..sz {
                    if book[i].price == price {
                        for j in i..sz - 1 {
                            book[j] = book[j + 1];
                        }
                        self.book_sizes[leg][side] = sz - 1;
                        break;
                    }
                }
            }
            _ => {}
        }

        // Recompute implied book via sweep
        let nb = self.sweep(true, out_bids);
        let na = self.sweep(false, out_asks);
        (nb, na)
    }
}

impl MyImpliedBook {
    // Sweep through levels consuming quantity, like aggressing the book.
    // Implied bid: sell legs with w>0 (use bid side), buy legs with w<0 (use ask side).
    // Implied ask: buy legs with w>0 (use ask side), sell legs with w<0 (use bid side).
    fn sweep(&self, is_bid: bool, out: &mut [Level]) -> usize {
        // Determine which side to read from for each leg
        let mut sides = [0usize; MAX_LEGS];
        for i in 0..self.num_legs {
            let w = self.weights[i];
            sides[i] = if is_bid {
                if w > 0 { 0 } else { 1 }
            } else {
                if w > 0 { 1 } else { 0 }
            };
        }

        // Check all legs have at least one level
        for i in 0..self.num_legs {
            if self.book_sizes[i][sides[i]] == 0 {
                return 0;
            }
        }

        // Working copy of quantities remaining at each level
        let mut remaining = [[0i64; MAX_DEPTH]; MAX_LEGS];
        let mut pos = [0usize; MAX_LEGS];  // current level index per leg
        for i in 0..self.num_legs {
            let s = sides[i];
            for j in 0..self.book_sizes[i][s] {
                remaining[i][j] = self.books[i][s][j].quantity;
            }
        }

        let mut count = 0;
        while count < self.depth {
            // Check all legs have remaining levels
            if (0..self.num_legs).any(|i| pos[i] >= self.book_sizes[i][sides[i]]) {
                break;
            }

            // Compute implied price at current positions
            let mut price: i64 = 0;
            for i in 0..self.num_legs {
                price += self.weights[i] as i64 * self.books[i][sides[i]][pos[i]].price;
            }

            // Find min available quantity (adjusted for weight)
            let mut min_qty = i64::MAX;
            for i in 0..self.num_legs {
                let avail = remaining[i][pos[i]] / (self.weights[i] as i64).abs();
                if avail < min_qty { min_qty = avail; }
            }
            if min_qty <= 0 {
                break;
            }

            // Emit this level
            out[count] = Level { price, quantity: min_qty };
            count += 1;

            // Consume quantity from each leg
            for i in 0..self.num_legs {
                remaining[i][pos[i]] -= min_qty * (self.weights[i] as i64).abs();
                if remaining[i][pos[i]] <= 0 {
                    pos[i] += 1;
                }
            }
        }
        count
    }
}
