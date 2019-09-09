#include "nsProtoSimAgent.h"
#include "ip.h"  	// for ns-2 hdr_ip def
#include "flags.h"  // for ns-2 hdr_flags def

// (TBD) should "NsProtoSimAgent" specify a different packet type?
NsProtoSimAgent::NsProtoSimAgent()
 : Agent(PT_UDP), scheduler(NULL)
{
    scheduler = &Scheduler::instance();
}

NsProtoSimAgent::~NsProtoSimAgent()
{
   
}

int NsProtoSimAgent::command(int argc, const char*const* argv)
{	
   if (argc >= 2)
   {
        if (!strcmp("startup", argv[1]))
        {
            if (OnStartup(argc-1, argv+1))
            {
                return TCL_OK;
            }
            else
            {
                fprintf(stderr, "NsProtoSimAgent::command() bad command\n");
                return TCL_ERROR;       
            }   
        }
        else if (!strcmp("shutdown", argv[1]))
        {
            OnShutdown();
            return TCL_OK;    
        }  
        else if (ProcessCommands(argc, argv))
        {
            return TCL_OK;   
        } 
   }
   return Agent::command(argc, argv);
}  // end NsProtoSimAgent::command()

bool NsProtoSimAgent::InvokeSystemCommand(const char* cmd)
{
    PLOG(PL_DETAIL, "NsProtoSimAgent: invoking command \"%s\"\n", cmd);

    Tcl& tcl = Tcl::instance();	
    Agent* agent = dynamic_cast<Agent*>(this);
	tcl.evalf("%s set node_", agent->name());
    const char* nodeName = tcl.result();
    
    tcl.evalf("%s %s", nodeName, cmd);

    return true;
}

ProtoSimAgent::SocketProxy* NsProtoSimAgent::OpenSocket(ProtoSocket& theSocket)
{
 	PLOG(PL_MAX, "NsProtoSimAgent::OpenSocket Entering \n");   

    // Create an appropriate ns-2 transport agent
    Tcl& tcl = Tcl::instance();
    if (ProtoSocket::UDP == theSocket.GetProtocol()) 
        tcl.evalf("eval new Agent/ProtoUdpSocket");
    else
        tcl.evalf("eval new Agent/ProtoSocket/TCP");
     
	const char* tclResult =  tcl.result();

	NSSocketProxy* socketAgent=NULL;
	
	if (tclResult!=NULL)
		socketAgent = dynamic_cast<NSSocketProxy*>(tcl.lookup(tclResult));
    
    if (socketAgent)
    {
        socketAgent->AttachSocket(theSocket);
        socket_proxy_list.Prepend(*socketAgent);
        return socketAgent;   
    }    
    else
    {
        fprintf(stderr, "NsProtoSimAgent::OpenSocket() error creating socket agent\n");
        return NULL;
    }
 	PLOG(PL_MAX, "NsProtoSimAgent::OpenSocket Exiting \n");   
}  // end NSProtoSimAgent::OpenSocket()

void NsProtoSimAgent::CloseSocket(ProtoSocket& theSocket)
{
 	PLOG(PL_MAX, "NsProtoSimAgent::CloseSocket Entering \n");   

    ASSERT(theSocket.IsOpen());

	PLOG(PL_DETAIL, "NsProtoSimAgent : Closing Socket \n");

    NSSocketProxy* socketAgent = static_cast<NSSocketProxy*>(theSocket.GetHandle());

	// Let the socket decide how it is to be closed using an over-ridden socket-specific implementation
	// see UDP or TCP implementation
	 
	socketAgent->Close();
	
    socket_proxy_list.Remove(*socketAgent);
    delete socketAgent;
 	PLOG(PL_MAX, "NsProtoSimAgent::CloseSocket Exiting \n");   
	
}  // end NSProtoSimAgent::OpenSocket()

