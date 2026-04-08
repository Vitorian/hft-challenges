// Benchmark harness for Challenge 04: Event Scheduler No Cancel (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.

mod types;

#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::EventScheduler;

#[derive(Clone, Copy)]
enum OpType { Schedule, Advance, QuerySize, QueryNext }

#[derive(Clone, Copy)]
struct Operation {
    op: OpType,
    time_ns: i64,
    event_idx: usize,
}

fn generate_workload(seed: u64) -> (Vec<Operation>, Vec<Operation>) {
    let mut rng = Rng::new(seed);
    let num_events: usize = 500_000;
    let timed_ops: usize = 200_000;
    let start_time: i64 = 1_000_000_000;

    let pick_offset = |rng: &mut Rng| -> i64 {
        let r = rng.next_range(100);
        if r < 50 { rng.next_i64_range(1, 1_000_000) }
        else if r < 80 { rng.next_i64_range(1_000_000, 100_000_000) }
        else { rng.next_i64_range(100_000_000, 60_000_000_000) }
    };

    // Setup
    let mut setup = Vec::with_capacity(num_events);
    for i in 0..num_events {
        setup.push(Operation {
            op: OpType::Schedule,
            time_ns: start_time + pick_offset(&mut rng),
            event_idx: i,
        });
    }

    // Timed
    let mut timed = Vec::with_capacity(timed_ops);
    let mut current_time = start_time;

    for _ in 0..timed_ops {
        let r = rng.next_range(100) as i32;
        if r < 40 {
            let idx = rng.next_range(num_events as u64) as usize;
            timed.push(Operation {
                op: OpType::Schedule,
                time_ns: current_time + pick_offset(&mut rng),
                event_idx: idx,
            });
        } else if r < 70 {
            current_time += rng.next_i64_range(1_000, 1_000_000);
            timed.push(Operation { op: OpType::Advance, time_ns: current_time, event_idx: 0 });
        } else if r < 85 {
            timed.push(Operation { op: OpType::QuerySize, time_ns: 0, event_idx: 0 });
        } else {
            timed.push(Operation { op: OpType::QueryNext, time_ns: 0, event_idx: 0 });
        }
    }

    (setup, timed)
}

fn main() {
    let mut harness = hftu_bench::Harness::new();

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let (setup, timed) = generate_workload(0xDEAD);
        let mut all_latencies: Vec<u64> = Vec::with_capacity(timed.len() * iterations as usize);

        for _ in 0..iterations {
            let mut sched = solution::MyEventScheduler::new();
            let mut fire_count: u64 = 0;

            for op in &setup {
                sched.schedule(op.event_idx, op.time_ns);
            }

            for op in &timed {
                let t0 = cycle_start();
                match op.op {
                    OpType::Schedule => sched.schedule(op.event_idx, op.time_ns),
                    OpType::Advance => {
                        let n = sched.advance(op.time_ns, &mut |_, _| { fire_count += 1; });
                        black_box(n);
                    }
                    OpType::QuerySize => { black_box(sched.size()); }
                    OpType::QueryNext => { black_box(sched.next_event_time()); }
                }
                clobber();
                let t1 = cycle_end();
                all_latencies.push(t1 - t0);
            }
        }

        all_latencies.sort_unstable();
        let n = all_latencies.len();
        let p99 = all_latencies[(n * 99 / 100).min(n - 1)];
        let p50 = all_latencies[n * 50 / 100];
        let max = all_latencies[n - 1];
        let avg: f64 = all_latencies.iter().sum::<u64>() as f64 / n as f64;
        eprintln!("  Latency: p50={p50}  p99={p99}  max={max}  avg={avg:.0}  n={n}");

        p99 * n as u64
    }, 0);

    std::process::exit(harness.run());
}
