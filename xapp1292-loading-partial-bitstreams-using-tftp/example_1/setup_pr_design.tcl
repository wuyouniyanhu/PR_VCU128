set tclParams [list hd.visual 1] 

#Define location for "Tcl" directory. Defaults to "./Tcl"
set tclHome "../common/non_project_pr_tcl"
if {[file exists $tclHome]} {
   set tclDir $tclHome
} elseif {[file exists "./Tcl"]} {
   set tclDir  "./Tcl"
} else {
   error "ERROR: No valid location found for required Tcl scripts. Set \$tclDir in design.tcl to a valid location."
}

####Input Directories
set srcDir     "../common/Sources"
set xdcDir     "../common/Sources/xdc"
# Used by the scripts but the directory doesn't exist or get created
set netlistDir "./Sourced/netlist"


set synthDir  "./Synth"
set implDir   "./Implement"
set dcpDir    "./Checkpoint"
set bitDir    "./Bitstreams"


####Report and DCP controls - values: 0-required min; 1-few extra; 2-all
set verbose      1
set dcpLevel     1

####Source required Tcl Procs
source $tclDir/design_utils.tcl
source $tclDir/synth_utils.tcl
source $tclDir/impl_utils.tcl
source $tclDir/hd_floorplan_utils.tcl


# =====================================================
# Top Definition
# =====================================================
set top "top"
set static "Static"
add_module $static
set_attribute module $static moduleName    $top
set_attribute module $static top_level     1

set_attribute module $static vlog          [list    \
                                             "../common/Sources/hdl/top/count_rp.v"   \
                                             "../common/Sources/hdl/top/shift_rp.v"   \
                                            ]
set_attribute module $static vhdl          [list                                  \
                                            ../common/Sources/hdl/top/top.vhd       work    \
                                            project/project.srcs/sources_1/bd/static_bd/hdl/static_bd.vhd work \
                                            project/project.srcs/sources_1/bd/static_bd/hdl/static_bd_wrapper.vhd work \
                                            ]


set_attribute module $static bd          [list                                                       \
                                            project/project.srcs/sources_1/bd/static_bd/static_bd.bd \
                                          ]

set_attribute module $static ip          [list                                                            \
                                              ./Sources/generated/ila_icap.srcs/sources_1/ip/ila_icap/ila_icap.xci \
                                         ]



set_attribute module $static synthXDC    [list $xdcDir/${top}.xdc]
set_attribute module $static synth        ${run.topSynth}

# =====================================================
# RP Module Definitions
# =====================================================
set module1 "shift"

set module1_variant1 "shift_right"
set variant $module1_variant1
add_module $variant
set_attribute module $variant moduleName   $module1
set_attribute module $variant vlog         [list ../common/Sources/hdl/$variant/$variant.v]
set_attribute module $variant synth        ${run.rmSynth}

set module1_variant2 "shift_left"
set variant $module1_variant2
add_module $variant
set_attribute module $variant moduleName   $module1
set_attribute module $variant vlog         [list ../common/Sources/hdl/$variant/$variant.v]
set_attribute module $variant synth        ${run.rmSynth}

set module1_inst "inst_shift"

# =====================================================
# RP Module Definitions
# =====================================================
set module2 "count"

set module2_variant1 "count_up"
set variant $module2_variant1
add_module $variant
set_attribute module $variant moduleName   $module2
set_attribute module $variant vlog         [list ../common/Sources/hdl/$variant/$variant.v]
set_attribute module $variant synth        ${run.rmSynth}

set module2_variant2 "count_down"
set variant $module2_variant2
add_module $variant
set_attribute module $variant moduleName   $module2
set_attribute module $variant vlog         [list ../common/Sources/hdl/$variant/$variant.v]
set_attribute module $variant synth        ${run.rmSynth}

set module2_inst "inst_count"

# ===========================================================================
# Configuration (Implementation) Definition - Replicate for each Config
# ===========================================================================
set config "Config_${module1_variant1}_${module2_variant1}" 

add_config $config
set_attribute config $config top                 $top
set_attribute config $config implXDC             [list $xdcDir/${top}.xdc]
set_attribute config $config impl                ${run.prImpl} 
set_attribute config $config settings            [list [list $static           $top          implement] \
                                                       [list $module1_variant1 $module1_inst implement] \
                                                       [list $module2_variant1 $module2_inst implement] \
                                                 ]
set_attribute config $config verify              ${run.prVerify} 
set_attribute config $config bitstream           ${run.writeBitstream} 
set_attribute config $config bitstream_settings  [list "BITSTREAM.STARTUP.STARTUPCLK      CCLK"    \
                                                       "BITSTREAM.CONFIG.EXTMASTERCCLK_EN DISABLE" \
                                                       "BITSTREAM.CONFIG.BPI_SYNC_MODE    DISABLE" \
                                                       "BITSTREAM.CONFIG.PERSIST          NO"      \
                                                       "BITSTREAM.GENERAL.COMPRESS        FALSE"   \
                                                  ]
set_attribute config $config bitstream_options "-bin_file"


# ===========================================================================
# Configuration (Implementation) Definition - Replicate for each Config
# ===========================================================================
set config "Config_${module1_variant2}_${module2_variant2}" 

add_config $config
set_attribute config $config top                 $top
set_attribute config $config implXDC             [list $xdcDir/${top}.xdc]
set_attribute config $config impl                ${run.prImpl} 
set_attribute config $config settings            [list [list $static           $top          import]    \
                                                       [list $module1_variant2 $module1_inst implement] \
                                                       [list $module2_variant2 $module2_inst implement] \
                                                 ]
set_attribute config $config verify              ${run.prVerify} 
set_attribute config $config bitstream           ${run.writeBitstream} 
set_attribute config $config bitstream_settings  [list "BITSTREAM.STARTUP.STARTUPCLK      CCLK"    \
                                                       "BITSTREAM.CONFIG.EXTMASTERCCLK_EN DISABLE" \
                                                       "BITSTREAM.CONFIG.BPI_SYNC_MODE    DISABLE" \
                                                       "BITSTREAM.CONFIG.PERSIST          NO"      \
                                                       "BITSTREAM.CONFIG.CONFIGFALLBACK   DISABLE" \
                                                       "BITSTREAM.GENERAL.COMPRESS        FALSE"   \
                                                  ]
set_attribute config $config bitstream_options "-bin_file"


