//****************************************Copyright (c)***********************************//
//ԭ�Ӹ����߽�ѧƽ̨��www.yuanzige.com
//����֧�֣�www.openedv.com
//�Ա����̣�http://openedv.taobao.com
//��ע΢�Ź���ƽ̨΢�źţ�"����ԭ��"����ѻ�ȡZYNQ & FPGA & STM32 & LINUX���ϡ�
//��Ȩ���У�����ؾ���
//Copyright(C) ����ԭ�� 2018-2028
//All rights reserved
//----------------------------------------------------------------------------------------
// File name:           mt9v034_hdmi
// Last modified Date:  2020/05/04 9:19:08
// Last Version:        V1.0
// Descriptions:        mt9v034����ͷHDMI��ʾʵ��
//                      
//----------------------------------------------------------------------------------------
// Created by:          ����ԭ��
// Created date:        2019/05/04 9:19:08
// Version:             V1.0
// Descriptions:        The original version
//
//----------------------------------------------------------------------------------------
//****************************************************************************************//

module mt9v034_hdmi(    
    input                 sys_clk      ,  //ϵͳʱ��
    input                 sys_rst_n    ,  //ϵͳ��λ���͵�ƽ��Ч
    //����ͷ�ӿ�                       
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
    inout   [15:0]        ddr3_dq      ,  //DDR3 ����
    inout   [1:0]         ddr3_dqs_n   ,  //DDR3 dqs��
    inout   [1:0]         ddr3_dqs_p   ,  //DDR3 dqs��  
    output  [13:0]        ddr3_addr    ,  //DDR3 ��ַ   
    output  [2:0]         ddr3_ba      ,  //DDR3 banck ѡ��
    output                ddr3_ras_n   ,  //DDR3 ��ѡ��
    output                ddr3_cas_n   ,  //DDR3 ��ѡ��
    output                ddr3_we_n    ,  //DDR3 ��дѡ��
    output                ddr3_reset_n ,  //DDR3 ��λ
    output  [0:0]         ddr3_ck_p    ,  //DDR3 ʱ����
    output  [0:0]         ddr3_ck_n    ,  //DDR3 ʱ�Ӹ�
    output  [0:0]         ddr3_cke     ,  //DDR3 ʱ��ʹ��
    output  [0:0]         ddr3_cs_n    ,  //DDR3 Ƭѡ
    output  [1:0]         ddr3_dm      ,  //DDR3_dm
    output  [0:0]         ddr3_odt     ,  //DDR3_odt									   
    //hdmi�ӿ�                           
    input                 hpdin,    
    output                tmds_clk_p   ,  // TMDS ʱ��ͨ��
    output                tmds_clk_n   ,
    output  [2:0]         tmds_data_p  ,  // TMDS ����ͨ��
    output  [2:0]         tmds_data_n  ,
    output                tmds_oen     ,  // TMDS ���ʹ��
    output                hpdout
    );                                 
									   
//parameter define                     
parameter  SLAVE_ADD = 7'b1001_000     ;  //slave  address         90  
parameter  BIT_CTRL   = 1'b0           ;  //OV7725���ֽڵ�ַΪ8λ  0:8λ 1:16λ
parameter  DATA_CTRL  = 1'b1           ;  //OV7725������Ϊ8λ  0:8λ 1:16λ
parameter  CLK_FREQ   = 26'd50_000_000 ;  //i2c_driģ�������ʱ��Ƶ�� 50.0MHz
parameter  I2C_FREQ   = 18'd250_000    ;  //I2C��SCLʱ��Ƶ��,������400KHz

parameter  V_CMOS_DISP = 11'd480;         //CMOS�ֱ���--��
parameter  H_CMOS_DISP = 11'd640;         //CMOS�ֱ���--��				
									   									   
