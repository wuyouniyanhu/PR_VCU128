close_project -quiet

source ../common/set_part.tcl -notrace
create_project project ./project -part $part -force
set_property board_part $board [current_project]
set_property target_language VHDL [current_project]
create_bd_design "static_bd"



# =======================================================================
# |          Source required IP scripts                                 | 
# =======================================================================

source [get_property REPOSITORY [get_ipdefs *prc:1.1]]/xilinx/prc_v1_1/tcl/api.tcl -notrace


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

set_property -dict [list CONFIG.NUM_PORTS {3}] [get_bd_cells cpu_xlconcat]


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
# |                        Generate the PRC                             | 
# =======================================================================

create_bd_cell -type ip -vlnv xilinx.com:ip:prc:1.1 prc
prc_v1_1::set_property -dict [list                                                               \
                                  CONFIG.HAS_AXI_LITE_IF                                   1                       \
                                  CONFIG.RESET_ACTIVE_LEVEL                                1                       \
                                  CONFIG.CP_FIFO_DEPTH                                     16                      \
                                  CONFIG.CP_FIFO_TYPE                                      "lutram"                \
                                  CONFIG.CDC_STAGES                                        2                       \
                                  CONFIG.VS.vs_shift.HAS_AXIS_STATUS                       1                       \
                                  CONFIG.VS.vs_shift.HAS_AXIS_CONTROL                      0                       \
                                  CONFIG.VS.vs_shift.NUM_TRIGGERS_ALLOCATED                4                       \
                                  CONFIG.VS.vs_shift.NUM_HW_TRIGGERS                       0                       \
                                  CONFIG.VS.vs_shift.NUM_RMS_ALLOCATED                     3                       \
                                  CONFIG.VS.vs_shift.HAS_POR_RM                            1                       \
                                  CONFIG.VS.vs_shift.POR_RM                                rm_shift_right          \
                                  CONFIG.VS.vs_shift.SKIP_RM_STARTUP_AFTER_RESET           0                       \
                                  CONFIG.VS.vs_shift.START_IN_SHUTDOWN                     1                       \
                                  CONFIG.VS.vs_shift.SHUTDOWN_ON_ERROR                     1                       \
                                  CONFIG.VS.vs_shift.RM.rm_shift_left.SHUTDOWN_REQUIRED    no                      \
                                  CONFIG.VS.vs_shift.RM.rm_shift_left.STARTUP_REQUIRED     no                      \
                                  CONFIG.VS.vs_shift.RM.rm_shift_left.RESET_REQUIRED       high                    \
                                  CONFIG.VS.vs_shift.RM.rm_shift_left.RESET_DURATION       3                       \
                                  CONFIG.VS.vs_shift.RM.rm_shift_left.BS.0.ADDRESS         0  \
                                  CONFIG.VS.vs_shift.RM.rm_shift_left.BS.0.SIZE            0     \
                                  CONFIG.VS.vs_shift.RM.rm_shift_right.SHUTDOWN_REQUIRED   no                      \
                                  CONFIG.VS.vs_shift.RM.rm_shift_right.STARTUP_REQUIRED    no                      \
                                  CONFIG.VS.vs_shift.RM.rm_shift_right.RESET_REQUIRED      high                    \
                                  CONFIG.VS.vs_shift.RM.rm_shift_right.RESET_DURATION      10                      \
                                  CONFIG.VS.vs_shift.RM.rm_shift_right.BS.0.ADDRESS        0 \
                                  CONFIG.VS.vs_shift.RM.rm_shift_right.BS.0.SIZE           0    \
                                  CONFIG.VS.vs_count.HAS_AXIS_STATUS                       1                       \
                                  CONFIG.VS.vs_count.HAS_AXIS_CONTROL                      0                       \
                                  CONFIG.VS.vs_count.NUM_TRIGGERS_ALLOCATED                4                       \
                                  CONFIG.VS.vs_count.NUM_HW_TRIGGERS                       0                       \
                                  CONFIG.VS.vs_count.NUM_RMS_ALLOCATED                     2                       \
                                  CONFIG.VS.vs_count.HAS_POR_RM                            1                       \
                                  CONFIG.VS.vs_count.POR_RM                                rm_count_up             \
                                  CONFIG.VS.vs_count.SKIP_RM_STARTUP_AFTER_RESET           0                       \
                                  CONFIG.VS.vs_count.START_IN_SHUTDOWN                     1                       \
                                  CONFIG.VS.vs_count.SHUTDOWN_ON_ERROR                     0                       \
                                  CONFIG.VS.vs_count.RM.rm_count_up.SHUTDOWN_REQUIRED      no                      \
                                  CONFIG.VS.vs_count.RM.rm_count_up.STARTUP_REQUIRED       no                      \
                                  CONFIG.VS.vs_count.RM.rm_count_up.RESET_REQUIRED         high                    \
                                  CONFIG.VS.vs_count.RM.rm_count_up.RESET_DURATION         16                      \
                                  CONFIG.VS.vs_count.RM.rm_count_up.BS.0.ADDRESS           0    \
                                  CONFIG.VS.vs_count.RM.rm_count_up.BS.0.SIZE              0       \
                                  CONFIG.VS.vs_count.RM.rm_count_down.SHUTDOWN_REQUIRED    no                      \
                                  CONFIG.VS.vs_count.RM.rm_count_down.STARTUP_REQUIRED     no                      \
                                  CONFIG.VS.vs_count.RM.rm_count_down.RESET_REQUIRED       high                    \
                                  CONFIG.VS.vs_count.RM.rm_count_down.RESET_DURATION       32                      \
                                  CONFIG.VS.vs_count.RM.rm_count_down.BS.0.ADDRESS         0  \
                                  CONFIG.VS.vs_count.RM.rm_count_down.BS.0.SIZE            0     \
             ] [get_bd_cells prc]



