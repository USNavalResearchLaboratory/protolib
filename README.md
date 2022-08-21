       Protean Protocol Prototyping Library (PROTOLIB)

![CMake build](https://github.com/USNavalResearchLaboratory/protolib/workflows/CMake%20build/badge.svg)

OVERVIEW
 
Protolib is not so much a library as it is a toolkit.  The goal of the Protolib
is to provide a set of simple, cross-platform C++ classes that allow development
of network protocols and applications that can run on different platforms and in
network simulation environments.  Although Protolib is principally for research
purposes, the code has been constructed to provide robust, efficient performance
and adaptability to real applications.

Currently Protolib supports most Unix platforms (including MacOS X) and WIN32
platforms.  The most recent version also supports building Protolib-based code
for the ns-2 simulation environment. The OPNET simulation tool has also been
supported in the past and could be once again with a small amount of effort.

CLASSES:

ProtoAddress:    Network address container class with support
                 for IPv4, IPv6, and "SIM" address types.  Also
                 includes functions for name/address
                 resolution.

ProtoSocket:     Network socket container class that provides
                 consistent interface for use of operating
                 system (or simulation environment) transport
                 sockets. Provides support for asynchronous
                 notification to ProtoSocket::Listeners.  The
                 ProtoSocket class may be used stand-alone, or
                 with other classes described below.  A
                 ProtoSocket may be instantiated as either a
                 UDP or TCP socket.

ProtoTimer:      This is a generic timer class which will
                 notify a ProtoTimer::Listener upon timeout.

ProtoTimerMgr:   This class manages ProtoTimer instances when
                 they are "activated".  The ProtoDispatcher
                 (below) derives from this to manage
                 ProtoTimers for an application.  (The
                 ProtoSimAgent base class contains a
                 ProtoTimerMgr to similarly manage timers for a
                 simulation instance).
                 
ProtoList:       Basic linked list data structure, but is a
                 ProtoIterable that enables the list to be
                 updated within ProtoList::Iterator loops, etc.
                 A template class ProtoListTemplate is provided
                 to make it easy to create lists of user-defined
                 ProtoList::Item subclass types.

ProtoTree:       Flexible implementation of a Patricia tree
                 data structure.  Includes a ProtoTree::Item
                 which may be derived from or used as a
                 container for  whatever data structures and
                 application may require. A template class 
                 ProtoListTemplate is provided to make it easy 
                 to create lists of user-defined ProtoList::Item 
                 subclass types.  Also ProtoSortedTree is provided
                 as a threaded Patricia tree that allows multiple
                 entries for the same key value with controls to 
                 allow sorting and closest match search even for
                 items indexed with numeric (int, double, etc)
                 vslues.
                 
ProtoQueue:      A slightly heavier weight derivation from the basic
                 ProtoList and ProtoTree data structures.  Unlike
                 ProtoList::Item or ProtoTree::Item instances that
                 are limited to exclusive inclusion in a single
                 list or tree, ProtoQueue:Item instances may be 
                 members of multiple ProtoQueues.  Queue variants include
                 ProtoSimpleQueue - linked list usable for FIFO, stack, etc
                 ProtoIndexedQueue - ProtoTree of queue items indexed by a key
                 ProtoSortedQueue - ProtoSortedTree of queue items.
                 Again, template classes are provided these to make it easy
                 to create and manage user-derived ProtoQueue::Item types.

ProtoRouteTable: Class based on the ProtoTree Patricia tree to
                 store routing table information. Uses the
                 ProtoAddress class to store network routing
                 addresses.  It's a pretty dumbed-down routing
                 table at the moment, but may be enhanced in
                 the future.  Example use of the ProtoTree.

ProtoRouteMgr:   Base class for providing  a consistent
                 interface to manage operating system (or
                 other) routing engines.
                 
ProtoPipe:       Socket-like mechanism (with both datagram and
                 stream support) useful for interprocess
                 communications (uses Unix domain sockets on
                 Unix, other stuff on Win32 & WinCE)
                 
ProtoCap:        Interface class for raw MAC-layer packet capture.
                 Platform implementations of this class vary
                 including a "pcap" based implementation.
                 
ProtoDetour:     Inbound/outbound packet _interception_ class.
                 Platform implementations vary ... works with
                 firewall stuff.  Win32 version based around
                 NDIS intermediate driver in progress.

ProtoDispatcher: This class provides a core around which Unix
                 and Win32 applications using Protolib can be
                 implemented.  It's "Run()" method provides a
                 "main loop" which uses the "select()" system
                 call on Unix and the similar
                 "MsgWaitForMultipleObjectsEx()" system call on
                 Win32.  It is planned to eventually provide
                 some built-in support for threading in the
                 future (e.g. the ProtoDispatcher::Run() method
                 might execute in a thread, dispatching events
                 to a parent thread).

ProtoApp:        Provides a base class for implementing
                 Protolib-based command-line applications. Note
                 that "ProtoApp" and "ProtoSimAgent" are
                 designed such that subclasses can be derived
                 from either to reuse the same code in either a
                 real-world applications or as an "agent"
                 (entity) within a network simulation
                 environment (e.g. ns-2, OPNET).  A "background"
                 command is included for Win32 to launch the
                 app without a terminal window.

ProtoSimAgent:   Base class for simulation agent derivations. 
                 Currently an ns-2 agent base class is derived
                 from this, but it is possible that other
                 simulation environments  (e.g. OPNET, Qualnet)
                 might be supported in a similar fashion.

NsProtoSimAgent: Simulation agent base class for creating ns-2
                 instantiations of Protolib-based network
                 protocols and applications.

ProtoExample:    Example class which derives either from
                 ProtoApp or NsProtoSimAgent, depending upon
                 compile-time macro definitions.  It provides
                 equivalent functionality in either the
                 simulation environment or as a real-world
                 command-line application.  It demonstrates the
                 use/operation of ProtoSocket based UDP
                 transmission/reception, a ProtoTimer, and an
                 example ProtoSocket-based TCP client-server
                 exchange.   (NOTE: TCP operation is not yet
                 supported in the simulation environment.  This
                 will completed in coming months.  I plan to
                 extend ns-2 TCP agents to support actual
                 transfer of user data to support this.)

NsProtoTCPSocketAgent: TCP implementation of the NSSocketProxy 
		 class for providing TCP support within Protolib.  
		 This class provides a hub for interfacing with 
		 the underlying TCP toolkit for providing access 
		 to the TCP support.  NsProtoTCPSocketAgent 
	         essentially auto-detects which sockets it should 
		 be (i.e. a client or a server) and then instantiates 
		 the underlying implementations in order to provide 
		 that behaviour, which are either a TCPSocketAgent
		 or a TCPServerSocketAgent:

TCPSocketAgent:  The TCP socket agent implementation that can be 
		 used for clients and that can be used as the socket 
		 responsible for the server-side of the  connection 
		 after a call to accept.

TCPServerSocketAgent: The TCP server implementation that only 
		 implements part of the TCP protocol (by listening 
		 for the initial SYN connection requests from clients).  
		 The server simply acts as a broker for assigning 
		 TCPSocketAgents when clients request connections, 
		 either by creating the sockets itself or by sending an 
		 ACCEPT event to the application in order for it to 
		 create the socket and call the accept function 
		 manually.  The latter approach is the norm.

OTHER:

The Protolib code also includes some simple, general purpose debugging
routines which can output to "stderr" or optionally log to a specified file. 
See "protoDebug.h" for details.

There are also some supporting classes not described here and some work to be
done. Also, more complete documentation, including Doxygen-based code documentation 
and a "Developer's Guide" with examples needs to be provided. 




