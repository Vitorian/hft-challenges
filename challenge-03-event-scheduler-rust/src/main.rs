// Benchmark harness for Challenge 03: Event Scheduler with cancel (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// The certified benchmark runs the same checks on private seeds (and a larger
// held-out stream): a solution that fails here fails there.
mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, check_failed, clobber, cycle_start, cycle_end, Rng};
use std::collections::{BTreeMap, HashMap};
use types::EventScheduler;

#[derive(Clone, Copy, PartialEq, Eq)]
enum OpType { Schedule, Cancel, Advance, QuerySize, QueryNext }

#[derive(Clone, Copy)]
struct Operation { op: OpType, event_id: u64, time_us: i64 }

fn pick_offset(rng: &mut Rng) -> i64 {
    let r = rng.next_range(100);
    if r < 50 { rng.next_i64_range(1, 1_000) }
    else if r < 80 { rng.next_i64_range(1_000, 100_000) }
    else { rng.next_i64_range(100_000, 60_000_000) }
}

/// The public benchmark's generator: `num_setup` events prefilled, then `timed_ops`
/// operations (30% schedule, 15% cancel, 25% advance, 15% size, 15% next_event_time).
fn generate_workload(seed: u64, num_setup: usize, timed_ops: usize) -> (Vec<Operation>, Vec<Operation>) {
    let mut rng = Rng::new(seed);
    let start_time: i64 = 1_000_000;
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

fn op_name(op: OpType) -> &'static str {
    match op {
        OpType::Schedule => "schedule()",
        OpType::Cancel => "cancel()",
        OpType::Advance => "advance()",
        OpType::QuerySize => "size()",
        OpType::QueryNext => "next_event_time()",
    }
}

