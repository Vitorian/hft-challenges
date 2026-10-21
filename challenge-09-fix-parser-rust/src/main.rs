// Benchmark harness for Challenge 09: FIX Parser (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// The certified benchmark runs the same checks on private seeds (and a larger
// held-out stream): a solution that fails here fails there.
mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, check_failed, clobber, cycle_start, cycle_end, Rng};
use std::collections::HashMap;
use types::{FixParser, ParsedOrder, SymbolEntry};

#[derive(Clone, Copy)]
struct Expected { msg_type: u8, side: i8, symbol_id: u32, timestamp: i64, price: i64, quantity: i64, valid: bool }

fn generate_symbols(count: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let len = 1 + rng.next_range(5) as usize;
        (0..len).map(|_| b'A' + rng.next_range(26) as u8).collect()
    }).collect()
}

/// The public generator's messages (same layout and value ranges), `bad_per_mille` of
/// them with a wrong checksum, and what a correct parser returns for each.
fn generate_batch(count: usize, symbols: &[Vec<u8>], seed: u64, bad_per_mille: u64) -> (Vec<u8>, Vec<Expected>) {
    // Symbol ids as build() hands them out: the index in the table, the last entry
    // winning a repeated symbol (as the skeleton's map insert does).
    let mut ids: HashMap<&[u8], u32> = HashMap::new();
    for (i, s) in symbols.iter().enumerate() { ids.insert(s.as_slice(), i as u32); }
    let mut rng = Rng::new(seed);
    let mut data = Vec::with_capacity(count * 110);
    let mut expected = Vec::with_capacity(count);
    for i in 0..count {
        let sym = &symbols[rng.next_range(symbols.len() as u64) as usize];
        let msg_type = [b'D', b'8', b'F', b'G'][rng.next_range(4) as usize];
        let side = 1 + rng.next_range(2) as i64;
        let price = rng.next_i64_range(100, 99999);
        let qty = rng.next_i64_range(1, 10000);
        let ts = 1700000000000000000i64 + i as i64;
        let seq = i as i64 + 1;
        let body = format!("8=FIX.4.4\x0135={}\x0149=SENDER\x0156=TARGET\x0134={}\x0152={}\x0155={}\x0154={}\x0144={}.{:08}\x0138={}\x01",
            msg_type as char, seq, ts, String::from_utf8_lossy(sym), side,
            price / 100_000_000, (price % 100_000_000).abs(), qty);
        let cksum: u32 = body.bytes().map(|b| b as u32).sum::<u32>() % 256;
        let bad = rng.next_range(1000) < bad_per_mille;
        let cs = if bad { (cksum + 1 + rng.next_range(255) as u32) % 256 } else { cksum };
        data.extend_from_slice(format!("{}10={:03}\x01", body, cs).as_bytes());
        expected.push(Expected { msg_type, side: side as i8, symbol_id: ids[sym.as_slice()], timestamp: ts, price, quantity: qty, valid: !bad });
    }
    (data, expected)
}

/// Every message's `valid`, and every field of the messages with a good checksum.
fn check_orders(test: &str, expected: &[Expected], got: &[ParsedOrder], quiet: bool) -> bool {
    if got.len() != expected.len() {
        return check_failed(test, &format!("expected {} messages, got {}", expected.len(), got.len()));
    }
    let mut wrong = 0;
    for (i, (e, g)) in expected.iter().zip(got).enumerate() {
        let mut diffs: Vec<String> = Vec::new();
        if g.valid != e.valid { diffs.push(format!("valid={} expected {}", g.valid, e.valid)); }
        if e.valid {
            if g.msg_type != e.msg_type { diffs.push(format!("msg_type={:?} expected {:?}", g.msg_type as char, e.msg_type as char)); }
            if g.side != e.side { diffs.push(format!("side={} expected {}", g.side, e.side)); }
            if g.symbol_id != e.symbol_id { diffs.push(format!("symbol_id={} expected {}", g.symbol_id, e.symbol_id)); }
            if g.timestamp != e.timestamp { diffs.push(format!("timestamp={} expected {}", g.timestamp, e.timestamp)); }
            if g.price != e.price { diffs.push(format!("price={} expected {}", g.price, e.price)); }
            if g.quantity != e.quantity { diffs.push(format!("quantity={} expected {}", g.quantity, e.quantity)); }
        }
        if !diffs.is_empty() {
            wrong += 1;
            if wrong <= 5 { check_failed(test, &format!("message {i}: {}", diffs.join(", "))); }
        }
    }
    if wrong > 0 {
        return check_failed(test, &format!("{wrong} of {} messages parsed wrong", expected.len()));
    }
    if !quiet { eprintln!("  {test}: {} messages checked", got.len()); }
    true
}

fn main() {
    const SYMBOL_SEED: u64 = 0xABCD;
    const SCORED_SEED: u64 = 0xDEAD;
    const CORRECTNESS_SEED: u64 = 0xC0BB_EC7;
    const HELDOUT_SEED: u64 = 0xB16B_A7C4;
    const SCORED_COUNT: usize = 10_000;
    const BAD_PER_MILLE: u64 = 50;

    fn parse_and_check(test: &str, count: usize, seed: u64, bad_per_mille: u64) -> bool {
        let symbols = generate_symbols(500, SYMBOL_SEED);
        let entries: Vec<SymbolEntry> = symbols.iter().enumerate().map(|(i, s)| SymbolEntry { symbol: s, id: i as u32 }).collect();
        let (data, expected) = generate_batch(count, &symbols, seed, bad_per_mille);
        let mut parser = solution::MyFixParser::new();
        parser.build(&entries);
        let orders = parser.parse_batch(&data);
        if !check_orders(test, &expected, &orders, false) { return false; }
        let again = parser.parse_batch(&data);
        if !check_orders(test, &expected, &again, true) {
            return check_failed(test, "a second parse_batch() on the same parser gave a different result");
        }
        true
    }

    let mut harness = hftu_bench::Harness::new();
    harness.add_validation("correctness", || parse_and_check("correctness", 2_000, CORRECTNESS_SEED, 100));
    harness.add_validation("heldout", || parse_and_check("heldout", 20_000, HELDOUT_SEED, BAD_PER_MILLE));

    harness.add_benchmark("BM_Solution", SCORED_COUNT as u64, |iterations| {
        let symbols = generate_symbols(500, SYMBOL_SEED);
        let entries: Vec<SymbolEntry> = symbols.iter().enumerate().map(|(i, s)| SymbolEntry { symbol: s, id: i as u32 }).collect();
        let (data, expected) = generate_batch(SCORED_COUNT, &symbols, SCORED_SEED, BAD_PER_MILLE);
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut parser = solution::MyFixParser::new();
            parser.build(&entries);
            let start = cycle_start();
            let orders = parser.parse_batch(&data);
            clobber();
            let end = cycle_end();
            total += end - start;
            black_box(&orders);
            if !check_orders("timed", &expected, &orders, true) {
                println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
                std::process::exit(1);
            }
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
