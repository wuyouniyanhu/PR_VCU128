# This script copies the required files to the TFTP server running at "server_ip_addr".
#  !!!!
#    TODO: Change the server_ip_addr to point at the machine running your TFTP server. 
#  !!!!

set server_ip_addr 149.199.131.172
set tftp_dest_path "example_1"

puts "======================================================================="
puts "Copying design files to $tftp_dest_path on TFTP server running at      "
puts "$server_ip_addr.                                                    "
puts "If there are problems with this, please ensure that the TFTP server is "
puts "running at $server_ip_addr and that it can accept file writes.         "
puts "Also check that it is not blocked by your firewall                     " 
puts "======================================================================="

set files ""
lappend files {*}[glob -directory Partials *.bin]

foreach file $files {
    set dest ${tftp_dest_path}/[file tail $file]
    puts "  Transferring $file to ${dest}"
    puts "  tftp $server_ip_addr -c mode binary put $file $dest"
    if { [catch { exec tftp $server_ip_addr -m binary -c put $file $dest} msg]} {
        puts "ERROR:  tftp transfer has failed"
        puts "  SRC: $file"
        puts "  DST: $dest"
        puts "Error message: $msg"
        puts "Aborting script.  Please copy the following files to the TFTP server manually"
        foreach file $files {
            set dest [file tail $file]
            puts "   $file to ${dest}"
        }
        return -1
    }
}


