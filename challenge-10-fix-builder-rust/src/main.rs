// Benchmark harness for Challenge 10: FIX Encoder (Rust)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// The certified benchmark runs the same checks on private seeds (and a larger
// held-out stream): a solution that fails here fails there.
mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, check_failed, clobber, cycle_start, cycle_end, Rng};
use types::{FixBuilder, OrderFields, TemplateField};

const SOH: u8 = 1;

fn tf(tag: i32, v: &[u8]) -> TemplateField { TemplateField { tag, value: v.to_vec() } }

/// The public benchmark's template.
fn public_template() -> Vec<TemplateField> {
    vec![tf(8, b"FIX.4.4"), tf(35, b"D"), tf(49, b"SENDER"), tf(56, b"TARGET"), tf(34, b"0"), tf(52, b"0"),
         tf(11, b"0"), tf(55, b"AAPL"), tf(54, b"1"), tf(44, b"0.00000000"), tf(38, b"0")]
}

/// A fuller template: more header and body tags, the order fields in another order, a long text.
fn rich_template() -> Vec<TemplateField> {
    vec![tf(8, b"FIX.4.4"), tf(35, b"D"), tf(49, b"FIX-GW-02"), tf(56, b"NYSE-ARCA"), tf(34, b"0"), tf(52, b"0"),
         tf(1, b"PROP-ALPHA"), tf(11, b"0"), tf(21, b"1"), tf(55, b"NVDA"), tf(54, b"1"), tf(40, b"2"),
         tf(44, b"0.00000000"), tf(38, b"0"), tf(59, b"0"), tf(15, b"USD"), tf(207, b"XNYS"), tf(100, b"ARCX"),
         tf(47, b"A"), tf(167, b"CS"), tf(22, b"8"), tf(48, b"200456"), tf(76, b"BROKER02"),
         tf(58, b"Certified benchmark order from HFT University challenge-10 session 20261020-run")]
}

/// The public generator's orders: the same value ranges, a private seed.
fn public_orders(count: usize, seed: u64) -> Vec<OrderFields> {
    let mut rng = Rng::new(seed);
    (0..count).map(|i| OrderFields {
        timestamp: 1700000000000000000 + i as i64,
        seq_num: i as u64 + 1,
        cl_ord_id: rng.next_u64(),
        price: rng.next_i64_range(1000_00000000, 9999_00000000),
        quantity: rng.next_i64_range(1, 10000),
        side: 1 + rng.next_range(2) as i8,
    }).collect()
}

/// Wider values: prices from 0.00000001 to 999,999.99999999, quantities to 50,000,
/// ids and sequence numbers of every width, timestamps across several years.
fn wide_orders(count: usize, seed: u64) -> Vec<OrderFields> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let price_digits = 1 + rng.next_range(14) as u32;
        let id_digits = 1 + rng.next_range(20) as u32;
        OrderFields {
            timestamp: 1_600_000_000_000_000_000 + rng.next_range(200_000_000_000_000_000) as i64,
            seq_num: rng.next_range(1 << 40),
            cl_ord_id: if id_digits == 20 { rng.next_u64() } else { rng.next_range(10u64.pow(id_digits)) },
            price: rng.next_i64_range(1, 10i64.pow(price_digits) - 1),
            quantity: rng.next_i64_range(1, 50_000),
            side: 1 + rng.next_range(2) as i8,
        }
    }).collect()
}

/// Decimal integer, leading zeros allowed; anything else is a mismatch.
fn parse_int(v: &[u8]) -> Option<i64> {
    if v.is_empty() || !v.iter().all(u8::is_ascii_digit) { return None; }
    Some(v.iter().fold(0i64, |a, &b| a.wrapping_mul(10).wrapping_add((b - b'0') as i64)))
}

