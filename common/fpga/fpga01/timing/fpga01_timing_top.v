// Timing harness for FPGA 01 (not part of the participant's design): registers every input and
// output of fpga01_echo, so all paths through it (combinational ones included) are timed register to
// register in an out-of-context run. The simulated latency is measured without these registers.
`timescale 1ns / 1ps
`default_nettype none
module fpga01_timing_top (
    input  wire        clk,
    input  wire        rst,
    input  wire [63:0] s_tdata,
    input  wire [7:0]  s_tkeep,
    input  wire        s_tvalid,
    input  wire        s_tlast,
    input  wire        s_tuser,
    output reg  [63:0] m_tdata,
    output reg  [7:0]  m_tkeep,
    output reg         m_tvalid,
    output reg         m_tlast,
    output reg         m_tuser
);
    reg [63:0] d_q; reg [7:0] k_q; reg v_q, l_q, u_q, rst_q;
    wire [63:0] d; wire [7:0] k; wire v, l, u;
    always @(posedge clk) begin
        d_q <= s_tdata; k_q <= s_tkeep; v_q <= s_tvalid; l_q <= s_tlast; u_q <= s_tuser; rst_q <= rst;
        m_tdata <= d; m_tkeep <= k; m_tvalid <= v; m_tlast <= l; m_tuser <= u;
    end
    fpga01_echo dut (.clk(clk), .rst(rst_q), .s_tdata(d_q), .s_tkeep(k_q), .s_tvalid(v_q), .s_tlast(l_q),
        .s_tuser(u_q), .m_tdata(d), .m_tkeep(k), .m_tvalid(v), .m_tlast(l), .m_tuser(u));
endmodule
`default_nettype wire
