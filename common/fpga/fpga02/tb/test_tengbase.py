"""Self-tests of the 10GBASE-R model: the transmitter and the receiver must agree.
    python3 -m pytest -q test_tengbase.py
"""
import random

import tengbase as t


def frames_random(rng, n, ipg=(5, 20)):
    return [t.TxFrame(bytes(rng.randrange(256) for _ in range(rng.randrange(60, 1515))),
                      ipg=rng.randrange(*ipg)) for _ in range(n)]


def test_round_trip_and_byte0_positions():
    rng = random.Random(1)
    for trial in range(5):
        fr = frames_random(rng, 60)
        bits, fr = t.transmit(fr, rng)
        got = t.RxModel().run(bits)
        assert len(got) == len(fr)
        for f, (data, bad, b0) in zip(fr, got):
            assert data == f.data and not bad
            assert b0 == f.byte0_bit
        assert {f.lane_start for f in fr} == {0, 4}, "both start lanes must occur"


def test_every_offset_locks():
    rng = random.Random(2)
    for off in range(0, 66 * 2):
        fr = frames_random(rng, 3)
        bits, fr = t.transmit(fr, rng, offset=off)
        got = t.RxModel().run(bits)
        assert [g[0] for g in got] == [f.data for f in fr], f"offset {off}"


def test_minimum_ipg_and_ordered_sets():
    rng = random.Random(3)
    fr = frames_random(rng, 200, ipg=(4, 6))
    bits, fr = t.transmit(fr, rng, os_every=3)
    got = t.RxModel().run(bits)
    assert [g[0] for g in got] == [f.data for f in fr]
    assert not any(g[1] for g in got)


def test_bad_fcs_and_xgmii_error_are_flagged():
    rng = random.Random(4)
    fr = frames_random(rng, 40)
    for i, f in enumerate(fr):
        if i % 3 == 1:
            f.bad_fcs = True
        elif i % 3 == 2:
            f.error_at = rng.randrange(len(f.data))
    bits, fr = t.transmit(fr, rng)
    got = t.RxModel().run(bits)
    assert len(got) == len(fr)
    for i, (f, g) in enumerate(zip(fr, got)):
        assert g[1] == (i % 3 != 0), f"frame {i}"
        if i % 3 == 0:
            assert g[0] == f.data


def test_bit_errors_flag_only_the_hit_frame():
    rng = random.Random(5)
    fr = frames_random(rng, 30)
    bits, fr = t.transmit(fr, rng)
    hit = 10
    # flip one payload bit inside frame 10's data (well after byte 0, away from block headers)
    p = fr[hit].byte0_bit + 8 * 20
    while (p - (fr[0].byte0_bit - 2 - 8 * 0)) % 66 in (0, 1):
        p += 1
    bits = t.flip_bits(bits, [p])
    got = t.RxModel().run(bits)
    assert len(got) == len(fr)
    for i, g in enumerate(got):
        assert g[1] == (i == hit), f"frame {i} bad={g[1]}"


def test_lock_loss_and_reacquire():
    rng = random.Random(6)
    fr = frames_random(rng, 80)
    bits, fr = t.transmit(fr, rng, offset=0)
    # corrupt 20 consecutive sync headers in the middle: lock must drop, then come back
    start_blk = len(bits) // 66 // 2
    pos = [66 * (start_blk + k) for k in range(20)]
    bits = t.flip_bits(bits, pos)                       # header 10/01 -> 11/00
    rx = t.RxModel()
    got = rx.run(bits)
    assert rx.lock_losses >= 1
    got_data = [g[0] for g in got if not g[1]]
    # frames well after the burst are received intact
    tail = [f.data for f in fr[-20:]]
    assert got_data[-20:] == tail
