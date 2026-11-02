// Benchmark harness for Challenge 11: Rolling Counter (Rust, v2, 2026-11)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// How the score is measured (identical to the certified benchmark, which only
// differs in its seeds):
//   * The event stream (time steps and event counts) is generated BEFORE the
//     timed region; inside the loop there is nothing but your three calls, an
//     add and one store per op.
//   * Every count() is recorded and the recorded run is verified afterwards
//     against an exact oracle with the ± precision contract: per op, with
//     cutoff = now - interval, lo = events with time > cutoff + precision and
//     hi = events with time >= cutoff - precision; lo <= count() <= hi.
//   * The score is cycles per thousand ops (cycles/kop): median of ten passes
//     of one million ops each, after one untimed warm-up pass. Fresh instance
//     and fresh stream for every pass.
//   * hftu-bench runs validations before benchmarks, so the timed passes run
//     in main() first and the harness reports their median; "scored_counts"
//     reports the verification of those passes. Edge-case checks below.

mod types;
#[path = "../solution/mod.rs"]
mod solution;

use hftu_bench::{self, black_box, clobber, cycle_end, cycle_start, Rng};
use solution::MyRollingCounter;
use types::RollingCounter;

// ---------------------------------------------------------------------------
// Seeds (PUBLIC — the certified benchmark uses different ones)
// ---------------------------------------------------------------------------
const SEED_STANDARD: u64 = 0x0000_0000_0000_ABCD;
const SEED_VALIDATION: u64 = 0x0000_0000_0C0A_AEC7;

const SCORED_PASSES: usize = 10; // timed passes; the median counts
const SCORED_OPS: usize = 1_000_000; // per pass; ops_per_iteration = SCORED_OPS / 1000

#[derive(Clone, Copy, Default)]
struct Op {
    dt: u32,
    ev: u32,
}

struct Workload {
    name: &'static str,
    interval: i64,
    precision: i64,
    dt_min: u32,
    dt_span: u64, // dt = dt_min + rng.next_range(dt_span)
    ev_min: u32,
    ev_span: u64, // ev = ev_min + rng.next_range(ev_span)
    seed: u64,
}

// ~1 kHz with a 1 s window and 1 ms precision (the scored workload).
const W_STANDARD: Workload = Workload {
    name: "BM_Solution",
    interval: 1_000_000_000,
    precision: 1_000_000,
    dt_min: 1_000_000,
    dt_span: 500_001,
    ev_min: 0,
    ev_span: 6,
    seed: SEED_STANDARD,
};
// Validation-only shapes (same as the C++ twin's other two goals).
const W_HIGH_RATE: Workload = Workload { name: "high_rate", interval: 1_000_000, precision: 1_000, dt_min: 100, dt_span: 51, ev_min: 1, ev_span: 20, seed: 0 };
const W_SPARSE: Workload = Workload { name: "sparse", interval: 1_000_000_000, precision: 10_000_000, dt_min: 1_000_000, dt_span: 499_000_001, ev_min: 0, ev_span: 3, seed: 0 };

fn pass_seed(seed: u64, pass: i64) -> u64 {
    seed ^ ((pass + 1) as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15)
}

/// Fills `ops` and returns the stream's start time (never 0: the API only
/// promises monotonic timestamps).
fn fill_stream(ops: &mut [Op], w: &Workload, seed: u64) -> i64 {
    let mut rng = Rng::new(seed);
    let t0 = 1_000_000_000_000i64 + rng.next_range(1u64 << 40) as i64;
    for o in ops.iter_mut() {
        o.dt = w.dt_min + rng.next_range(w.dt_span) as u32;
        o.ev = w.ev_min + rng.next_range(w.ev_span) as u32;
    }
    t0
}

// ---------------------------------------------------------------------------
// Oracle: the exact event log with the ± precision contract. For the current
// time t and cutoff = t - interval:
//   lo = sum of events with time >  cutoff + precision   (certainly inside)
//   hi = sum of events with time >= cutoff - precision   (possibly inside)
// Timestamps are monotonic, so both cursors only move forward.
// ---------------------------------------------------------------------------
struct Oracle {
    interval: i64,
    precision: i64,
    t: i64,
    ev: Vec<(i64, u64)>,
    lo_i: usize,
    hi_i: usize,
    sum_lo: u64,
    sum_hi: u64,
    total: u64,
}

