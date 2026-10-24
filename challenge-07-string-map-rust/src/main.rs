// Benchmark harness for Challenge 07: String Map (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// The timed lookups are the keys in a random order, copied into a buffer of their own
// (not the slices insert() received), and every timed answer is checked. Validations: a
// two-key map, then 100,000 held-out keys whose copies plus 20,000 misses are looked up
// shuffled and then in insertion order. The generator repeats short keys; a repeated
// key may answer the value of its first or its last insert. The certified run uses the
// same checks with other seeds.

mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, check_failed, clobber, cycle_start, cycle_end, Rng};
use std::collections::{HashMap, HashSet};
use types::StringMap;

const COUNT: usize = 100_000;
const MAX_LEN: usize = 16;
const MISSES: usize = 20_000;

fn generate_keys(count: usize, max_len: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let len = 1 + rng.next_range(max_len as u64) as usize;
        (0..len).map(|_| b'A' + rng.next_range(26) as u8).collect()
    }).collect()
}

/// `count` keys of the same shape that are not in `known`.
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

/// The keys in `order`, copied end to end into one buffer: (bytes, (offset, len) each).
fn query_buffer(keys: &[&[u8]], order: &[usize]) -> (Vec<u8>, Vec<(usize, usize)>) {
    let mut bytes = Vec::with_capacity(keys.len() * MAX_LEN);
    let mut spans = Vec::with_capacity(order.len());
    for &i in order {
        spans.push((bytes.len(), keys[i].len()));
        bytes.extend_from_slice(keys[i]);
    }
    (bytes, spans)
}

/// Each key's first and last inserted value (insert(keys[i], i)).
fn reference(keys: &[Vec<u8>]) -> HashMap<&[u8], (u32, u32)> {
    let mut map: HashMap<&[u8], (u32, u32)> = HashMap::with_capacity(keys.len());
    for (i, k) in keys.iter().enumerate() {
        let v = i as u32;
        map.entry(k.as_slice()).and_modify(|e| e.1 = v).or_insert((v, v));
    }
    map
}

fn answer_ok(reference: &HashMap<&[u8], (u32, u32)>, key: &[u8], got: Option<u32>) -> bool {
    match (reference.get(key), got) {
        (Some(&(first, last)), Some(v)) => v == first || v == last,
        (None, None) => true,
        _ => false,
    }
}

fn describe(reference: &HashMap<&[u8], (u32, u32)>, key: &[u8], got: Option<u32>) -> String {
    let want = match reference.get(key) {
        Some(&(f, l)) if f == l => format!("Some({f})"),
        Some(&(f, l)) => format!("Some({f}) or Some({l})"),
        None => "None".to_string(),
    };
    format!("find({:?}) returned {:?}, expected {want}", String::from_utf8_lossy(key), got)
}

/// Inserts a fresh key set and checks every hit and miss, shuffled, then every key again
/// in insertion order.
fn validate_heldout(test: &str, key_seed: u64, miss_seed: u64, order_seed: u64) -> bool {
    let keys = generate_keys(COUNT, MAX_LEN, key_seed);
    let refmap = reference(&keys);
    let known: HashSet<&[u8]> = refmap.keys().copied().collect();
    let misses = generate_misses(&known, MISSES, miss_seed);

    let mut pool: Vec<&[u8]> = keys.iter().map(|k| k.as_slice()).collect();
    pool.extend(misses.iter().map(|k| k.as_slice()));

    let mut sm = solution::MyStringMap::new();
    for (i, k) in keys.iter().enumerate() {
        sm.insert(k, i as u32);
    }
    let (bytes, spans) = query_buffer(&pool, &permutation(pool.len(), order_seed));
    for &(at, len) in &spans {
        let q = &bytes[at..at + len];
        let got = sm.find(q);
        if !answer_ok(&refmap, q, got) {
            return check_failed(test, &describe(&refmap, q, got));
        }
    }
    let in_order: Vec<usize> = (0..COUNT).collect();
    let (bytes, spans) = query_buffer(&pool, &in_order);
    for &(at, len) in &spans {
        let q = &bytes[at..at + len];
        let got = sm.find(q);
        if !answer_ok(&refmap, q, got) {
            return check_failed(test, &format!("in insertion order: {}", describe(&refmap, q, got)));
        }
    }
    eprintln!("  {test}: {} lookups verified ({} distinct keys, {} misses)", pool.len() + COUNT, refmap.len(), MISSES);
    true
}

fn main() {
    const SCORED_SEED: u64 = 0xDEAD;
    const SCORED_ORDER_SEED: u64 = 0xDEAD_0001;
    const HELDOUT_SEED: u64 = 0x5EED_0711;
    const HELDOUT_MISS_SEED: u64 = 0x5EED_0712;
    const HELDOUT_ORDER_SEED: u64 = 0x5EED_0713;

    let mut harness = hftu_bench::Harness::new();

    harness.add_validation("correctness", || {
        let mut sm = solution::MyStringMap::new();
        sm.insert(b"HELLO", 42);
        sm.insert(b"WORLD", 99);
        if sm.find(b"HELLO") != Some(42) { return check_failed("correctness", "HELLO != 42"); }
        if sm.find(b"WORLD") != Some(99) { return check_failed("correctness", "WORLD != 99"); }
        if sm.find(b"MISSING").is_some() { return check_failed("correctness", "MISSING should be None"); }
        true
    });
    harness.add_validation("heldout", || validate_heldout("heldout", HELDOUT_SEED, HELDOUT_MISS_SEED, HELDOUT_ORDER_SEED));

    harness.add_benchmark("BM_Solution", 200_000, |iterations| {
        let keys = generate_keys(COUNT, MAX_LEN, SCORED_SEED);
        let refmap = reference(&keys);
        let pool: Vec<&[u8]> = keys.iter().map(|k| k.as_slice()).collect();
        let (bytes, spans) = query_buffer(&pool, &permutation(COUNT, SCORED_ORDER_SEED));
        let queries: Vec<&[u8]> = spans.iter().map(|&(at, len)| &bytes[at..at + len]).collect();
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut sm = solution::MyStringMap::new();
            let mut answers: Vec<Option<u32>> = vec![None; COUNT];
            let start = cycle_start();
            for (j, key) in keys.iter().enumerate() { sm.insert(key, j as u32); }
            for (q, a) in queries.iter().zip(answers.iter_mut()) {
                *a = black_box(sm.find(q));
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
