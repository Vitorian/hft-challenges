mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{ImpliedBook, BookUpdate, Level};

fn generate_updates(count: usize, num_legs: usize, seed: u64) -> Vec<BookUpdate> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| BookUpdate {
        leg_index: rng.next_range(num_legs as u64) as u16,
        side: rng.next_range(2) as i8,
        action: [0, 0, 0, 1, 1, 2][rng.next_range(6) as usize], // weighted: more adds
        level: Level {
            price: rng.next_i64_range(90_00, 110_00),
            quantity: rng.next_i64_range(1, 1000),
        },
    }).collect()
}

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let weights = [1, -2, 1]; // butterfly
        let depth = 5;
        let updates = generate_updates(100_000, weights.len(), 0xCAFE);
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut book = solution::MyImpliedBook::new();
            book.build(&weights, depth);
            let mut out_bids = vec![Level::default(); depth];
            let mut out_asks = vec![Level::default(); depth];
            let start = cycle_start();
            for u in &updates {
                let (nb, na) = book.on_update(u, &mut out_bids, &mut out_asks);
                black_box(nb); black_box(na);
            }
            clobber();
            let end = cycle_end();
            total += end - start;
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
