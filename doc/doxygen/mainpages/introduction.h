
/**

@page mainpage_introduction Introduction

@li @ref page_introduction_whatis
@li @ref page_introduction_key_classes
@li @ref page_introduction_downloads
@li @ref page_introduction_acknowledgements
@li @ref page_introduction_authorization

<hr>

@section page_introduction_whatis What is Protolib?

The Protean Protocol Prototyping Library (ProtoLib) is a cross-platform C/C++ 
library that allows applications to be built while supporting a variety of 
platforms including Linux, Windows, WinCE/PocketPC, MacOS, FreeBSD, Solaris, 
etc as well as the simulation environments of NS2 and Opnet.

Protolib is not so much a library as it is a toolkit. "Protokit" is a term we 
have gravitated towards. In either case, the goal of the Protolib is to provide 
a set of simple, cross-platform C++ classes that allow development of network 
protocols and applications that can run on different platforms and in network 
simulation environments. While Protolib provides an overall framework for 
developing working protocol implementations, applications, and simulation modules, 
the individual classes are designed for use as stand-alone components when possible. 
Although Protolib is principally for research purposes, the code has been 
constructed to provide robust, inefficient performance and adaptability to real 
applications. In some cases, the code consists of data structures, etc useful in 
protocol implementations and, in other cases, provides common, cross-platform 
interfaces to system services and functions (e.g., sockets, timers, routing 
tables, etc).

Currently Protolib supports most Unix platforms (including MacOS X) and WIN32 
platforms. The most recent version also supports building Protolib-based code 
for the ns-2 and OPNET simulation environments. Some code is also provided to 
allow code based on Protolib to be used in a wxWidgets application. The wxWidgets 
project is a cross-platform graphical user interface (GUI) toolkit for creating 
applications using the C++ programming language. We have used wxWidgets for 
providing graphical user interfaces for some of our prototype network applications.
A java native interface for ProtoPipe is included as well.

<hr>
@section page_introduction_key_classes Some of the classes available

This table provides a listing and explanation of many of the classes contained in 
Protolib. Work is in progress to create and embed Doxygen-based documentation 
within the Protolib source code tree.

@li ProtoAddress:  	Network address container class with support for IPv4, IPv6, 
ETH, and "SIM" address types. Also includes functions for name/address resolution.

@li ProtoSocket: 	Network socket container class that provides consistent interface 
for use of operating system (or simulation environment) transport sockets. Provides 
support for synchronous notification to ProtoSocket::Listeners. The ProtoSocket 
class may be used stand-alone, or with other classes described below. A ProtoSocket 
may be instantiated as either a UDP or TCP socket.

@li ProtoTimer  	This is a generic timer class which will notify a 
ProtoTimer::Listener upon timeout.

@li ProtoTimerMgr 	This class manages ProtoTimer instances when they are 
"activated". The ProtoDispatcher(see below) derives from this to manage 
ProtoTimers for an application. (The ProtoSimAgent base class contains a ProtoTimerMgr 
to similarly manage timers for a simulation instance).

@li ProtoTree 	Flexible implementation of a Patricia tree data structure. 
Includes a ProtoTree::Item which may be derived from or used as a container for 
whatever data structures and application may require.

@li ProtoRouteTable 	Class based on the ProtoTree Patricia tree to store routing 
table information. Uses the ProtoAddress class to store network routing addresses. It's a 
pretty dumbed-down routing table at the moment, but may be enhanced in the future. 

@li ProtoRouteMgr 	Base class used to provide a common interface to system (or other 
external) router tables. Implementations for Linux, BSD (incl. MacOS), and Win32/WinCE 
are included. Implementations for other routing/forwarding daemons like Quagga or 
Xorp may be provided in the future.

@li ProtoPkt 	Base class for a suite of network protocol packet/message building/parsing 
classes that provide methods for setting/getting protocol field values to/from a buffer.

@li ProtoPktIP 	Classes are provided for building/parsing IPv4 and IPv6 packets to/from 
a buffer space. A ProtoPktUDP class is also provided.

@li ProtoPktETH This class that provides access to and control of ETHernet
header fields for the associated buffer space. 

@li ProtoPktARP Useful for building/parsing ARP packets.

@li ProtoPktRTP Useful for building/parsing Real-Time Protocol (RTP), RFC3550, messages.

@li ProtoGraph Base class for managing graph data structures.

@li ManetMsg Class that implements the General packet format being developed by the 
IETF MANET working group (based on ProtoPkt).

@li ManetGraph 	Derived from ProtoGraph and uses ProtoAddress structures to provide 
a suitable graph structure for keeping and exploring multi-hop network state. Supports 
a notion of multiple interfaces per node, etc.

@li ProtoBitmask 	Classes for managing and manipulating bitmasks to maintain binary 
state. A "sliding" bitmask class is provided that uses a circular buffer approach to 
maintain continuing, sequenced state.

@li ProtoXml 	Classes for parsing and creating XML content.

@li ProtoPipe 	Socket-like mechanism (with both datagram and stream support) useful 
for interprocess communications (uses Unix domain sockets on Unix, other stuff on 
Win32 & WinCE)

@li ProtoCap 	Interface class for raw MAC-layer packet capture. Platform implementations 
of this class vary including a "libpcap" based implementation.

@li ProtoVif Base class providing interface support for virtual interfaces.

@li ProtoDetour 	Inbound/outbound packet _interception_ class. Platform implementations 
vary ... but generally leverages system firewall interfaces. A Win32 version based 
around NDIS intermediate driver is in progress.

@li ProtoChannel 	Base class for hooking into asynchronous I/O (via Unix file descriptors 
or Win32 HANDLEs). Used as base class for ProtoPipe, ProtoDetour, ProtoCap, etc 
(ProtoSocket is currently an exception here because of distinction of SOCKETs vs. 
HANDLEs on Win32 platforms - This may be revisited in the future).

@li ProtoDispatcher 	This class provides a core around which Unix and Win32 
applications using Protolib can be implemented. It's "Run()" method provides a 
"main loop" which uses the "select()" system call on Unix and the similar 
"MsgWaitForMultipleObjectsEx()" system call on Win32. It is planned to eventually 
provide some built-in support for threading in the future (e.g. the 
ProtoDispatcher::Run() method might execute in a thread, dispatching events to a 
parent thread).

@li ProtoApp 	Provides a base class for implementing Protolib-based command-line 
applications. Note that "ProtoApp" and "ProtoSimAgent" are designed such that subclasses 
can be derived from either to reuse the same code in either a real-world applications or 
as an "agent" (entity) within a network simulation environment (e.g. ns-2, OPNET). Also 
note a built-in "background" command is included for Win32 to launch the app without a 
terminal window.

@li wxProtoApp 	Base class that can be used to create applications using Protolib 
components and the wxWidgets GUI toolkit.

@li ProtoSimAgent 	Base class for simulation agent derivations. Currently an ns-2 
agent base class is derived from this, but it is possible that other simulation 
environments (e.g. OPNET, Qualnet) might be supported in a similar fashion.

@li NsProtoSimAgent 	Simulation agent base class for creating ns-2 instantiations 
of Protolib-based network protocols and applications.

@li OpnetProtoSimProcess 	Simulation process base class for creating OPNET 
instantiations of Protolib-based network protocols and applications.

@li ProtoExample 	Example class which derives either from ProtoApp or NsProtoSimAgent, 
depending upon compile-time macro definitions. It provides equivalent functionality 
in either the simulation environment or as a real-world command-line application. It 
demonstrates the use/operation of ProtoSocket based UDP transmission/reception, a 
ProtoTimer, and an example ProtoSocket-based TCP client-server exchange. (NOTE: 
Protolib TCP operation is not yet supported in the ns-2 simulation environment. This 
will completed in the near future. The plan is to extend ns-2 TCP agents to support 
actual transfer of user data to support this.)

<hr>
@section page_introduction_downloads Downloads

Protolib source code is available at: http://downloads.pf.itd.nrl.navy.mil/protolib/. 
However, it is important to note that the tarballs (other than the "nightly build") are 
not updated very often and the appropriate version of the Protolib source tree is 
generally packaged as part of source code distributions for stable releases of 
other NRL/Protean products.

If you are interested in more information about ProtoLib, please contact 
protolib@pf.itd.nrl.navy.mil. There is also a ProtoLib Users mailing list. This mailing 
list is monitored by the ProtoLib developer(s). 

<hr>
@section page_introduction_acknowledgements Acknowledgements

Protolib was developed by the Networks and Communications Systems Branch
of the Naval Research Library by:

@li Brian Adamson
@li Joe Macker

<hr>
@section page_introduction_authorization Authorization to Use and Distribute

<em>
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson and Joe Macker of the Naval Research 
 *       Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 

</em>

*/

