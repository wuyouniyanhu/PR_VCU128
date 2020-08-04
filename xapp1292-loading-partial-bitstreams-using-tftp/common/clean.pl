#!/usr/bin/perl -w

# Clean the work area back to a completely fresh state.  It will remove DCPs and bitstreams
#
use strict;
use Getopt::Long;
my $version = "1.0";
my $pgm = $0;

my @files_to_remove = qw( *.log
                          vivado*.jou
                          vivado*.str
                          project
                          .sim
                          .hw
                          asisseq.v
                          export_sim_options.cfg
                          fsm_encoding.os
                          usage_statistics_webtalk.xml
                          usage_statistics_webtalk.html
                          Bitstreams/*
                          Partials/*
                          Implement/*
                          Synth/*
                          Checkpoint/*
                          hd_visual
                          Sources/generated/*
                          .Xil
                          .cache
                          static_bd_wrapper.hdf
                          static_bd_wrapper.hwdef
                          SDK/*
                          sw/webtalk
                          sw/.Xil
                          sw/RemoteSystemsTempFiles
                  );


foreach (@files_to_remove){
  my $cmd = "rm -rf $_";
  print $cmd ."\n";
  system($cmd);
}

# Now clean the sw directory

my @files_to_remove = qw( *.o
                          *.elf
                          *.log
                          *.bit
                       );

foreach (@files_to_remove){
  my $cmd = "find sw -name \'$_\' -delete";
  print $cmd ."\n";
  system($cmd);
}
