###############################################################
# Source scripts need for implementation
###############################################################
if {[info exists tclDir]} {
   source $tclDir/ooc_impl.tcl
   source $tclDir/pr_impl.tcl
   source $tclDir/impl.tcl
   source $tclDir/step.tcl
   source $tclDir/log.tcl
}
###############################################################
# Find netlist for specified module
###############################################################
proc get_module_file { module } {
   global synthDir
   global netlistDir
   
   set moduleName [get_attribute module $module moduleName]
   set synthDCP   [get_attribute module $module synthCheckpoint]
   set searchFiles [list $synthDCP \
                         $synthDir/$module/${moduleName}_synth.dcp  \
                         $netlistDir/$module/${moduleName}.edf      \
                         $netlistDir/$module/${moduleName}.edn      \
                         $netlistDir/$module/${moduleName}.ngc      \
                   ]
   set moduleFile ""
   foreach file $searchFiles {
      if {[file exists $file]} {
         set moduleFile $file
         break
      }
   } 
   if {![llength $moduleFile]} {
      set errMsg "\nERROR: No synthesis netlist or checkpoint file found for $module."
      append errMsg "Searched directories:"
      foreach file $searchFiles {
         append errMsg "$file\n"
      }
      error $errMsg
   }
   return $moduleFile
}

###############################################################
# Read in all implemented OOC modules
###############################################################
proc get_ooc_results { implementations } {
   global dcpLevel
   upvar resultDir resultDir

   foreach ooc $implementations {
      set instName       [get_attribute ooc $ooc inst]
      set hierInst       [get_attribute ooc $ooc hierInst]
      set readCheckpoint [get_attribute ooc $ooc implCheckpoint]
      set preservation   [get_attribute ooc $ooc preservation]
      set hd.isolated    [get_attribute ooc $ooc hd.isolated]

      if {![file exists $readCheckpoint]} {
      set errMsg "\nERROR: Specified OOC Checkpoint $readCheckpoint does not exist."
      error $errMsg
      }
      set time [clock seconds]
      puts "\tReading in checkpoint $readCheckpoint for $instName"
      if {${hd.isolated}} {
         command "set_property HD.ISOLATED 1 \[get_cells $hierInst]"
      } else {
         command "set_property HD.PARTITION 1 \[get_cells $hierInst]"
      }
      command "read_checkpoint -cell $hierInst $readCheckpoint -strict" "$resultDir/read_checkpoint_${instName}.log"
   
      if {$dcpLevel > 1} {
         command "write_checkpoint -force $resultDir/${instName}_link_design.dcp" "$resultDir/temp.log"
      }
   
      if {[string match $preservation "logical"] || [string match $preservation "placement"] || [string match $preservation "routing"]} {
         set time [clock seconds]
         puts "\tLocking $hierInst \[[clock format $time -format {%a %b %d %H:%M:%S %Y}]\]"
         command "lock_design -level $preservation $hierInst" "$resultDir/lock_design_$instName.log"
      } elseif {[string match $preservation "none"] || [string match $preservation ""] } {
         puts "\tSkipping lock_design for $hierInst"
      } else {
         set errMsg "\nERROR: Unknown value \"$preservation\" specified for lock_design for cell $hierInst."
         error $errMsg
      }
   }
}

