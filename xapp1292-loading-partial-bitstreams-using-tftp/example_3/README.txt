Setup
=====
Modify copy_files_to_tftp_server.tcl to set the TFTP server address correctly


Running
=======
cd example_3

../common/clean.pl        # Optional: Clean any previously generated results
vivado -mode batch -source gen_ip.tcl   
vivado -mode batch -source create_ipi_design.tcl  
vivado -mode batch -source ../common/implement_design.tcl
vivado -mode batch -source ../common/create_bitstreams.tcl
vivado -mode batch -source ../common/export_to_sdk.tcl

# Ensure the TFTP server is running and unblocked before executing this command
vivado -mode batch -source copy_files_to_tftp_server.tcl 

This builds the HW platform
