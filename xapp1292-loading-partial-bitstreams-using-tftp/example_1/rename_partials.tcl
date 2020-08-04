# ===========================================================================
# Rename the partials so that the software can predict their names
# ===========================================================================

source ../common/pr_info.tcl -notrace

# ----------------------------------------
# Get the configuration for the PRC
# ----------------------------------------

# If this is run as part of a larger script then the project will already be open.
current_project project
close_project 
open_project ./project/project.xpr 
source [get_property REPOSITORY [get_ipdefs *prc:1.1]]/xilinx/prc_v1_1/tcl/api.tcl -notrace
set config [get_property CONFIG.ALL_PARAMS [get_ips static_bd_prc_0]]
close_project -quiet

foreach partial $partials {
    put $partial
    set VSM  [dict get $partial VSM]
    set RM   [dict get $partial RM]
    set file [dict get $partial FILE]

    set vsm_id [prc_v1_1::priv::get_vs_id config $VSM]
    set rm_id  [prc_v1_1::priv::get_rm_id config $VSM $RM]
    set new_name rp${vsm_id}_rm${rm_id}


    puts "Renaming Partials/$file.bin Partials/$new_name.bin"
    file rename -force Partials/$file.bin Partials/$new_name.bin
}
