###############################################################
###   Main flow - Do Not Edit
###############################################################
set runLog "run"
set commandLog "command"
set criticalLog "critical"
set logs [list $runLog $commandLog $criticalLog]
foreach log $logs {
   if {[file exists ${log}.log]} {
      file copy -force $log.log ${log}_prev.log
   }
}
set rfh [open "$runLog.log" w]
set cfh [open "$commandLog.log" w]
set wfh [open "$criticalLog.log" w]

#### Set Tcl Params
if {[info exists tclParams] && [llength $tclParams] > 0} {
   set_parameters $tclParams
}

#### Run Synthesis on any modules requiring synthesis
if {[llength $modules] > 0} {
   foreach module $modules {
      if {[get_attribute module $module synth]} {
       puts $rfh "\n#HD: Running synthesis for block $module"
       command "puts \"#HD: Running synthesis for block $module\""
       synth $module
       puts "#HD: Synthesis of module $module complete\n"
    }
  }
}

#### Run Top-Down implementation before OOC
if {[llength $implementations] > 0} {
   foreach impl $implementations {
      if {[get_attribute impl $impl impl] && [get_attribute impl $impl td.impl]} {
         #Override directives if directive file is specified
         if {[info exists useDirectives]} {
            puts "#HD: Overriding directives for implementation $impl"
            set_directives impl $impl
         }
         puts $rfh "#HD: Running implementation $impl"
         command "puts \"#HD: Running implementation $impl\""
         impl $impl
         puts "#HD: Implementation $impl complete\n"
      }
   }
}
#### Run OOC Implementations
if {[llength $ooc_implementations] > 0} {
   foreach ooc_impl $ooc_implementations {
      if {[get_attribute ooc $ooc_impl impl]} {
         #Override directives if directive file is specified
         if {[info exists useDirectives]} {
            puts "#HD: Overriding directives for implementation $ooc_impl"
            set_directives ooc $ooc_impl
         }
         puts $rfh "#HD: Running ooc implementation $ooc_impl (OUT-OF-CONTEXT)"
         command "puts \"#HD: Running OOC implementation $ooc_impl (OUT-OF-CONTEXT)\""
         ooc_impl $ooc_impl
         puts "#HD: OOC implementation of $ooc_impl complete\n"
      }
   }
}

#### Run PR configurations
if {[llength $configurations] > 0} {
   sort_configurations
   foreach config $configurations {
      if {[get_attribute config $config impl]} {
         #Override directives if directive file is specified
         if {[info exists useDirectives]} {
            puts "#HD: Overriding directives for configuration $config"
            set_directives config $config
         }
         puts $rfh "#HD: Running PR implementation $config (Partial Reconfiguration)" 
         command "puts \"#HD: Running PR implementation $config (Partial Reconfiguration)\"" 
         pr_impl $config
         puts "#HD: PR implementation of $config complete\n"
      }
   }
}

#### Run Assembly and Flat implementations
if {[llength $implementations] > 0} {
   foreach impl $implementations {
      if {[get_attribute impl $impl impl] && ![get_attribute impl $impl td.impl]} {
         #Override directives if directive file is specified
         if {[info exists useDirectives]} {
            puts "#HD: Overriding directives for implementation $impl"
            set_directives impl $impl
         }
         puts $rfh "#HD: Running implementation $impl"
         command "puts \"#HD: Running implementation $impl\""
         impl $impl
         puts "#HD: Implementation $impl complete\n"
      }
   }
}

#### Run PR verify 
if {[llength $configurations] > 1} {
   puts $rfh "#HD: Running pr_verify on all configurations([llength $configurations])" 
   command "puts \"#HD: Running pr_verify on all configurations([llength $configurations])\""
   verify_configs $configurations
}

#### Genearte PR bitstreams 
if {[llength $configurations] > 0} {
   puts $rfh "#HD: Running write_bitstream on all configurations([llength $configurations])" 
   command "puts \"#HD: Running write_bitstream on all configurations([llength $configurations])\""
   generate_pr_bitstreams $configurations
}

close $rfh
close $cfh
close $wfh