###############################################################
# Verify all configurations 
###############################################################
proc generate_pr_bitstreams { configs } {
   global dcpDir bitDir implDir rfh 

   #command "file delete -force $bitDir"
   if {![file exists $bitDir]} {
      command "file mkdir $bitDir"
   }

   foreach config $configs {
      set top         [get_attribute config $config top]
      set settings    [get_attribute config $config settings]
      set bitstream   [get_attribute config $config bitstream]
      set bitOptions  [get_attribute config $config bitstream_options]
      set bitSettings [get_attribute config $config bitstream_settings]
      if {$bitstream} {
         puts $rfh "\tRunning write_bitstream on $config"
         puts "\tRunning write_bitstream on $config"
         set logFile "$bitDir/write_bitstream_${config}.log"
         foreach setting $settings {
            lassign $setting name cell state
            if {[string match $cell $top]} {
               set configFile "$implDir/$config/${top}_route_design.dcp"
               if {[file exists $configFile]} {
                  command "open_checkpoint $configFile" "$bitDir/open_checkpoint_$config.log"
                  foreach setting $bitSettings {
                     puts "\tSetting property $setting"
                     command "set_property $setting \[current_design]"
                  }
                  command "write_bitstream -force $bitOptions $bitDir/$config" "$bitDir/$config.log"
               } else {
                  puts $rfh "\tERROR: The route_design DCP $configFile was not found for $config"
                  puts "\tERROR: The route_design DCP $configFile was not found for $config"
                  continue
               }
   
               #For the initial configuration, generate blanking bit files
               if {[string match $state "implement"]} {
                  puts $rfh "\tGenerating blanking bitstream from config $config"
                  puts "\tGenerating blanking bitstream from config $config"
                  foreach rm $settings {
                     lassign $rm rm_name rm_cell rm_state 
                     if {![string match $rm_cell $top]} {
                        command "update_design -cell $rm_cell -black_box" "$bitDir/update_design_$rm_name.log"
                     }
                  }
                  command "write_bitstream -force $bitOptions $bitDir/blanking" "$bitDir/blanking.log"
               }
            }
         }
         command "close_project" "$bitDir/temp.log"
      } else {
         puts "\tSkipping write_bitstream for Configuration $config with attribute bitstream set to $bitstream"
      }
   }
}

###############################################################
# Verify all configurations 
###############################################################
proc verify_configs { configs } {
   global implDir rfh

   #Compare Initial Configuration to all others
   set initialConfig [lindex $configs 0]
   set initialConfigTop [get_attribute config $initialConfig top]
   set initialConfigFile $implDir/$initialConfig/${initialConfigTop}_route_design.dcp

   set numConfigs [llength $configs]
   set additionalConfigs ""
   set additionalConfigFiles ""
   for {set i 1} {$i < $numConfigs} {incr i} {
      set config [lindex $configs $i]
      set verify [get_attribute config $config verify]
      if {$verify} {
         lappend additionalConfigs $config
         set configTop [get_attribute config $config top]
         set configFile $implDir/$config/${configTop}_route_design.dcp
         lappend additionalConfigFiles $configFile
      } else {
         puts "\tSkipping pr_verify for Configuration $config with attribute verify set to $verify"
      }
   }
   
   if {[llength $additionalConfigFiles]} {
      set msg "\tRunning pr_verify between initial config $initialConfig and additional configurations $additionalConfigs"
      puts $rfh "$msg"
      puts "$msg"
      set logFile "pr_verify_results.log"
      command "pr_verify -full_check -initial $initialConfigFile -additional \"$additionalConfigFiles\"" $logFile
      #Parse log file for errors or successful results
      set lfh [open $logFile r]
      set log_data [read $lfh]
      close $lfh
      set log_lines [split $log_data "\n" ]
      foreach line $log_lines {
         if {[string match "*Vivado 12-3253*" $line] || [string match "*ERROR:*" $line]} {
            puts $rfh "$line"
            puts "$line"
         }
      }
   }
   flush $rfh
}

###############################################################
# Add all XDC files in list, and mark as OOC if applicable
###############################################################
proc add_xdc { xdc } {
   puts "\tAdding XDC files"
   #Flatten list if nested lists exist
   set files [join [join $xdc]]
   foreach file $files {
      if {[file exists $file]} {
         command "add_files $file"
         set file_split [split $file "/"]
         set fileName [lindex $file_split end]
         if {[string match "*ooc*" $fileName]} {
            command "set_property USED_IN {implementation out_of_context} \[get_files $file\]"
         } 
         if {[string match "*late*" $fileName]} {
            command "set_property PROCESSING_ORDER late \[get_files $file\]"
         }
      } else {
         set errMsg "\nERROR: Could not find specified XDC: $file" 
         error $errMsg 
      }
   }
}

