"""FPGA 02 testbench (cocotb): low-latency 10GBASE-R receive path.

DUT: module fpga02_rx #(W) (clk, rst, rx_data[W-1:0], block_lock, m_tdata[W-1:0], m_tkeep[W/8-1:0],
                            m_tvalid, m_tlast, m_tuser)
W = 32 (322.265625 MHz, 3.103 ns) or 64 (161.1328125 MHz, 6.206 ns); rx_data[0] = earliest line bit.

Oracle: tengbase.RxModel on the same line bits (validated bit-exact against Corundum's PHY).
Checks: every good frame comes out byte-exact (no FCS), in order, none missing, m_tuser = 0; frames
the model finds bad may be dropped or come out with m_tuser = 1, never as good; output beats of a
frame are contiguous; TKEEP all ones except a contiguous low mask on the last beat; block_lock is
up before the first frame. Score: worst latency over every good frame, in ns:
(time the first output beat is valid - time the first bit of frame byte 0 entered rx_data).

Environment: FPGA02_W (32 or 64), FPGA02_SEED (public tests use 1), FPGA02_SCALE, FPGA02_RESULTS.
"""
import json
import os
import random

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ReadOnly, RisingEdge
from cocotb.utils import get_sim_time

import tengbase as t

W = int(os.environ.get("FPGA02_W") or "64")
SEED = int(os.environ.get("FPGA02_SEED") or "1")
SCALE = int(os.environ.get("FPGA02_SCALE") or "1")
RESULTS = os.environ.get("FPGA02_RESULTS") or "results.json"
PERIOD_PS = {32: 3104, 64: 6208}[W]          # simulation clock (cocotb needs an even ps period)
NOMINAL_NS = {32: 3.1030, 64: 6.2061}[W]      # 322.265625 / 161.1328125 MHz, used for the score
RELOCK_BLOCKS = 400                           # frames this soon after a lock disturbance are optional
BPB = W // 8

SUMMARY = {"W": W, "period_ps": PERIOD_PS, "seed": SEED, "scale": SCALE, "tests": {},
           "latency_max_cycles": None, "latency_max_ns": None, "frames_good": 0}


def now_ps():
    return int(get_sim_time("ps"))


class Harness:
    def __init__(self, dut):
        self.dut = dut
        self.out = []            # (bytes, tuser, first_ps)
        self.errors = []
        self.lock_ps = None
        self.t0 = None           # time chunk 0 was applied

    async def start(self):
        d = self.dut
        cocotb.start_soon(Clock(d.clk, PERIOD_PS, units="ps").start())
        d.rst.value = 1
        d.rx_data.value = 0
        for _ in range(8):
            await RisingEdge(d.clk)
        d.rst.value = 0
        await RisingEdge(d.clk)
        cocotb.start_soon(self._monitor())

    async def _monitor(self):
        d = self.dut
        buf, first, in_frame = bytearray(), None, False
        while True:
            await RisingEdge(d.clk)
            await ReadOnly()
            if self.lock_ps is None and d.block_lock.value.is_resolvable and int(d.block_lock.value):
                self.lock_ps = now_ps()
            v = d.m_tvalid.value
            if not v.is_resolvable:
                if self.t0 is not None:
                    self.errors.append(f"{now_ps()} ps: m_tvalid is X/Z")
                continue
            if int(v) == 0:
                if in_frame:
                    self.errors.append(f"{now_ps()} ps: idle cycle inside an output frame")
                    buf, first, in_frame = bytearray(), None, False
                continue
            if not (d.m_tdata.value.is_resolvable and d.m_tkeep.value.is_resolvable and d.m_tlast.value.is_resolvable):
                self.errors.append(f"{now_ps()} ps: X/Z on m_tdata/m_tkeep/m_tlast while m_tvalid=1")
                continue
            data, keep, last = int(d.m_tdata.value), int(d.m_tkeep.value), int(d.m_tlast.value)
            if first is None:
                first = now_ps()
            in_frame = True
            full = (1 << BPB) - 1
            if not last and keep != full:
                self.errors.append(f"{now_ps()} ps: m_tkeep 0x{keep:x} on a beat that is not the last")
            if last and keep not in [(1 << n) - 1 for n in range(1, BPB + 1)]:
                self.errors.append(f"{now_ps()} ps: m_tkeep 0x{keep:x} is not a contiguous low mask")
            for b in range(BPB):
                if keep >> b & 1:
                    buf.append(data >> (8 * b) & 0xFF)
            if last:
                user = int(d.m_tuser.value) if d.m_tuser.value.is_resolvable else 1
                self.out.append((bytes(buf), user, first))
                buf, first, in_frame = bytearray(), None, False

    async def send_bits(self, bits):
        d = self.dut
        bits = list(bits) + [0] * (-len(bits) % W)
        for k in range(0, len(bits), W):
            await RisingEdge(d.clk)
            v = 0
            for i in range(W):
                v |= bits[k + i] << i
            d.rx_data.value = v
            if k == 0:
                self.t0 = now_ps()
        for _ in range(3000):
            await RisingEdge(d.clk)


