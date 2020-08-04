# This script generates an instance of the PRC, and also creates a modelsim
# compile script called compile_prc.do

# This TCL needs to be run by vivado batch mode"
#
#   vivado -mode batch -source gen_ip.tcl


source ../common/pr_info.tcl -notrace
source ../common/utils.tcl -notrace


# =======================================================================
# |                        Generate the ila_icap                            | 
# =======================================================================
set ip_name ila_icap
create_and_setup_project ./Sources/generated $ip_name $part $board
create_ip -name ila -vendor xilinx.com -library ip -module_name $ip_name

set_property -dict [list CONFIG.C_PROBE3_WIDTH        {1} \
                         CONFIG.C_PROBE2_WIDTH        {1} \
                         CONFIG.C_PROBE1_WIDTH        {4} \
                         CONFIG.C_PROBE0_WIDTH        {32}\
                         CONFIG.C_DATA_DEPTH          {32768} \
                         CONFIG.C_NUM_OF_PROBES       {4} \
                         CONFIG.C_TRIGOUT_EN          {true} \
                         CONFIG.C_EN_STRG_QUAL        {1} \
                         CONFIG.C_INPUT_PIPE_STAGES   {0} \
                         CONFIG.C_ADV_TRIGGER         {true} \
                         CONFIG.C_PROBE3_MU_CNT       {4} \
                         CONFIG.C_PROBE2_MU_CNT       {4} \
                         CONFIG.C_PROBE1_MU_CNT       {4} \
                         CONFIG.C_PROBE0_MU_CNT       {4} \
                         CONFIG.ALL_PROBE_SAME_MU     {false} \
                         CONFIG.C_MONITOR_TYPE        {Native} \
                         CONFIG.ALL_PROBE_SAME_MU_CNT {2} \
                         CONFIG.C_ENABLE_ILA_AXI_MON  {false}] [get_ips $ip_name]


set_property generate_synth_checkpoint false [get_files  ./Sources/generated/$ip_name.srcs/sources_1/ip/$ip_name/$ip_name.xci]
generate_target {all} [get_ips $ip_name]


