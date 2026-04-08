mod types;
#[path = "../solution/mod.rs"]
mod solution;
use hftu_bench::{self, black_box, clobber, cycle_start, cycle_end, Rng};
use types::{FixBuilder, OrderFields, TemplateField};

fn main() {
    let mut harness = hftu_bench::Harness::new();
    harness.add_benchmark("BM_Solution", 100_000, |iterations| {
        let template = vec![
            TemplateField { tag: 8, value: b"FIX.4.4".to_vec() },
            TemplateField { tag: 35, value: b"D".to_vec() },
            TemplateField { tag: 49, value: b"SENDER".to_vec() },
            TemplateField { tag: 56, value: b"TARGET".to_vec() },
            TemplateField { tag: 34, value: b"0".to_vec() },
            TemplateField { tag: 52, value: b"0".to_vec() },
            TemplateField { tag: 11, value: b"0".to_vec() },
            TemplateField { tag: 55, value: b"AAPL".to_vec() },
            TemplateField { tag: 54, value: b"1".to_vec() },
            TemplateField { tag: 44, value: b"0.00000000".to_vec() },
            TemplateField { tag: 38, value: b"0".to_vec() },
        ];

        let mut rng = Rng::new(0xBEEF);
        let orders: Vec<OrderFields> = (0..100_000).map(|i| OrderFields {
            timestamp: 1700000000000000000 + i,
            seq_num: i as u64 + 1,
            cl_ord_id: rng.next_u64(),
            price: rng.next_i64_range(1000_00000000, 9999_00000000),
            quantity: rng.next_i64_range(1, 10000),
            side: 1 + rng.next_range(2) as i8,
        }).collect();

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
        }
        total
    }, 0);
    std::process::exit(harness.run());
}
