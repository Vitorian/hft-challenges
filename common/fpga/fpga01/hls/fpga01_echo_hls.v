// fpga01_echo for the HLS lane: adapts the Vitis HLS top `udp_echo(hls::stream<beat_t>& in,
// hls::stream<beat_t>& out)` (ap_ctrl_none, ap_axiu<64,1,0,0>) to the challenge ports.
// HLS renames the stream arguments in/out to in_r/out_r (VHDL keywords). The input has no
// back-pressure (the MAC cannot stall) and the output is never stalled.
`timescale 1ns / 1ps
`default_nettype none
module fpga01_echo (
    input  wire        clk,
    input  wire        rst,
    input  wire [63:0] s_tdata,
    input  wire [7:0]  s_tkeep,
    input  wire        s_tvalid,
    input  wire        s_tlast,
    input  wire        s_tuser,
    output wire [63:0] m_tdata,
    output wire [7:0]  m_tkeep,
    output wire        m_tvalid,
    output wire        m_tlast,
    output wire        m_tuser
);
    udp_echo hls_inst (
        .ap_clk(clk),
        .ap_rst_n(!rst),
        .in_r_TDATA(s_tdata),
        .in_r_TKEEP(s_tkeep),
        .in_r_TSTRB(s_tkeep),
        .in_r_TUSER(s_tuser),
        .in_r_TLAST(s_tlast),
        .in_r_TVALID(s_tvalid),
        .in_r_TREADY(),
        .out_r_TDATA(m_tdata),
        .out_r_TKEEP(m_tkeep),
        .out_r_TSTRB(),
        .out_r_TUSER(m_tuser),
        .out_r_TLAST(m_tlast),
        .out_r_TVALID(m_tvalid),
        .out_r_TREADY(1'b1)
    );
endmodule
`default_nettype wire
