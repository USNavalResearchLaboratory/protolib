set ns_ [new Simulator]

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
   
puts "Creating Protean agents ..."   
# Create two Protean example agents and attach to nodes
set p1 [new Agent/ProtoExample]
set p2 [new Agent/ProtoExample]

$ns_ attach-agent $n1 $p1
$ns_ attach-agent $n2 $p2
    
puts "Starting simulation ..." 

$ns_ at 1.0 "$p1 startup listen 44"

$ns_ at 2.0 "$p2 startup connect 0/44"

$ns_ at 400.0 "$p2 shutdown"

$ns_ at 500.0 "puts {half way there ...}"

$ns_ at 1000.0 "finish $ns_ $f"

proc finish {ns_ f} {
    $ns_ flush-trace
	close $f
    $ns_ halt
    delete $ns_
    puts "Simulation complete."
}

$ns_ run