impl Oracle {
    fn new(interval: i64, precision: i64, capacity: usize) -> Self {
        Oracle { interval, precision, t: 0, ev: Vec::with_capacity(capacity), lo_i: 0, hi_i: 0, sum_lo: 0, sum_hi: 0, total: 0 }
    }
    fn update(&mut self, t: i64) {
        self.t = t;
    }
    fn add_event(&mut self, c: u64) {
        self.ev.push((self.t, c));
        self.total += c;
    }
    /// (lo, hi) for count() right now.
    fn bounds(&mut self) -> (u64, u64) {
        let cutoff = self.t - self.interval;
        while self.hi_i < self.ev.len() && self.ev[self.hi_i].0 < cutoff - self.precision {
            self.sum_hi += self.ev[self.hi_i].1;
            self.hi_i += 1;
        }
        while self.lo_i < self.ev.len() && self.ev[self.lo_i].0 <= cutoff + self.precision {
            self.sum_lo += self.ev[self.lo_i].1;
            self.lo_i += 1;
        }
        (self.total - self.sum_lo, self.total - self.sum_hi)
    }
}

// ---------------------------------------------------------------------------
// The timed loop: the solution's three calls, an add and one store per op.
// ---------------------------------------------------------------------------
fn timed_pass(rc: &mut MyRollingCounter, mut t: i64, ops: &[Op], rec: &mut [u32]) -> u64 {
    let start = cycle_start();
    for (op, r) in ops.iter().zip(rec.iter_mut()) {
        t += op.dt as i64;
        rc.update(t);
        rc.add_event(op.ev as usize);
        *r = rc.count() as u32;
    }
    clobber();
    let end = cycle_end();
    black_box(t);
    end - start
}

/// Replays the stream through the oracle and checks every recorded count.
/// Some((op, got, lo, hi)) for the first bad op.
fn verify_recorded(w: &Workload, mut t: i64, ops: &[Op], rec: &[u32]) -> Option<(usize, u64, u64, u64)> {
    let mut o = Oracle::new(w.interval, w.precision, ops.len());
    for (j, (op, &r)) in ops.iter().zip(rec.iter()).enumerate() {
        t += op.dt as i64;
        o.update(t);
        o.add_event(op.ev as u64);
        let (lo, hi) = o.bounds();
        let got = r as u64;
        if got < lo || got > hi {
            return Some((j, got, lo, hi));
        }
    }
    None
}

struct Scored {
    median_cycles: u64,
    failure: Option<String>,
}

/// One untimed warm-up pass on a throwaway instance, then SCORED_PASSES timed
/// passes on fresh instances and fresh streams, each verified; the median
/// pass is the score (cycles for SCORED_OPS ops).
fn run_scored(w: &Workload) -> Scored {
    let mut ops = vec![Op::default(); SCORED_OPS];
    let mut rec = vec![0u32; SCORED_OPS];
    {
        let t0 = fill_stream(&mut ops, w, pass_seed(w.seed, -1));
        let mut rc = MyRollingCounter::new(w.interval, w.precision);
        timed_pass(&mut rc, t0, &ops, &mut rec);
        black_box(&rec);
    }
    let mut cycles = Vec::with_capacity(SCORED_PASSES);
    let mut failure = None;
    for pass in 0..SCORED_PASSES {
        let t0 = fill_stream(&mut ops, w, pass_seed(w.seed, pass as i64));
        let mut rc = MyRollingCounter::new(w.interval, w.precision);
        cycles.push(timed_pass(&mut rc, t0, &ops, &mut rec));
        if failure.is_none() {
            if let Some((op, got, lo, hi)) = verify_recorded(w, t0, &ops, &rec) {
                failure = Some(format!("{}: pass {} op {}: count {} outside [{}, {}]", w.name, pass, op, got, lo, hi));
            }
        }
    }
    cycles.sort_unstable();
    Scored { median_cycles: cycles[cycles.len() / 2], failure }
}

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------
fn fail(test: &str, msg: String) -> bool {
    hftu_bench::check_failed(test, &msg)
}

/// Streams a workload shape (untimed) through the solution and the oracle.
fn stream_check(test: &str, label: &str, w: &Workload, n: usize, seed: u64) -> bool {
    let mut ops = vec![Op::default(); n];
    let t0 = fill_stream(&mut ops, w, seed);
    let mut rc = MyRollingCounter::new(w.interval, w.precision);
    let mut o = Oracle::new(w.interval, w.precision, n);
    let mut t = t0;
    for (j, op) in ops.iter().enumerate() {
        t += op.dt as i64;
        rc.update(t);
        o.update(t);
        rc.add_event(op.ev as usize);
        o.add_event(op.ev as u64);
        let (lo, hi) = o.bounds();
        let got = rc.count() as u64;
        if got < lo || got > hi {
            return fail(test, format!("{}: op {}: count {} outside [{}, {}]", label, j, got, lo, hi));
        }
    }
    true
}

fn expect_eq(test: &str, label: &str, got: u64, want: u64) -> bool {
    got == want || fail(test, format!("{}: count {}, expected {}", label, got, want))
}

