mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{FixParser, SymbolEntry};

fn generate_fix_messages(count: usize, symbols: &[Vec<u8>], seed: u64) -> Vec<u8> {
    let mut rng = Rng::new(seed);
    let mut data = Vec::new();
    for i in 0..count {
        let sym = &symbols[rng.next_range(symbols.len() as u64) as usize];
        let msg_type = [b'D', b'8', b'F', b'G'][rng.next_range(4) as usize];
        let side = 1 + rng.next_range(2) as i64;
        let price = rng.next_i64_range(100, 99999);
        let qty = rng.next_i64_range(1, 10000);
        let ts = 1700000000000000000i64 + i as i64;
        let seq = i as i64 + 1;
        // Build message without checksum first
        let body = format!("8=FIX.4.4\x0135={}\x0149=SENDER\x0156=TARGET\x0134={}\x0152={}\x0155={}\x0154={}\x0144={}.{:08}\x0138={}\x01",
            msg_type as char, seq, ts,
            String::from_utf8_lossy(sym), side,
            price / 100_000_000, (price % 100_000_000).abs(), qty);
        let cksum: u32 = body.as_bytes().iter().map(|&b| b as u32).sum();
        let full = format!("{}10={:03}\x01", body, cksum % 256);
        data.extend_from_slice(full.as_bytes());
    }
    data
}

fn generate_symbols(count: usize, seed: u64) -> Vec<Vec<u8>> {
    let mut rng = Rng::new(seed);
    (0..count).map(|_| {
        let len = 1 + rng.next_range(5) as usize;
        (0..len).map(|_| b'A' + rng.next_range(26) as u8).collect()
    }).collect()
}

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 10_000, |iterations| {
        let symbols = generate_symbols(500, 0xABCD);
        let entries: Vec<SymbolEntry> = symbols.iter().enumerate()
            .map(|(i, s)| SymbolEntry { symbol: s, id: i as u32 }).collect();
        let data = generate_fix_messages(10_000, &symbols, 0xDEAD);
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
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
