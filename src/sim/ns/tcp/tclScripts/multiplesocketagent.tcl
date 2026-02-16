# Create multicast enabled simulator instance
set ns_ [new Simulator]

set f [open /dev/stderr w]
$ns_ trace-all $f

# Create three nodes
set n1 [$ns_ node]
set n2 [$ns_ node]
set n3 [$ns_ node]
set n4 [$ns_ node]

# Put a link between them

$ns_ duplex-link $n1 $n3 64kB 100ms DropTail
$ns_ queue-limit $n1 $n3 100
$ns_ duplex-link-op $n1 $n3 queuePos 0.5
$ns_ duplex-link-op $n1 $n3 orient right

$ns_ duplex-link $n3 $n4 64kB 100ms DropTail
$ns_ queue-limit $n3 $n4 100
$ns_ duplex-link-op $n3 $n4 queuePos 0.5
$ns_ duplex-link-op $n3 $n4 orient right

$ns_ duplex-link $n3 $n2 64kB 100ms DropTail
$ns_ queue-limit $n3 $n2 100
$ns_ duplex-link-op $n3 $n2 queuePos 0.5
$ns_ duplex-link-op $n3 $n2 orient right

$ns_ duplex-link $n2 $n4 64kB 100ms DropTail
$ns_ queue-limit $n2 $n4 100
$ns_ duplex-link-op $n2 $n4 queuePos 0.5
$ns_ duplex-link-op $n2 $n4 orient right


#client
set tcp1 [new Agent/TCP/SocketAgent]

#server
set tcp2 [new Agent/TCP/ServerSocketAgent]

#client
set tcp3 [new Agent/TCP/SocketAgent]

# Choices - FULLTCP, RENOFULLTCP, SACKFULLTCP, TAHOEFULLTCP

$tcp1 setProtocol FULLTCP
$tcp2 setProtocol FULLTCP
$tcp3 setProtocol FULLTCP

$tcp1 attach-to-node $n1
$tcp2 attach-to-node $n2
$tcp3 attach-to-node $n3

$tcp1 set-var window_ 100
$tcp1 set-var fid_ 1
$tcp3 set-var window_ 100
$tcp3 set-var fid_ 1


$ns_ at 1.0 "$tcp2 listen"

$ns_ at 1.0 "$tcp1 tcp-connect 1 0"
$ns_ at 2.0 "$tcp3 tcp-connect 1 0"

$ns_ at 3.0 "$tcp1 send 10 helloFrom1"

$ns_ at 3.0 "$tcp3 send 10 helloFrom3"

$ns_ at 4.0 "$tcp1 send 15 helloAgainFrom1"

$ns_ at 5.0 "$tcp3 send 15 helloAgainFrom3"

$ns_ at 1000.0 "finish $ns_ $f"

proc finish {ns_ f} {
close $f
$ns_ halt
delete $ns_
}

$ns_ run
