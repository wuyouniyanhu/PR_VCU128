close_project -quiet
source ../common/set_part.tcl -notrace
create_project project ./project -part $part  -force
set_property board_part $board [current_project]
set_property target_language VHDL [current_project]
create_bd_design "static_bd"


# =======================================================================
# Create the MIG
# =======================================================================
create_bd_cell -type ip -vlnv xilinx.com:ip:mig_7series mig
apply_bd_automation -rule xilinx.com:bd_rule:mig_7series -config {Board_Interface "ddr3_sdram" }  [get_bd_cells mig]
apply_bd_automation -rule xilinx.com:bd_rule:board -config {Board_Interface "reset ( FPGA Reset ) " }  [get_bd_pins mig/sys_rst]

# =======================================================================
# Create the Microblaze
# =======================================================================
create_bd_cell -type ip -vlnv xilinx.com:ip:microblaze cpu
apply_bd_automation -rule xilinx.com:bd_rule:microblaze -config {local_mem "128KB" ecc "None" cache "None" debug_module "Debug Only" axi_periph "Enabled" axi_intc "1" clk "/mig/ui_clk (100 MHz)" }  [get_bd_cells cpu]
set_property -dict [list CONFIG.C_NUMBER_OF_PC_BRK {8} CONFIG.C_NUMBER_OF_RD_ADDR_BRK {4} CONFIG.C_NUMBER_OF_WR_ADDR_BRK {4}] [get_bd_cells cpu]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/cpu (Periph)" Clk "Auto" }  [get_bd_intf_pins mig/S_AXI]

set_property range 1M [get_bd_addr_segs {cpu/Data/SEG_dlmb_bram_if_cntlr_Mem}]
set_property range 1M [get_bd_addr_segs {cpu/Instruction/SEG_ilmb_bram_if_cntlr_Mem}]

set_property -dict [list CONFIG.NUM_PORTS {4}] [get_bd_cells cpu_xlconcat]


# =======================================================================
# Create the Ethernet
# =======================================================================

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_ethernetlite:3.0 ethernet
set_property -dict [list CONFIG.MII_BOARD_INTERFACE {mii} CONFIG.MDIO_BOARD_INTERFACE {mdio_mdc}] [get_bd_cells ethernet]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/cpu (Periph)" Clk "Auto" }  [get_bd_intf_pins ethernet/S_AXI]
apply_bd_automation -rule xilinx.com:bd_rule:board -config {Board_Interface "mii ( Onboard PHY ) " }  [get_bd_intf_pins ethernet/MII]
apply_bd_automation -rule xilinx.com:bd_rule:board -config {Board_Interface "mdio_mdc ( Onboard PHY ) " }  [get_bd_intf_pins ethernet/MDIO]
connect_bd_net [get_bd_pins ethernet/ip2intc_irpt] [get_bd_pins cpu_xlconcat/In2]



# =======================================================================
# Create the UART
# =======================================================================
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_uartlite:2.0 uart
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/cpu (Periph)" Clk "Auto" }  [get_bd_intf_pins uart/S_AXI]
apply_bd_automation -rule xilinx.com:bd_rule:board -config {Board_Interface "rs232_uart" }  [get_bd_intf_pins uart/UART]
connect_bd_net [get_bd_pins uart/interrupt] [get_bd_pins cpu_xlconcat/In0]


# =======================================================================
# Create the timer
# =======================================================================
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_timer:2.0 timer
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/cpu (Periph)" Clk "Auto" }  [get_bd_intf_pins timer/S_AXI]
connect_bd_net [get_bd_pins timer/interrupt] [get_bd_pins cpu_xlconcat/In1]


# =======================================================================
# |                        Generate AXI HWICAP                          | 
# =======================================================================

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_hwicap:3.0 axi_hwicap_0
set_property -dict [list CONFIG.C_ICAP_EXTERNAL {1}] [get_bd_cells axi_hwicap_0]
set_property -dict [list CONFIG.C_WRITE_FIFO_DEPTH {1024} CONFIG.C_OPERATION {0}] [get_bd_cells axi_hwicap_0]
set_property -dict [list CONFIG.C_INCLUDE_STARTUP {1}] [get_bd_cells axi_hwicap_0]
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:icap_rtl:1.0 ICAP
connect_bd_intf_net [get_bd_intf_pins axi_hwicap_0/ICAP] [get_bd_intf_ports ICAP]
connect_bd_net [get_bd_pins axi_hwicap_0/icap_clk] [get_bd_pins mig/ui_clk]
connect_bd_net [get_bd_pins axi_hwicap_0/s_axi_aclk] [get_bd_pins mig/ui_clk]
connect_bd_net [get_bd_pins axi_hwicap_0/s_axi_aresetn] [get_bd_pins rst_mig_100M/peripheral_aresetn]
connect_bd_net [get_bd_pins axi_hwicap_0/ip2intc_irpt] [get_bd_pins cpu_xlconcat/In3]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/cpu (Periph)" Clk "Auto" }  [get_bd_intf_pins axi_hwicap_0/S_AXI_LITE]

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 constant_0
set_property -dict [list CONFIG.CONST_VAL {0}] [get_bd_cells constant_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 constant_1
set_property -dict [list CONFIG.CONST_VAL {1}] [get_bd_cells constant_1]

connect_bd_net [get_bd_pins constant_1/dout] [get_bd_pins axi_hwicap_0/cap_gnt]
connect_bd_net [get_bd_pins constant_0/dout] [get_bd_pins axi_hwicap_0/cap_rel]



create_bd_port -dir O -type clk clk_100
connect_bd_net [get_bd_pins /mig/ui_clk] [get_bd_ports clk_100]



open_bd_design {./project/project.srcs/sources_1/bd/static_bd/static_bd.bd}

validate_bd_design 
set_property synth_checkpoint_mode None [get_files ./project/project.srcs/sources_1/bd/static_bd/static_bd.bd] 
generate_target all [get_files  ./project/project.srcs/sources_1/bd/static_bd/static_bd.bd]

