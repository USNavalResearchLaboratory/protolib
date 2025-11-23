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

set p1 [new Agent/TCP/TCPSocketExample]
set p2 [new Agent/TCP/TCPSocketExample]

$ns_ attach-agent $n1 $p1
$ns_ attach-agent $n2 $p2

$p1 setClientConnections 1
$p2 setServerConnections 1

$ns_ at 1.0 "$p2 createServer"
$ns_ at 2.0 "$p1 createClients"
$ns_ at 3.0 "$p1 setConnectTo 1"
$ns_ at 4.0 "$p1 connectClients"
$ns_ at 7.0 "$p1 sendData"

$ns_ at 500.0 "$p2 close"

$ns_ at 1000.0 "finish $ns_ $f"

proc finish {ns_ f} {
close $f
$ns_ halt
delete $ns_
}

$ns_ run