fn expect_in(test: &str, label: &str, got: u64, lo: u64, hi: u64) -> bool {
    (lo..=hi).contains(&got) || fail(test, format!("{}: count {} outside [{}, {}]", label, got, lo, hi))
}

/// The documented example, and the three workload shapes streamed through the
/// oracle on seeds of their own.
fn correctness() -> bool {
    let t = "correctness";
    let mut pass = true;
    {
        let mut rc = MyRollingCounter::new(1_000_000_000, 1_000_000);
        rc.update(100_000_000);
        rc.add_event(5);
        rc.update(600_000_000);
        rc.add_event(3);
        pass &= expect_eq(t, "example: both events in the window", rc.count() as u64, 8);
        rc.update(1_200_000_000);
        pass &= expect_eq(t, "example: first event expired", rc.count() as u64, 3);
    }
    {
        let mut rc = MyRollingCounter::new(100, 10);
        rc.update(50);
        rc.add_event(10);
        rc.update(200);
        pass &= expect_eq(t, "everything expired", rc.count() as u64, 0);
    }
    let s = SEED_VALIDATION;
    pass &= stream_check(t, "standard stream", &W_STANDARD, 200_000, s ^ 0x1111_1111_1111_1111);
    pass &= stream_check(t, "high-rate stream", &W_HIGH_RATE, 200_000, s ^ 0x2222_2222_2222_2222);
    pass &= stream_check(t, "sparse stream", &W_SPARSE, 200_000, s ^ 0x3333_3333_3333_3333);
    if pass {
        eprintln!("  correctness: example, three streams of 200k ops");
    }
    pass
}