NsProtoSimAgent::NSSocketProxy::NSSocketProxy()
 : ecn_capable(false), ecn_status(false), 
   mcast_ttl(255), mcast_loopback(false), 
   recv_data(NULL), recv_data_len(0), recv_data_offset(0)
{    
}
 
NsProtoSimAgent::NSSocketProxy::~NSSocketProxy()
{
}

bool NsProtoSimAgent::NSSocketProxy::Bind(UINT16& thePort)
{
	PLOG(PL_MAX, "NsProtoSimAgent::NSSocketProxy::Bind Entering \n");   

    ASSERT(proto_socket && proto_socket->IsOpen());
	 
    Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
    Tcl& tcl = Tcl::instance();	
    tcl.evalf("%s set node_", simAgent->name());
    const char* nodeName = tcl.result();
	
	if (nodeName==NULL)
		return false;
    
    Agent* socketProxyAgent = dynamic_cast<Agent*>(this);
    
    if (thePort > 0)
        tcl.evalf("%s attach %s %hu", nodeName, socketProxyAgent->name(), thePort);
    else
        tcl.evalf("%s attach %s", nodeName, socketProxyAgent->name());
    
    thePort = (UINT16)socketProxyAgent->port();
     
	PLOG(PL_MAX, "NsProtoSimAgent::NSSocketProxy::Bind Exiting \n");   

    return true;       
}  // end NsProtoSimAgent::NSSocketProxy::Bind() 


// New default close method for NSSocketProxy 
bool NsProtoSimAgent::NSSocketProxy::Close() {
	PLOG(PL_MAX, "NsProtoSimAgent::NSSocketProxy::Close Entering \n");   

    Tcl& tcl = Tcl::instance();	
	tcl.evalf("Simulator instance");
    
    char simName[32];
    strncpy(simName, tcl.result(), 32);
  
    Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
    ASSERT(NULL != simAgent);
    tcl.evalf("%s set node_", simAgent->name());
    const char* nodeName = tcl.result();
 
	if (nodeName==NULL)
		return false;

    Agent* socketProxyAgent = dynamic_cast<Agent*>(this); 	

	PLOG(PL_MAX, "NsProtoSimAgent : Running: %s detach-agent %s %s\n", simName, nodeName, socketProxyAgent->name());
    tcl.evalf("%s detach-agent %s %s", simName, nodeName, socketProxyAgent->name());
	
	PLOG(PL_MAX, "NsProtoSimAgent::NSSocketProxy::Close Exiting \n");   
	return true;
}  // end NsProtoSimAgent::NSSocketProxy::Close()

  
bool NsProtoSimAgent::NSSocketProxy::SetNodeColor(const char* color)
{
	PLOG(PL_DETAIL, "NsProtoSimAgent::NSSocketProxy::SetNodeColor Entering \n");   
    Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
    Tcl& tcl = Tcl::instance();	
    tcl.evalf("%s set node_", simAgent->name());
    const char* nodeName = tcl.result();

    tcl.evalf("%s color %s", nodeName,  color);
    return true;
}  // end NsProtoSimAgent:: SetNodeColor()

bool NsProtoSimAgent::NSSocketProxy::RecvFrom(char* buffer, 
                                              unsigned int& numBytes, 
                                              ProtoAddress& srcAddr)
{
	PLOG(PL_MAX, "NsProtoSimAgent::NSSocketProxy::RecvFrom Entering \n");   
    if (recv_data)
    {
	    PLOG(PL_DETAIL, "NSSocketProxy::RecvFrom: bytes requested = %i\n", numBytes);
		PLOG(PL_DETAIL, "NSSocketProxy::RecvFrom: data in read buffer = %i, current readpoint = %i\n", recv_data_len, recv_data_offset);
		PLOG(PL_DETAIL, "NSSocketProxy::RecvFrom: Sender Address: %s, port %i\n", src_addr.GetHostString(), src_addr.GetPort()); 
						
		unsigned long remainingBuffer = (unsigned long)recv_data_len-recv_data_offset;
		unsigned long readpoint= (unsigned long )recv_data+recv_data_offset;
		srcAddr = src_addr;
		
        if (numBytes >= remainingBuffer)
        {
            numBytes = remainingBuffer;
            memcpy(buffer, (char *)readpoint, numBytes);
            recv_data = NULL;
            recv_data_len = 0;
            recv_data_offset=0;
            return true;
        }
        else // requested read is smaller than buffer, so read only part of the input data
        {						
            memcpy(buffer, (char *)readpoint, numBytes);
			recv_data_offset+=numBytes;
            return true;    
        }        
    }
    else // EOF
    {
        numBytes = -1; // for compatibility with Java
        srcAddr = src_addr;
        recv_data = NULL;
        recv_data_len = 0;
        recv_data_offset=0;
        return false;       
    }   
	PLOG(PL_MAX, "NsProtoSimAgent::NSSocketProxy::RecvFrom Exiting \n");   
}  // end NsProtoSimAgent::NSSocketProxy::RecvFrom()