/// Decimal price to fixed-point * 10^8 (the C++ benchmark's reader: digits and one point).
fn parse_price(v: &[u8]) -> Option<i64> {
    let (mut whole, mut frac, mut frac_digits, mut points) = (0i64, 0i64, 0u32, 0);
    for &c in v {
        if c == b'.' { points += 1; }
        else if c.is_ascii_digit() {
            if points == 1 { if frac_digits < 8 { frac = frac * 10 + (c - b'0') as i64; frac_digits += 1; } }
            else { whole = whole.wrapping_mul(10).wrapping_add((c - b'0') as i64); }
        } else { return None; }
    }
    if points > 1 || v.is_empty() { return None; }
    for _ in frac_digits..8 { frac *= 10; }
    Some(whole.wrapping_mul(100_000_000).wrapping_add(frac))
}

/// Tag 52: the integer nanoseconds the public skeleton writes, or a FIX UTCTimestamp
/// YYYYMMDD-HH:MM:SS with up to nine fractional digits (as the C++ benchmark reads it).
fn parse_timestamp(v: &[u8]) -> Option<i64> {
    if v.iter().all(u8::is_ascii_digit) { return parse_int(v); }
    if v.len() < 17 || v[8] != b'-' || v[11] != b':' || v[14] != b':' { return None; }
    let d = |i: usize| -> Option<i64> { v[i].is_ascii_digit().then(|| (v[i] - b'0') as i64) };
    let num = |lo: usize, hi: usize| -> Option<i64> { (lo..hi).try_fold(0i64, |a, i| Some(a * 10 + d(i)?)) };
    let (year, month, day) = (num(0, 4)?, num(4, 6)?, num(6, 8)?);
    let (hour, min, sec) = (num(9, 11)?, num(12, 14)?, num(15, 17)?);
    let mut nanos = 0i64;
    if v.len() > 17 {
        if v[17] != b'.' || v.len() > 27 { return None; }
        let mut fd = 0;
        for i in 18..v.len() { nanos = nanos * 10 + d(i)?; fd += 1; }
        for _ in fd..9 { nanos *= 10; }
    }
    let (y, m) = if month <= 2 { (year - 1, month + 12) } else { (year, month) };
    let days = 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + day - 719469;
    Some((days * 86400 + hour * 3600 + min * 60 + sec) * 1_000_000_000 + nanos)
}

/// The message's fields as (tag, value, offset of the byte after its SOH).
fn split_fields(msg: &[u8]) -> Option<Vec<(&[u8], &[u8], usize)>> {
    if msg.last() != Some(&SOH) { return None; }
    let mut out = Vec::new();
    let mut start = 0;
    for (i, &b) in msg.iter().enumerate() {
        if b != SOH { continue; }
        let f = &msg[start..i];
        let eq = f.iter().position(|&c| c == b'=')?;
        if eq == 0 || !f[..eq].iter().all(u8::is_ascii_digit) { return None; }
        out.push((&f[..eq], &f[eq + 1..], i + 1));
        start = i + 1;
    }
    Some(out)
}

fn verify(msg: &[u8], template: &[TemplateField], order: &OrderFields) -> Result<(), String> {
    let fields = split_fields(msg).ok_or("not a sequence of tag=value fields, each ended by SOH")?;
    let (first, last) = (fields.first().ok_or("empty message")?, fields.last().unwrap());
    if first.0 != b"8" || first.1 != b"FIX.4.4" { return Err("does not start with 8=FIX.4.4".into()); }
    if last.0 != b"10" { return Err("the last field is not 10=".into()); }
    let tag10 = msg.len() - last.1.len() - 4;
    let sum = msg[..tag10].iter().map(|&b| b as u32).sum::<u32>() % 256;
    if last.1.len() != 3 || parse_int(last.1) != Some(sum as i64) {
        return Err(format!("checksum 10={} but the bytes before it sum to {sum:03}", String::from_utf8_lossy(last.1)));
    }
    if let Some(f) = fields.iter().find(|f| f.0 == b"9") {
        if parse_int(f.1) != Some((tag10 - f.2) as i64) {
            return Err(format!("9={} but the body up to 10= is {} bytes", String::from_utf8_lossy(f.1), tag10 - f.2));
        }
    }
    for t in template {
        let tag = t.tag.to_string().into_bytes();
        let got = fields.iter().find(|f| f.0 == tag.as_slice()).ok_or(format!("tag {} missing", t.tag))?.1;
        let shown = || String::from_utf8_lossy(got).into_owned();
        match t.tag {
            34 => if parse_int(got) != Some(order.seq_num as i64) { return Err(format!("34={} but seq_num is {}", shown(), order.seq_num)); },
            11 => if parse_int(got) != Some(order.cl_ord_id as i64) { return Err(format!("11={} but cl_ord_id is {}", shown(), order.cl_ord_id)); },
            38 => if parse_int(got) != Some(order.quantity) { return Err(format!("38={} but quantity is {}", shown(), order.quantity)); },
            54 => if parse_int(got) != Some(order.side as i64) { return Err(format!("54={} but side is {}", shown(), order.side)); },
            44 => if parse_price(got) != Some(order.price) { return Err(format!("44={} but price is {} (fixed-point 1e8)", shown(), order.price)); },
            52 => if parse_timestamp(got) != Some(order.timestamp) { return Err(format!("52={} but timestamp is {} ns", shown(), order.timestamp)); },
            _ => if got != t.value.as_slice() { return Err(format!("{}={} but the template says {}", t.tag, shown(), String::from_utf8_lossy(&t.value))); },
        }
    }
    Ok(())
}

