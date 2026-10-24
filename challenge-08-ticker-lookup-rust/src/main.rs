// Benchmark harness for Challenge 08: Ticker Lookup (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// find() is timed on every symbol, looked up in a random order from a buffer of
// their own (not the slices build() received). Validations: a two-entry table, then a
// held-out symbol set with 10,000 misses, shuffled and then in build() order. A symbol
// given more than one value may answer its first or its last value. The timed answers
// are checked too. The certified run uses the same checks with other seeds.

mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, check_failed, clobber, cycle_start, cycle_end, Rng};
use std::collections::{HashMap, HashSet};
use types::{TickerEntry, TickerLookup};

const COUNT: usize = 50_000;
const MAX_LEN: usize = 6;
const MISSES: usize = 10_000;

/// The public benchmark's generator: `count` symbols of 1..=max_len letters A-Z.
fn generate_symbols(count: usize, max_len: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let len = 1 + rng.next_range(max_len as u64) as usize;
        (0..len).map(|_| b'A' + rng.next_range(26) as u8).collect()
    }).collect()
}

/// `count` symbols of the same shape that are not in `known`.
fn generate_misses(known: &HashSet<&[u8]>, count: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    let mut out = Vec::with_capacity(count);
    while out.len() < count {
        let len = 1 + rng.next_range(MAX_LEN as u64) as usize;
        let s: Vec<u8> = (0..len).map(|_| b'A' + rng.next_range(26) as u8).collect();
        if !known.contains(s.as_slice()) {
            out.push(s);
        }
    }
    out
}

/// 0..n in a random order (Fisher-Yates).
fn permutation(n: usize, seed: u64) -> Vec<usize> {
    let mut rng = Rng::new(seed);
    let mut p: Vec<usize> = (0..n).collect();
    for i in (1..n).rev() {
        let j = rng.next_range((i + 1) as u64) as usize;
        p.swap(i, j);
    }
    p
}

/// The symbols in `order`, copied end to end into one buffer: (bytes, (offset, len) each).
fn query_buffer(symbols: &[&[u8]], order: &[usize]) -> (Vec<u8>, Vec<(usize, usize)>) {
    let mut bytes = Vec::with_capacity(symbols.len() * MAX_LEN);
    let mut spans = Vec::with_capacity(order.len());
    for &i in order {
        spans.push((bytes.len(), symbols[i].len()));
        bytes.extend_from_slice(symbols[i]);
    }
    (bytes, spans)
}

/// Each symbol's first and last value in `entries`.
fn reference<'a>(entries: &[TickerEntry<'a>]) -> HashMap<&'a [u8], (u32, u32)> {
    let mut map: HashMap<&[u8], (u32, u32)> = HashMap::with_capacity(entries.len());
    for e in entries {
        map.entry(e.symbol).and_modify(|v| v.1 = e.value).or_insert((e.value, e.value));
    }
    map
}

/// Whether `got` is an acceptable answer for `symbol`.
fn answer_ok(reference: &HashMap<&[u8], (u32, u32)>, symbol: &[u8], got: Option<u32>) -> bool {
    match (reference.get(symbol), got) {
        (Some(&(first, last)), Some(v)) => v == first || v == last,
        (None, None) => true,
        _ => false,
    }
}

fn describe(reference: &HashMap<&[u8], (u32, u32)>, symbol: &[u8], got: Option<u32>) -> String {
    let want = match reference.get(symbol) {
        Some(&(f, l)) if f == l => format!("Some({f})"),
        Some(&(f, l)) => format!("Some({f}) or Some({l})"),
        None => "None".to_string(),
    };
    format!("find({:?}) returned {:?}, expected {want}", String::from_utf8_lossy(symbol), got)
}

