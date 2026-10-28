// fpga01_echo skeleton: a simple, correct, SLOW UDP echo (store and forward).
//
// Every frame is stored completely; when its last beat has arrived the frame is checked and, if it
// is UDP/IPv4 to 192.168.1.128:1234, sent back with the Ethernet MACs, IPv4 addresses and UDP ports
// swapped. The latency is therefore the whole frame time. Making that fast is the challenge.
//
// Ports: one 64-bit beat per clock (156.25 MHz), byte 0 of the frame in bits 7:0, no TREADY on
// either side: the input cannot be stalled, and the output must not pause inside a frame.

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
    output reg  [63:0] m_tdata,
    output reg  [7:0]  m_tkeep,
    output reg         m_tvalid,
    output reg         m_tlast,
    output reg         m_tuser
);

// ---------------------------------------------------------------------------------------------
// Receive: store beats, keep the first five (the header window), decide at the last beat
// ---------------------------------------------------------------------------------------------
(* ram_style = "block" *) reg [63:0] mem [0:511];

reg  [8:0]  wr_ptr;              // next beat to write
reg  [8:0]  fstart;              // first beat of the frame being received
reg  [8:0]  nbeats;              // beats so far in this frame
reg  [63:0] h0, h1, h2, h3, h4;  // beats 0..4

always @(posedge clk) begin
    if (s_tvalid) mem[wr_ptr] <= s_tdata;
end

// the header as it will be after this beat (the last beat may itself be beat 4)
wire [63:0] b0 = (nbeats == 0) ? s_tdata : h0;
wire [63:0] b1 = (nbeats == 1) ? s_tdata : h1;
wire [63:0] b2 = (nbeats == 2) ? s_tdata : h2;
wire [63:0] b3 = (nbeats == 3) ? s_tdata : h3;
wire [63:0] b4 = (nbeats == 4) ? s_tdata : h4;

function [3:0] popcount8(input [7:0] k);
    integer i;
    begin
        popcount8 = 0;
        for (i = 0; i < 8; i = i + 1) popcount8 = popcount8 + k[i];
    end
endfunction