bool NsProtoSimAgent::NSSocketProxy::JoinGroup(const ProtoAddress& groupAddr)
{
    if (groupAddr.IsBroadcast()) return true; // bcast membership implicit
    Agent* socketAgent = dynamic_cast<Agent*>(this);
    ASSERT(socketAgent);
    
    ProtoSocket* s = GetSocket();
    if (!s->IsBound()) s->Bind(0);  // can only join on bound sockets (TBD) return false instead???
    
    Tcl& tcl = Tcl::instance();	
    tcl.evalf("%s set node_", socketAgent->name());
	const char* nodeName = tcl.result();
    tcl.evalf("%s join-group %s %d", nodeName, 
              socketAgent->name(), groupAddr.SimGetAddress());	
    return true;
}  // end NsProtoSimAgent::NSSocketProxy::JoinGroup()

bool NsProtoSimAgent::NSSocketProxy::LeaveGroup(const ProtoAddress& groupAddr)
{
    if (groupAddr.IsBroadcast()) return true; // bcast membership implicit
    Tcl& tcl = Tcl::instance();	
    Agent* socketAgent = dynamic_cast<Agent*>(this);
    ASSERT(socketAgent);
	tcl.evalf("%s set node_", socketAgent->name());
	const char* nodeName = tcl.result();
    tcl.evalf("%s leave-group %s %d", nodeName, 
              socketAgent->name(), groupAddr.SimGetAddress());	
	return true;
}  // end NsProtoSimAgent::NSSocketProxy::LeaveGroup(()

/////////////////////////////////////////////////////////////////////////
// NsProtoSimAgent::UdpSocketAgent implementation
//
static class ProtoSocketUdpAgentClass : public TclClass {
public:
	ProtoSocketUdpAgentClass() : TclClass("Agent/ProtoUdpSocket") {}
	TclObject* create(int, const char*const*) {
		return (new NsProtoSimAgent::UdpSocketAgent());
	}
} class_proto_socket_udp_agent;

NsProtoSimAgent::UdpSocketAgent::UdpSocketAgent()
 : Agent(PT_UDP)
{
}

