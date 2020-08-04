# Search for "TODO" to find parts of the script you might want/need to modify

# ==========================================================================
# This script has three stages:
#  1. Implement the design, but don't create the bitstreams
#  2. Program the PRC directly in the static netlist
#  3. Create all bitstreams
# ==========================================================================


# =====================================================
# Define Part, Package, Speedgrade 
# =====================================================
source ../common/set_part.tcl

# =====================================================
#  Setup Variables
# =====================================================

####flow control
set run.topSynth       1
set run.rmSynth        1
set run.prImpl         1
set run.prVerify       1
set run.writeBitstream 0

####Report and DCP controls - values: 0-required min; 1-few extra; 2-all
set verbose      1
set dcpLevel     1

source setup_pr_design.tcl



# ===========================================================================
# Task / flow portion
# ===========================================================================
# Build the designs
source $tclDir/run.tcl


