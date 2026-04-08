// HFT University benchmark harness for Rust — standalone, no external dependencies.
// Do NOT modify this file — it will be overwritten during certified runs.
//
// Rust port of benchmark_harness.h. Produces identical JSON output.

pub mod tsc;

pub use tsc::{cycle_start, cycle_end};

use std::sync::atomic::{compiler_fence, Ordering};

// ---------------------------------------------------------------------------
// Compiler barriers
// ---------------------------------------------------------------------------

/// Prevent the compiler from optimizing away the value.
/// Equivalent to C++ do_not_optimize().
#[inline]
pub fn black_box<T>(x: T) -> T {
    std::hint::black_box(x)
}

/// Full compiler memory fence. Equivalent to C++ clobber().
#[inline]
pub fn clobber() {
    compiler_fence(Ordering::SeqCst);
}

// ---------------------------------------------------------------------------
// Deterministic RNG (simple xoshiro256++ — fast, good distribution)
// ---------------------------------------------------------------------------

pub struct Rng {
    s: [u64; 4],
}

impl Rng {
    pub fn new(seed: u64) -> Self {
        // SplitMix64 to initialize state from a single seed
        let mut z = seed;
        let mut s = [0u64; 4];
        for slot in &mut s {
            z = z.wrapping_add(0x9e3779b97f4a7c15);
            let mut x = z;
            x = (x ^ (x >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
            x = (x ^ (x >> 27)).wrapping_mul(0x94d049bb133111eb);
            *slot = x ^ (x >> 31);
        }
        Self { s }
    }

    #[inline]
    pub fn next_u64(&mut self) -> u64 {
        let result = (self.s[0].wrapping_add(self.s[3]))
            .rotate_left(23)
            .wrapping_add(self.s[0]);
        let t = self.s[1] << 17;
        self.s[2] ^= self.s[0];
        self.s[3] ^= self.s[1];
        self.s[1] ^= self.s[2];
        self.s[0] ^= self.s[3];
        self.s[2] ^= t;
        self.s[3] = self.s[3].rotate_left(45);
        result
    }

    /// Random u64 in [0, bound)
    #[inline]
    pub fn next_range(&mut self, bound: u64) -> u64 {
        // Lemire's fast range reduction
        let x = self.next_u64();
        ((x as u128 * bound as u128) >> 64) as u64
    }

    /// Random i64 in [lo, hi]
    #[inline]
    pub fn next_i64_range(&mut self, lo: i64, hi: i64) -> i64 {
        let range = (hi - lo) as u64 + 1;
        lo + self.next_range(range) as i64
    }
}

/// Default RNG with seed 42 (matches C++ harness convention).
pub fn rng() -> Rng {
    Rng::new(42)
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

type ValidateFn = Box<dyn Fn() -> bool>;

struct ValidateDef {
    name: &'static str,
    func: ValidateFn,
}

/// Check helper: prints FAIL message and returns false.
pub fn check_failed(test: &str, msg: &str) -> bool {
    eprintln!("FAIL [{}]: {}", test, msg);
    false
}

// ---------------------------------------------------------------------------
// Benchmark runner
// ---------------------------------------------------------------------------

type BenchmarkFn = Box<dyn Fn(i32) -> u64>;

struct BenchmarkDef {
    name: &'static str,
    ops_per_iteration: u64,
    func: BenchmarkFn,
    fixed_iterations: i32, // 0 = auto-calibrate
}

struct BenchmarkResult {
    name: &'static str,
    ops_per_iteration: u64,
    iterations: u64,
    total_cycles: u64,
}

/// Builder for registering benchmarks and validations, then running them.
pub struct Harness {
    validations: Vec<ValidateDef>,
    benchmarks: Vec<BenchmarkDef>,
}

impl Harness {
    pub fn new() -> Self {
        Self {
            validations: Vec::new(),
            benchmarks: Vec::new(),
        }
    }

    /// Register a validation that must pass before benchmarks run.
    pub fn add_validation(&mut self, name: &'static str, f: impl Fn() -> bool + 'static) {
        self.validations.push(ValidateDef {
            name,
            func: Box::new(f),
        });
    }

    /// Register a benchmark.
    /// `ops` = number of operations per iteration (for cycles_per_op calculation).
    /// `fixed_iters` = 0 for auto-calibration, >0 for fixed iteration count.
    pub fn add_benchmark(
        &mut self,
        name: &'static str,
        ops: u64,
        f: impl Fn(i32) -> u64 + 'static,
        fixed_iters: i32,
    ) {
        self.benchmarks.push(BenchmarkDef {
            name,
            ops_per_iteration: ops,
            func: Box::new(f),
            fixed_iterations: fixed_iters,
        });
    }

    /// Run all validations. Returns true if all pass.
    fn run_validations(&self) -> bool {
        if self.validations.is_empty() {
            return true;
        }
        eprintln!("Running {} validation(s)...", self.validations.len());
        let mut all_pass = true;
        for v in &self.validations {
            let ok = (v.func)();
            eprintln!("  {}: {}", v.name, if ok { "PASS" } else { "FAIL" });
            if !ok {
                all_pass = false;
            }
        }
        all_pass
    }

    /// Auto-calibrate iteration count to target ~1 billion cycles.
    fn calibrate(f: &BenchmarkFn) -> i32 {
        f(1); // warmup
        let single_cycles = f(1);

        const TARGET_CYCLES: u64 = 1_000_000_000;
        let n = TARGET_CYCLES / single_cycles.max(1);
        (n as i32).clamp(3, 1000)
    }

    /// Run validations then benchmarks. Prints JSON to stdout. Returns exit code.
    pub fn run(self) -> i32 {
        // Validate first — refuse to benchmark incorrect solutions
        if !self.run_validations() {
            println!("{{\"error\": \"Validation failed\", \"benchmarks\": []}}");
            return 1;
        }

        let mut results: Vec<BenchmarkResult> = Vec::with_capacity(self.benchmarks.len());

        for def in &self.benchmarks {
            let iters = if def.fixed_iterations > 0 {
                def.fixed_iterations
            } else {
                Self::calibrate(&def.func)
            };
            let cycles = (def.func)(iters);
            results.push(BenchmarkResult {
                name: def.name,
                ops_per_iteration: def.ops_per_iteration,
                iterations: iters as u64,
                total_cycles: cycles,
            });
        }

        // Output JSON — matching C++ format exactly
        print!("{{\n  \"benchmarks\": [\n");
        for (i, r) in results.iter().enumerate() {
            let cycles_per_op =
                r.total_cycles as f64 / (r.iterations as f64 * r.ops_per_iteration as f64);
            print!("    {{\n");
            print!("      \"name\": \"{}\",\n", r.name);
            print!("      \"iterations\": {},\n", r.iterations);
            print!("      \"ops_per_iteration\": {},\n", r.ops_per_iteration);
            print!("      \"total_cycles\": {},\n", r.total_cycles);
            print!("      \"cycles_per_op\": {:.2}\n", cycles_per_op);
            if i + 1 < results.len() {
                print!("    }},\n");
            } else {
                print!("    }}\n");
            }
        }
        println!("  ]\n}}");
        0
    }
}

impl Default for Harness {
    fn default() -> Self {
        Self::new()
    }
}
