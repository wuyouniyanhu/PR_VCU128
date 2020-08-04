#####################################################################################
## Constraints from file : 'top.xdc'
#####################################################################################
#
##///////////////////////////////////////////////////////////////////////////////
##// Copyright (c) 2005-2016 Xilinx, Inc.
##// This design is confidential and proprietary of Xilinx, Inc.
##// All Rights Reserved.
##///////////////////////////////////////////////////////////////////////////////
##//   ____  ____
##//  /   /\/   /
##// /___/  \  /   Vendor: Xilinx
##// \   \   \/    Version: 
##//  \   \        Application: Partial Reconfiguration
##//  /   /        Filename: top.xdc
##// /___/   /\    Date Last Modified: 
##// \   \  /  \
##//  \___\/\___\
##// Device: KC705 board Rev 1.1
##// Design Name: 
##///////////////////////////////////////////////////////////////////////////////



#-----------------------------------
# ICAPE2 constraints
#-----------------------------------
# set icap_clock       [get_clocks -of_objects [get_ports ICAPE2_inst/CLK]]
set icap_clock       [get_clocks -of_objects [get_ports ICAPE3_inst/CLK]]


#-----------------------------------
# ICAPE3 constraints
#-----------------------------------
set_property PACKAGE_PIN BM29 [get_ports reset]
set_property IOSTANDARD LVCMOS12 [get_ports reset]


#-----------------------------------
# LED LOCs
#-----------------------------------
#    LED[0-3] shift
#    LED[4-7] count
#-----------------------------------VCU128 LEDs :
#LED0
set_property PACKAGE_PIN BH24     [get_ports shift_out[0]]
set_property IOSTANDARD LVCMOS18 [get_ports shift_out[0]]

# LED1
set_property PACKAGE_PIN BG24     [get_ports shift_out[1]]
set_property IOSTANDARD LVCMOS18 [get_ports shift_out[1]]

# LED2
set_property PACKAGE_PIN BG25     [get_ports shift_out[2]]
set_property IOSTANDARD LVCMOS18 [get_ports shift_out[2]]

# LED3
set_property PACKAGE_PIN BF25     [get_ports shift_out[3]]
set_property IOSTANDARD LVCMOS18 [get_ports shift_out[3]]

# LED4
set_property PACKAGE_PIN BF26    [get_ports count_out[0]]
set_property IOSTANDARD LVCMOS18 [get_ports count_out[0]]

# LED5
set_property PACKAGE_PIN BF27     [get_ports count_out[1]]
set_property IOSTANDARD LVCMOS18 [get_ports count_out[1]]

# LED6
set_property PACKAGE_PIN BG27     [get_ports count_out[2]]
set_property IOSTANDARD LVCMOS18 [get_ports count_out[2]]

# LED7
set_property PACKAGE_PIN BG28     [get_ports count_out[3]]
set_property IOSTANDARD LVCMOS18 [get_ports count_out[3]]

#################################################################################

#-------------------------------------------------
# pblock_count 
#-------------------------------------------------
create_pblock pblock_count
add_cells_to_pblock [get_pblocks pblock_count]  [get_cells -quiet [list inst_count]]
resize_pblock [get_pblocks pblock_count] -add {SLICE_X136Y50:SLICE_X145Y99}
resize_pblock [get_pblocks pblock_count] -add {RAMB18_X6Y20:RAMB18_X6Y39}
resize_pblock [get_pblocks pblock_count] -add {RAMB36_X6Y10:RAMB36_X6Y19}
set_property RESET_AFTER_RECONFIG 1 [get_pblocks pblock_count]

#-------------------------------------------------
# pblock_shift 
#-------------------------------------------------
create_pblock pblock_shift
add_cells_to_pblock [get_pblocks pblock_shift]  [get_cells -quiet [list inst_shift]]
resize_pblock pblock_shift -add {SLICE_X4Y150:SLICE_X11Y199 RAMB18_X0Y60:RAMB18_X0Y79 RAMB36_X0Y30:RAMB36_X0Y39}
set_property RESET_AFTER_RECONFIG 1 [get_pblocks pblock_shift]


#-----------------------------------
# End
#-----------------------------------
