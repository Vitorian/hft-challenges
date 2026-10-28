// Timing harness for FPGA 02 (not part of the participant's design): registers every input and
// output of fpga02_rx, so all paths through it (combinational ones included) are timed register to
// register in an out-of-context run. The simulated latency is measured without these registers.
`timescale 1ns / 1ps
`default_nettype none
module fpga02_timing_top #(parameter W = 64) (
    input  wire           clk,
    input  wire           rst,
    input  wire [W-1:0]   rx_data,
    output reg            block_lock,
    output reg  [W-1:0]   m_tdata,
    output reg  [W/8-1:0] m_tkeep,
    output reg            m_tvalid,
    output reg            m_tlast,
    output reg            m_tuser
);
    reg [W-1:0] rx_q;
    reg         rst_q;
    wire           bl;
    wire [W-1:0]   d;
    wire [W/8-1:0] k;
    wire           v, l, u;
    always @(posedge clk) begin
        rx_q <= rx_data;
        rst_q <= rst;
        block_lock <= bl;
        m_tdata <= d;
        m_tkeep <= k;
        m_tvalid <= v;
        m_tlast <= l;
        m_tuser <= u;
    end
    fpga02_rx #(.W(W)) dut (.clk(clk), .rst(rst_q), .rx_data(rx_q), .block_lock(bl),
        .m_tdata(d), .m_tkeep(k), .m_tvalid(v), .m_tlast(l), .m_tuser(u));
endmodule
`default_nettype wire
