// Benchmark harness for Challenge 12: Feed Arbiter (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// The certified benchmark runs the same checks on private seeds (and a larger
// held-out stream): a solution that fails here fails there.
mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, check_failed, clobber, cycle_start, cycle_end, Rng};
use types::{FeedArbiter, Message};

/// Room past the stream's end: a wrong arbiter may emit more than there is.
const SLACK: usize = 4096;

/// The stream in order: sequences 1..=count with the public generator's payload ranges.
fn canonical(count: usize, seed: u64) -> Vec<Message> {
    let mut rng = Rng::new(seed);
    (1..=count as u64).map(|seq| Message {
        sequence: seq,
        timestamp: 1_000_000_000 + seq * 1000,
        price: rng.next_i64_range(100_00000000, 999_00000000),
        order_id: rng.next_range(1_000_000) as u32,
        quantity: rng.next_i64_range(1, 10_000) as i32,
        symbol_id: rng.next_range(200) as u16,
        msg_type: [b'A', b'X', b'E', b'P'][rng.next_range(4) as usize],
        side: rng.next_range(2) as u8,
        _pad: [0; 8],
    }).collect()
}

/// The public benchmark's delivery: the stream shuffled, then a tenth of it repeated.
fn shuffled(canon: &[Message], seed: u64) -> Vec<Message> {
    let mut rng = Rng::new(seed);
    let mut msgs = canon.to_vec();
    for i in (1..msgs.len()).rev() {
        let j = rng.next_range((i + 1) as u64) as usize;
        msgs.swap(i, j);
    }
    for _ in 0..canon.len() / 10 {
        let idx = rng.next_range(msgs.len() as u64) as usize;
        msgs.push(msgs[idx]);
    }
    msgs
}

/// Two feeds A and B with gaps (a burst of up to `max_burst` messages seen by one feed
/// only, with probability `gap_pct`), interleaved: the C++ private benchmark's delivery.
fn gap_feed(canon: &[Message], gap_pct: u64, max_burst: usize, seed: u64) -> Vec<Message> {
    let mut rng = Rng::new(seed);
    let n = canon.len();
    let (mut a, mut b): (Vec<Message>, Vec<Message>) = (Vec::new(), Vec::new());
    let mut i = 0;
    while i < n {
        if rng.next_range(100) < gap_pct {
            let len = (1 + rng.next_range(max_burst as u64) as usize).min(n - i);
            b.extend_from_slice(&canon[i..i + len]);
            i += len;
        } else if rng.next_range(100) < gap_pct {
            let len = (1 + rng.next_range(max_burst as u64) as usize).min(n - i);
            a.extend_from_slice(&canon[i..i + len]);
            i += len;
        } else {
            a.push(canon[i]);
            b.push(canon[i]);
            i += 1;
        }
    }
    let mut out = Vec::with_capacity(a.len() + b.len());
    let (mut ai, mut bi) = (0, 0);
    while ai < a.len() || bi < b.len() {
        let pick_a = if ai >= a.len() { false }
            else if bi >= b.len() { true }
            else if a[ai].sequence <= b[bi].sequence { rng.next_range(100) < 60 }
            else { rng.next_range(100) < 40 };
        if pick_a { out.push(a[ai]); ai += 1; } else { out.push(b[bi]); bi += 1; }
    }
    out
}

fn same(x: &Message, y: &Message) -> bool {
    x.sequence == y.sequence && x.timestamp == y.timestamp && x.price == y.price && x.order_id == y.order_id
        && x.quantity == y.quantity && x.symbol_id == y.symbol_id && x.msg_type == y.msg_type && x.side == y.side
}

/// The emitted stream must be `canon` exactly: every sequence once, in order, payload intact.
fn check_output(test: &str, canon: &[Message], out: &[Message], emitted: usize, quiet: bool) -> bool {
    if emitted != canon.len() {
        return check_failed(test, &format!("emitted {emitted} messages, expected {}", canon.len()));
    }
    for (i, (c, o)) in canon.iter().zip(out).enumerate() {
        if o.sequence != c.sequence {
            return check_failed(test, &format!("message {i} has sequence {}, expected {}", o.sequence, c.sequence));
        }
        if !same(c, o) {
            return check_failed(test, &format!("message {i} (sequence {}) does not carry the payload that was received", c.sequence));
        }
    }
    if !quiet { eprintln!("  {test}: {emitted} messages verified"); }
    true
}

/// Feeds `input` to a fresh arbiter, each call writing after what was emitted before.
fn validate(test: &str, canon: &[Message], input: &[Message]) -> bool {
    let mut arb = solution::MyFeedArbiter::new();
    arb.build(canon.len() as u64);
    let mut out = vec![Message::default(); canon.len() + SLACK];
    let mut emitted = 0usize;
    for msg in input {
        let at = emitted.min(canon.len());
        emitted += arb.on_message(msg, &mut out[at..]);
    }
    check_output(test, canon, &out, emitted, false)
}

fn main() {
    const SCORED_SEED: u64 = 0xBEEF;
    const SCORED_DELIVERY_SEED: u64 = 0xBEEF ^ 1;
    const SHUFFLED_SEED: u64 = 0x5EED;
    const SMALL_SEED: u64 = 0xC0DE01;
    const BURST_SEED: u64 = 0xC0DE02;
    const LARGE_SEED: u64 = 0xC0DE03;
    const COUNT: usize = 100_000;

    let mut harness = hftu_bench::Harness::new();
    harness.add_validation("shuffled", || { let c = canonical(COUNT, SHUFFLED_SEED); validate("shuffled", &c, &shuffled(&c, SHUFFLED_SEED ^ 1)) });
    harness.add_validation("small_gaps", || { let c = canonical(COUNT, SMALL_SEED); validate("small_gaps", &c, &gap_feed(&c, 10, 5, SMALL_SEED ^ 1)) });
    harness.add_validation("burst_gaps", || { let c = canonical(COUNT, BURST_SEED); validate("burst_gaps", &c, &gap_feed(&c, 15, 50, BURST_SEED ^ 1)) });
    harness.add_validation("large_gaps", || { let c = canonical(COUNT, LARGE_SEED); validate("large_gaps", &c, &gap_feed(&c, 20, 500, LARGE_SEED ^ 1)) });

    harness.add_benchmark("BM_Solution", COUNT as u64, |iterations| {
        let canon = canonical(COUNT, SCORED_SEED);
        let input = shuffled(&canon, SCORED_DELIVERY_SEED);
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut arb = solution::MyFeedArbiter::new();
            arb.build(COUNT as u64);
            let mut out = vec![Message::default(); COUNT + SLACK];
            let mut emitted = 0usize;
            let start = cycle_start();
            for msg in &input {
                let at = emitted.min(COUNT);
                emitted += arb.on_message(msg, &mut out[at..]);
            }
            clobber();
            let end = cycle_end();
            total += end - start;
            black_box(emitted);
            if !check_output("timed", &canon, &out, emitted, true) {
                println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
                std::process::exit(1);
            }
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