# Export VSM ports

create_bd_port -dir O vsm_vs_shift_rm_shutdown_req
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_rm_shutdown_req] [get_bd_ports vsm_vs_shift_rm_shutdown_req]

create_bd_port -dir O vsm_vs_shift_rm_reset
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_rm_reset] [get_bd_ports vsm_vs_shift_rm_reset]

create_bd_port -dir O vsm_vs_shift_sw_shutdown_req
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_sw_shutdown_req] [get_bd_ports vsm_vs_shift_sw_shutdown_req]

create_bd_port -dir O vsm_vs_shift_sw_startup_req
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_sw_startup_req] [get_bd_ports vsm_vs_shift_sw_startup_req]

create_bd_port -dir O vsm_vs_count_rm_shutdown_req
connect_bd_net [get_bd_pins /prc/vsm_vs_count_rm_shutdown_req] [get_bd_ports vsm_vs_count_rm_shutdown_req]

create_bd_port -dir O vsm_vs_count_rm_reset
connect_bd_net [get_bd_pins /prc/vsm_vs_count_rm_reset] [get_bd_ports vsm_vs_count_rm_reset]

create_bd_port -dir O vsm_vs_count_sw_shutdown_req
connect_bd_net [get_bd_pins /prc/vsm_vs_count_sw_shutdown_req] [get_bd_ports vsm_vs_count_sw_shutdown_req]

create_bd_port -dir O vsm_vs_count_sw_startup_req
connect_bd_net [get_bd_pins /prc/vsm_vs_count_sw_startup_req] [get_bd_ports vsm_vs_count_sw_startup_req]

create_bd_port -dir I vsm_vs_shift_rm_shutdown_ack
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_rm_shutdown_ack] [get_bd_ports vsm_vs_shift_rm_shutdown_ack]

create_bd_port -dir I vsm_vs_count_rm_shutdown_ack
connect_bd_net [get_bd_pins /prc/vsm_vs_count_rm_shutdown_ack] [get_bd_ports vsm_vs_count_rm_shutdown_ack]

create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 vsm_vs_shift_m_axis_status
set_property -dict [list CONFIG.PHASE [get_property CONFIG.PHASE [get_bd_intf_pins prc/vsm_vs_shift_m_axis_status]]] [get_bd_intf_ports vsm_vs_shift_m_axis_status]
connect_bd_intf_net [get_bd_intf_pins prc/vsm_vs_shift_m_axis_status] [get_bd_intf_ports vsm_vs_shift_m_axis_status]

create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 vsm_vs_count_m_axis_status
set_property -dict [list CONFIG.PHASE [get_property CONFIG.PHASE [get_bd_intf_pins prc/vsm_vs_count_m_axis_status]]] [get_bd_intf_ports vsm_vs_count_m_axis_status]
connect_bd_intf_net [get_bd_intf_pins prc/vsm_vs_count_m_axis_status] [get_bd_intf_ports vsm_vs_count_m_axis_status]

create_bd_port -dir O vsm_vs_shift_rm_decouple
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_rm_decouple] [get_bd_ports vsm_vs_shift_rm_decouple]

create_bd_port -dir O vsm_vs_shift_event_error
connect_bd_net [get_bd_pins /prc/vsm_vs_shift_event_error] [get_bd_ports vsm_vs_shift_event_error]

create_bd_port -dir O vsm_vs_count_rm_decouple
connect_bd_net [get_bd_pins /prc/vsm_vs_count_rm_decouple] [get_bd_ports vsm_vs_count_rm_decouple]

create_bd_port -dir O vsm_vs_count_event_error
connect_bd_net [get_bd_pins /prc/vsm_vs_count_event_error] [get_bd_ports vsm_vs_count_event_error]



# Export the ICAP port.  
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:icap_rtl:1.0 ICAP
connect_bd_intf_net [get_bd_intf_pins prc/ICAP] [get_bd_intf_ports ICAP]