wire [12:0] nbytes = {nbeats, 3'b000} + popcount8(s_tkeep);   // bytes including this last beat

// byte n of the frame is in beat n/8, bits 8*(n%8)+7 : 8*(n%8)
wire match = (nbytes >= 42) &&
             (b1[39:32] == 8'h08) && (b1[47:40] == 8'h00) &&            // EtherType 0x0800
             (b1[55:48] == 8'h45) &&                                    // IPv4, no options
             (b2[63:56] == 8'd17) &&                                    // UDP
             (b2[37:32] == 6'd0) && (b2[47:40] == 8'h00) &&             // MF = 0, offset = 0
             (b3[55:48] == 8'd192) && (b3[63:56] == 8'd168) &&          // dst IP 192.168.1.128
             (b4[7:0] == 8'd1) && (b4[15:8] == 8'd128) &&
             (b4[39:32] == 8'h04) && (b4[47:40] == 8'hD2);              // dst port 1234

// swapped header: MACs (bytes 0-5 <-> 6-11), IPs (26-29 <-> 30-33), ports (34-35 <-> 36-37)
wire [127:0] mac_in  = {b1, b0};
wire [127:0] mac_out = {mac_in[127:96], mac_in[47:0], mac_in[95:48]};
wire [63:0]  sw0 = mac_out[63:0];
wire [63:0]  sw1 = mac_out[127:64];
// bytes 24..39 = {b4, b3}, byte 24 + i in bits 8i+7:8i: src IP 26..29 = bits 47:16,
// dst IP 30..33 = bits 79:48, src port 34..35 = bits 95:80, dst port 36..37 = bits 111:96
wire [127:0] ip_in  = {b4, b3};
wire [127:0] ip_out = {ip_in[127:112],               // bytes 38..39 unchanged
                       ip_in[95:80], ip_in[111:96],  // ports: 36..37 <- 34..35, 34..35 <- 36..37
                       ip_in[47:16],                 // bytes 30..33 <- old src IP 26..29
                       ip_in[79:48],                 // bytes 26..29 <- old dst IP 30..33
                       ip_in[15:0]};                 // bytes 24..25 unchanged
wire [63:0]  sw3 = ip_out[63:0];
wire [63:0]  sw4 = ip_out[127:64];

// descriptor FIFO (32 frames)
reg  [8:0]  dq_start [0:31];
reg  [8:0]  dq_beats [0:31];
reg  [7:0]  dq_keep  [0:31];
reg         dq_user  [0:31];
reg  [63:0] dq_h0 [0:31], dq_h1 [0:31], dq_h2 [0:31], dq_h3 [0:31], dq_h4 [0:31];
reg  [4:0]  dq_wr, dq_rd;

always @(posedge clk) begin
    if (s_tvalid) begin
        case (nbeats)
            9'd0: h0 <= s_tdata;
            9'd1: h1 <= s_tdata;
            9'd2: h2 <= s_tdata;
            9'd3: h3 <= s_tdata;
            9'd4: h4 <= s_tdata;
            default: ;
        endcase
        if (nbeats == 0) fstart <= wr_ptr;
        if (s_tlast) begin
            nbeats <= 0;
            if (match) begin
                dq_start[dq_wr] <= (nbeats == 0) ? wr_ptr : fstart;
                dq_beats[dq_wr] <= nbeats + 1;
                dq_keep[dq_wr] <= s_tkeep;
                dq_user[dq_wr] <= s_tuser;
                dq_h0[dq_wr] <= sw0;
                dq_h1[dq_wr] <= sw1;
                dq_h2[dq_wr] <= b2;
                dq_h3[dq_wr] <= sw3;
                dq_h4[dq_wr] <= sw4;
                dq_wr <= dq_wr + 1;
                wr_ptr <= wr_ptr + 1;
            end else begin
                wr_ptr <= (nbeats == 0) ? wr_ptr : fstart;   // drop: forget the frame
            end
        end else begin
            nbeats <= nbeats + 1;
            wr_ptr <= wr_ptr + 1;
        end
    end
    if (rst) begin
        wr_ptr <= 0;
        nbeats <= 0;
        dq_wr <= 0;
    end
end

// ---------------------------------------------------------------------------------------------
// Send: one beat per clock, frames back to back. rd_q is registered from the block RAM and
// m_tdata is registered from rd_q, so the read address runs two beats ahead of the beat being
// sent; on the last beat it jumps to the next frame. Beats 0..4 come from the descriptor.
// ---------------------------------------------------------------------------------------------
reg  [8:0]  rd_addr;
reg  [63:0] rd_q;
always @(posedge clk) rd_q <= mem[rd_addr];

reg         active;
reg  [4:0]  cur;                 // descriptor being sent
reg  [8:0]  idx;                 // beat being sent

wire        have_next = active ? (cur + 5'd1 != dq_wr) : (dq_rd != dq_wr);
wire [4:0]  next_d    = active ? cur + 5'd1 : dq_rd;
wire        last_beat = active && (idx + 1 == dq_beats[cur]);

always @(posedge clk) begin
    m_tvalid <= 1'b0;
    m_tlast <= 1'b0;
    m_tuser <= 1'b0;
    if (active) begin
        m_tvalid <= 1'b1;
        case (idx)
            9'd0: m_tdata <= dq_h0[cur];
            9'd1: m_tdata <= dq_h1[cur];
            9'd2: m_tdata <= dq_h2[cur];
            9'd3: m_tdata <= dq_h3[cur];
            9'd4: m_tdata <= dq_h4[cur];
            default: m_tdata <= rd_q;
        endcase
        m_tkeep <= last_beat ? dq_keep[cur] : 8'hFF;
        m_tlast <= last_beat;
        m_tuser <= last_beat ? dq_user[cur] : 1'b0;
    end

    if (!active || last_beat) begin
        if (active) dq_rd <= cur + 1;
        if (have_next) begin
            active <= 1'b1;
            cur <= next_d;
            idx <= 0;
            rd_addr <= dq_start[next_d] + 9'd1;   // rd_q holds beat k while beat k is sent
        end else begin
            active <= 1'b0;
        end
    end else begin
        idx <= idx + 1;
        rd_addr <= dq_start[cur] + idx + 9'd2;
    end
    if (rst) begin
        active <= 1'b0;
        dq_rd <= 0;
        m_tvalid <= 1'b0;
    end
end

endmodule

`default_nettype wire
