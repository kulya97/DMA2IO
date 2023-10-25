# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: E:\test\hl_zynq7020-mt9v034\ARM\DMA2GPIO_system\_ide\scripts\systemdebugger_dma2gpio_system_standalone.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source E:\test\hl_zynq7020-mt9v034\ARM\DMA2GPIO_system\_ide\scripts\systemdebugger_dma2gpio_system_standalone.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-HS1 210512180081" && level==0 && jtag_device_ctx=="jsn-JTAG-HS1-210512180081-23727093-0"}
fpga -file E:/test/hl_zynq7020-mt9v034/ARM/DMA2GPIO/_ide/bitstream/top_module.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw E:/test/hl_zynq7020-mt9v034/ARM/top_module/export/top_module/hw/top_module.xsa -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
source E:/test/hl_zynq7020-mt9v034/ARM/DMA2GPIO/_ide/psinit/ps7_init.tcl
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
dow E:/test/hl_zynq7020-mt9v034/ARM/DMA2GPIO/Debug/DMA2GPIO.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "*A9*#0"}
con
