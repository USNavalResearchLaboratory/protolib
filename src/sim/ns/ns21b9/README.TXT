(NOTE: These instructions need to be revised for the
       Protolib version 2.x code.  The included
       "ns229-Makefile.in" can be used but expects NRL
       MDP, NORM, and MGEN ns-2 extensions to also be
       used.  The "patch-ns.sh" script is what I use
       to create some symbolic links and put the Makefile.in
       in place)


To use PROTOLIB with ns, you will need to at least modify
the ns "Makefile.in" to build the PROTOLIB code into ns. 
To do this, use the following steps:


1)  Make a link to the PROTOLIB source directory in the ns
    source directory.  (I use "protolib" for the link  name
    in the steps below). 

2)  Provide paths to the PROTOLIB include files by setting

    PROTOLIB_INCLUDES = -Iprotolib/common -Iprotolib/ns

    and adding $(PROTOLIB_INCLUDES) to the "INCLUDES" macro
    already defined in the  ns "Makefile.in" 

3)  Define compile-time CFLAGS needed for the PROTOLIB code
    by setting

    PROTOLIB_FLAGS = -DUNIX -DNS2 -DPROTO_DEBUG -DHAVE_ASSERT

    and adding $(PROTOLIB_FLAGS) to the "CFLAGS" macro
    already defined in the ns "Makefile.in"

4)  Add the list of PROTOLIB object files to get compiled
    and linked during the ns build.  For UDP and TCP support, set

OBJ_PROTOLIB_CPP = \
	$(PROTOLIB)/common/protoTimer.o \
	$(PROTOLIB)/ns/nsProtoSimAgent.o \
	$(PROTOLIB)/common/protoSimAgent.o \
	$(PROTOLIB)/common/protoDebug.o \
	$(PROTOLIB)/common/protoAddress.o \
	$(PROTOLIB)/common/protoSimSocket.o \
	$(PROTOLIB)/ns/nsRouteMgr.o \
	$(PROTOLIB)/common/protoRouteMgr.o \
	$(PROTOLIB)/common/protoRouteTable.o \
	$(PROTOLIB)/common/protoTree.o \
	$(PROTOLIB)/common/protoExample.o \
	$(PROTOLIB)/common/protoBitmask.o\
	$(PROTOLIB)/ns/tcp/TCPData.o \
	$(PROTOLIB)/ns/tcp/TCPDataFIFO.o \
	$(PROTOLIB)/ns/tcp/TCPSocketApp.o \
	$(PROTOLIB)/ns/tcp/TCPEvent.o \
	$(PROTOLIB)/ns/tcp/TCPSocketAgent.o \
	$(PROTOLIB)/ns/tcp/TCPServerSocketAgent.o \
	$(PROTOLIB)/ns/tcp/TCPSocketFactory.o \
	$(PROTOLIB)/ns/tcp/TCPSocketExample.o \
	$(PROTOLIB)/ns/nsTCPProtoSocketAgent.o 

    and then add $(OBJ_PROTOLIB_CPP) to the list in the
    "OBJ" macro already defined in the ns "Makefile.in"

    Note: "nsProtoAgent.cpp" contains a starter ns agent
    which uses the PROTOLIB ProtocolTimer and UdpSocket
    classes.

5)  Add the the rule for .cpp files to ns-2 "Makefile.in":

    .cpp.o: @rm -f $@ $(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $*.cpp

    and add to the ns-2 Makefile.in "SRC" macro definition:

    $(OBJ_CPP:.o=.cpp)


6)  Run "./configure" in the ns source directory to create
    a new Makefile and  then type "make ns" to rebuild ns.

    
Brian Adamson <mailto://adamson@itd.nrl.navy.mil> 
31 October 2005
