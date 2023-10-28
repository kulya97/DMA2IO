# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct E:\DMA2IO\hl_zynq7020-DMA2IO\ARM\top_module\platform.tcl
# 
# OR launch xsct and run below command.
# source E:\DMA2IO\hl_zynq7020-DMA2IO\ARM\top_module\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {top_module}\
-hw {E:\DMA2IO\hl_zynq7020-DMA2IO\top_module.xsa}\
-out {E:/DMA2IO/hl_zynq7020-DMA2IO/ARM}

platform write
domain create -name {standalone_ps7_cortexa9_0} -display-name {standalone_ps7_cortexa9_0} -os {standalone} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {empty_application}
platform generate -domains 
platform active {top_module}
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
platform generate -quick
platform generate
bsp reload
platform generate -domains 
platform config -updatehw {E:/DMA2IO/hl_zynq7020-DMA2IO/top_module.xsa}
platform generate -domains 
platform generate
platform clean
platform generate
