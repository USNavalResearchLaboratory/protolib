# Create multicast enabled simulator instance
set ns_ [new Simulator]

set f [open /dev/stderr w]
$ns_ trace-all $f

# Create four nodes
set n1 [$ns_ node]
set n2 [$ns_ node]

set n3 [$ns_ node]
set n4 [$ns_ node]

# Put a link between them
$ns_ duplex-link $n1 $n3 64kb 100ms DropTail
$ns_ queue-limit $n1 $n3 100
$ns_ duplex-link-op $n1 $n3 queuePos 0.5
$ns_ duplex-link-op $n1 $n3 orient right

$ns_ duplex-link $n3 $n4 80kb 100ms DropTail
$ns_ queue-limit $n3 $n4 100
$ns_ duplex-link-op $n3 $n4 queuePos 0.5
$ns_ duplex-link-op $n3 $n4 orient right

$ns_ duplex-link $n2 $n4 64kb 100ms DropTail
$ns_ queue-limit $n2 $n4 100
$ns_ duplex-link-op $n2 $n4 queuePos 0.5
$ns_ duplex-link-op $n2 $n4 orient right

set tcp1 [new Agent/TCP/SocketAgent]
set tcp2 [new Agent/TCP/ServerSocketAgent]

# Choices - FULLTCP, RENOFULLTCP, SACKFULLTCP, TAHOEFULLTCP

$tcp1 setProtocol FULLTCP
$tcp2 setProtocol FULLTCP

$tcp1 attach-to-node $n1
$tcp2 attach-to-node $n2

$tcp1 set-var window_ 100
$tcp1 set-var fid_ 1

$tcp2 listen

$ns_ at 1.0 "$tcp1 tcp-connect 1 0"
$ns_ at 2.0 "$tcp1 send 10 hellodude"

$ns_ at 999.0 "$tcp1 detach-from-node"
$ns_ at 999.0 "$tcp2 detach-from-node"

$ns_ at 1000.0 "finish $ns_ $f"

proc finish {ns_ f} {
close $f
$ns_ halt
delete $ns_
}

$ns_ run