async def run_case(dut, name, bits, disturb=None):
    """disturb: optional (first_bit, last_bit) of a deliberate lock disturbance. Good reference frames
    starting inside it or within RELOCK_BLOCKS after it are optional: receivers re-acquire lock at
    different speeds, so missing them is allowed (if one comes out it must still be byte-exact)."""
    h = Harness(dut)
    await h.start()
    model = t.RxModel()
    expected = model.run(bits)
    await h.send_bits(bits)

    errors = list(h.errors)
    def optional(b0):
        return disturb is not None and disturb[0] <= b0 <= disturb[1] + 66 * RELOCK_BLOCKS
    good_exp = [(data, b0) for data, bad, b0 in expected if not bad]
    good_out = [(data, first) for data, user, first in h.out if not user]
    lat = []
    p = 0
    for k, (got, first) in enumerate(good_out):
        j = next((j for j in range(p, min(p + 64, len(good_exp))) if good_exp[j][0] == got), None)
        if j is None:
            near = good_exp[p][0] if p < len(good_exp) else b""
            diff = next((i for i in range(min(len(got), len(near))) if got[i] != near[i]), min(len(got), len(near)))
            errors.append(f"output frame {k} with m_tuser=0 ({len(got)} B) matches no good reference frame "
                          f"(next expected {len(near)} B, first difference at byte {diff})")
            continue
        for m in range(p, j):
            if not optional(good_exp[m][1]):
                errors.append(f"good reference frame {m} ({len(good_exp[m][0])} B) never came out as good")
        t_in = h.t0 + (good_exp[j][1] // W) * PERIOD_PS
        lat.append(round((first - t_in) / PERIOD_PS))
        p = j + 1
    for m in range(p, len(good_exp)):
        if not optional(good_exp[m][1]):
            errors.append(f"good reference frame {m} ({len(good_exp[m][0])} B) never came out as good")
    if h.lock_ps is None:
        errors.append("block_lock never came up")
    elif expected and h.lock_ps > h.t0 + (expected[0][2] // W) * PERIOD_PS:
        errors.append("block_lock came up after the first frame had started")

    bad_out = sum(1 for _, user, _ in h.out if user)
    stats = {"frames_reference": len(expected), "good_reference": len(good_exp), "good_out": len(good_out),
             "bad_out": bad_out,
             "lock_blocks": None if h.lock_ps is None else round((h.lock_ps - h.t0) / PERIOD_PS * W / 66),
             "latency_min_cycles": min(lat) if lat else None, "latency_max_cycles": max(lat) if lat else None,
             "errors": len(errors)}
    SUMMARY["tests"][name] = stats
    if lat:
        SUMMARY["frames_good"] += len(lat)
        prev = SUMMARY["latency_max_cycles"]
        SUMMARY["latency_max_cycles"] = max(lat) if prev is None else max(prev, max(lat))
        SUMMARY["latency_max_ns"] = round(SUMMARY["latency_max_cycles"] * NOMINAL_NS, 3)
    SUMMARY["pass"] = all(v["errors"] == 0 for v in SUMMARY["tests"].values())
    with open(RESULTS, "w") as fh:
        json.dump(SUMMARY, fh, indent=2)
    dut._log.info(f"{name}: reference {len(expected)} frames ({len(good_exp)} good), out {len(good_out)} good + "
                  f"{bad_out} flagged, lock after {stats['lock_blocks']} blocks, latency "
                  f"{stats['latency_min_cycles']}..{stats['latency_max_cycles']} cycles, {len(errors)} errors")
    for e in errors[:15]:
        dut._log.error(e)
    assert not errors, f"{name}: {len(errors)} errors"


def frames(rng, n, sizes=(60, 1515), ipg=(5, 20)):
    return [t.TxFrame(bytes(rng.randrange(256) for _ in range(rng.randrange(*sizes))), ipg=rng.randrange(*ipg))
            for _ in range(n)]


@cocotb.test()
async def test_01_sizes(dut):
    """Every frame size 64..1518 (60..1514 bytes without FCS)."""
    rng = random.Random(SEED * 1000 + 1)
    fr = [t.TxFrame(bytes(rng.randrange(256) for _ in range(n)), ipg=rng.randrange(5, 14)) for n in range(60, 1515)]
    bits, _ = t.transmit(fr, rng)
    await run_case(dut, "sizes", bits)


@cocotb.test()
async def test_02_min_ipg_ordered_sets(dut):
    """Short frames at minimum inter-packet gaps, ordered sets between them."""
    rng = random.Random(SEED * 1000 + 2)
    bits, _ = t.transmit(frames(rng, 400 * SCALE, sizes=(60, 128), ipg=(4, 6)), rng, os_every=4)
    await run_case(dut, "min_ipg_ordered_sets", bits)


@cocotb.test()
async def test_03_bad_frames(dut):
    """Bad FCS and /E/ inside frames, between good ones (some /E/ placed so the FCS alone cannot catch it)."""
    rng = random.Random(SEED * 1000 + 3)
    fr = frames(rng, 120 * SCALE)
    for i, f in enumerate(fr):
        if i % 4 == 1:
            f.bad_fcs = True
        elif i % 4 == 3:
            f.error_at = rng.randrange(len(f.data))
            if i % 8 == 7:
                # make the bytes around the /E/ 0xFE already: a receiver that ignores the control
                # flag then sees the original bytes and a valid FCS; only real /E/ handling flags it
                pos = rng.randrange(16, len(f.data) - 16)
                f.data = f.data[:pos - 8] + b"\xfe" * 16 + f.data[pos + 8:]
                f.error_at = pos
    bits, _ = t.transmit(fr, rng)
    await run_case(dut, "bad_frames", bits)


@cocotb.test()
async def test_04_bit_errors(dut):
    """Random single-bit errors anywhere on the line (payload or sync header)."""
    rng = random.Random(SEED * 1000 + 4)
    bits, fr = t.transmit(frames(rng, 150 * SCALE), rng)
    start = fr[0].byte0_bit
    flips = sorted(rng.randrange(start, len(bits)) for _ in range(40 * SCALE))
    await run_case(dut, "bit_errors", t.flip_bits(bits, flips))


@cocotb.test()
async def test_05_lock_loss(dut):
    """A burst of corrupted sync headers drops block lock; frames after re-lock must be good."""
    rng = random.Random(SEED * 1000 + 5)
    off = rng.randrange(66)
    bits, fr = t.transmit(frames(rng, 120), rng, offset=off)
    mid = (len(bits) - off) // 66 // 2
    burst = [off + 66 * (mid + k) for k in range(24)]
    await run_case(dut, "lock_loss", t.flip_bits(bits, burst), disturb=(burst[0], burst[-1]))


@cocotb.test()
async def test_06_offsets(dut):
    """Fresh link-up from many bit offsets, a few frames each."""
    rng = random.Random(SEED * 1000 + 6)
    for n in range(6 * SCALE):
        bits, _ = t.transmit(frames(rng, 4), rng, offset=rng.randrange(66 * 3))
        await run_case(dut, f"offset_{n}", bits)
