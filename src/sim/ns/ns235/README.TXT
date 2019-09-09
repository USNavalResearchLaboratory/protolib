	(NOTE: The included "ns235-Makefile.in" can be used 
	 but expects NRL PROTOLIB, NORM, OLSR, and MGEN ns-2 
	 extensions to be used.)

QUICK INSTALL 
(using the supplied ns235-Makefile.in)

1) Download source code of the following NRL programs PROTOLIB,
   NORM, OLSR, and MGEN.
  
  a) Using svn:
    1. make current directory /ns-allinone-2.35/ns-2.35/
    2. type "svn checkout --username <username> https://pf.itd.nrl.navy.mil/svnroot/protolib/trunk protolib"
    3. type "svn checkout --username <username> https://pf.itd.nrl.navy.mil/svnroot/norm/trunk norm"
    3. type "svn checkout --username <username> https://pf.itd.nrl.navy.mil/svnroot/nrlolsr/trunk nrlolsr"
    3. type "svn checkout --username <username> https://pf.itd.nrl.navy.mil/svnroot/mgen/trunk mgen"
  b) Using cvs: (depreciated older code)
    1. make current directory /ns-allinone-2.35/ns-2.35/
    2. type "cvs -d :pserver:anonymous@pf.itd.nrl.navy.mil:/cvsroot/protolib co protolib"
    3. type "cvs -d :pserver:anonymous@pf.itd.nrl.navy.mil:/cvsroot/norm co norm"
    4. type "cvs -d :pserver:anonymous@pf.itd.nrl.navy.mil:/cvsroot/olsr co nrlolsr"
    5. type "cvs -d :pserver:anonymous@pf.itd.nrl.navy.mil:/cvsroot/mgen co mgen"
  c) Using tarballs:
    1. make current directory /ns-allinone-2.35/ns-2.35/
    2. download norm, nrlolsr, mgen (nightly build tar file use is encouraged)
    3. type "tar -xvzf <tarfilenamehere>.tgz
  d) Using symbolic links (Note: only works if you have already downloaded the nrl code)
    1. make current directory /ns-allinone-2.35/ns-2.35/
    2. type "ln -s <target-protolib-source-dir> protolib
    3. type "ln -s <target-norm-source-dir> norm
    4. type "ln -s <target-nrolsr-source-dir> nrlolsr
    5. type "ln -s <target-mgen-source-dir> mgen
    (Note: using this method will create ns specific .o files in the source trees which can
     cause issues if building these programs for other envirnments)

2) Copy the files within this directory to the appropriate places in the ns 2.35 code tree
  a) make current directory /ns-allinone-2.35/ns-2.35/
    (Note: you may want to make backups of the files being written over before performing the following commands)
  b) type "cp protolib/src/sim/ns/ns234/cmu-trace.h trace/"
  c) type "cp protolib/src/sim/ns/ns234/cmu-trace.cc trace/"
  d) type "cp protolib/src/sim/ns/ns234/packet.h common/"
  e) type "cp protolib/src/sim/ns/ns234/packet.cc common/"
  f) type "cp protolib/src/sim/ns/ns234/priqueue.cc queue/"
  g) type "cp protolib/src/sim/ns/ns234/ns-lib.tcl tcl/lib/"
  
3) Using the supplied makefile build your protolib enabled ns2.35 build
  a) Make current directory /ns-allinone-2.35/ns-2.35/
    (Note: you may want to make backups of the files being written over before performing the following commands)
  b) type "cp protolib/src/sim/ns/ns234/ns234-Makefile.in Makefile.in"
  c) type "./configure"
  d) type "make clean"
  e) type "make ns"

=========================
LONG INSTALL INSTRUCTIONS 
(provided mainly as hints for those building on top of a 
modified ns code tree in which the quick install did not work)

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
	protolib/ns/nsProtoSimAgent.o protolib/ns/nsRouteMgr.o \
    protolib/ns/nsProtoSimAgent.o protolib/common/protoSimAgent.o \
	protolib/common/protoSimSocket.o protolib/common/protoAddress.o \
	protolib/common/protoTimer.o protolib/common/protoExample.o \
	protolib/common/protoDebug.o protolib/common/protoTree.o \
	protolib/ns/nsRouteMgr.o protolib/common/protoRouteTable.o \
	protolib/common/protoBitmask.o protolib/ns/nsProtolibMK.o \
	protolib/common/protoTime.o

    and then add $(OBJ_PROTOLIB_CPP) to the list in the
    "OBJ" macro already defined in the ns "Makefile.in"

    Note: "nsProtoAgent.cpp" contains a starter ns agent
    which uses the PROTOLIB ProtocolTimer and UdpSocket
    classes.

5)  Add the the rule for .cpp files to ns-2 "Makefile.in":

    .cpp.o: @rm -f $@ $(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $*.cpp

    and add to the ns-2 Makefile.in "SRC" macro definition:

    $(OBJ_CPP:.o=.cpp)

6) Change your ns-2.35/common/packet.h file to include info on ProtolibMK:

    a) add:
   
       "#define HDR_PROTOLIBMK(p) (hdr_protolibmk::access(p))"
       near the top of the file with the other similar #defines.
       
    b)  Before the end of the statics for packet_t, change the 
        current PT_NTYPE line to 
    
        "static const packet_t PT_PROTOLIBMK=73;"

         and change the last entry to
       
         "PT_NTYPE = 74; // This MUST be the LAST one"

    c) add "name_[PT_PROTOLIBMK]= "ProtolibMK";" in the p_info() section

7) Change your ns-2.35/trace/cmu-trace.h file adding ProtolibMK hooks
   add "void format_protolibmk(Packet *p, int offset);" at the bottom with
   	the other "format_xxx" functions.

8) Change your ns-2.35/trace/cmu-trace.cc file adding ProtolibManetKernel hooks:

   a) add "#include <nsProtoManetKernel.h>" at the end of the #includes
   
   b) add empty function:
      "void CMUTrace::format_protolibmk(Packet *p, int offset) {return;}"
   
   c) add to the switch(ch->ptype()) (default one) statement:

   "case PT_PROTOLIBMK:
		format_protolibmk(p, offset);
		break;"

9) Change your ns-2.35/tcl/lib/ns-lib.tcl file:

   a) add to the "switch -exact $routingAgent" statement:
   
   "ProtolibMK {
		set ragent [$self create-protolibmk-agent $node]
  	}"
    
   b) add along with other Simulator create functions:

   "Simulator instproc create-protolibmk-agent {node} {
		# create a dummie wireless agent
		# it will foward packets up to protolib manet
		# and just act as a wedge into ns
		# used by protolib wireless manets
		set ragent [new Agent/ProtolibMK [$node node-addr]]
		$node set ragent_ $ragent
		return $ragent
   	}"

10) Last edit! Change ns-2.35/queue/priqueue.cc file, add 
   
   "case PT_PROTOLIBMK:"
   
   to the switch statment in the if(Prefer_Routing_Protocols) conditional (at top)


6)  Run "./configure" in the ns source directory to create
    a new Makefile and  then type "make ns" to rebuild ns.

    
Brian Adamson <mailto://brian.adamson@nrl.navy.mil> 
Justin Dean <mailto://justin.dean@nrl.navy.mil>
1/14/2014
