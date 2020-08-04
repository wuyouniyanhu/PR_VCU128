# =======================================================================
# Create and setup a project in <directory> with <name>
# =======================================================================
proc create_and_setup_project {directory name part board} {
    close_project -quiet
    create_project -force $name $directory -part $part

    set_property target_language VHDL [current_project]
    set_property coreContainer.enable 0 [current_project]

    update_ip_catalog
}