fn main() {
    const SCORED_SEED: u64 = 0xDEAD;
    const CORRECTNESS_SEED: u64 = 0x1234_5678;
    const RESCHEDULE_SEED: u64 = 0x5EED_0003;

    /// The reference: a timeline of ids per time and a lookup of each pending id's time.
    struct Reference { timeline: BTreeMap<i64, Vec<u64>>, lookup: HashMap<u64, i64> }
    impl Reference {
        fn new() -> Self { Self { timeline: BTreeMap::new(), lookup: HashMap::new() } }
        fn schedule(&mut self, id: u64, t: i64) {
            if let Some(old) = self.lookup.insert(id, t) {
                let v = self.timeline.get_mut(&old).unwrap();
                v.retain(|&x| x != id);
                if v.is_empty() { self.timeline.remove(&old); }
            }
            self.timeline.entry(t).or_default().push(id);
        }
        fn cancel(&mut self, id: u64) -> bool {
            match self.lookup.remove(&id) {
                None => false,
                Some(t) => {
                    let v = self.timeline.get_mut(&t).unwrap();
                    v.retain(|&x| x != id);
                    if v.is_empty() { self.timeline.remove(&t); }
                    true
                }
            }
        }
        /// Fires everything due at `now`; the (id, time) pairs are appended to `fired`.
        fn advance(&mut self, now: i64, fired: &mut Vec<(u64, i64)>) -> u32 {
            let mut n = 0;
            while let Some((&t, _)) = self.timeline.iter().next() {
                if t > now { break; }
                for id in self.timeline.remove(&t).unwrap() {
                    self.lookup.remove(&id);
                    fired.push((id, t));
                    n += 1;
                }
            }
            n
        }
        fn size(&self) -> u64 { self.lookup.len() as u64 }
        fn next_event_time(&self) -> i64 { self.timeline.keys().next().copied().unwrap_or(i64::MAX) }
    }

    /// Runs `timed` on the solution and the reference, comparing every answer.
    /// Ok(checks) or Err(what differed first).
    fn compare_stream(sched: &mut solution::MyEventScheduler, reference: &mut Reference, timed: &[Operation]) -> Result<usize, String> {
        let mut fired_u: Vec<(u64, i64)> = Vec::new();
        let mut fired_r: Vec<(u64, i64)> = Vec::new();
        let mut checks = 0;
        for (i, op) in timed.iter().enumerate() {
            match op.op {
                OpType::Schedule => {
                    sched.schedule(op.event_id, op.time_us);
                    reference.schedule(op.event_id, op.time_us);
                }
                OpType::Cancel => {
                    let (u, r) = (sched.cancel(op.event_id), reference.cancel(op.event_id));
                    if u != r { return Err(format!("cancel({}): got {u}, expected {r} (op #{i} of {})", op.event_id, timed.len())); }
                    checks += 1;
                }
                OpType::Advance => {
                    fired_u.clear();
                    fired_r.clear();
                    let nu = sched.advance(op.time_us, &mut |id, t| fired_u.push((id, t)));
                    let nr = reference.advance(op.time_us, &mut fired_r);
                    fired_u.sort_unstable();
                    fired_r.sort_unstable();
                    if nu as usize != fired_u.len() {
                        return Err(format!("advance({}): returned {nu} but called fire_fn {} times (op #{i})", op.time_us, fired_u.len()));
                    }
                    if fired_u != fired_r {
                        let early = fired_u.iter().filter(|e| !fired_r.contains(e)).count();
                        let missing = fired_r.iter().filter(|e| !fired_u.contains(e)).count();
                        return Err(format!(
                            "advance({}): fired {} events, expected {} ({early} not due or with the wrong time, {missing} due but not fired) (op #{i})",
                            op.time_us, fired_u.len(), fired_r.len()));
                    }
                    checks += 1;
                }
                OpType::QuerySize => {
                    let (u, r) = (sched.size(), reference.size());
                    if u != r { return Err(format!("size(): got {u}, expected {r} (op #{i})")); }
                    checks += 1;
                }
                OpType::QueryNext => {
                    let (u, r) = (sched.next_event_time(), reference.next_event_time());
                    if u != r { return Err(format!("next_event_time(): got {u}, expected {r} (op #{i}, size {})", reference.size())); }
                    checks += 1;
                }
            }
        }
        Ok(checks)
    }

    fn prefilled(seed: u64, num_setup: usize, timed_ops: usize) -> (solution::MyEventScheduler, Reference, Vec<Operation>) {
        let (setup, timed) = generate_workload(seed, num_setup, timed_ops);
        let mut sched = solution::MyEventScheduler::new();
        let mut reference = Reference::new();
        for op in &setup {
            sched.schedule(op.event_id, op.time_us);
            reference.schedule(op.event_id, op.time_us);
        }
        (sched, reference, timed)
    }

    fn stream_validation(test: &'static str, seed: u64, num_setup: usize, timed_ops: usize) -> bool {
        let (mut sched, mut reference, timed) = prefilled(seed, num_setup, timed_ops);
        match compare_stream(&mut sched, &mut reference, &timed) {
            Ok(n) => { eprintln!("  {test}: {n} checks passed"); true }
            Err(e) => check_failed(test, &e),
        }
    }

    let mut harness = hftu_bench::Harness::new();

    harness.add_validation("correctness", || stream_validation("correctness", CORRECTNESS_SEED, 5_000, 10_000));

    harness.add_validation("reschedule", || {
        let (mut sched, mut reference, _) = prefilled(RESCHEDULE_SEED, 5_000, 0);
        let mut pending: Vec<u64> = (1..=5_000).collect(); // ids pending in the reference
        let mut rng = Rng::new(RESCHEDULE_SEED ^ 0x9E37_79B9_7F4A_7C15);
        let mut now: i64 = 1_000_000;
        let mut next_id: u64 = 5_001;
        let (mut checks, mut reschedules) = (0, 0);
        let mut fired_u: Vec<(u64, i64)> = Vec::new();
        let mut fired_r: Vec<(u64, i64)> = Vec::new();
        for i in 0..10_000 {
            let r = rng.next_range(100);
            if r < 30 && !pending.is_empty() {
                // Replace a pending event: a new time, later or earlier than its current one.
                let id = pending[rng.next_range(pending.len() as u64) as usize];
                let t = now + pick_offset(&mut rng);
                sched.schedule(id, t);
                reference.schedule(id, t);
                reschedules += 1;
            } else if r < 42 {
                let id = next_id; next_id += 1;
                let t = now + pick_offset(&mut rng);
                sched.schedule(id, t);
                reference.schedule(id, t);
                pending.push(id);
            } else if r < 52 && !pending.is_empty() {
                let k = rng.next_range(pending.len() as u64) as usize;
                let id = pending.swap_remove(k);
                let (u, e) = (sched.cancel(id), reference.cancel(id));
                if u != e { return check_failed("reschedule", &format!("cancel({id}) of a rescheduled event: got {u}, expected {e} (op #{i})")); }
                checks += 1;
            } else if r < 72 {
                now += rng.next_i64_range(1, 1_000);
                fired_u.clear();
                fired_r.clear();
                let nu = sched.advance(now, &mut |id, t| fired_u.push((id, t)));
                let nr = reference.advance(now, &mut fired_r);
                for &(id, _) in &fired_r {
                    if let Some(p) = pending.iter().position(|&x| x == id) { pending.swap_remove(p); }
                }
                fired_u.sort_unstable();
                fired_r.sort_unstable();
                if nu != nr || fired_u != fired_r {
                    let early = fired_u.iter().filter(|e| !fired_r.contains(e)).count();
                    let missing = fired_r.iter().filter(|e| !fired_u.contains(e)).count();
                    return check_failed("reschedule", &format!(
                        "advance({now}) after rescheduling: fired {} events (returned {nu}), expected {} ({early} not due at this time, {missing} due but not fired) (op #{i})",
                        fired_u.len(), fired_r.len()));
                }
                checks += 1;
            } else if r < 86 {
                let (u, e) = (sched.size(), reference.size());
                if u != e { return check_failed("reschedule", &format!("size() after rescheduling: got {u}, expected {e} (op #{i})")); }
                checks += 1;
            } else {
                let (u, e) = (sched.next_event_time(), reference.next_event_time());
                if u != e { return check_failed("reschedule", &format!("next_event_time() after rescheduling: got {u}, expected {e} (op #{i})")); }
                checks += 1;
            }
        }
        eprintln!("  reschedule: {checks} checks passed over {reschedules} reschedules");
        true
    });

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let (setup, timed) = generate_workload(SCORED_SEED, 500_000, 200_000);
        // What the reference answers to every timed operation, and what it has left.
        let mut reference = Reference::new();
        for op in &setup { reference.schedule(op.event_id, op.time_us); }
        let mut scratch: Vec<(u64, i64)> = Vec::new();
        let expected: Vec<u64> = timed.iter().map(|op| match op.op {
            OpType::Schedule => { reference.schedule(op.event_id, op.time_us); 0 }
            OpType::Cancel => reference.cancel(op.event_id) as u64,
            OpType::Advance => { scratch.clear(); reference.advance(op.time_us, &mut scratch) as u64 }
            OpType::QuerySize => reference.size(),
            OpType::QueryNext => reference.next_event_time() as u64,
        }).collect();
        // Everything still pending, and the time that fires it all (within the workload's
        // own range: an advance to i64::MAX is not something the contract asks for).
        let drain_time = reference.next_event_time().max(timed.iter().map(|op| op.time_us).max().unwrap_or(0))
            .max(reference.timeline.keys().next_back().copied().unwrap_or(0));
        let mut left: Vec<(u64, i64)> = Vec::new();
        reference.advance(drain_time, &mut left);
        left.sort_unstable();

        let mut all_lat: Vec<u64> = Vec::with_capacity(timed.len() * iterations as usize);
        let mut got: Vec<u64> = vec![0; timed.len()];
        for _ in 0..iterations {
            let mut sched = solution::MyEventScheduler::new();
            let mut fires: u64 = 0;
            for op in &setup { sched.schedule(op.event_id, op.time_us); }
            for (i, op) in timed.iter().enumerate() {
                let t0 = cycle_start();
                let r = match op.op {
                    OpType::Schedule => { sched.schedule(op.event_id, op.time_us); 0 }
                    OpType::Cancel => sched.cancel(op.event_id) as u64,
                    OpType::Advance => sched.advance(op.time_us, &mut |_, _| { fires += 1; }) as u64,
                    OpType::QuerySize => sched.size(),
                    OpType::QueryNext => sched.next_event_time() as u64,
                };
                clobber();
                let t1 = cycle_end();
                got[i] = r;
                all_lat.push(t1 - t0);
            }
            // Outside the timed window: every answer, then everything still pending.
            let mut failed = false;
            if let Some(i) = (0..timed.len()).find(|&i| got[i] != expected[i]) {
                check_failed("timed", &format!("{} answered {}, expected {} (op #{i} of the scored run)", op_name(timed[i].op), got[i] as i64, expected[i] as i64));
                failed = true;
            }
            let mut rest: Vec<(u64, i64)> = Vec::new();
            sched.advance(drain_time, &mut |id, t| rest.push((id, t)));
            rest.sort_unstable();
            if rest != left {
                check_failed("timed", &format!("after the scored run {} events were still pending, expected {} (fire_fn was called {fires} times)", rest.len(), left.len()));
                failed = true;
            }
            if sched.size() != 0 || sched.next_event_time() != i64::MAX {
                check_failed("timed", &format!("drained scheduler reports size {} and next_event_time {}", sched.size(), sched.next_event_time()));
                failed = true;
            }
            if failed {
                println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
                std::process::exit(1);
            }
        }
        all_lat.sort_unstable();
        let n = all_lat.len();
        let p99 = all_lat[(n * 99 / 100).min(n - 1)];
        eprintln!("  p50={} p99={p99} max={} n={n}", all_lat[n / 2], all_lat[n - 1]);
        p99 * n as u64
    }, 0);
    std::process::exit(harness.run());
}
