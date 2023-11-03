`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2023/06/01 16:31:07
// Design Name: 
// Module Name: top_module
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module top_module (
    input sys_clk,
    input sys_rstn,

    // DDR3
    output [14:0] ddr3_addr,
    output [ 2:0] ddr3_ba,
    output        ddr3_cas_n,
    output        ddr3_ck_n,
    output        ddr3_ck_p,
    output        ddr3_cke,
    output        ddr3_cs_n,
    output [ 3:0] ddr3_dm,
    inout  [31:0] ddr3_dq,
    inout  [ 3:0] ddr3_dqs_n,
    inout  [ 3:0] ddr3_dqs_p,
    output        ddr3_odt,
    output        ddr3_ras_n,
    output        ddr3_reset_n,
    output        ddr3_we_n,
    input         UART_RX,
    output        UART_TX,
    output [31:0] GPIO
);


  wire PL_CLK_100M;

  wire PS_RSTN;
  wire RST_N;
  assign RST_N = PS_RSTN & sys_rstn;



  wire [31:0] M_AXIS_tdata;
  wire [ 3:0] M_AXIS_tkeep;
  wire        M_AXIS_tlast;
  wire        M_AXIS_tready;
  wire        M_AXIS_tvalid;
  assign M_AXIS_tready = 1'b1;

  design_1_wrapper u_design_1_wrapper (

      .M_AXIS_tdata (GPIO),
      .M_AXIS_tkeep (M_AXIS_tkeep),
      .M_AXIS_tlast (M_AXIS_tlast),
      .M_AXIS_tready(M_AXIS_tready),
      .M_AXIS_tvalid(M_AXIS_tvalid),

      .PL_CLK_100M(PL_CLK_100M),
      .PS_RSTN    (PS_RSTN),

      .UART_rxd         (UART_RX),
      .UART_txd         (UART_TX),
      .FIXED_IO_ddr_vrn (),
      .FIXED_IO_ddr_vrp (),
      .FIXED_IO_mio     (),
      .FIXED_IO_ps_clk  (),
      .FIXED_IO_ps_porb (),
      .FIXED_IO_ps_srstb(),
      .ddr3_addr        (ddr3_addr[14:0]),
      .ddr3_ba          (ddr3_ba[2:0]),
      .ddr3_cas_n       (ddr3_cas_n),
      .ddr3_ck_n        (ddr3_ck_n),
      .ddr3_ck_p        (ddr3_ck_p),
      .ddr3_cke         (ddr3_cke),
      .ddr3_cs_n        (ddr3_cs_n),
      .ddr3_dm          (ddr3_dm[3:0]),
      .ddr3_dq          (ddr3_dq[31:0]),
      .ddr3_dqs_n       (ddr3_dqs_n[3:0]),
      .ddr3_dqs_p       (ddr3_dqs_p[3:0]),
      .ddr3_odt         (ddr3_odt),
      .ddr3_ras_n       (ddr3_ras_n),
      .ddr3_reset_n     (ddr3_reset_n),
      .ddr3_we_n        (ddr3_we_n)
  );
endmodule