/// Builds a table from a fresh symbol set and checks every hit and miss, shuffled, then
/// every symbol again in build() order.
fn validate_heldout(test: &str, symbol_seed: u64, miss_seed: u64, order_seed: u64) -> bool {
    let owned = generate_symbols(COUNT, MAX_LEN, symbol_seed);
    let entries: Vec<TickerEntry> = owned.iter().enumerate()
        .map(|(i, s)| TickerEntry { symbol: s, value: i as u32 }).collect();
    let refmap = reference(&entries);
    let known: HashSet<&[u8]> = refmap.keys().copied().collect();
    let misses = generate_misses(&known, MISSES, miss_seed);

    let mut pool: Vec<&[u8]> = owned.iter().map(|s| s.as_slice()).collect();
    pool.extend(misses.iter().map(|s| s.as_slice()));
    let (bytes, spans) = query_buffer(&pool, &permutation(pool.len(), order_seed));

    let mut tl = solution::MyTickerLookup::new();
    tl.build(&entries);
    for &(at, len) in &spans {
        let q = &bytes[at..at + len];
        let got = tl.find(q);
        if !answer_ok(&refmap, q, got) {
            return check_failed(test, &describe(&refmap, q, got));
        }
    }
    let in_order: Vec<usize> = (0..COUNT).collect();
    let (bytes, spans) = query_buffer(&pool, &in_order);
    for &(at, len) in &spans {
        let q = &bytes[at..at + len];
        let got = tl.find(q);
        if !answer_ok(&refmap, q, got) {
            return check_failed(test, &format!("in build() order: {}", describe(&refmap, q, got)));
        }
    }
    eprintln!("  {test}: {} lookups verified ({} symbols, {} misses)", pool.len() + COUNT, refmap.len(), MISSES);
    true
}

fn main() {
    const SCORED_SEED: u64 = 0xBEEF;
    const SCORED_ORDER_SEED: u64 = 0xBEEF_0001;
    const HELDOUT_SEED: u64 = 0x5EED_0008;
    const HELDOUT_MISS_SEED: u64 = 0x5EED_0009;
    const HELDOUT_ORDER_SEED: u64 = 0x5EED_000A;

    let mut harness = hftu_bench::Harness::new();

    harness.add_validation("correctness", || {
        let mut tl = solution::MyTickerLookup::new();
        tl.build(&[
            TickerEntry { symbol: b"AAPL", value: 1 },
            TickerEntry { symbol: b"GOOG", value: 2 },
        ]);
        if tl.find(b"AAPL") != Some(1) { return check_failed("correctness", "AAPL != 1"); }
        if tl.find(b"GOOG") != Some(2) { return check_failed("correctness", "GOOG != 2"); }
        if tl.find(b"MSFT").is_some() { return check_failed("correctness", "MSFT should be None"); }
        true
    });
    harness.add_validation("heldout", || validate_heldout("heldout", HELDOUT_SEED, HELDOUT_MISS_SEED, HELDOUT_ORDER_SEED));

    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let owned = generate_symbols(COUNT, MAX_LEN, SCORED_SEED);
        let entries: Vec<TickerEntry> = owned.iter().enumerate()
            .map(|(i, s)| TickerEntry { symbol: s, value: i as u32 }).collect();
        let refmap = reference(&entries);
        let pool: Vec<&[u8]> = owned.iter().map(|s| s.as_slice()).collect();
        let (bytes, spans) = query_buffer(&pool, &permutation(COUNT, SCORED_ORDER_SEED));
        let queries: Vec<&[u8]> = spans.iter().map(|&(at, len)| &bytes[at..at + len]).collect();
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut tl = solution::MyTickerLookup::new();
            tl.build(&entries);
            let mut answers: Vec<Option<u32>> = vec![None; COUNT];
            let start = cycle_start();
            for (q, a) in queries.iter().zip(answers.iter_mut()) {
                *a = black_box(tl.find(q));
            }
            clobber();
            let end = cycle_end();
            total += end - start;
            for (q, &a) in queries.iter().zip(&answers) {
                if !answer_ok(&refmap, q, a) {
                    check_failed("timed", &describe(&refmap, q, a));
                    println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
                    std::process::exit(1);
                }
            }
        }
        total
    }, 0);

    std::process::exit(harness.run());
}
