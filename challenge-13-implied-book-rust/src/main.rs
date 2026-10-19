mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{ImpliedBook, BookUpdate, Level};

// Same workloads as the C++ challenge-13-implied-book benchmark.

struct UpdateStream {
    weights: Vec<i32>,
    depth: usize,
    updates: Vec<BookUpdate>,
}

/// Uniform f64 in [0, 1).
fn unit(rng: &mut Rng) -> f64 {
    (rng.next_u64() >> 11) as f64 * (1.0 / (1u64 << 53) as f64)
}

// Generate a realistic stream of book updates for a multi-leg synthetic
fn generate_updates(weights: &[i32], depth: usize, num_updates: usize, seed: u64) -> UpdateStream {
    let mut rng = Rng::new(seed);
    let num_legs = weights.len();

    #[derive(Clone, Copy, Default)]
    struct BookState {
        sizes: [usize; 2],
        prices: [[i64; 10]; 2],
        qtys: [[i64; 10]; 2],
    }
    let mut state = vec![BookState::default(); num_legs];
    let mut updates = Vec::with_capacity(num_updates + 2 * depth * num_legs);
    let mk = |leg: usize, side: usize, action: u8, price: i64, quantity: i64| BookUpdate {
        leg_index: leg as u16, side: side as i8, action, level: Level { price, quantity },
    };

    // Seed each leg with `depth` levels per side
    for leg in 0..num_legs {
        let bid_base = 10000 - leg as i64 * 100;
        let ask_base = 10050 - leg as i64 * 100;
        for i in 0..depth {
            let st = &mut state[leg];
            let bp = bid_base - i as i64 * 10;
            let bq = rng.next_i64_range(1, 500);
            st.prices[0][i] = bp;
            st.qtys[0][i] = bq;
            st.sizes[0] += 1;
            updates.push(mk(leg, 0, 0, bp, bq));

            let ap = ask_base + i as i64 * 10;
            let aq = rng.next_i64_range(1, 500);
            st.prices[1][i] = ap;
            st.qtys[1][i] = aq;
            st.sizes[1] += 1;
            updates.push(mk(leg, 1, 0, ap, aq));
        }
    }

    // Generate random updates
    for _ in 0..num_updates {
        let leg = rng.next_range(num_legs as u64) as usize;
        let side = rng.next_range(2) as usize;
        let st = &mut state[leg];
        let sz = st.sizes[side];
        let p = unit(&mut rng);

        if sz == 0 || p < 0.4 {
            // Add a level
            if sz < depth {
                // A new price past the worst level (below the worst bid, above the worst ask)
                let price = if side == 0 { st.prices[0][sz - 1] - 10 } else { st.prices[1][sz - 1] + 10 };
                let qty = rng.next_i64_range(1, 500);
                st.prices[side][sz] = price;
                st.qtys[side][sz] = qty;
                st.sizes[side] += 1;
                updates.push(mk(leg, side, 0, price, qty));
            } else {
                // Book full, modify instead
                let pos = rng.next_range(sz as u64) as usize;
                let qty = rng.next_i64_range(1, 500);
                st.qtys[side][pos] = qty;
                updates.push(mk(leg, side, 1, st.prices[side][pos], qty));
            }
        } else if p < 0.7 {
            // Modify
            let pos = rng.next_range(sz as u64) as usize;
            let qty = rng.next_i64_range(1, 500);
            st.qtys[side][pos] = qty;
            updates.push(mk(leg, side, 1, st.prices[side][pos], qty));
        } else {
            // Delete
            if sz > 1 {
                let pos = rng.next_range(sz as u64) as usize;
                updates.push(mk(leg, side, 2, st.prices[side][pos], st.qtys[side][pos]));
                for i in pos..sz - 1 {
                    st.prices[side][i] = st.prices[side][i + 1];
                    st.qtys[side][i] = st.qtys[side][i + 1];
                }
                st.sizes[side] -= 1;
            }
        }
    }

    UpdateStream { weights: weights.to_vec(), depth, updates }
}

fn run_stream(us: &UpdateStream, iterations: i32) -> u64 {
    let mut bids = [Level::default(); 10];
    let mut asks = [Level::default(); 10];
    let mut book = solution::MyImpliedBook::new();
    let mut total: u64 = 0;
    for _ in 0..iterations {
        book.build(&us.weights, us.depth);
        let start = cycle_start();
        for u in &us.updates {
            black_box(book.on_update(u, &mut bids, &mut asks));
        }
        clobber();
        total += cycle_end() - start;
    }
    total
}

fn main() {
    let butterfly = generate_updates(&[1, -2, 1], 5, 50_000, 0xBF1A01);
    let spread = generate_updates(&[1, -1], 5, 50_000, 0x5BEAD1);
    let strip = generate_updates(&[1, 1, 1], 5, 50_000, 0x57F1B0);

    let mut harness = hftu_bench::Harness::new();
    let n = butterfly.updates.len() as u64;
    harness.add_benchmark("butterfly_3leg", n, move |iters| run_stream(&butterfly, iters), 0);
    let n = spread.updates.len() as u64;
    harness.add_benchmark("spread_2leg", n, move |iters| run_stream(&spread, iters), 0);
    let n = strip.updates.len() as u64;
    harness.add_benchmark("strip_3leg", n, move |iters| run_stream(&strip, iters), 0);
    std::process::exit(harness.run());
}