# Connect the PRC to the AXI Interconnect
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Master "/cpu (Periph)" Clk "Auto" }  [get_bd_intf_pins prc/s_axi_reg]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {Slave "/mig/S_AXI" Clk "Auto" }  [get_bd_intf_pins prc/m_axi_mem]

connect_bd_net [get_bd_pins prc/icap_clk] [get_bd_pins prc/clk]
connect_bd_net [get_bd_pins prc/icap_reset] [get_bd_pins prc/reset]

# ===========================================
# Add an ILA to the PRC's SHIFT status output
# ==========================================
create_bd_cell -type ip -vlnv xilinx.com:ip:ila ila_vs_shift_status

set_property -dict [list CONFIG.C_SLOT_0_AXI_PROTOCOL {AXI4S} CONFIG.C_NUM_OF_PROBES {9}] [get_bd_cells ila_vs_shift_status]
connect_bd_intf_net [get_bd_intf_ports vsm_vs_shift_m_axis_status] [get_bd_intf_pins ila_vs_shift_status/SLOT_0_AXIS]
connect_bd_net [get_bd_pins ila_vs_shift_status/clk] [get_bd_pins prc/clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:ila ila_vsm_shift_outputs
set_property -dict [list CONFIG.C_NUM_OF_PROBES {5} CONFIG.C_MONITOR_TYPE {Native} CONFIG.C_ENABLE_ILA_AXI_MON {false}] [get_bd_cells ila_vsm_shift_outputs]

connect_bd_net [get_bd_pins prc/vsm_vs_shift_rm_shutdown_req ] [get_bd_pins ila_vsm_shift_outputs/probe0]
connect_bd_net [get_bd_pins prc/vsm_vs_shift_rm_decouple     ] [get_bd_pins ila_vsm_shift_outputs/probe1]
connect_bd_net [get_bd_pins prc/vsm_vs_shift_sw_shutdown_req ] [get_bd_pins ila_vsm_shift_outputs/probe2]
connect_bd_net [get_bd_pins prc/vsm_vs_shift_sw_startup_req  ] [get_bd_pins ila_vsm_shift_outputs/probe3]
connect_bd_net [get_bd_pins prc/vsm_vs_shift_rm_reset        ] [get_bd_pins ila_vsm_shift_outputs/probe4] 
connect_bd_net [get_bd_pins ila_vsm_shift_outputs/clk        ] [get_bd_pins prc/clk]


# ===========================================
# Add an ILA to the PRC's COUNT status output
# ==========================================
create_bd_cell -type ip -vlnv xilinx.com:ip:ila ila_vs_count_status

set_property -dict [list CONFIG.C_SLOT_0_AXI_PROTOCOL {AXI4S} CONFIG.C_NUM_OF_PROBES {9}] [get_bd_cells ila_vs_count_status]
connect_bd_intf_net [get_bd_intf_ports vsm_vs_shift_m_axis_status] [get_bd_intf_pins ila_vs_count_status/SLOT_0_AXIS]
connect_bd_net [get_bd_pins ila_vs_count_status/clk] [get_bd_pins prc/clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:ila ila_vsm_count_outputs
set_property -dict [list CONFIG.C_NUM_OF_PROBES {5} CONFIG.C_MONITOR_TYPE {Native} CONFIG.C_ENABLE_ILA_AXI_MON {false}] [get_bd_cells ila_vsm_count_outputs]

connect_bd_net [get_bd_pins prc/vsm_vs_count_rm_shutdown_req ] [get_bd_pins ila_vsm_count_outputs/probe0]
connect_bd_net [get_bd_pins prc/vsm_vs_count_rm_decouple     ] [get_bd_pins ila_vsm_count_outputs/probe1]
connect_bd_net [get_bd_pins prc/vsm_vs_count_sw_shutdown_req ] [get_bd_pins ila_vsm_count_outputs/probe2]
connect_bd_net [get_bd_pins prc/vsm_vs_count_sw_startup_req  ] [get_bd_pins ila_vsm_count_outputs/probe3]
connect_bd_net [get_bd_pins prc/vsm_vs_count_rm_reset        ] [get_bd_pins ila_vsm_count_outputs/probe4] 
connect_bd_net [get_bd_pins ila_vsm_count_outputs/clk        ] [get_bd_pins prc/clk]

create_bd_port -dir O -type clk clk_100
connect_bd_net [get_bd_pins /mig/ui_clk] [get_bd_ports clk_100]

open_bd_design {./project/project.srcs/sources_1/bd/static_bd/static_bd.bd}

validate_bd_design 
set_property synth_checkpoint_mode None [get_files ./project/project.srcs/sources_1/bd/static_bd/static_bd.bd] 
generate_target all [get_files  ./project/project.srcs/sources_1/bd/static_bd/static_bd.bd]
