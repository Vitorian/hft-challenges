// fpga02_rx skeleton: a simple, correct, SLOW 10GBASE-R receive path (store and forward).
//
// raw bits -> gearbox (W -> 66) -> clause 49 block lock -> descrambler -> 64b/66b decoder ->
// MAC RX (preamble/SFD, FCS check, errors) -> frame buffer -> AXI-stream out
//
// Every frame is stored completely and its FCS checked before its first byte is sent, so the
// latency is the whole frame time. Making that fast is the challenge.
//
// rx_data[0] is the earliest bit on the line. Line bit order: sync header bit 0, bit 1, then
// payload bit 0..63 (IEEE 802.3 clause 49). W = 32 (322.265625 MHz) or 64 (161.1328125 MHz).

`timescale 1ns / 1ps
`default_nettype none

module fpga02_rx #(
    parameter W = 64
) (
    input  wire           clk,
    input  wire           rst,
    input  wire [W-1:0]   rx_data,
    output wire           block_lock,
    output reg  [W-1:0]   m_tdata,
    output reg  [W/8-1:0] m_tkeep,
    output reg            m_tvalid,
    output reg            m_tlast,
    output reg            m_tuser
);

// ---------------------------------------------------------------------------------------------
// Gearbox: collect W bits per clock, emit one 66-bit block when 66 are available
// ---------------------------------------------------------------------------------------------
localparam BUFW = 66 + W;

reg [BUFW-1:0] gb_buf;
reg [7:0]      gb_cnt;
reg            slip;            // request: drop one bit (from the lock state machine)
reg            blk_valid;
reg [65:0]     blk;
reg            blk_fresh;       // block was taken after the most recent slip was applied
reg            slip_pending;

reg [BUFW-1:0] nb;
reg [7:0]      nc;

