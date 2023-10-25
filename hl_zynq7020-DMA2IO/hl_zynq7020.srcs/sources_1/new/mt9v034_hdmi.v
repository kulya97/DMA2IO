//****************************************Copyright (c)***********************************//
//原子哥在线教学平台：www.yuanzige.com
//技术支持：www.openedv.com
//淘宝店铺：http://openedv.taobao.com
//关注微信公众平台微信号："正点原子"，免费获取ZYNQ & FPGA & STM32 & LINUX资料。
//版权所有，盗版必究。
//Copyright(C) 正点原子 2018-2028
//All rights reserved
//----------------------------------------------------------------------------------------
// File name:           mt9v034_hdmi
// Last modified Date:  2020/05/04 9:19:08
// Last Version:        V1.0
// Descriptions:        mt9v034摄像头HDMI显示实验
//                      
//----------------------------------------------------------------------------------------
// Created by:          正点原子
// Created date:        2019/05/04 9:19:08
// Version:             V1.0
// Descriptions:        The original version
//
//----------------------------------------------------------------------------------------
//****************************************************************************************//

module mt9v034_hdmi(    
    input                 sys_clk      ,  //系统时钟
    input                 sys_rst_n    ,  //系统复位，低电平有效
    //摄像头接口                       
	//cmos interface
	output	     	      cmos_scl     ,  //cmos i2c clock
	inout			      cmos_sda     ,  //cmos i2c data
	input			      cmos_vsync   ,  //cmos vsync
	input			      cmos_href    ,  //cmos hsync refrence
	input			      cmos_pclk    ,  //cmos pxiel clock
	input	[7:0]	      cmos_data    ,  //cmos data	
	output		          cmos_reset   ,  //cmos reset	
	output		          cmos_pwdn    ,  //cmos pwer down  
    // DDR3                            
    inout   [15:0]        ddr3_dq      ,  //DDR3 数据
    inout   [1:0]         ddr3_dqs_n   ,  //DDR3 dqs负
    inout   [1:0]         ddr3_dqs_p   ,  //DDR3 dqs正  
    output  [13:0]        ddr3_addr    ,  //DDR3 地址   
    output  [2:0]         ddr3_ba      ,  //DDR3 banck 选择
    output                ddr3_ras_n   ,  //DDR3 行选择
    output                ddr3_cas_n   ,  //DDR3 列选择
    output                ddr3_we_n    ,  //DDR3 读写选择
    output                ddr3_reset_n ,  //DDR3 复位
    output  [0:0]         ddr3_ck_p    ,  //DDR3 时钟正
    output  [0:0]         ddr3_ck_n    ,  //DDR3 时钟负
    output  [0:0]         ddr3_cke     ,  //DDR3 时钟使能
    output  [0:0]         ddr3_cs_n    ,  //DDR3 片选
    output  [1:0]         ddr3_dm      ,  //DDR3_dm
    output  [0:0]         ddr3_odt     ,  //DDR3_odt									   
    //hdmi接口                           
    input                 hpdin,    
    output                tmds_clk_p   ,  // TMDS 时钟通道
    output                tmds_clk_n   ,
    output  [2:0]         tmds_data_p  ,  // TMDS 数据通道
    output  [2:0]         tmds_data_n  ,
    output                tmds_oen     ,  // TMDS 输出使能
    output                hpdout
    );                                 
									   
//parameter define                     
parameter  SLAVE_ADD = 7'b1001_000     ;  //slave  address         90  
parameter  BIT_CTRL   = 1'b0           ;  //OV7725的字节地址为8位  0:8位 1:16位
parameter  DATA_CTRL  = 1'b1           ;  //OV7725的数据为8位  0:8位 1:16位
parameter  CLK_FREQ   = 26'd50_000_000 ;  //i2c_dri模块的驱动时钟频率 50.0MHz
parameter  I2C_FREQ   = 18'd250_000    ;  //I2C的SCL时钟频率,不超过400KHz

parameter  V_CMOS_DISP = 11'd480;         //CMOS分辨率--行
parameter  H_CMOS_DISP = 11'd640;         //CMOS分辨率--列				
									   									   
