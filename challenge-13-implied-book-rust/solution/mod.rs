// Challenge 13: Implied Book — Skeleton (Rust)
// Naive full recalculation on every update. You can do much better!

use crate::types::{ImpliedBook, BookUpdate, Level};

pub struct MyImpliedBook {
    num_legs: usize,
    depth: usize,
    weights: [i32; 8],
    books: [[[Level; 10]; 2]; 8],   // [leg][side][level]
    book_sizes: [[usize; 2]; 8],
}

impl ImpliedBook for MyImpliedBook {
    fn new() -> Self {
        Self {
            num_legs: 0,
            depth: 0,
            weights: [0; 8],
            books: [[[Level::default(); 10]; 2]; 8],
            book_sizes: [[0; 2]; 8],
        }
    }

    fn build(&mut self, weights: &[i32], depth: usize) {
        self.num_legs = weights.len();
        self.depth = depth;
        for (i, &w) in weights.iter().enumerate() {
            self.weights[i] = w;
        }
    }

    fn on_update(&mut self, update: &BookUpdate,
                 out_bids: &mut [Level], out_asks: &mut [Level]) -> (usize, usize) {
        let leg = update.leg_index as usize;
        let side = update.side as usize;

        // Apply update to outright book
        match update.action {
            0 => { // add
                let n = self.book_sizes[leg][side];
                if n < 10 {
                    self.books[leg][side][n] = update.level;
                    self.book_sizes[leg][side] = n + 1;
                    // Keep sorted
                    let s = &mut self.books[leg][side][..n + 1];
                    if side == 0 {
                        s.sort_by(|a, b| b.price.cmp(&a.price)); // bids: descending
                    } else {
                        s.sort_by(|a, b| a.price.cmp(&b.price)); // asks: ascending
                    }
                }
            }
            1 => { // modify
                for i in 0..self.book_sizes[leg][side] {
                    if self.books[leg][side][i].price == update.level.price {
                        self.books[leg][side][i].quantity = update.level.quantity;
                        break;
                    }
                }
            }
            2 => { // delete
                let n = self.book_sizes[leg][side];
                for i in 0..n {
                    if self.books[leg][side][i].price == update.level.price {
                        for j in i..n - 1 {
                            self.books[leg][side][j] = self.books[leg][side][j + 1];
                        }
                        self.book_sizes[leg][side] = n - 1;
                        break;
                    }
                }
            }
            _ => {}
        }

        // Recalculate implied levels
        let nb = self.sweep(true, out_bids);
        let na = self.sweep(false, out_asks);
        (nb, na)
    }
}

impl MyImpliedBook {
    fn sweep(&self, is_bid: bool, out: &mut [Level]) -> usize {
        // For each leg, pick bid or ask side based on weight sign
        // Implied bid: buy legs with +weight (use ask), sell legs with -weight (use bid)
        // Implied ask: buy legs with -weight (use bid), sell legs with +weight (use ask)
        let mut min_depth = self.depth;
        for i in 0..self.num_legs {
            let side = if is_bid {
                if self.weights[i] > 0 { 1 } else { 0 } // bid: positive weight uses ask
            } else {
                if self.weights[i] > 0 { 0 } else { 1 } // ask: positive weight uses bid
            };
            let n = self.book_sizes[i][side];
            if n < min_depth { min_depth = n; }
        }

        for d in 0..min_depth {
            let mut price: i64 = 0;
            let mut quantity = i64::MAX;
            for i in 0..self.num_legs {
                let side = if is_bid {
                    if self.weights[i] > 0 { 1 } else { 0 }
                } else {
                    if self.weights[i] > 0 { 0 } else { 1 }
                };
                let w = self.weights[i].abs() as i64;
                let level = &self.books[i][side][d];
                price += self.weights[i] as i64 * level.price;
                let q = level.quantity / w;
                if q < quantity { quantity = q; }
            }
            out[d] = Level { price, quantity };
        }
        min_depth
    }
}
