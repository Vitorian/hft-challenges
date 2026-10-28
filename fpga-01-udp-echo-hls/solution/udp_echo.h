// FPGA 01, HLS lane: the function you write.
//
//   void udp_echo(hls::stream<beat_t>& in, hls::stream<beat_t>& out);
//
// One 64-bit beat per clock (156.25 MHz), bytes in little-endian lanes (byte 0 = bits 7:0), TKEEP on
// the last beat, TLAST ends the frame, TUSER[0] on the last beat = bad frame. The input cannot be
// stalled (the MAC has no back-pressure) and the output must not pause inside a frame. Echo every
// UDP/IPv4 frame (no IP options, not fragmented) addressed to 192.168.1.128:1234 by swapping the
// Ethernet MACs, the IPv4 addresses and the UDP ports; drop everything else. Keep the top-level
// pragmas: ap_ctrl_none (free running) and axis on both ports.
#ifndef UDP_ECHO_H
#define UDP_ECHO_H

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

typedef ap_axiu<64, 1, 0, 0> beat_t;   // data[63:0], keep[7:0], strb[7:0], user[0], last

static const unsigned FPGA_IP_B0 = 192, FPGA_IP_B1 = 168, FPGA_IP_B2 = 1, FPGA_IP_B3 = 128;
static const unsigned ECHO_PORT = 1234;

void udp_echo(hls::stream<beat_t>& in, hls::stream<beat_t>& out);

#endif
