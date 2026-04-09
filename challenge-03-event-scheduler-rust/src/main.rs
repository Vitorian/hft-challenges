mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::EventScheduler;

#[derive(Clone, Copy)]
enum OpType { Schedule, Cancel, Advance, QuerySize, QueryNext }

#[derive(Clone, Copy)]
struct Operation { op: OpType, event_id: u64, time_us: i64 }

fn generate_workload(seed: u64) -> (Vec<Operation>, Vec<Operation>) {
    let mut rng = Rng::new(seed);
    let num_setup: usize = 500_000;
    let timed_ops: usize = 200_000;
    let start_time: i64 = 1_000_000;

    let pick_offset = |rng: &mut Rng| -> i64 {
        let r = rng.next_range(100);
        if r < 50 { rng.next_i64_range(1, 1_000) }
        else if r < 80 { rng.next_i64_range(1_000, 100_000) }
        else { rng.next_i64_range(100_000, 60_000_000) }
    };

    let mut setup = Vec::with_capacity(num_setup);
    let mut next_id: u64 = 1;
    let mut active_ids: Vec<u64> = Vec::new();
    for _ in 0..num_setup {
        let id = next_id; next_id += 1;
        setup.push(Operation { op: OpType::Schedule, event_id: id, time_us: start_time + pick_offset(&mut rng) });
        active_ids.push(id);
    }

    let mut timed = Vec::with_capacity(timed_ops);
    let mut current_time = start_time;
    for _ in 0..timed_ops {
        let r = rng.next_range(100) as i32;
        if r < 30 {
            let id = next_id; next_id += 1;
            timed.push(Operation { op: OpType::Schedule, event_id: id, time_us: current_time + pick_offset(&mut rng) });
            active_ids.push(id);
        } else if r < 45 && !active_ids.is_empty() {
            let idx = rng.next_range(active_ids.len() as u64) as usize;
            let id = active_ids.swap_remove(idx);
            timed.push(Operation { op: OpType::Cancel, event_id: id, time_us: 0 });
        } else if r < 70 {
            current_time += rng.next_i64_range(1, 1_000);
            timed.push(Operation { op: OpType::Advance, event_id: 0, time_us: current_time });
        } else if r < 85 {
            timed.push(Operation { op: OpType::QuerySize, event_id: 0, time_us: 0 });
        } else {
            timed.push(Operation { op: OpType::QueryNext, event_id: 0, time_us: 0 });
        }
    }
    (setup, timed)
}

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let (setup, timed) = generate_workload(0xDEAD);
        let mut all_lat: Vec<u64> = Vec::with_capacity(timed.len() * iterations as usize);
        for _ in 0..iterations {
            let mut sched = solution::MyEventScheduler::new();
            let mut fires: u64 = 0;
            for op in &setup { sched.schedule(op.event_id, op.time_us); }
            for op in &timed {
                let t0 = cycle_start();
                match op.op {
                    OpType::Schedule => sched.schedule(op.event_id, op.time_us),
                    OpType::Cancel => { black_box(sched.cancel(op.event_id)); }
                    OpType::Advance => { let n = sched.advance(op.time_us, &mut |_, _| { fires += 1; }); black_box(n); }
                    OpType::QuerySize => { black_box(sched.size()); }
                    OpType::QueryNext => { black_box(sched.next_event_time()); }
                }
                clobber();
                let t1 = cycle_end();
                all_lat.push(t1 - t0);
            }
        }
        all_lat.sort_unstable();
        let n = all_lat.len();
        let p99 = all_lat[(n * 99 / 100).min(n - 1)];
        eprintln!("  p50={} p99={p99} max={} n={n}", all_lat[n/2], all_lat[n-1]);
        p99 * n as u64
    }, 0);
    std::process::exit(harness.run());
}