###############################################################
# A proc to read in XDC files post link_design 
###############################################################
proc readXDC { xdc } {
   upvar resultDir resultDir

   puts "\tAdding XDC files"
   #Flatten list if nested lists exist
   set files [join [join $xdc]]
   foreach file $files {
      if {[file exists $file]} {
         command "read_xdc $file" "$resultDir/read_xdc.log"
      } else {
         set errMsg "\nERROR: Could not find specified XDC: $file" 
         error $errMsg 
      }
   }
}

###############################################################
# Add all IP netlists in list 
###############################################################
proc add_cores { cores } {
   puts "\tAdding core files"
   #Flatten list if nested lists exist
   set files [join [join $cores]]
   foreach file $files {
      if {[string length $file] > 0} { 
         if {![file exists $file]} {
            #command "add_files $file"
            set errMsg "\nERROR: Could not find specified IP netlist: $file" 
            error $errMsg
         }
      }
   }
   command "add_files $files"
}

#==============================================================
# TCL proc for running DRC on post-route_design to catch 
# Critical Warnings. These will be errors in write_bitstream. 
# Catches unroutes, antennas, etc. 
#==============================================================
proc check_drc { module {ruleDeck all} {quiet 0} } {
   upvar reportDir reportDir

   if {[info exists reportDir]==0} {
      set reportDir "."
   }
   puts "\tRunning report_drc with ruledeck $ruleDeck.\n\tResults saved to $reportDir/${module}_drc_$ruleDeck.rpt" 
   command "report_drc -ruledeck $ruleDeck -name $module -file $reportDir/${module}_drc_$ruleDeck.rpt" "$reportDir/temp.log"
   set Advisories   [get_drc_violations -quiet -name $module -filter {SEVERITY=~"Advisory"}]
   set Warnings     [get_drc_violations -quiet -name $module -filter {SEVERITY=~"Warning"}]
   set CritWarnings [get_drc_violations -quiet -name $module -filter {SEVERITY=~"Critical Warning"}]
   set Errors       [get_drc_violations -quiet -name $module -filter {SEVERITY=~"Error"}]
   puts "\tAdvisories: [llength $Advisories]; Warnings: [llength $Warnings]; Critical Warnings: [llength $CritWarnings]; Errors: [llength $Errors];"

   if {[llength $Errors]} {
      if {!$quiet} {
         set errMsg "\nERROR: DRC found [llength $Errors] errors ($Errors)."
      } else {
         puts "\tCritical Warning: DRC found [llength $Errors] errors ($Errors)."
      }
      foreach error $Errors {
         puts "\n\t${error}: [get_property DESCRIPTION [get_drc_violations -name $module $error]]"
      }
      #Stop the script for Errors, unless user specifies quiet as true
      if {!$quiet} {
         error $errMsg
      }
   }

   if {[llength $CritWarnings]} {
      if {!$quiet} {
         set errMsg "\nERROR: DRC found [llength $CritWarnings] Critical Warnings ($CritWarnings)."
      } else {
         puts "\tCritical Warning: DRC found [llength $CritWarnings] Critical Warnings ($CritWarnings)."
      }
      foreach cw $CritWarnings {
         puts "\n\t${cw}: [get_property DESCRIPTION [get_drc_violations -name $module $cw]]"
      }
      #Stop the script for Critcal Warnings, unless user specifies quiet as true
      if {!$quiet} {
         error $errMsg
      }
   }
}