//wire define                          
wire         clk_100m                  ;  //100mhz时钟
wire         clk_100m_shift            ;  //100mhz时钟,相位偏移时钟
wire         clk_50m                   ;  //50mhz时钟,提供给lcd驱动时钟
wire         locked                    ;  //时钟锁定信号
wire         rst_n                     ;  //全局复位 								    
wire         i2c_exec                  ;  //I2C触发执行信号
wire  [15:0] i2c_data                  ;  //I2C要配置的地址与数据(高8位地址,低8位数据)          
wire         cam_init_done             ;  //摄像头初始化完成
wire         i2c_done                  ;  //I2C寄存器配置完成信号
wire         i2c_dri_clk               ;  //I2C操作时钟								    
wire         wr_en                     ;  //DDR3控制器模块写使能
wire  [15:0] wr_data                   ;  //DDR3控制器模块写数据
wire         rdata_req                 ;  //DDR3控制器模块读使能
wire  [15:0] rd_data                   ;  //DDR3控制器模块读数据
wire         cmos_frame_valid          ;  //数据有效使能信号
wire         init_calib_complete       ;  //DDR3初始化完成init_calib_complete
wire         sys_init_done             ;  //系统初始化完成(DDR3初始化+摄像头初始化)
wire         clk_200m                  ;  //ddr3参考时钟
wire         cmos_frame_vsync          ;  //输出帧有效场同步信号
wire         lcd_de                    ;  //LCD 数据输入使能
wire         cmos_frame_href           ;  //输出帧有效行同步信号 
wire  [27:0] app_addr_rd_min           ;  //读DDR3的起始地址
wire  [27:0] app_addr_rd_max           ;  //读DDR3的结束地址
wire  [7:0]  rd_bust_len               ;  //从DDR3中读数据时的突发长度
wire  [27:0] app_addr_wr_min           ;  //写DDR3的起始地址
wire  [27:0] app_addr_wr_max           ;  //写DDR3的结束地址
wire  [7:0]  wr_bust_len               ;  //从DDR3中读数据时的突发长度
wire  [9:0]  pixel_xpos_w              ;  //像素点横坐标
wire  [9:0]  pixel_ypos_w              ;  //像素点纵坐标   
wire         lcd_clk                   ;  //分频产生的LCD 采样时钟
wire  [10:0] h_disp                    ;  //LCD屏水平分辨率
wire  [10:0] v_disp                    ;  //LCD屏垂直分辨率     
wire  [10:0] h_pixel                   ;  //存入ddr3的水平分辨率        
wire  [10:0] v_pixel                   ;  //存入ddr3的屏垂直分辨率 
wire  [15:0] lcd_id                    ;  //LCD屏的ID号
wire  [27:0] ddr3_addr_max             ;  //存入DDR3的最大读写地址 
wire  [7:0]  cmos_frame_data           ;  //cmos frame data output
wire  [7:0]	 i2c_addr                  ;  
wire  [15:0] i2c_wr_data               ;  
wire  [7:0]  cam_frame_data;

//*****************************************************
//**                    main code
//*****************************************************

//待时钟锁定后产生复位结束信号
assign  rst_n = sys_rst_n & locked;

//系统初始化完成：DDR3初始化完成
assign  sys_init_done = init_calib_complete  & i2c_config_done;

assign	cmos_reset  = 1'b1;		//cmos work state (50us delay)
assign	cmos_pwdn   = 1'b0;		//cmos power on

assign  wr_data = {cam_frame_data[7:3],{cam_frame_data[7:2]},cam_frame_data[7:3]}; //  R = G = B
                   					                    
i2c_cfg  u_i2c_cfg(
                   .clk          (i2c_dri),
                   .rst_n        (sys_rst_n),
                   .i2c_done     (i2c_done),
                   .i2c_exec     (i2c_exec),
                   .i2c_addr     (i2c_addr),
                   .i2c_wr_data  (i2c_wr_data),                
                   .cfg_done     (i2c_config_done)
                   );

//I2C驱动模块
i2c_dri #(
    .SLAVE_ADDR         (SLAVE_ADD),    //参数传递
    .CLK_FREQ           (CLK_FREQ  ),              
    .I2C_FREQ           (I2C_FREQ  ) 
    )
