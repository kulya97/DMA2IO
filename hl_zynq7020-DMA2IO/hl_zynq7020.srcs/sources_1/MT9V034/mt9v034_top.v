`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2023/06/19 21:17:43
// Design Name: 
// Module Name: mt9v034_top
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


module mt9v034_top (
    input clk_50m,
    input clk_24m,
    input sys_rst_n,

    //cmos interface
    output       cmos_scl,         //cmos i2c clock
    inout        cmos_sda,         //cmos i2c data
    input        cmos_vsync,       //cmos vsync
    input        cmos_href,        //cmos hsync refrence
    input        cmos_pclk,        //cmos pxiel clock
    input  [7:0] cmos_data,        //cmos data	
    output       cmos_reset,       //cmos reset	
    output       cmos_pwdn,        //cmos pwer down  
    output       cmos_xclk,
    output       cmos_config_done,

    output       cmos_frame_vsync,
    output       cmos_frame_href,
    output [7:0] cmos_frame_data,
    output       cmos_frame_valid
);
  //parameter define                     
  parameter SLAVE_ADD = 7'b1001_000;  //slave  address         90  
  parameter BIT_CTRL = 1'b0;  //OV7725的字节地址为8位  0:8位 1:16位
  parameter DATA_CTRL = 1'b1;  //OV7725的数据为8位  0:8位 1:16位
  parameter CLK_FREQ = 26'd50_000_000;  //i2c_dri模块的驱动时钟频率 50.0MHz
  parameter I2C_FREQ = 18'd250_000;  //I2C的SCL时钟频率,不超过400KHz

  wire        i2c_dri;
  wire        i2c_done;
  wire        i2c_exec;
  wire        i2c_addr;
  wire [15:0] i2c_wr_data;
  wire        i2c_config_done;
  assign cmos_config_done = i2c_config_done;

  assign cmos_reset       = 1'b1;  //cmos work state (50us delay)
  assign cmos_pwdn        = 1'b0;  //cmos power on

  i2c_cfg u_i2c_cfg (
      .clk        (i2c_dri),
      .rst_n      (sys_rst_n),
      .i2c_done   (i2c_done),
      .i2c_exec   (i2c_exec),
      .i2c_addr   (i2c_addr),
      .i2c_wr_data(i2c_wr_data),
      .cfg_done   (i2c_config_done)
  );




  //I2C驱动模块
  i2c_dri #(
      .SLAVE_ADDR(SLAVE_ADD),  //参数传递
      .CLK_FREQ  (CLK_FREQ),
      .I2C_FREQ  (I2C_FREQ)
  ) u_i2c_dr (
      .clk  (clk_50m),
      .rst_n(sys_rst_n),

      .i2c_exec  (i2c_exec),
      .bit_ctrl  (BIT_CTRL),
      .data_ctrl (DATA_CTRL),
      .i2c_rh_wl (0),                 //固定为0，只用到了IIC驱动的写操作   
      .i2c_addr  ({8'b0, i2c_addr}),
      .i2c_data_w(i2c_wr_data),
      .i2c_data_r(),
      .i2c_done  (i2c_done),
      .scl       (cmos_scl),
      .sda       (cmos_sda),
      .dri_clk   (i2c_dri)            //I2C操作时钟
  );

  cmos_capture_raw_gray cmos_capture_raw_gray (
      //global clock
      .clk_cmos(clk_24m),         //24MHz CMOS Driver clock input
      .rst_n   (i2c_config_done), //global reset

      //CMOS Sensor Interface
      .cmos_pclk (cmos_pclk),   //24MHz CMOS Pixel clock input
      .cmos_xclk (cmos_xclk),   //24MHz drive clock
      .cmos_data (cmos_data),   //8 bits cmos data input
      .cmos_vsync(cmos_vsync),  //L: vaild, H: invalid
      .cmos_href (cmos_href),   //H: vaild, L: invalid

      //CMOS SYNC Data output
      .cmos_frame_vsync(cmos_frame_vsync),  //cmos frame data vsync valid signal
      .cmos_frame_href (cmos_frame_href),   //cmos frame data href vaild  signal
      .cmos_frame_data (cmos_frame_data),   //cmos frame gray output 
      .cmos_frame_clken(cmos_frame_valid),  //cmos frame data output/capture enable clock

      //user interface
      .cmos_fps_rate()
  );
endmodule
