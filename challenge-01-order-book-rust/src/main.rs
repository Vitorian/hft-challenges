// Benchmark harness for Challenge 01: Order Book (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end};
use types::OrderBook;

#[derive(Clone, Copy)]
enum OpType {
    Add,
    Cancel,
    BestBid,
    BestAsk,
}

#[derive(Clone, Copy)]
struct Operation {
    op: OpType,
    id: u64,
    side: i32,
    price: i64,
    quantity: i64,
}

fn generate_workload(n: usize) -> Vec<Operation> {
    let mut rng = hftu_bench::rng();
    let mut ops = Vec::with_capacity(n);
    let mut active_ids: Vec<u64> = Vec::new();
    let mut next_id: u64 = 1;

    for _ in 0..n {
        let r = rng.next_range(100) as i32;
        if r < 60 {
            let id = next_id;
            next_id += 1;
            ops.push(Operation {
                op: OpType::Add,
                id,
                side: rng.next_range(2) as i32,
                price: rng.next_i64_range(1, 1_000_000),
                quantity: rng.next_i64_range(1, 10_000),
            });
            active_ids.push(id);
        } else if r < 80 && !active_ids.is_empty() {
            let idx = rng.next_range(active_ids.len() as u64) as usize;
            let cancel_id = active_ids[idx];
            let last = active_ids.len() - 1;
            active_ids.swap(idx, last);
            active_ids.pop();
            ops.push(Operation {
                op: OpType::Cancel,
                id: cancel_id,
                side: 0,
                price: 0,
                quantity: 0,
            });
        } else if r < 90 {
            ops.push(Operation {
                op: OpType::BestBid,
                id: 0,
                side: 0,
                price: 0,
                quantity: 0,
            });
        } else {
            ops.push(Operation {
                op: OpType::BestAsk,
                id: 0,
                side: 0,
                price: 0,
                quantity: 0,
            });
        }
    }
    ops
}

fn run_workload(book: &mut impl OrderBook, ops: &[Operation]) {
    for op in ops {
        match op.op {
            OpType::Add => book.add_order(op.id, op.side, op.price, op.quantity),
            OpType::Cancel => book.cancel_order(op.id),
            OpType::BestBid => {
                black_box(book.best_bid());
            }
            OpType::BestAsk => {
                black_box(book.best_ask());
            }
        }
    }
}

fn main() {
    let mut harness = hftu_bench::Harness::new();

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let ops = generate_workload(100_000);
        let mut total_cycles: u64 = 0;

        for _ in 0..iterations {
            let mut book = solution::MyOrderBook::new();
            let start = cycle_start();
            run_workload(&mut book, &ops);
            clobber();
            let end = cycle_end();
            total_cycles += end - start;
        }
        total_cycles
    }, 0);

    std::process::exit(harness.run());
}
