###########################
#### Implement Modules ####
###########################
proc impl {impl} {
   global part
   global dcpLevel
   global verbose
   global implDir
   global xdcDir
   global dcpDir
   global modules
   global ooc_implementations 

   set top                 [get_attribute impl $impl top]
   set implXDC             [get_attribute impl $impl implXDC]
   set linkXDC             [get_attribute impl $impl linkXDC]
   set cores               [get_attribute impl $impl cores]
   set hd                  [get_attribute impl $impl hd.impl]
   set td                  [get_attribute impl $impl td.impl]
   set ic                  [get_attribute impl $impl ic.impl]
   set partitions          [get_attribute impl $impl partitions]
   set link                [get_attribute impl $impl link]
   set opt                 [get_attribute impl $impl opt]
   set opt.pre             [get_attribute impl $impl opt.pre]
   set opt_options         [get_attribute impl $impl opt_options]
   set opt_directive       [get_attribute impl $impl opt_directive]
   set place               [get_attribute impl $impl place]
   set place.pre           [get_attribute impl $impl place.pre]
   set place_options       [get_attribute impl $impl place_options]
   set place_directive     [get_attribute impl $impl place_directive]
   set phys                [get_attribute impl $impl phys]
   set phys.pre            [get_attribute impl $impl phys.pre]
   set phys_options        [get_attribute impl $impl phys_options]
   set phys_directive      [get_attribute impl $impl phys_directive]
   set route               [get_attribute impl $impl route]
   set route.pre           [get_attribute impl $impl route.pre]
   set route_options       [get_attribute impl $impl route_options]
   set route_directive     [get_attribute impl $impl route_directive]
   set bitstream           [get_attribute impl $impl bitstream]
   set bitstream.pre       [get_attribute impl $impl bitstream.pre]
   set bitstream_options   [get_attribute impl $impl bitstream_options]
   set bitstream_settings  [get_attribute impl $impl bitstream_settings]
   set drc.quiet           [get_attribute impl $impl drc.quiet]


   if {($hd && $td) || ($hd && $ic) || ($td && $ic)} {
      set errMsg "\nERROR: Implementation $impl has more than one of the following flow variables set to 1 \n\thd.impl\n\ttd.impl\n\tic.impl\nOnly one of these variables can be set true at one time. To run multiple flows, create separate implementation runs."
      error $errMsg
   }
   # Make the implementation directory if needed
   if {![file exists $implDir]} {
      command "file mkdir $implDir"
   }
   set resultDir "$implDir/$impl"
   set reportDir "$resultDir/reports"
   puts "\tWriting results to: $resultDir"
   puts "\tWriting reports to: $reportDir"

   ###########################################
   # Linking
   ###########################################
   if {$link} {
      command "file delete -force $resultDir"
      command "file mkdir $resultDir"
      command "file mkdir $reportDir"
   
      #Create in-memory project
      command "create_project -in_memory -part $part" "$resultDir/create_project.log"
   
      ###########################################
      # Define the top-level sources
      ###########################################
      foreach module $modules {
         set name [get_attribute module $module moduleName]
         if {[string match $name $top]} {
            set topFile [get_module_file $module]
         }
      }
      if {[info exists topFile]} {
         command "add_files $topFile"
      } else {
         set errMsg "\nERROR: No module found with with attribute \"moduleName\" equal to $top, which is defined as \"top\" for implementation $impl."
         error $errMsg
      }
   
      #### Read in IP Netlists 
      if {[llength $cores] > 0} {
         add_cores $cores
      }
      
      #### Read in XDC file
      if {[llength $implXDC] > 0} {
         add_xdc $implXDC
      } else {
         puts "\tWARNING: No XDC file specified for $impl"
      }
   
      ##############################################
      # Link the top-level design with black boxes 
      ##############################################
      set time [clock seconds]
      puts "\t#HD: Running link_design for $top \[[clock format $time -format {%a %b %d %H:%M:%S %Y}]\]"
      command "link_design -mode default -part $part -top $top" "$resultDir/${top}_link_design.log"
   
      ##############################################
      # Bring in OOC module check points
      ##############################################
      if {$hd && [llength $ooc_implementations] > 0} {
         get_ooc_results $ooc_implementations
      }

      if {$td} {
         #Turn phys_opt_design and route_design for TD run
         set phys  [set_attribute impl $impl phys  0]
         set route [set_attribute impl $impl route 0]

         command "puts \"\t#HD: Creating OOC constraints\""
         foreach ooc $ooc_implementations {
            #Set HD.PARTITION and create set_logic_* constraints
            set instName [get_attribute ooc $ooc inst]
            set hierInst [get_attribute ooc $ooc hierInst]
            command "set_property HD.PARTITION 1 \[get_cells $hierInst\]"
            set_partpin_range $hierInst
            create_set_logic $instName $hierInst $xdcDir
            create_ooc_clocks $instName $hierInst $xdcDir
         }
      }

      if {$ic} {
         if {![file exists $dcpDir]} {
            command "file mkdir $dcpDir"
         }   

         foreach partition $partitions {
            lassign $partition module cell state
            set moduleName [get_attribute module $module moduleName]
            set name [lindex [split $cell "/"] end]
            if {![string match $moduleName $top]} {
               if {[string match $state "implement"]} {
                  set partitionFile [get_module_file $module]
               } elseif {[string match $state "import"]} {
                  set partitionFile "$dcpDir/${name}_route_design.dcp"
               } else {
                  set errMsg "\nERROR: Unrecognized state $state for $cell. Supported values are implemenet or import."
                  error $errMsg
               }

               set fileSplit [split $partitionFile "."]
               set type [lindex $fileSplit end]
               if {[string match $type "dcp"]} {
                  puts "\tReading in checkpoint $partitionFile for $cell ($module)"
                  command "read_checkpoint -cell $cell $partitionFile -strict" "$resultDir/read_checkpoint_$name.log"
               } elseif {[string match $type "edf"] || [string match $type "edn"]} {
                  puts "\tUpdating design with $partitionFile for $cell ($module)"
                  command "update_design -cells $cell -from_file $partitionFile" "$resultDir/update_design_$name.log"
               } else {
                  set errMsg "\nERROR: Invalid file type \"$type\" for $partitionFile.\n"
                  error $errMsg
               }

               if {[string match $state "import"]} {
                  set time [clock seconds]
                  puts "\tLocking $cell \[[clock format $time -format {%a %b %d %H:%M:%S %Y}]\]"
                  command "lock_design -level routing $cell" "lock_design_$name.log"
               }

               ##Mark module with HD.PARTITION if being implemented
               if {[string match $state "implement"]} {
                  command "set_property HD.PARTITION 1 \[get_cells $cell]"
#                  command "set_property DONT_TOUCH 1 \[get_cells $cell]"
               }
            }
         }
      }

      ##############################################
      # Read in any linkXDC files 
      ##############################################
      if {[llength $linkXDC] > 0} {
         readXDC $linkXDC
      }

      if {$dcpLevel > 0} {
         command "write_checkpoint -force $resultDir/${top}_link_design.dcp" "$resultDir/temp.log"
      }
      puts "\t#HD: Completed link_design"
      puts "\t##########################\n"

      if {$verbose > 1} {
         command "report_utilization -file $reportDir/${top}_utilization_link_design.rpt" "$resultDir/temp.log"
      } 
      #Run methodology DRCs and cattch any Critical Warnings or Error (module ruledeck quiet)
      check_drc $top methodology_checks 1
      #Run timing DRCs and cattch any Critical Warnings or Error (module ruledeck quiet)
      check_drc $top timing_checks 1
   }
   
   ############################################################################################
   # Implementation steps: opt_design, place_design, phys_opt_design, route_design
   ############################################################################################
   set impl_start [clock seconds]
   if {$opt} {
      impl_step opt_design $top $opt_options $opt_directive ${opt.pre}
   }

   if {$place} {
      impl_step place_design $top $place_options $place_directive ${place.pre}

      #### If Top-Down, write out XDCs 
      if {$td} {
         puts "\n\tWriting instance level XDC files."
         foreach ooc $ooc_implementations {
            set instName [get_attribute ooc $ooc inst]
            set hierInst [get_attribute ooc $ooc hierInst]
            write_hd_xdc $instName $hierInst $xdcDir
         }
      }
   }

   if {$phys} {
      impl_step phys_opt_design $top $phys_options $phys_directive ${phys.pre}
   }

   if {$route} {
      impl_step route_design $top $route_options $route_directive ${route.pre}
 
      #Run report_timing_summary on final design
      command "report_timing_summary -file $reportDir/${top}_timing_summary.rpt" "$resultDir/temp.log"
   
      #Run a final DRC that catches any Critical Warnings (module ruledeck quiet)
      check_drc $top bitstream_checks ${drc.quiet}
   
   }
   
   foreach partition $partitions {
      lassign $partition module cell state
      set moduleName [get_attribute module $module moduleName]
      set name [lindex [split $cell "/"] end]
      if {![string match $moduleName $top]} {
         command "write_checkpoint -force -cell $cell $resultDir/${name}_route_design.dcp" "$resultDir/temp.log"
         command "file copy -force $resultDir/${name}_route_design.dcp $dcpDir"
      }
   }

   set impl_end [clock seconds]
   log_time final $impl_start $impl_end 
   log_data $impl $top

   if {$bitstream} {
      impl_step write_bitstream $top $bitstream_options none ${bitstream.pre} $bitstream_settings
   }
   
   command "close_project"
   command "\n"
}
