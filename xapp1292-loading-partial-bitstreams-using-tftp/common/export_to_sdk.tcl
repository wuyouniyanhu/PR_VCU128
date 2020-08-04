create_project -in_memory
source ../common/set_part.tcl
source ../common/pr_info.tcl

read_bd  ./project/project.srcs/sources_1/bd/static_bd/static_bd.bd 
read_vhd ./project/project.srcs/sources_1/bd/static_bd/hdl/static_bd_wrapper.vhd 
read_vhd ../common/Sources/hdl/top/top.vhd
generate_target all [get_files ./project/project.srcs/sources_1/bd/static_bd/static_bd.bd] 
write_hwdef -force -file ./SDK/static_bd_wrapper.hwdef 
write_sysdef -force -hwdef ./SDK/static_bd_wrapper.hwdef -bitfile Bitstreams/${static_dcp}.bit -file ./SDK/static_bd_wrapper.hdf


# If pr_platform exists then it should have the static bitstream in it.  Exporting the project to 
# SDK doesn't seem to update this bitstream if the platform hasn't changed.  That's a potential problem
# as we might end up using an old version of the static design.  This code forces the static bitstream to be 
# the latest one 

if {[file exists ./sw/pr_platform]} {
    file copy -force Bitstreams/${static_dcp}.bit sw/pr_platform
}