//wire define                          
wire         clk_100m                  ;  //100mhzʱ��
wire         clk_100m_shift            ;  //100mhzʱ��,��λƫ��ʱ��
wire         clk_50m                   ;  //50mhzʱ��,�ṩ��lcd����ʱ��
wire         locked                    ;  //ʱ�������ź�
wire         rst_n                     ;  //ȫ�ָ�λ 								    
wire         i2c_exec                  ;  //I2C����ִ���ź�
wire  [15:0] i2c_data                  ;  //I2CҪ���õĵ�ַ������(��8λ��ַ,��8λ����)          
wire         cam_init_done             ;  //����ͷ��ʼ�����
wire         i2c_done                  ;  //I2C�Ĵ�����������ź�
wire         i2c_dri_clk               ;  //I2C����ʱ��								    
wire         wr_en                     ;  //DDR3������ģ��дʹ��
wire  [15:0] wr_data                   ;  //DDR3������ģ��д����
wire         rdata_req                 ;  //DDR3������ģ���ʹ��
wire  [15:0] rd_data                   ;  //DDR3������ģ�������
wire         cmos_frame_valid          ;  //������Чʹ���ź�
wire         init_calib_complete       ;  //DDR3��ʼ�����init_calib_complete
wire         sys_init_done             ;  //ϵͳ��ʼ�����(DDR3��ʼ��+����ͷ��ʼ��)
wire         clk_200m                  ;  //ddr3�ο�ʱ��
wire         cmos_frame_vsync          ;  //���֡��Ч��ͬ���ź�
wire         lcd_de                    ;  //LCD ��������ʹ��
wire         cmos_frame_href           ;  //���֡��Ч��ͬ���ź� 
wire  [27:0] app_addr_rd_min           ;  //��DDR3����ʼ��ַ
wire  [27:0] app_addr_rd_max           ;  //��DDR3�Ľ�����ַ
wire  [7:0]  rd_bust_len               ;  //��DDR3�ж�����ʱ��ͻ������
wire  [27:0] app_addr_wr_min           ;  //дDDR3����ʼ��ַ
wire  [27:0] app_addr_wr_max           ;  //дDDR3�Ľ�����ַ
wire  [7:0]  wr_bust_len               ;  //��DDR3�ж�����ʱ��ͻ������
wire  [9:0]  pixel_xpos_w              ;  //���ص������
wire  [9:0]  pixel_ypos_w              ;  //���ص�������   
wire         lcd_clk                   ;  //��Ƶ������LCD ����ʱ��
wire  [10:0] h_disp                    ;  //LCD��ˮƽ�ֱ���
wire  [10:0] v_disp                    ;  //LCD����ֱ�ֱ���     
wire  [10:0] h_pixel                   ;  //����ddr3��ˮƽ�ֱ���        
wire  [10:0] v_pixel                   ;  //����ddr3������ֱ�ֱ��� 
wire  [15:0] lcd_id                    ;  //LCD����ID��
wire  [27:0] ddr3_addr_max             ;  //����DDR3������д��ַ 
wire  [7:0]  cmos_frame_data           ;  //cmos frame data output
wire  [7:0]	 i2c_addr                  ;  
wire  [15:0] i2c_wr_data               ;  
wire  [7:0]  cam_frame_data;

//*****************************************************
//**                    main code
//*****************************************************

//��ʱ�������������λ�����ź�
assign  rst_n = sys_rst_n & locked;

//ϵͳ��ʼ����ɣ�DDR3��ʼ�����
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

//I2C����ģ��
i2c_dri #(
    .SLAVE_ADDR         (SLAVE_ADD),    //��������
    .CLK_FREQ           (CLK_FREQ  ),              
    .I2C_FREQ           (I2C_FREQ  ) 
    )