/// Regimes the scored workload never exercises.
fn edge_cases() -> bool {
    let t = "edge_cases";
    let mut pass = true;
    const I: i64 = 1_000_000_000;
    const P: i64 = 1_000_000;

    // count() before any update, events before any update, zero-count events.
    // The API does not say what "the current time" is before the first update
    // (the skeleton uses 0; stamping them at the first update is as valid), so
    // the checks hold under either reading: still inside at I - 2P, gone at
    // 2I + 2P.
    {
        let mut rc = MyRollingCounter::new(I, P);
        pass &= expect_eq(t, "count before any update", rc.count() as u64, 0);
        rc.add_event(4);
        rc.add_event(0);
        pass &= expect_eq(t, "events added before the first update", rc.count() as u64, 4);
        rc.update(I - 2 * P);
        pass &= expect_eq(t, "events from before the first update, still inside", rc.count() as u64, 4);
        rc.update(2 * I + 2 * P);
        pass &= expect_eq(t, "events from before the first update, expired", rc.count() as u64, 0);
    }
    // repeated update() with the same timestamp; jumps far beyond the window
    {
        let mut rc = MyRollingCounter::new(I, P);
        rc.update(5 * I);
        rc.add_event(7);
        rc.update(5 * I);
        rc.update(5 * I);
        pass &= expect_eq(t, "repeated update with the same timestamp", rc.count() as u64, 7);
        rc.update(5 * I + I / 2);
        rc.add_event(1);
        rc.update(50 * I);
        pass &= expect_eq(t, "jump of 45 windows", rc.count() as u64, 0);
        rc.add_event(2);
        pass &= expect_eq(t, "events after the jump", rc.count() as u64, 2);
    }
    // jumps of exactly interval ± precision around a single event
    {
        let mut rc = MyRollingCounter::new(I, P);
        let e = 3 * I + 123_457;
        rc.update(e);
        rc.add_event(9);
        rc.update(e + I - 2 * P);
        pass &= expect_eq(t, "event at cutoff + 2 precision", rc.count() as u64, 9);
        rc.update(e + I);
        pass &= expect_in(t, "event exactly at the cutoff", rc.count() as u64, 0, 9);
        rc.update(e + I + 2 * P);
        pass &= expect_eq(t, "event at cutoff - 2 precision", rc.count() as u64, 0);
    }
    // totals past 2^32
    {
        let mut rc = MyRollingCounter::new(I, P);
        rc.update(7 * I);
        rc.add_event(3_000_000_000);
        rc.add_event(3_000_000_000);
        pass &= expect_eq(t, "total past 2^32", rc.count() as u64, 6_000_000_000);
        rc.update(7 * I + I / 2);
        rc.add_event(1);
        rc.update(8 * I + 2 * P);
        pass &= expect_eq(t, "large events expired", rc.count() as u64, 1);
    }
    // non-power-of-two parameters; interval not a multiple of precision
    {
        let w = Workload { name: "odd", interval: 1_000_000_007, precision: 999, dt_min: 500, dt_span: 2_000, ev_min: 0, ev_span: 4, seed: 0 };
        pass &= stream_check(t, "interval 1,000,000,007 / precision 999", &w, 100_000, SEED_VALIDATION ^ 0x4444_4444_4444_4444);
    }
    // precision 1 ns with a 1,000 ns window: exact semantics forced
    {
        let w = Workload { name: "exact", interval: 1_000, precision: 1, dt_min: 1, dt_span: 40, ev_min: 0, ev_span: 3, seed: 0 };
        pass &= stream_check(t, "precision 1 ns, window 1,000 ns", &w, 100_000, SEED_VALIDATION ^ 0x5555_5555_5555_5555);
    }
    // a deep window: interval / precision = 10^6
    {
        let w = Workload { name: "deep", interval: 1_000_000_000, precision: 1_000, dt_min: 100, dt_span: 1_901, ev_min: 0, ev_span: 3, seed: 0 };
        pass &= stream_check(t, "window of 10^6 precision units", &w, 200_000, SEED_VALIDATION ^ 0x6666_6666_6666_6666);
    }
    // start times near 2^62 and near i64::MAX (20k steps of at most 1.5 ms stay
    // 20 windows short of the end)
    for k in 0..2u64 {
        let t0 = if k == 0 { 1i64 << 62 } else { i64::MAX - 50 * I };
        let mut rc = MyRollingCounter::new(I, P);
        let mut o = Oracle::new(I, P, 20_000);
        let mut rng = Rng::new(SEED_VALIDATION ^ (0x7777_7777_7777_7777 + k));
        let mut tt = t0;
        for j in 0..20_000usize {
            tt += 1_000_000 + rng.next_range(500_001) as i64;
            let ev = rng.next_range(6);
            rc.update(tt);
            o.update(tt);
            rc.add_event(ev as usize);
            o.add_event(ev);
            let (lo, hi) = o.bounds();
            let got = rc.count() as u64;
            if got < lo || got > hi {
                pass &= fail(t, format!("start near {}: op {}: count {} outside [{}, {}]", if k == 0 { "2^62" } else { "i64::MAX" }, j, got, lo, hi));
                break;
            }
        }
    }
    // two counters alive and interleaved, different parameters
    {
        let mut a = MyRollingCounter::new(I, P);
        let mut b = MyRollingCounter::new(1_000_000, 1_000);
        let mut oa = Oracle::new(I, P, 100_000);
        let mut ob = Oracle::new(1_000_000, 1_000, 100_000);
        let mut rng = Rng::new(SEED_VALIDATION ^ 0x8888_8888_8888_8888);
        let (mut ta, mut tb) = (2 * I, 2 * I);
        for j in 0..100_000usize {
            ta += 1_000_000 + rng.next_range(500_001) as i64;
            tb += 100 + rng.next_range(51) as i64;
            let (ea, eb) = (rng.next_range(6), 1 + rng.next_range(20));
            a.update(ta);
            oa.update(ta);
            a.add_event(ea as usize);
            oa.add_event(ea);
            b.update(tb);
            ob.update(tb);
            b.add_event(eb as usize);
            ob.add_event(eb);
            let (lo, hi) = oa.bounds();
            if !(lo..=hi).contains(&(a.count() as u64)) {
                pass &= fail(t, format!("two counters: counter A op {} off", j));
                break;
            }
            let (lo, hi) = ob.bounds();
            if !(lo..=hi).contains(&(b.count() as u64)) {
                pass &= fail(t, format!("two counters: counter B op {} off", j));
                break;
            }
        }
    }
    // construct and drop many instances
    {
        let mut sum = 0u64;
        for k in 0..2_000i64 {
            let mut rc = MyRollingCounter::new(I, P);
            rc.update(I + k);
            rc.add_event(1);
            sum += rc.count() as u64;
        }
        pass &= expect_eq(t, "2000 instances constructed and dropped", sum, 2_000);
    }
    if pass {
        eprintln!("  edge cases: all passed");
    }
    pass
}

fn main() {
    // The timed passes, before the harness runs anything.
    let scored = run_scored(&W_STANDARD);

    let mut harness = hftu_bench::Harness::new();
    harness.add_validation("correctness", correctness);
    harness.add_validation("edge_cases", edge_cases);
    let failure = scored.failure.clone();
    harness.add_validation("scored_counts", move || match &failure {
        None => {
            eprintln!("  scored counts: {} passes x {} ops verified", SCORED_PASSES, SCORED_OPS);
            true
        }
        Some(msg) => hftu_bench::check_failed("scored_counts", msg),
    });

    // ops_per_iteration = SCORED_OPS / 1000: the reported figure is cycles per
    // thousand ops (unit cycles/kop). The harness calls this once.
    let median = scored.median_cycles;
    harness.add_benchmark(W_STANDARD.name, (SCORED_OPS / 1000) as u64, move |_| median, 1);

    std::process::exit(harness.run());
}
