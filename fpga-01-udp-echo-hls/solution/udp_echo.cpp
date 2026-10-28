// FPGA 01 HLS skeleton: a simple, correct, SLOW UDP echo (store and forward).
//
// A free-running pipeline: one call per clock. Each call stores the incoming beat (if any) and sends
// one beat of a queued frame (if any). A frame is checked when its last beat has arrived and, if it
// must be echoed, queued with its swapped header; so the latency is the whole frame time. Making
// that fast is the challenge.
#include "udp_echo.h"

static ap_uint<8> byte_of(const ap_uint<64>& w, int i) { return w.range(8 * i + 7, 8 * i); }

void udp_echo(hls::stream<beat_t>& in, hls::stream<beat_t>& out) {
#pragma HLS INTERFACE axis port = in
#pragma HLS INTERFACE axis port = out
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS PIPELINE II = 1 style = flp

    // frame storage: 512 beats
    static ap_uint<64> mem[512];
#pragma HLS BIND_STORAGE variable = mem type = ram_s2p impl = bram
#pragma HLS DEPENDENCE variable = mem inter false
#pragma HLS DEPENDENCE variable = mem intra false

    // receive state
    static ap_uint<9> wr_ptr = 0, fstart = 0, nbeats = 0;
    static ap_uint<64> h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;

    // queue of frames to send: start, beats, last TKEEP, TUSER, swapped header beats 0..4
    static ap_uint<9> dq_start[32], dq_beats[32];
    static ap_uint<8> dq_keep[32];
    static bool dq_user[32];
    static ap_uint<64> dq_h[5][32];
#pragma HLS ARRAY_PARTITION variable = dq_h dim = 1 complete
#pragma HLS DEPENDENCE variable = dq_start inter false
#pragma HLS DEPENDENCE variable = dq_beats inter false
#pragma HLS DEPENDENCE variable = dq_keep inter false
#pragma HLS DEPENDENCE variable = dq_user inter false
#pragma HLS DEPENDENCE variable = dq_h inter false
    static ap_uint<5> dq_wr = 0, dq_rd = 0;

    // send state
    static bool active = false;
    static ap_uint<5> cur = 0;
    static ap_uint<9> idx = 0;

    // ---- receive ----
    beat_t b;
    if (in.read_nb(b)) {
        mem[wr_ptr] = b.data;
        ap_uint<64> b0 = (nbeats == 0) ? b.data : h0;
        ap_uint<64> b1 = (nbeats == 1) ? b.data : h1;
        ap_uint<64> b2 = (nbeats == 2) ? b.data : h2;
        ap_uint<64> b3 = (nbeats == 3) ? b.data : h3;
        ap_uint<64> b4 = (nbeats == 4) ? b.data : h4;
        if (nbeats == 0) { h0 = b.data; fstart = wr_ptr; }
        if (nbeats == 1) h1 = b.data;
        if (nbeats == 2) h2 = b.data;
        if (nbeats == 3) h3 = b.data;
        if (nbeats == 4) h4 = b.data;
        ap_uint<9> start = (nbeats == 0) ? wr_ptr : fstart;
        if (b.last) {
            ap_uint<13> nbytes = (ap_uint<13>(nbeats) << 3) + ap_uint<4>(b.keep[0] + b.keep[1] + b.keep[2] + b.keep[3] +
                                                                       b.keep[4] + b.keep[5] + b.keep[6] + b.keep[7]);
            bool match = nbytes >= 42 &&
                         byte_of(b1, 4) == 0x08 && byte_of(b1, 5) == 0x00 &&           // EtherType IPv4
                         byte_of(b1, 6) == 0x45 &&                                    // no IP options
                         byte_of(b2, 7) == 17 &&                                      // UDP
                         (byte_of(b2, 4) & 0x3F) == 0 && byte_of(b2, 5) == 0 &&       // not fragmented
                         byte_of(b3, 6) == FPGA_IP_B0 && byte_of(b3, 7) == FPGA_IP_B1 &&
                         byte_of(b4, 0) == FPGA_IP_B2 && byte_of(b4, 1) == FPGA_IP_B3 &&
                         byte_of(b4, 4) == (ECHO_PORT >> 8) && byte_of(b4, 5) == (ECHO_PORT & 0xFF);
            if (match) {
                // MACs: bytes 0-5 <-> 6-11 (beats 0 and 1)
                ap_uint<128> m = (b1, b0);
                ap_uint<128> ms = (m.range(127, 96), m.range(47, 0), m.range(95, 48));
                // IPs 26-29 <-> 30-33, ports 34-35 <-> 36-37 (beats 3 and 4; byte 24+i at bits 8i+7:8i)
                ap_uint<128> p = (b4, b3);
                ap_uint<128> ps = (p.range(127, 112), p.range(95, 80), p.range(111, 96), p.range(47, 16),
                                   p.range(79, 48), p.range(15, 0));
                dq_start[dq_wr] = start;
                dq_beats[dq_wr] = nbeats + 1;
                dq_keep[dq_wr] = b.keep;
                dq_user[dq_wr] = b.user;
                dq_h[0][dq_wr] = ms.range(63, 0);
                dq_h[1][dq_wr] = ms.range(127, 64);
                dq_h[2][dq_wr] = b2;
                dq_h[3][dq_wr] = ps.range(63, 0);
                dq_h[4][dq_wr] = ps.range(127, 64);
                dq_wr++;
                wr_ptr++;
            } else {
                wr_ptr = start;                                                       // drop the frame
            }
            nbeats = 0;
        } else {
            nbeats++;
            wr_ptr++;
        }
    }

    // ---- send: one beat per call, frames back to back ----
    if (active) {
        beat_t o;
        ap_uint<64> d = mem[(ap_uint<9>)(dq_start[cur] + idx)];
        if (idx < 5) d = dq_h[idx][cur];
        bool last = (idx + 1 == dq_beats[cur]);
        o.data = d;
        o.keep = last ? dq_keep[cur] : ap_uint<8>(0xFF);
        o.strb = o.keep;
        o.user = last ? dq_user[cur] : false;
        o.last = last;
        out.write(o);
        if (last) {
            dq_rd = cur + 1;
            if (ap_uint<5>(cur + 1) != dq_wr) {
                cur = cur + 1;
                idx = 0;
            } else {
                active = false;
            }
        } else {
            idx++;
        }
    } else if (dq_rd != dq_wr) {
        active = true;
        cur = dq_rd;
        idx = 0;
    }
}