u_i2c_dr(
    .clk                (clk_50m),
    .rst_n              (sys_rst_n     ),

    .i2c_exec           (i2c_exec  ),   
    .bit_ctrl           (BIT_CTRL  ),   
    .data_ctrl          (DATA_CTRL),
    .i2c_rh_wl          (0),            //�̶�Ϊ0��ֻ�õ���IIC������д����   
    .i2c_addr           ({8'b0,i2c_addr}),   
    .i2c_data_w         (i2c_wr_data),   
    .i2c_data_r         (),   
    .i2c_done           (i2c_done  ),    
    .scl                (cmos_scl   ),   
    .sda                (cmos_sda   ),   
    .dri_clk            (i2c_dri)       //I2C����ʱ��
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
    .clk_200m              (clk_200m),                //ϵͳʱ��
    .sys_rst_n             (rst_n),                   //��λ,����Ч
    .sys_init_done         (sys_init_done),           //ϵͳ��ʼ�����
    .init_calib_complete   (init_calib_complete),     //ddr3��ʼ������ź�    
    //ddr3�ӿ��ź�         
    .app_addr_rd_min       (28'd0),                   //��DDR3����ʼ��ַ
    .app_addr_rd_max       (V_CMOS_DISP*H_CMOS_DISP), //��DDR3�Ľ�����ַ
    .rd_bust_len           (H_CMOS_DISP[10:3]),       //��DDR3�ж�����ʱ��ͻ������
    .app_addr_wr_min       (28'd0),                   //дDDR3����ʼ��ַ
    .app_addr_wr_max       (V_CMOS_DISP*H_CMOS_DISP), //дDDR3�Ľ�����ַ
    .wr_bust_len           (H_CMOS_DISP[10:3]),       //��DDR3�ж�����ʱ��ͻ������
    // DDR3 IO�ӿ�                
    .ddr3_dq               (ddr3_dq),                 //DDR3 ����
    .ddr3_dqs_n            (ddr3_dqs_n),              //DDR3 dqs��
    .ddr3_dqs_p            (ddr3_dqs_p),              //DDR3 dqs��  
    .ddr3_addr             (ddr3_addr),               //DDR3 ��ַ   
    .ddr3_ba               (ddr3_ba),                 //DDR3 banck ѡ��
    .ddr3_ras_n            (ddr3_ras_n),              //DDR3 ��ѡ��
    .ddr3_cas_n            (ddr3_cas_n),              //DDR3 ��ѡ��
    .ddr3_we_n             (ddr3_we_n),               //DDR3 ��дѡ��
    .ddr3_reset_n          (ddr3_reset_n),            //DDR3 ��λ
    .ddr3_ck_p             (ddr3_ck_p),               //DDR3 ʱ����
    .ddr3_ck_n             (ddr3_ck_n),               //DDR3 ʱ�Ӹ�  
    .ddr3_cke              (ddr3_cke),                //DDR3 ʱ��ʹ��
    .ddr3_cs_n             (ddr3_cs_n),               //DDR3 Ƭѡ
    .ddr3_dm               (ddr3_dm),                 //DDR3_dm
    .ddr3_odt              (ddr3_odt),                //DDR3_odt
    //�û�
    .ddr3_read_valid       (1'b1),                    //DDR3 ��ʹ��
    .ddr3_pingpang_en      (1'b1),                    //DDR3 ƹ�Ҳ���ʹ��
    .wr_clk                (cmos_pclk),               //дʱ��
    .wr_load               (cmos_frame_vsync),        //����Դ�����ź�   
	.datain_valid          (cmos_frame_valid),        //������Чʹ���ź�
    .datain                (wr_data),                 //��Ч���� 
    .rd_clk                (pixel_clk),               //��ʱ�� 
    .rd_load               (rd_vsync),                //���Դ�����ź�    
    .dataout               (rd_data),                 //rfifo�������
    .rdata_req             (rdata_req)                //������������     
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

//HDMI������ʾģ��    
hdmi_top u_hdmi_top(
    .pixel_clk            (pixel_clk),
    .pixel_clk_5x         (pixel_clk_5x),    
    .sys_rst_n            (sys_init_done & rst_n),
    //hdmi�ӿ�
    .hpdin                (hpdin),                     
    .tmds_clk_p           (tmds_clk_p),   // TMDS ʱ��ͨ��
    .tmds_clk_n           (tmds_clk_n),
    .tmds_data_p          (tmds_data_p),  // TMDS ����ͨ��
    .tmds_data_n          (tmds_data_n),
    .tmds_oen             (tmds_oen),     // TMDS ���ʹ��
    .hpdout               (hpdout),
    //�û��ӿ� 
    .video_vs             (rd_vsync),     //HDMI���ź�  
    .h_disp               (h_disp),       //HDMI��ˮƽ�ֱ���
    .v_disp               (v_disp),       //HDMI����ֱ�ֱ���  
    .pixel_xpos           (),
    .pixel_ypos           (),          
    .data_in              (rd_data),      //��������
    .data_req             (rdata_req)     //������������   
); 
             
endmodule