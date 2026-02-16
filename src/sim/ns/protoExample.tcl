# Protean example agent example

# Create multicast enabled simulator instance
set ns_ [new Simulator -multicast on]
$ns_ multicast

# Open a trace file
set f [open protoExample.tr w]
$ns_ trace-all $f

# Create two nodes
set n1 [$ns_ node]
set n2 [$ns_ node]

# Put a link between them
$ns_ duplex-link $n1 $n2 64kb 100ms DropTail
$ns_ queue-limit $n1 $n2 100
$ns_ duplex-link-op $n1 $n2 queuePos 0.5
$ns_ duplex-link-op $n1 $n2 orient right

# Configure multicast routing for topology
set mproto DM
set mrthandle [$ns_ mrtproto $mproto  {}]
 if {$mrthandle != ""} {
     $mrthandle set_c_rp [list $n1]
}

# 5) Allocate a multicast address to use
set group [Node allocaddr]
   
puts "Creating Protean agents ..."   
# Create two Protean example agents and attach to nodes
set p1 [new Agent/ProtoExample]
$ns_ attach-agent $n1 $p1

set p2 [new Agent/ProtoExample]
$ns_ attach-agent $n2 $p2
    
# Have one Protean agent transmit to the other
puts "Starting simulation ..." 

# Start example sender and receiver
$ns_ at 0.0 "$p2 recv $group/5000"
$ns_ at 0.0 "$p1 recv $group/5000"

$ns_ at 0.0 "$p1 send $group/5000"

#$ns_ at 0.0 "$p1 startup send [$n2 node-addr]/5000"

$ns_ at 50.0 "puts {half way there ...}"

$ns_ at 99.0 "$p2 shutdown"
$ns_ at 99.0 "$p1 shutdown"

# Stop
$ns_ at 99.0 "$p1 stop"
$ns_ at 99.0 "$p2 stop"
$ns_ at 10.0 "finish $ns_ $f"

proc finish {ns_ f} {
    $ns_ flush-trace
	close $f
    $ns_ halt
    delete $ns_
    puts "Simulation complete."
}

$ns_ run