#==============================================================
# TCL proc for Checking if a cell's pins have PartPins.
# Not all pins should have PartPins (clocks, etc) so filter out 
#==============================================================
proc get_bad_pins { cell } {
   set noPP_pins [get_pins -of [get_cells $cell] -filter "HD.ASSIGNED_PPLOCS!~*INT* && HD.ASSIGNED_PPLOCS!~*INT*"]
   set io_pins [get_pins -of [get_nets -of [get_cells -hier -filter LIB_CELL=~*BUF*]] -filter PARENT_CELL==$cell]
   set count 0
   set clock_pins ""
   set bad_pins ""
   foreach pin $noPP_pins {
      set clock [get_clocks -quiet -of [get_pins $pin]]
      if { [lsearch -exact $io_pins $pin]!="-1" } {
  #       puts "Found match: $pin"
      } else {
         if { $clock!=""} {
  #          puts "Found clock: $pin"
            lappend clock_pins $pin
         } else {
  #          puts "No match found: $pin"
            lappend bad_pins $pin
         }
      }
   }
   puts "[join $bad_pins "\n"]"
   puts "\nTotal Number of pins without PP: [llength $noPP_pins]"
   puts "Total Number of pins connected to buffers: [llength $io_pins]"
   puts "Total Number of clock pins: [llength $clock_pins]"
   puts "Total Number of \"bad\" pins: [llength $bad_pins]"
}

#==============================================================
# TCL proc for Checking if a cell's ports have PartPins.
# Not all pins should have PartPins (clocks, etc) so filter out 
#==============================================================
proc get_bad_ports { } {
   set noPP_ports [get_ports -filter "HD.PARTPIN_LOCS!~*INT* && HD.ASSIGNED_PPLOCS!~*INT*"]
   set io_ports [get_ports -of [get_nets -of [get_cells -hier -filter LIB_CELL=~*BUF*]]]
   set count 0
   set clock_ports ""
   set bad_ports ""
   foreach port $noPP_ports {
      set clock [get_clocks -quiet -of [get_ports $port]]
      if { [lsearch -exact $io_ports $port]!="-1" } {
   #      puts "Found match: $port"
      } else {
         if { $clock!=""} {
   #         puts "Found clock: $port"
             lappend clock_ports $port
         } else {
   #         puts "No match found: $port"
            lappend bad_ports $port
         }
      }
   }
   puts "[join $bad_ports "\n"]"
   puts "\nTotal Number of ports without PP: [llength $noPP_ports]"
   puts "Total Number of ports connected to buffers: [llength $io_ports]"
   puts "Total Number of clock ports: [llength $clock_ports]"
   puts "Total Number of \"bad\" ports: [llength $bad_ports]"
}