u_i2c_dr(
    .clk                (clk_50m),
    .rst_n              (sys_rst_n     ),

    .i2c_exec           (i2c_exec  ),   
    .bit_ctrl           (BIT_CTRL  ),   
    .data_ctrl          (DATA_CTRL),
    .i2c_rh_wl          (0),            //固定为0，只用到了IIC驱动的写操作   
    .i2c_addr           ({8'b0,i2c_addr}),   
    .i2c_data_w         (i2c_wr_data),   
    .i2c_data_r         (),   
    .i2c_done           (i2c_done  ),    
    .scl                (cmos_scl   ),   
    .sda                (cmos_sda   ),   
    .dri_clk            (i2c_dri)       //I2C操作时钟
    );
    
 cmos_capture_raw_gray
(
	//global clock
	.clk_cmos				(clk_24m),			//24MHz CMOS Driver clock input
	.rst_n					(sys_init_done),	//global reset

	//CMOS Sensor Interface
	.cmos_pclk				(cmos_pclk),  		//24MHz CMOS Pixel clock input
	.cmos_xclk				(),		            //24MHz drive clock
	.cmos_data				(cmos_data),		//8 bits cmos data input
	.cmos_vsync				(cmos_vsync),		//L: vaild, H: invalid
	.cmos_href				(cmos_href),		//H: vaild, L: invalid
	
	//CMOS SYNC Data output
	.cmos_frame_vsync		(cmos_frame_vsync),	//cmos frame data vsync valid signal
	.cmos_frame_href		(cmos_frame_href),	//cmos frame data href vaild  signal
	.cmos_frame_data		(cam_frame_data),	//cmos frame gray output 
	.cmos_frame_clken		(cmos_frame_valid),	//cmos frame data output/capture enable clock
	
	//user interface
	.cmos_fps_rate			()		
);      
    
ddr3_top u_ddr3_top (
    .clk_200m              (clk_200m),                //系统时钟
    .sys_rst_n             (rst_n),                   //复位,低有效
    .sys_init_done         (sys_init_done),           //系统初始化完成
    .init_calib_complete   (init_calib_complete),     //ddr3初始化完成信号    
    //ddr3接口信号         
    .app_addr_rd_min       (28'd0),                   //读DDR3的起始地址
    .app_addr_rd_max       (V_CMOS_DISP*H_CMOS_DISP), //读DDR3的结束地址
    .rd_bust_len           (H_CMOS_DISP[10:3]),       //从DDR3中读数据时的突发长度
    .app_addr_wr_min       (28'd0),                   //写DDR3的起始地址
    .app_addr_wr_max       (V_CMOS_DISP*H_CMOS_DISP), //写DDR3的结束地址
    .wr_bust_len           (H_CMOS_DISP[10:3]),       //从DDR3中读数据时的突发长度
    // DDR3 IO接口                
    .ddr3_dq               (ddr3_dq),                 //DDR3 数据
    .ddr3_dqs_n            (ddr3_dqs_n),              //DDR3 dqs负
    .ddr3_dqs_p            (ddr3_dqs_p),              //DDR3 dqs正  
    .ddr3_addr             (ddr3_addr),               //DDR3 地址   
    .ddr3_ba               (ddr3_ba),                 //DDR3 banck 选择
    .ddr3_ras_n            (ddr3_ras_n),              //DDR3 行选择
    .ddr3_cas_n            (ddr3_cas_n),              //DDR3 列选择
    .ddr3_we_n             (ddr3_we_n),               //DDR3 读写选择
    .ddr3_reset_n          (ddr3_reset_n),            //DDR3 复位
    .ddr3_ck_p             (ddr3_ck_p),               //DDR3 时钟正
    .ddr3_ck_n             (ddr3_ck_n),               //DDR3 时钟负  
    .ddr3_cke              (ddr3_cke),                //DDR3 时钟使能
    .ddr3_cs_n             (ddr3_cs_n),               //DDR3 片选
    .ddr3_dm               (ddr3_dm),                 //DDR3_dm
    .ddr3_odt              (ddr3_odt),                //DDR3_odt
    //用户
    .ddr3_read_valid       (1'b1),                    //DDR3 读使能
    .ddr3_pingpang_en      (1'b1),                    //DDR3 乒乓操作使能
    .wr_clk                (cmos_pclk),               //写时钟
    .wr_load               (cmos_frame_vsync),        //输入源更新信号   
	.datain_valid          (cmos_frame_valid),        //数据有效使能信号
    .datain                (wr_data),                 //有效数据 
    .rd_clk                (pixel_clk),               //读时钟 
    .rd_load               (rd_vsync),                //输出源更新信号    
    .dataout               (rd_data),                 //rfifo输出数据
    .rdata_req             (rdata_req)                //请求数据输入     
     );                

 clk_wiz_0 u_clk_wiz_0
   (
    // Clock out ports
    .clk_out1              (clk_200m),     
    .clk_out2              (clk_50m),
    .clk_out3              (pixel_clk),     
    .clk_out4              (pixel_clk_5x),    
    .clk_out5              (clk_24m),    
    // Status and control signals
    .reset                 (1'b0), 
    .locked                (locked),       
   // Clock in ports
    .clk_in1               (sys_clk)
    );     

//HDMI驱动显示模块    
hdmi_top u_hdmi_top(
    .pixel_clk            (pixel_clk),
    .pixel_clk_5x         (pixel_clk_5x),    
    .sys_rst_n            (sys_init_done & rst_n),
    //hdmi接口
    .hpdin                (hpdin),                     
    .tmds_clk_p           (tmds_clk_p),   // TMDS 时钟通道
    .tmds_clk_n           (tmds_clk_n),
    .tmds_data_p          (tmds_data_p),  // TMDS 数据通道
    .tmds_data_n          (tmds_data_n),
    .tmds_oen             (tmds_oen),     // TMDS 输出使能
    .hpdout               (hpdout),
    //用户接口 
    .video_vs             (rd_vsync),     //HDMI场信号  
    .h_disp               (h_disp),       //HDMI屏水平分辨率
    .v_disp               (v_disp),       //HDMI屏垂直分辨率  
    .pixel_xpos           (),
    .pixel_ypos           (),          
    .data_in              (rd_data),      //数据输入
    .data_req             (rdata_req)     //请求数据输入   
); 
             
endmodule