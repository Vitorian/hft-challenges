// TSC cycle measurement — Rust port of benchmark_harness.h
//
// x86_64: CPUID+RDTSC/RDTSCP+CPUID per Intel white paper 324264-001.
// Other platforms: fallback to clock_gettime (nanoseconds, not cycles).
// Scores from non-x86 platforms are NOT comparable to certified scores.

#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::{__cpuid, __rdtscp, _rdtsc};

/// Serializing TSC read (start of timed region).
/// CPUID serializes the pipeline, then RDTSC reads the counter.
#[cfg(target_arch = "x86_64")]
#[inline]
pub fn cycle_start() -> u64 {
    unsafe {
        __cpuid(0); // serialize
        _rdtsc() as u64
    }
}

/// Serializing TSC read (end of timed region).
/// RDTSCP reads counter + serializes, then CPUID drains the pipeline.
#[cfg(target_arch = "x86_64")]
#[inline]
pub fn cycle_end() -> u64 {
    unsafe {
        let mut _aux: u32 = 0;
        let tsc = __rdtscp(&mut _aux);
        __cpuid(0); // full drain
        tsc
    }
}

// ---------------------------------------------------------------------------
// AArch64 fallback: cntvct_el0 virtual timer counter
// ---------------------------------------------------------------------------

#[cfg(target_arch = "aarch64")]
#[inline]
pub fn cycle_start() -> u64 {
    let val: u64;
    unsafe {
        core::arch::asm!("mrs {}, cntvct_el0", out(reg) val);
    }
    val
}

#[cfg(target_arch = "aarch64")]
#[inline]
pub fn cycle_end() -> u64 {
    cycle_start()
}

// ---------------------------------------------------------------------------
// Generic fallback: nanosecond wall clock (not cycle-accurate)
// ---------------------------------------------------------------------------

#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
#[inline]
pub fn cycle_start() -> u64 {
    use std::time::Instant;
    static START: std::sync::OnceLock<Instant> = std::sync::OnceLock::new();
    let start = START.get_or_init(Instant::now);
    start.elapsed().as_nanos() as u64
}

#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
#[inline]
pub fn cycle_end() -> u64 {
    cycle_start()
}