fn main() {
    const SCORED_SEED: u64 = 0xBEEF;
    const PUBLIC_CHECK_SEED: u64 = 0xBA11D;
    const RICH_CHECK_SEED: u64 = 0xDB1CE;
    const SAMPLE_SEED: u64 = 0x5A11;
    const SCORED_COUNT: usize = 100_000;

    fn check_template(test: &str, template: &[TemplateField], orders: &[OrderFields]) -> bool {
        let mut builder = solution::MyFixBuilder::new();
        builder.build(template);
        let mut wrong = 0;
        for (i, order) in orders.iter().enumerate() {
            let msg = builder.encode(order);
            if let Err(e) = verify(msg, template, order) {
                wrong += 1;
                if wrong <= 3 {
                    let shown: String = msg.iter().take(160).map(|&b| if b == SOH { '|' } else { b as char }).collect();
                    check_failed(test, &format!("message {i}: {e}; message ({} bytes): {shown}", msg.len()));
                }
            }
        }
        if wrong > 0 { return check_failed(test, &format!("{wrong} of {} messages wrong", orders.len())); }
        eprintln!("  {test}: {} messages verified", orders.len());
        true
    }

    let mut harness = hftu_bench::Harness::new();
    harness.add_validation("correctness", || check_template("correctness", &public_template(), &public_orders(2_000, PUBLIC_CHECK_SEED)));
    harness.add_validation("correctness_wide", || check_template("correctness_wide", &rich_template(), &wide_orders(2_000, RICH_CHECK_SEED)));

    harness.add_benchmark("BM_Solution", SCORED_COUNT as u64, |iterations| {
        let template = public_template();
        let orders = public_orders(SCORED_COUNT, SCORED_SEED);
        let mut sample_rng = Rng::new(SAMPLE_SEED);
        let mut total: u64 = 0;
        for _ in 0..iterations {
            let mut builder = solution::MyFixBuilder::new();
            builder.build(&template);
            let start = cycle_start();
            for order in &orders {
                black_box(builder.encode(order));
            }
            clobber();
            let end = cycle_end();
            total += end - start;
            // Outside the timed window: the last timed message, then a sample re-encoded.
            let mut failed = verify(builder.encode(&orders[SCORED_COUNT - 1]), &template, &orders[SCORED_COUNT - 1])
                .map_err(|e| check_failed("timed", &format!("last message of the scored run: {e}"))).is_err();
            for _ in 0..1_000 {
                let i = sample_rng.next_range(SCORED_COUNT as u64) as usize;
                if let Err(e) = verify(builder.encode(&orders[i]), &template, &orders[i]) {
                    check_failed("timed", &format!("message {i} of the scored run: {e}"));
                    failed = true;
                    break;
                }
            }
            if failed {
                println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
                std::process::exit(1);
            }
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