bool NsProtoSimAgent::UdpSocketAgent::SendTo(const char*         buffer, 
                                             unsigned int&       buflen,
                                             const ProtoAddress& dstAddr)
{
	PLOG(PL_MAX, "NsProtoSimAgent::UdpSocketAgent::SendTo Entering \n");   
	PLOG(PL_DETAIL, "NsProtoSimAgent::UdpSocketAgent::SendTo called: %s length %u\n", buffer, buflen);   

    // Allocate _and_ init packet
    Packet* p = allocpkt(buflen);
    if (p)
    {
        // Set packet destination addr/port
        hdr_ip* iph = hdr_ip::access(p);
        if (ecn_capable)
            hdr_flags::access(p)->ect() = 1;  // ecn enabled 
        iph->daddr() = dstAddr.SimGetAddress();
        iph->dport() = dstAddr.GetPort();   
        if (dstAddr.IsMulticast())
            iph->ttl() = mcast_ttl;
        else
            iph->ttl() = 255;
        
        PLOG(PL_DETAIL, "NsProtoSimAgent::UdpSocketAgent::sending to %u port %u\n", iph->daddr(), iph->dport());   

        hdr_cmn* ch = hdr_cmn::access(p);  
        // We could "fake" other packet types here if we liked
        ch->ptype() = PT_UDP;
        // Copy data to be sent into packet
        memcpy(p->accessdata(), buffer, buflen);
        // Set packet length field to UDP payload size for now
        // (TBD) Should we add UDP/IP header overhead?
        ch->size() = buflen;
        send(p, 0);
        return true;
    }
    else
    {
       PLOG(PL_ERROR, "NsProtoSimAgent::UdpSocketAgent::SendTo() new Packet error");
       return false;
    }
	PLOG(PL_MAX, "NsProtoSimAgent::UdpSocketAgent::SendTo Exiting \n");   
}  // end NsProtoSimAgent::UdpSocketAgent::SendTo()


void NsProtoSimAgent::UdpSocketAgent::recv(Packet* pkt, Handler* /*h*/)
{
    PLOG(PL_MAX, "NsProtoSimAgent::UdpSocketAgent::recv() Entering\n");   

    if (pkt->userdata() && (PACKET_DATA == pkt->userdata()->type()))
    {
        PacketData* pktData = (PacketData*)pkt->userdata();
        char* recvData = (char*)pktData->data();
        unsigned int recvLen = pktData->size();
        ns_addr_t srcAddr = HDR_IP(pkt)->src(); 
        ns_addr_t dstAddr = HDR_IP(pkt)->dst();

        PLOG(PL_DETAIL, "Source address %u, my address %u, destination %u ...\n", srcAddr.addr_, addr(), dstAddr.addr_);
        /*  Adamson - commented this out as we _do_ want to allow "locally" arriving data if
            mcast_loopback is enabled! 
        if (addr() == srcAddr.addr_) {
           PLOG(PL_WARN, "Data arrived locally on node %u, ignoring ...\n", addr());
           PLOG(PL_WARN, "Details: my port %u, senders port %u\n", port(), srcAddr.port_);
           return;
           }
        */
		
        if (0 != hdr_flags::access(pkt)->ect())
        {
            //TRACE("received ecn capable packet status = %d (%d)...\n", 
            //        hdr_flags::access(pkt)->ecnecho(), hdr_flags::access(pkt)->ce());
            ecn_status = (0 != hdr_flags::access(pkt)->ce());
        }
        else
            ecn_status = false;  
                
		if (dstAddr.port_!=port()) {
			PLOG(PL_DEBUG, "My socket port is %u but destination port is %u, dropping packet ...\n", port(), dstAddr.port_);
			return;
		}
		
        bool isUnicast = (0 == (dstAddr.addr_ & 0x80000000));
        if ((addr() != srcAddr.addr_) || 
            (port() != srcAddr.port_) ||
            isUnicast || mcast_loopback)
        {
            recv_data = recvData;
            recv_data_len = recvLen;
			recv_data_offset=0; // reset the buffer
			PLOG(PL_DETAIL, "NsProtoSimAgent::UdpSocketAgent::recv recv_data_len = %i\n", recv_data_len);

            src_addr.SimSetAddress(srcAddr.addr_);
            src_addr.SetPort(srcAddr.port_);
			PLOG(PL_DETAIL, "NsProtoSimAgent::UdpSocketAgent::recv event being sent to socket ... \n");
            if (proto_socket)
                proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);  
        }
    }
    else 
    {
        PLOG(PL_WARN, "NsProtoSimAgent::UdpSocketAgent::recv() warning: "
                        "received packet with no payload\n");   
    }
	Packet::free(pkt);

	PLOG(PL_MAX, "NsProtoSimAgent::UdpSocketAgent::recv() Exiting \n");   
}  // end NsProtoSimAgent::UdpSocketAgent::recv()


