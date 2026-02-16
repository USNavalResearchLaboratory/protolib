
/**

@page mainpage_class_sum Summarized Class List by Category

This page contains a summarized listing of classes, please see the
<a href="annotated.html"><span>Class&nbsp;List</span></a>
 page for a full listing.


<table class='doctable' border='0' cellspacing='0' cellpadding='4'>
<tr><td>
@li @ref mainpage_core_io
@li @ref mainpage_pkt_parsing
</td><td>
@li @ref mainpage_data_structure
@li @ref mainpage_timer_classes
</td><td>
@li @ref mainpage_application
@li @ref mainpage_helper_classes
</td><td>
@li @ref mainpage_manet
</td></tr>
</table>

<hr>

@section mainpage_core_io I/O Classes

The following are the Protolib I/O Classes 

@li ProtoCap Interface class for raw MAC-layer packet capture. Platform 
implementations of this class vary.

@li PcapCap An implementation of the ProtoCap class using "libpcap" for
packet capture.

@li ProtoDetour Inbound/outbound packet _interception_ class. Platform implementations 
vary ... but generally leverages system firewall interfaces. A Win32 version based 
around NDIS intermediate driver is in progress.

@li ProtoChannel Base class for hooking into asynchronouse I/O (via Unix file 
descriptors or WIN32 HANDLEs).  Used as a base class for ProtoPipe, ProtoDetour,
ProtoCap, etc.

@li ProtoPipe Socket-like mechanism (with both datagram and stream support)
useful for interprocess communications (uses Unix domain sockets on Unix, other
stuff on Win32 & WinCE).

@li ProtoSocket Network socket container class that provides consistent interface 
for use of operating system (or simulation environment) transport sockets.

@li ProtoVif Base class providing virtual interfaces support.

<hr>

@section mainpage_pkt_parsing Packet Parsing

These are the classes relevant to packet parsing:

@li ProtoPkt Base class for a suite of network protocol packet/message 
building/parsing classes that provide methods for setting/getting protocol 
field values to/from a buffer.

@li ProtoPktARP Useful for building/parsing ARP packets.

@li ProtoPktAUTH IPv6 Authentication Header (AUTH) extension.

@li ProtoPktDPD IPv6 Simplified Multicast Forwarding Duplicate Packet
Detection (DPD) option.

@li ProtoPktESP IPv6 Encapsulating Security Protocol (ESP) header.

@li ProtoPktETH This class provides access to and control of ETHernet
header fields for the associated buffer space. 

@li ProtoPktFRAG Builds IPv6 FRAG extension.

@li ProtoPktIP 	Classes are provided for building/parsing IPv4 and IPv6 packets to/from 
a buffer space. 

@li ProtoPktRTP Useful for building/parsing Real-Time Protocol(RTP), RFC3550, 
messages.

@li ProtoPktUDP Parses UDP Packets

<hr>
@section mainpage_data_structure Data Structures

@li ProtoList This class provides a simple double linked-list class with a 
ProtoList::Item base class to use for deriving your own classes you wish to 
store in a ProtoList.  

@li ProtoGraph Base class for managing graph data structures.

@li ProtoRouteMgr Base class used to provide a common interface to system 
(or other external) router tables. Implementations for Linux, BSD (incl. MacOS), 
and Win32/WinCE are included. Implementations for other routing/forwarding 
daemons like Quagga or Xorp may be provided in the future.

@li ProtoRouteTable Class based on the ProtoTree Patricia tree to store 
routing table information. Uses the ProtoAddress class to store network 
routing addresses. It's a pretty dumbed-down routing table at the moment, 
but may be enhanced in the future. 

@li ProtoSpace Maintains a set of "nodes" in two dimensional space

@li ProtoSlidingMask This class provides space-efficient binary storage.

@li ProtoStack The ProtoStack class is like the ProtoList and similarly provides a
ProtoStackTemplate that can be used for deriving easier-to-
use variants for custom items sub-classed from the ProtoStack::Item.

@li ProtoTree Flexible implementation of a Patricia tree data structure. 
Includes a ProtoTree::Item which may be derived from or used as a container 
for whatever data structures and application may require.
<hr>
@section mainpage_timer_classes Timer Classes

@li ProtoTime Provides system time conversion routines.

@li ProtoTimer A generic timer class which will notify a ProtoTimer::Listener upon timeout.

@li ProtoTimerMgr 	This class manages ProtoTimer instances when they are "activated". 
The ProtoDispatcher derives from this to manage ProtoTimers for an application. 
(The ProtoSimAgent base class contains a ProtoTimerMgr to similarly manage timers for a 
simulation instance).
<hr>
@section mainpage_application Application Classes

@li ProtoApp Base class for implementing protolib-based command-line applications.

@li ProtoDispatcher This class provides a core around which Unix and Win32 applications 
using Protolib can be implemented.  

@li ProtoDispatcher::Controller Handles dispatching for the ProtoDispatcher

@li ProtoSimAgent Provides a base class for developing support for Protolib code in 
various network simulation environments (e.g. ns-2, OPNET, etc).  Contains a ProtoSocket
instance that acts as a liason to a corresponding simulation transport "Agent" instance.

<hr>
@section mainpage_helper_classes Helper Classes

@li ProtoAddress Network address container class with support for IPv4, IPv6, 
ETH, and "SIM" address types. Also includes functions for name/address resolution.

@li ProtoAddressList The "ProtoAddressList" helper class uses a Patrica tree (ProtoTree)
to keep a listing of addresses.  

@li ProtoBitmask Classes for managing and manipulating bitmasks to maintain 
binary state. A "sliding" bitmask class is provided that uses a circular 
buffer approach to maintain continuing, sequenced state.

@li ProtoDebug Set of routines useful for debugging.

@li ProtoXmlTree, ProtoXmlNode Classes for parsing and creating XML content.

<hr>
@section mainpage_manet Manet Classes

@li ManetMsg Class that implements the General packet format being developed by the 
IETF MANET working group (based on ProtoPkt).

@li ManetPkt Manet Pakcet parsing class.

@li ManetGraph Derived from ProtoGraph and uses ProtoAddress structures to provide 
a suitable graph structure for keeping and exploring multi-hop network state. 
Supports a notion of multiple interfaces per node, etc. 

*/