always @(posedge clk) begin
    nb = gb_buf | ({{(BUFW-W){1'b0}}, rx_data} << gb_cnt);
    nc = gb_cnt + W;
    if (slip) begin
        nb = nb >> 1;
        nc = nc - 1;
    end
    blk_valid <= 1'b0;
    if (slip) slip_pending <= 1'b1;
    if (nc >= 66) begin
        blk <= nb[65:0];
        blk_valid <= 1'b1;
        blk_fresh <= slip || slip_pending;
        slip_pending <= 1'b0;
        nb = nb >> 66;
        nc = nc - 66;
    end
    gb_buf <= nb;
    gb_cnt <= nc;
    if (rst) begin
        gb_buf <= 0;
        gb_cnt <= 0;
        blk_valid <= 1'b0;
        slip_pending <= 1'b0;
        blk_fresh <= 1'b0;
    end
end

// ---------------------------------------------------------------------------------------------
// Block lock (clause 49): 64 valid headers in a row to lock; while locked, 16 invalid headers
// within a window of 64 drop the lock. Unlocked + invalid header -> slip one bit.
// ---------------------------------------------------------------------------------------------
wire [1:0] sh = blk[1:0];
wire       sh_valid = (sh == 2'b10) || (sh == 2'b01);

reg       locked;
reg       slip_wait;            // a slip is in flight: ignore blocks taken before it applied
reg [6:0] good_cnt;
reg [6:0] win_cnt;
reg [4:0] bad_cnt;

assign block_lock = locked;

always @(posedge clk) begin
    slip <= 1'b0;
    if (blk_valid && slip_wait && !blk_fresh) begin
        // stale block from the old alignment: skip it
    end else if (blk_valid) begin
        slip_wait <= 1'b0;
        if (!locked) begin
            if (sh_valid) begin
                good_cnt <= good_cnt + 1;
                if (good_cnt == 63) begin
                    locked <= 1'b1;
                    win_cnt <= 0;
                    bad_cnt <= 0;
                end
            end else begin
                good_cnt <= 0;
                slip <= 1'b1;
                slip_wait <= 1'b1;
            end
        end else begin
            if (win_cnt == 63) begin
                win_cnt <= 0;
                bad_cnt <= 0;
            end else begin
                win_cnt <= win_cnt + 1;
            end
            if (!sh_valid) begin
                if (bad_cnt == 15) begin
                    locked <= 1'b0;
                    good_cnt <= 0;
                    slip <= 1'b1;
                    slip_wait <= 1'b1;
                end else if (win_cnt != 63) begin
                    bad_cnt <= bad_cnt + 1;
                end else begin
                    bad_cnt <= 1;
                end
            end
        end
    end
    if (rst) begin
        locked <= 1'b0;
        good_cnt <= 0;
        win_cnt <= 0;
        bad_cnt <= 0;
        slip <= 1'b0;
        slip_wait <= 1'b0;
    end
end

// ---------------------------------------------------------------------------------------------
// Descrambler x^58 + x^39 + 1 (self-synchronizing), one block per valid
// ---------------------------------------------------------------------------------------------
reg [57:0]  scr_state;          // last 58 received payload bits, scr_state[57] = most recent
reg [63:0]  dsc;
reg [1:0]   dsc_hdr;
reg         dsc_valid;
reg         dsc_locked;

wire [121:0] ext = {blk[65:2], scr_state};   // ext[j]: j < 58 history, j >= 58 current payload

integer i;
reg [63:0] d_tmp;
always @(posedge clk) begin
    dsc_valid <= blk_valid;
    if (blk_valid) begin
        for (i = 0; i < 64; i = i + 1)
            d_tmp[i] = ext[58 + i] ^ ext[58 + i - 39] ^ ext[58 + i - 58];
        dsc <= d_tmp;
        dsc_hdr <= sh;
        dsc_locked <= locked;
        scr_state <= blk[65:8];
    end
    if (rst) dsc_valid <= 1'b0;
end

// ---------------------------------------------------------------------------------------------
// Decoder: block -> 8 XGMII lanes (byte + control flag). Anything invalid -> all /E/.
// ---------------------------------------------------------------------------------------------
localparam [7:0] X_IDLE = 8'h07, X_START = 8'hfb, X_TERM = 8'hfd, X_ERR = 8'hfe, X_SEQ = 8'h9c, X_LPI = 8'h06;

function [7:0] ctrl_char(input [6:0] code);
    case (code)
        7'h00: ctrl_char = X_IDLE;
        7'h06: ctrl_char = X_LPI;
        default: ctrl_char = X_ERR;       // 7'h1e is /E/; reserved codes are treated as errors
    endcase
endfunction

function code_ok(input [6:0] code);
    code_ok = (code == 7'h00) || (code == 7'h06) || (code == 7'h1e);
endfunction

// all control codes a block type carries must be valid, else the whole block is /E/
function block_codes_ok(input [63:0] b);
    integer n;
    reg ok;
    begin
        ok = 1'b1;
        case (b[7:0])
            8'h1e: for (n = 0; n < 8; n = n + 1) ok = ok && code_ok(b[8 + 7*n +: 7]);
            8'h33, 8'h2d: for (n = 0; n < 4; n = n + 1) ok = ok && code_ok(b[8 + 7*n +: 7]);
            8'h4b: for (n = 0; n < 4; n = n + 1) ok = ok && code_ok(b[36 + 7*n +: 7]);
            8'h87: for (n = 0; n < 7; n = n + 1) ok = ok && code_ok(b[15 + 7*n +: 7]);
            8'h99: for (n = 0; n < 6; n = n + 1) ok = ok && code_ok(b[22 + 7*n +: 7]);
            8'haa: for (n = 0; n < 5; n = n + 1) ok = ok && code_ok(b[29 + 7*n +: 7]);
            8'hb4: for (n = 0; n < 4; n = n + 1) ok = ok && code_ok(b[36 + 7*n +: 7]);
            8'hcc: for (n = 0; n < 3; n = n + 1) ok = ok && code_ok(b[43 + 7*n +: 7]);
            8'hd2: for (n = 0; n < 2; n = n + 1) ok = ok && code_ok(b[50 + 7*n +: 7]);
            8'he1: ok = code_ok(b[57 +: 7]);
            default: ok = 1'b1;
        endcase
        block_codes_ok = ok;
    end
endfunction

reg [63:0] xd;
reg [7:0]  xc;
reg        x_valid;
reg        x_locked;
integer    k;

always @(posedge clk) begin
    x_valid <= dsc_valid;
    x_locked <= dsc_locked;
    if (dsc_valid) begin
        xd <= {8{X_ERR}};
        xc <= 8'hff;
        if (dsc_hdr == 2'b10) begin
            xd <= dsc;
            xc <= 8'h00;
        end else if (dsc_hdr == 2'b01 && block_codes_ok(dsc)) begin
            case (dsc[7:0])
                8'h1e: begin
                    for (k = 0; k < 8; k = k + 1) xd[8*k +: 8] <= ctrl_char(dsc[8 + 7*k +: 7]);
                    xc <= 8'hff;
                end
                8'h78: begin xd <= {dsc[63:8], X_START}; xc <= 8'h01; end
                8'h33: begin
                    for (k = 0; k < 4; k = k + 1) xd[8*k +: 8] <= ctrl_char(dsc[8 + 7*k +: 7]);
                    xd[63:32] <= {dsc[63:40], X_START};
                    xc <= 8'h1f;
                end
                8'h4b: begin
                    xd[31:0] <= {dsc[31:8], X_SEQ};
                    for (k = 0; k < 4; k = k + 1) xd[32 + 8*k +: 8] <= ctrl_char(dsc[36 + 7*k +: 7]);
                    xc <= 8'hf1;
                end
                8'h66: begin xd <= {dsc[63:40], X_START, dsc[31:8], X_SEQ}; xc <= 8'h11; end
                8'h55: begin xd <= {dsc[63:40], X_SEQ, dsc[31:8], X_SEQ}; xc <= 8'h11; end
                8'h2d: begin
                    for (k = 0; k < 4; k = k + 1) xd[8*k +: 8] <= ctrl_char(dsc[8 + 7*k +: 7]);
                    xd[63:32] <= {dsc[63:40], X_SEQ};
                    xc <= 8'h1f;
                end
                8'h87: begin xd <= {{7{X_IDLE}}, X_TERM}; xc <= 8'hff; end
                8'h99: begin xd <= {{6{X_IDLE}}, X_TERM, dsc[15:8]}; xc <= 8'hfe; end
                8'haa: begin xd <= {{5{X_IDLE}}, X_TERM, dsc[23:8]}; xc <= 8'hfc; end
                8'hb4: begin xd <= {{4{X_IDLE}}, X_TERM, dsc[31:8]}; xc <= 8'hf8; end
                8'hcc: begin xd <= {{3{X_IDLE}}, X_TERM, dsc[39:8]}; xc <= 8'hf0; end
                8'hd2: begin xd <= {{2{X_IDLE}}, X_TERM, dsc[47:8]}; xc <= 8'he0; end
                8'he1: begin xd <= {X_IDLE, X_TERM, dsc[55:8]}; xc <= 8'hc0; end
                8'hff: begin xd <= {X_TERM, dsc[63:8]}; xc <= 8'h80; end
                default: ;
            endcase
        end
    end
    if (rst) x_valid <= 1'b0;
end

// ---------------------------------------------------------------------------------------------
// Classification stage: everything the MAC needs to know about a block, registered
// ---------------------------------------------------------------------------------------------
reg         c_valid, c_locked;
reg  [63:0] c_xd;
reg  [3:0]  c_tpos;              // lane of /T/, 8 = none
reg  [3:0]  c_nb;                // data bytes before /T/ (8 if none)
reg         c_ctrl;              // a control character before /T/ (error inside a frame)
reg         c_start0, c_start4;  // /S/ in lane 0 / lane 4
reg         c_pre0_ok;           // lane-0 start block: lanes 1..7 = preamble + SFD
reg         c_pre4_ok;           // lane-4 start block: lanes 5..7 = 55 55 55
reg         c_pre4b_ok;          // block after a lane-4 start: lanes 0..3 = 55 55 55 D5, lanes 4..7 data

reg  [3:0]  tp;
reg         ci;
integer     l;

always @(posedge clk) begin
    c_valid <= x_valid;
    if (x_valid) begin
        tp = 4'd8;
        for (l = 7; l >= 0; l = l - 1)
            if (xc[l] && xd[8*l +: 8] == X_TERM) tp = l;
        ci = 1'b0;
        for (l = 0; l < 8; l = l + 1)
            if (xc[l] && (l < tp)) ci = 1'b1;
        c_xd <= xd;
        c_locked <= x_locked;
        c_tpos <= tp;
        c_nb <= (tp == 8) ? 4'd8 : tp;
        c_ctrl <= ci;
        c_start0 <= xc[0] && xd[7:0] == X_START;
        c_start4 <= xc[4] && xd[39:32] == X_START;
        c_pre0_ok <= xc[7:1] == 0 && xd[63:8] == 56'hD5555555555555;
        c_pre4_ok <= xc[7:5] == 0 && xd[63:40] == 24'h555555;
        c_pre4b_ok <= xc == 0 && xd[31:0] == 32'hD5555555;
    end
    if (rst) c_valid <= 1'b0;
end

// ---------------------------------------------------------------------------------------------
// MAC RX: frame bytes are realigned so stored word n holds frame bytes 8n..8n+7. CRC over the
// frame bytes plus FCS must leave the standard residue. Every CRC update has a fixed byte count
// (the terminate block's 0..7-byte variants are computed in parallel and selected), so the XOR
// trees stay shallow; the FCS verdict is registered one cycle after the frame ends.
// ---------------------------------------------------------------------------------------------
function [31:0] crc32_bytes(input [31:0] crc_in, input [63:0] data, input integer nbytes);
    integer b, j;
    reg [31:0] c;
    begin
        c = crc_in;
        for (b = 0; b < nbytes; b = b + 1)
            for (j = 0; j < 8; j = j + 1)
                c = (c >> 1) ^ ((c[0] ^ data[8*b + j]) ? 32'hEDB88320 : 32'h0);
        crc32_bytes = c;
    end
endfunction

localparam CRC_RESIDUE = 32'hDEBB20E3;
localparam MAXLEN = 2000;

localparam [1:0] M_IDLE = 2'd0, M_PRE = 2'd1, M_DATA = 2'd2;
reg  [1:0]  m_state;
reg         m_off4;              // frame byte 0 is in lane 4
reg  [31:0] hold;                // lanes 4..7 of the previous block (lane-4 frames)
reg  [31:0] crc;
reg  [11:0] flen;                // bytes so far (frame + FCS)
reg         fbad;

// CRC of this block for every possible byte count, selected by c_nb
wire [31:0] crc_n [0:8];
genvar gn;
generate
    for (gn = 0; gn <= 8; gn = gn + 1) begin : g_crc
        assign crc_n[gn] = crc32_bytes(crc, c_xd, gn);
    end
endgenerate
wire [31:0] crc_sel = crc_n[c_nb];

// frame buffer: 512 x 64 bit block RAM, one registered write per cycle, registered read
(* ram_style = "block" *) reg [63:0] fb [0:511];
reg         fbw_en;
reg  [8:0]  fbw_addr;
reg  [63:0] fbw_data;
reg         pend_en;             // second word of a lane-4 frame ending in lanes 5..7
reg  [8:0]  pend_addr;
reg  [63:0] pend_data;
reg  [8:0]  wr_ptr;              // next word to write
reg  [8:0]  fstart;              // first word of the current frame

always @(posedge clk) begin
    if (fbw_en) fb[fbw_addr] <= fbw_data;
end

// end of frame, verdict one cycle later
reg         e_valid;
reg  [31:0] e_crc;
reg         e_bad;
reg  [8:0]  e_start;
reg  [11:0] e_len;               // bytes without FCS

// descriptor FIFO
reg  [8:0]  dq_start [0:15];
reg  [11:0] dq_len   [0:15];
reg         dq_bad   [0:15];
reg  [3:0]  dq_wr, dq_rd;

reg  [11:0] tot;

always @(posedge clk) begin
    fbw_en <= 1'b0;
    e_valid <= 1'b0;
    if (pend_en) begin
        fbw_en <= 1'b1;
        fbw_addr <= pend_addr;
        fbw_data <= pend_data;
        pend_en <= 1'b0;
    end

    if (c_valid) begin
        case (m_state)
            M_IDLE: begin
                if (c_locked && c_start0) begin
                    m_off4 <= 1'b0;
                    fbad <= !c_pre0_ok;
                    m_state <= M_DATA;
                    crc <= 32'hFFFFFFFF;
                    flen <= 0;
                    fstart <= wr_ptr;
                end else if (c_locked && c_start4) begin
                    m_off4 <= 1'b1;
                    fbad <= !c_pre4_ok;
                    m_state <= M_PRE;
                    crc <= 32'hFFFFFFFF;
                    flen <= 0;
                    fstart <= wr_ptr;
                end
            end
            M_PRE: begin
                // lane-4 frames: lanes 0..3 = 55 55 55 D5, frame bytes 0..3 in lanes 4..7
                if (!c_pre4b_ok) fbad <= 1'b1;
                hold <= c_xd[63:32];
                flen <= 4;
                crc <= crc32_bytes(32'hFFFFFFFF, {32'd0, c_xd[63:32]}, 4);
                m_state <= c_locked ? M_DATA : M_IDLE;
            end
            M_DATA: begin
                fbw_en <= 1'b1;
                fbw_addr <= wr_ptr;
                fbw_data <= m_off4 ? {c_xd[31:0], hold} : c_xd;
                wr_ptr <= wr_ptr + 1;
                hold <= c_xd[63:32];
                crc <= crc_sel;
                tot = flen + c_nb;
                flen <= tot;
                if (c_tpos != 8 || c_ctrl || !c_locked || flen > MAXLEN) begin
                    if (m_off4 && c_tpos > 4 && c_tpos != 8) begin
                        pend_en <= 1'b1;
                        pend_addr <= wr_ptr + 1;
                        pend_data <= {32'd0, c_xd[63:32]};
                        wr_ptr <= wr_ptr + 2;
                    end
                    e_valid <= tot >= 5;
                    e_crc <= crc_sel;
                    e_bad <= fbad || c_ctrl || !c_locked || (c_tpos == 8);
                    e_start <= fstart;
                    e_len <= tot - 4;
                    m_state <= M_IDLE;
                end
            end
            default: m_state <= M_IDLE;
        endcase
    end

    if (e_valid) begin
        dq_start[dq_wr] <= e_start;
        dq_len[dq_wr] <= e_len;
        dq_bad[dq_wr] <= e_bad || (e_crc != CRC_RESIDUE);
        dq_wr <= dq_wr + 1;
    end

    if (rst) begin
        m_state <= M_IDLE;
        wr_ptr <= 0;
        dq_wr <= 0;
        fbw_en <= 1'b0;
        pend_en <= 1'b0;
        e_valid <= 1'b0;
    end
end

// ---------------------------------------------------------------------------------------------
// Output: stream complete frames from the buffer, W bits per beat, FCS stripped
// ---------------------------------------------------------------------------------------------
localparam BPB = W / 8;          // bytes per beat

localparam [1:0] O_IDLE = 2'd0, O_PRIME = 2'd1, O_RUN = 2'd2;
reg  [1:0]  o_state;
reg  [8:0]  o_raddr;             // word being fetched (one cycle read latency)
reg  [63:0] o_rd_q;              // fb[o_raddr] of the previous cycle
reg  [63:0] o_cur;               // W = 32: the word whose high half goes out next
reg  [11:0] o_left;              // bytes still to send
reg         o_half;              // W = 32: 0 = low half next, 1 = high half next
reg         o_bad;

always @(posedge clk) begin
    o_rd_q <= fb[o_raddr];
end

always @(posedge clk) begin
    m_tvalid <= 1'b0;
    m_tlast <= 1'b0;
    m_tuser <= 1'b0;
    case (o_state)
        O_IDLE: begin
            if (dq_rd != dq_wr) begin
                o_raddr <= dq_start[dq_rd];
                o_left <= dq_len[dq_rd];
                o_bad <= dq_bad[dq_rd];
                o_half <= 1'b0;
                dq_rd <= dq_rd + 1;
                o_state <= O_PRIME;
            end
        end
        O_PRIME: begin
            o_raddr <= o_raddr + 1;      // fb[start] arrives in o_rd_q at the end of this cycle
            o_state <= O_RUN;
        end
        O_RUN: begin
            m_tvalid <= 1'b1;
            if (W == 64) begin
                m_tdata <= o_rd_q[W-1:0];
                o_raddr <= o_raddr + 1;
            end else begin
                if (!o_half) begin
                    m_tdata <= o_rd_q[31:0];
                    o_cur <= o_rd_q;
                end else begin
                    m_tdata <= o_cur[63:32];
                    o_raddr <= o_raddr + 1;
                end
                o_half <= !o_half;
            end
            if (o_left <= BPB) begin
                m_tkeep <= {BPB{1'b1}} >> (BPB - o_left);
                m_tlast <= 1'b1;
                m_tuser <= o_bad;
                o_state <= O_IDLE;
            end else begin
                m_tkeep <= {BPB{1'b1}};
                o_left <= o_left - BPB;
            end
        end
        default: o_state <= O_IDLE;
    endcase
    if (rst) begin
        o_state <= O_IDLE;
        dq_rd <= 0;
        m_tvalid <= 1'b0;
    end
end

endmodule

`default_nettype wire
