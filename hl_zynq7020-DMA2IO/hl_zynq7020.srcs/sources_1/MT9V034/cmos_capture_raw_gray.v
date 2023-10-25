//****************************************Copyright (c)***********************************//
//ԭ�Ӹ����߽�ѧƽ̨��www.yuanzige.com
//����֧�֣�www.openedv.com
//�Ա����̣�http://openedv.taobao.com
//��ע΢�Ź���ƽ̨΢�źţ�"����ԭ��"����ѻ�ȡZYNQ & FPGA & STM32 & LINUX���ϡ�
//��Ȩ���У�����ؾ���
//Copyright(C) ����ԭ�� 2018-2028
//All rights reserved
//----------------------------------------------------------------------------------------
// File name:           cmos_capture_raw_gray
// Last modified Date:  2020/05/04 9:19:08
// Last Version:        V1.0
// Descriptions:        ͼ��ɼ�ģ��
//                      
//----------------------------------------------------------------------------------------
// Created by:          ����ԭ��
// Created date:        2019/05/04 9:19:08
// Version:             V1.0
// Descriptions:        The original version
//
//----------------------------------------------------------------------------------------
//****************************************************************************************//
`timescale 1ns/1ns

module cmos_capture_raw_gray
#(
	parameter			CMOS_FRAME_WAITCNT	=	4'd10		//Wait n fps for steady(OmniVision need 10 Frame)
															
)
(
	//global clock
	input				clk_cmos,			//24MHz CMOS Driver clock input
	input				rst_n,				//global reset

	//CMOS Sensor Interface
	input				cmos_pclk,			//24MHz CMOS Pixel clock input
	output				cmos_xclk,			//24MHz drive clock
	input				cmos_vsync,			//H : Data Valid; L : Frame Sync(Set it by register)
	input				cmos_href,			//H : Data vaild, L : Line Sync
	input		[7:0]	cmos_data,			//8 bits cmos data input
	
	//CMOS SYNC Data output
    output				cmos_frame_vsync,	//cmos frame data vsync valid signal
    output				cmos_frame_href,	//cmos frame data href vaild  signal
    output		[7:0]	cmos_frame_data,	//cmos frame RAW output	
    output				cmos_frame_clken,	//cmos frame data output/capture enable clock, 12MHz
	
	//user interface
	output	reg	[7:0]	cmos_fps_rate		//cmos frame output rate
);

//parameter define   
localparam	DELAY_TOP = 2 * 24_000000;	//2s delay

//reg define
reg	[27:0]	delay_cnt;
reg		    frame_sync_flag;
reg	[3:0]	cmos_fps_cnt;
reg	[1:0]	cmos_vsync_r, cmos_href_r;
reg	[7:0]	cmos_data_r0, cmos_data_r1;
reg	[8:0]	cmos_fps_cnt2;

//wire define
wire        cmos_vsync_end;
wire        delay_2s;

//*****************************************************
//**                    main code
//*****************************************************   

assign	cmos_vsync_end 		= 	(cmos_vsync_r[1] & ~cmos_vsync_r[0]) ? 1'b1 : 1'b0;	
assign	cmos_xclk = clk_cmos;	//24MHz CMOS XCLK output
assign	cmos_frame_clken = frame_sync_flag ? cmos_href_r[1] : 1'b0;
assign	cmos_frame_vsync = frame_sync_flag ? cmos_vsync_r[1] : 1'b0;//DFF 2 clocks
assign	cmos_frame_href  = frame_sync_flag ? cmos_href_r[1] : 1'b0;	//DFF 2 clocks
assign	cmos_frame_data	 = frame_sync_flag ? cmos_data_r1 : 8'd0;	//DFF 2 clocks
assign	delay_2s = (delay_cnt == DELAY_TOP - 1'b1) ? 1'b1 : 1'b0;

always@(posedge cmos_pclk or negedge rst_n)begin
	if(!rst_n)
		begin
		cmos_vsync_r <= 0;
		cmos_href_r <= 0;
		{cmos_data_r1, cmos_data_r0} <= 0;
		end
	else
		begin
		cmos_vsync_r <= {cmos_vsync_r[0], cmos_vsync};
		cmos_href_r <= {cmos_href_r[0], cmos_href};
		{cmos_data_r1, cmos_data_r0} <= {cmos_data_r0, cmos_data};
		end
end

//Wait for Sensor output Data valid 10 Frame of OmniVision
always@(posedge cmos_pclk or negedge rst_n)begin
	if(!rst_n)
		cmos_fps_cnt <= 0;
	else	//Wait until cmos init complete
		begin
		if(cmos_fps_cnt < CMOS_FRAME_WAITCNT)	
			cmos_fps_cnt <= cmos_vsync_end ? cmos_fps_cnt + 1'b1 : cmos_fps_cnt;
		else
			cmos_fps_cnt <= CMOS_FRAME_WAITCNT;
		end
end

//Come ture frame synchronization to ignore error frame or has not capture when vsync begin
always@(posedge cmos_pclk or negedge rst_n)begin
	if(!rst_n)
		frame_sync_flag <= 0;
	else if(cmos_fps_cnt == CMOS_FRAME_WAITCNT && cmos_vsync_end == 1)
		frame_sync_flag <= 1;
	else
		frame_sync_flag <= frame_sync_flag;
end

//Delay 2s for cmos fps counter
always@(posedge cmos_pclk or negedge rst_n)begin
	if(!rst_n)
		delay_cnt <= 0;
	else if(delay_cnt < DELAY_TOP - 1'b1)
		delay_cnt <= delay_cnt + 1'b1;
	else
		delay_cnt <= 0;
end

//cmos image output rate counter
always@(posedge cmos_pclk or negedge rst_n)begin
	if(!rst_n)
		begin
		cmos_fps_cnt2 <= 0;
		cmos_fps_rate <= 0;
		end
	else if(delay_2s == 1'b0)	//time is not reached
		begin
		cmos_fps_cnt2 <= cmos_vsync_end ? cmos_fps_cnt2 + 1'b1 : cmos_fps_cnt2;
		cmos_fps_rate <= cmos_fps_rate;
		end
	else	//time up
		begin
		cmos_fps_cnt2 <= 0;
		cmos_fps_rate <= cmos_fps_cnt2[8:1];	//divide by 2
		end
end

endmodule
