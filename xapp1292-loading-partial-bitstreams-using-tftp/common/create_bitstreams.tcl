source ../common/set_part.tcl
source ../common/pr_info.tcl

file mkdir Partials

open_checkpoint ./Implement/$static_dcp/top_route_design.dcp
write_debug_probes -force Partials/debug_nets.ltx



# =====================================================
# Generate the bitstreams
# =====================================================

set run.topSynth       0
set run.rmSynth        0
set run.prImpl         0
set run.prVerify       1

set run.writeBitstream 1

#source ../common/setup_pr_design.tcl
source setup_pr_design.tcl

source $tclDir/run.tcl

# Convert each partial bitfile into a bin file formatted for the ICAP port
#
foreach partial $partials {
    set p [dict get $partial FILE]
    set cmd "write_cfgmem -force -format BIN -interface SMAPx32 -disablebitswap -loadbit \"up 0 Bitstreams/$p.bit\" Partials/$p"

    eval $cmd 
}


# Now create a report with the sizes

foreach partial $partials {
    set name [dict get $partial RM]
    set file [dict get $partial FILE]
    set ret [file size Partials/$file.bin]
    puts "$name : $ret bytes"
}