#==============================================================
# TCL proc for inserting a proxy flop (FDCE) connection on 
# blackboxes. 
# - Allows for blackbox support in implementation. 
# - Marks the cell as HD.PARTITION to prevent optimization
# - Attempts to connect the proxy to the correct clock domain,
#   but connects it to the fist clock in the list if it can't
#==============================================================
proc insert_proxy_flop { cell } {

   if {[get_property IS_BLACKBOX [get_cells $cell]] == 0} {
      puts "ERROR: Specified cell $cell is not a blackbox.\nThe Tcl proc insert_proxy_flop can only be run on a cell that is a black box. Specify a black box cell, or run \'update_design -cell $cell -black_box\' prior to running this command."
      return 1
   }

   set all_in_pins [lsort [get_pins -of [get_cells $cell] -filter DIRECTION==IN]]
   set all_out_pins [lsort [get_pins -of [get_cells $cell] -filter DIRECTION==OUT]]

   #### Get a list of all clock pins (driver is a BUFG, BUFR, BUFH, etc)
   foreach inpin $all_in_pins {
      set driver [get_cells -of [get_pins -leaf -of [get_nets -of [get_pins $inpin]] -filter DIRECTION==OUT]]
      if {[string match -nocase "BUF*" [get_property -quiet LIB_CELL [get_cells $driver]]]} {
         lappend clocks $inpin 
      } else {
         lappend in_pins $inpin
      }
   }
   foreach clock $clocks {
      puts "Creating clock net \"$clock\""
      create_net $clock
      connect_net -net $clock -objects "$clock"
   }

   ####Process input pins, minus those driven by BUFG
   foreach inpin $in_pins {
      create_cell -reference FDCE ${inpin}_PROXY
      create_net $inpin
      connect_net -net $inpin -objects "$inpin ${inpin}_PROXY/D"
      set endpoints [get_cells -quiet -of [all_fanin -quiet -startpoints_only -flat [get_pins $inpin]]]
      set foundClock 0
      foreach endpoint $endpoints {
         #If the element is sequential 
         if {[get_property IS_SEQUENTIAL [get_cells $endpoint]]} {
            #puts "Found sequential driver \"$endpoint\" for pin $inpin"
            set driver_clock [get_nets -quiet [get_nets -quiet -segments -of [get_pins -of [get_cells $endpoint] -filter IS_CLOCK]] -filter NAME=~$cell/*]
            if {[llength $driver_clock] > 0 && [lsearch $clocks [lindex $driver_clock 0]] >= 0} {
               #puts "\tFound clock connections: $driver_clock"
               connect_net -net [lindex $driver_clock 0] -objects "${inpin}_PROXY/C"
               set foundClock 1
               break
            }
         } 
      }
      if {$foundClock == 0} {
         #Connect the inserted flop to the first clock in the list if the above fails 
         puts "No clock connection found for $inpin. Connecting proxy flop to clock [lindex $clocks 0]"
         connect_net -net [lindex $clocks 0] -objects "${inpin}_PROXY/C"
      }
   }

   ####Process output pins
   foreach outpin $all_out_pins {
      create_cell -reference FDCE ${outpin}_PROXY
      create_net $outpin
      connect_net -net $outpin -objects "$outpin ${outpin}_PROXY/Q"
      set endpoints [get_cells -quiet -of [all_fanin -quiet -startpoints_only -flat [get_pins $inpin]]]
      set foundClock 0
      foreach endpoint $endpoints {
         if {[get_property IS_SEQUENTIAL [get_cells $endpoint]]} {
            #puts "Found sequential load \"$endpoint\" for pin $outpin"
            set load_clock [get_nets -quiet [get_nets -quiet -segments -of [get_pins -of [get_cells $endpoint] -filter IS_CLOCK]] -filter NAME=~$cell/*]
            if {[llength $load_clock] > 0 && [lsearch $clocks [lindex $load_clock 0]] >= 0} {
               #puts "\tFound clock connections: $load_clock"
               connect_net -net [lindex $load_clock 0] -objects "${outpin}_PROXY/C"
               set foundClock 1
               break
            }
         }
      }
      if {$foundClock == 0} {
         #Connect the inserted flop to the first clock in the list if the above fails 
         puts "No clock connection found for $inpin. Connecting proxy flop to clock [lindex $clocks 0]"
         connect_net -net [lindex $clocks 0] -objects "${outpin}_PROXY/C"
      }
   }
   set_property HD.PARTITION 1 [get_cells $cell]
}

#==============================================================
# TCL proc for print out rule information for given ruledecks 
# Use get_drc_ruledecks to list valid ruledecks
#==============================================================
proc printRuleDecks { {decks ""} } {
   if {[llength $decks]} {
      set rules [get_drc_checks -of [get_drc_ruledecks $decks]]
      foreach rule $rules {
         set name [get_property NAME [get_drc_checks $rule]]
         set description [get_property DESCRIPTION [get_drc_checks $rule]]
         set severity [get_property SEVERITY [get_drc_checks $rule]]
         puts "\t${name}(${severity}): ${description}"
      }
   } else {
      puts "Rule Decks:\n\t[join [get_drc_ruledecks] "\n\t"]"
   }
}

#==============================================================
# TCL proc for print out rule information for given rules
#==============================================================
proc printRules { rules } {
   foreach rule $rules {
      set name [get_property NAME [get_drc_checks $rule]]
      set description [get_property DESCRIPTION [get_drc_checks $rule]]
      set severity [get_property SEVERITY [get_drc_checks $rule]]
      puts "\t${name}(${severity}): $description"
   }
}
