#include "opnetProtoSimProcess.h"
#include	"mgen.h"

    
extern "C"
{
#include    "oms_pr.h"
#include    "udp_api.h"
#include    "ip_addr_v4.h"
#include    "ip_rte_v4.h"
}

OpnetProtoSimProcess::OpnetProtoSimProcess() 
{
      
}

OpnetProtoSimProcess::~OpnetProtoSimProcess(void)
{
  
}

ProtoSimAgent::SocketProxy* OpnetProtoSimProcess::OpenSocket(ProtoSocket& theSocket)
{
#ifdef OP_DEBUG1
printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::OpenSocket() with udp_id = %d\n",
	udp_process_id);
#endif
UdpSocketProxy* udpProxy;
TcpSocketProxy* tcpProxy;
if (theSocket.GetProtocol() == theSocket.TCP)
	{
		tcpProxy = new TcpSocketProxy(this);
		if (tcpProxy)
			{
        	tcpProxy->AttachSocket(theSocket);
			socket_proxy_list.Prepend(*tcpProxy);
			return tcpProxy;             
			}    
		else
			{
			fprintf(stderr, "OpnetProtoSimProcess::OpenSocket() new SocketAgent error\n");
			return NULL;
			}
	}
else
	{
		udpProxy = new UdpSocketProxy(this);  // JPH
		if (udpProxy)
		{
        	udpProxy->AttachSocket(theSocket);
			socket_proxy_list.Prepend(*udpProxy);
			return udpProxy;             
			}    
		else
			{
			fprintf(stderr, "OpnetProtoSimProcess::OpenSocket() new SocketAgent error\n");
			return NULL;   
		}
	}
}  // end OpnetProtoSimProcess::OpenSocket()

void OpnetProtoSimProcess::CloseSocket(ProtoSocket& theSocket)
{
#ifdef OP_DEBUG1
printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::CloseSocket()\n");
#endif
    SocketProxy* socketProxy = static_cast<SocketProxy*>(theSocket.GetHandle());
    // (TBD) Do we to do any OPNET cleanup here???
	if (theSocket.GetProtocol() == theSocket.TCP)
	{
		ApiT_Tcp_App_Handle hndl = *(((TcpSocketProxy*)socketProxy)->sim_process->GetTcpAppHandle(((TcpSocketProxy*)socketProxy)->GetConn()));
		tcp_connection_close(hndl);
	}
    socket_proxy_list.Remove(*socketProxy);
}  // end OpnetProtoSimProcess::CloseSocket()


void OpnetProtoSimProcess::OnAccept(char* node_name)
{
    Ici* iciPtr = op_intrpt_ici();
    if (OPC_NIL != iciPtr)
    {
		IpT_Address remAddr;
		int remPort;
        int localPort;
		int connId;
		char iciformat[32];
		op_ici_format(iciPtr,iciformat);
		if (!strcmp("tcp_open_ind",iciformat))
		{
			op_ici_attr_get (iciPtr, "conn id", &connId);
			op_ici_attr_get (iciPtr, "rem addr", &remAddr);
			op_ici_attr_get (iciPtr, "rem port", &remPort);
			op_ici_attr_get (iciPtr, "local port", &localPort);
			ProtoAddress tcp_rem_addr;
			tcp_rem_addr.SimSetAddress(remAddr);
			tcp_rem_addr.SetPort(remPort);
			SetTcpRemAddress(tcp_rem_addr);
			printf("node: %s  time: %f   %s  connId: %d  remAddr: %x  remPort: %d  localPort: %d\n",
				node_name,op_sim_time(),iciformat,connId,remAddr,remPort,localPort);
			ProtoSimAgent::SocketProxy* tcpsockprxy = FindProxyByConn(connId);
			((TcpSocketProxy*)tcpsockprxy)->SetSrcAddr(tcp_rem_addr);
			//tcpsockprxy->GetSocket()->SetDestination(tcp_rem_addr);
			tcpsockprxy->GetSocket()->OnNotify(ProtoSocket::NOTIFY_INPUT);
		}
	}
}


void OpnetProtoSimProcess::OnReceive(int strm_indx,char* node_name)  // JPH 4/11/06 added strm_indx, 5/24/07 added node_name
{
#ifdef OP_DEBUG1
printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::OnReceive()\n");
#endif
    // Get packet from stream ZERO
    Ici* iciPtr = op_intrpt_ici();
    if (OPC_NIL != iciPtr)
    {
        // 1) Get packet dest port and find which local socket it goes to
        int localPort;
		int connId;
		char iciformat[32];
		op_ici_format(iciPtr,iciformat);
		//if (!strcmp(iciformat,"tcp_status_ind_mgen"))
		if (!strcmp(iciformat,"tcp_status_ind"))
		{
			/********* TCP ******************************/
			op_ici_attr_get(iciPtr, "conn_id", &connId);
			SocketProxy* socketProxy = socket_proxy_list.FindProxyByConn(connId);
			if (socketProxy)
			{
				Packet* pkt = op_pk_get(strm_indx);
            
				ReceivePacketMonitor(iciPtr, pkt);
            
				char* recvData;
				op_pk_fd_get(pkt, 0, &recvData);
				// Get recv packet length
				unsigned int recvLen  = op_pk_bulk_size_get(pkt) / 8;
				ProtoAddress srcAddr;
				ProtoAddress dstAddr;
				// Pass packet content/info to socket proxy
				static_cast<TcpSocketProxy*>(socketProxy)->OnReceive(recvData, recvLen, srcAddr, dstAddr);
				op_prg_mem_free(recvData);
				op_pk_destroy(pkt); 
			}
		}
		else if (!strcmp(iciformat,"udp_command_v3"))
		{
			/********* UDP ******************************/
			op_ici_attr_get(iciPtr, "local_port", &localPort);
			SocketProxy* socketProxy = socket_proxy_list.FindProxyByPort(localPort);
			if (socketProxy)
			{
				Packet* pkt = op_pk_get(strm_indx);  // JPH 4/11/06 added strm_indx
            
				ReceivePacketMonitor(iciPtr, pkt); // JPH 2/16/06
            
				// LP - 3-8-04 - added
				//#ifdef OPNET_OLSR
				//			record_receiving_stat(Pkt);
				//#endif  // end LP
				// Get recv packet payload
				char* recvData;
				op_pk_fd_get(pkt, 0, &recvData);
				// Get recv packet length
				int recvLen  = op_pk_bulk_size_get(pkt) / 8;
				// Get source addr/port
				IpT_Address remoteAddr;
				op_ici_attr_get(iciPtr, "rem_addr", &remoteAddr);
				int remotePort;
				op_ici_attr_get(iciPtr, "rem_port", &remotePort);
				IpT_Address localAddr;
			
				op_ici_attr_get(iciPtr, "src_addr", &localAddr);
#ifdef OP_DEBUG1
				printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::OnReceive() - remoteAdd - %u, localAdrr = %u, rec_lgn = %d\n",
					remoteAddr, localAddr, recvLen);
#endif			
				ProtoAddress srcAddr;
				srcAddr.SimSetAddress(remoteAddr);
				srcAddr.SetPort(remotePort);
				ProtoAddress dstAddr;
				dstAddr.SimSetAddress(localAddr);
				dstAddr.SetPort(localPort);
				// Pass packet content/info to socket proxy
				static_cast<UdpSocketProxy*>(socketProxy)->OnReceive(recvData, recvLen, srcAddr, dstAddr);
				op_prg_mem_free(recvData);
				op_pk_destroy(pkt); 
			}
        }
		else if (!strcmp(iciformat,"sink_command"))
		{
			Packet* pkt = op_pk_get(strm_indx);  // JPH 4/11/06 added strm_indx
            
			ReceivePacketMonitor(iciPtr, pkt); // JPH 2/16/06
            
			// Get recv packet payload
			char* recvData;
			op_pk_fd_get(pkt, 0, &recvData);
			// Get recv packet length
			int recvLen  = op_pk_bulk_size_get(pkt) / 8;
			// Get source addr/port
			IpT_Address remoteAddr;
			op_ici_attr_get(iciPtr, "rem_addr", &remoteAddr);
			int remotePort;
			op_ici_attr_get(iciPtr, "rem_port", &remotePort);
			IpT_Address localAddr;
			
			op_ici_attr_get(iciPtr, "src_addr", &localAddr);
			op_ici_attr_get(iciPtr, "local_port", &localPort);
#ifdef OP_DEBUG1
			printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::OnReceive() - remoteAdd - %u, localAdrr = %u, rec_lgn = %d\n",
				remoteAddr, localAddr, recvLen);
#endif			
			ProtoAddress srcAddr;
			srcAddr.SimSetAddress(remoteAddr);
			srcAddr.SetPort(remotePort);
			ProtoAddress dstAddr;
			dstAddr.SimSetAddress(localAddr);
			dstAddr.SetPort(localPort);
			// Pass packet content/info to mgen sink
			HandleMgenMessage(recvData,recvLen,srcAddr);
			op_prg_mem_free(recvData);
			op_pk_destroy(pkt); 		
		}
		else
    	{
        	// Generate an error and end simulation.  
	    	op_sim_end ("Error: DoReceive() Unrecognized ici format", "", "", "");
		}			
    }
    else
    {
        // Generate an error and end simulation.  
	    op_sim_end ("Error: DoReceive() OPC_NIL cmd", "", "", "");
    }
}  // end OpnetProtoSimProcess::OnReceive()



/**********************************************************************************************/
/**************************      UdpSocketProxy     *******************************************/


// LP 6-21-04 - replaced for new Ici parameter
/*
OpnetProtoSimProcess::UdpSocketProxy::UdpSocketProxy(Objid udpProcessId)
 : udp_process_id(udpProcessId),
   mcast_ttl(255), mcast_loopback(false), 
   recv_data(NULL), recv_data_len(0)
*/
OpnetProtoSimProcess::UdpSocketProxy::UdpSocketProxy(OpnetProtoSimProcess* simProcess)  // JPH
 : sim_process(simProcess),
   mcast_ttl(255), mcast_loopback(false), 
   recv_data(NULL), recv_data_len(0), udp_command_ici(NULL)

   // end LP
{ 
#ifdef OP_DEBUG2
printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::UdpSocketProxy::UdpSocketProxy(udpId = %ld)\n",
	udpProcessId);
#endif
}



OpnetProtoSimProcess::UdpSocketProxy::~UdpSocketProxy()
{
    // LP 6-21-04 - added
   op_ici_destroy (udp_command_ici);
   // end LP
}



bool OpnetProtoSimProcess::UdpSocketProxy::Bind(UINT16& thePort)
{
#ifdef OP_DEBUG1
printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::UdpSocketProxy::Bind, udp_process_id = %d)\n",
	sim_process->udp_process_id);
#endif
    // (TBD) What happens if (0 == thePort) ???

    // LP 6-21-04 - replaced for new Udp_command_ici
    /*
    Ici* cmd = op_ici_create("udp_command_v3");
    op_ici_attr_set(cmd, "local_port", (int)thePort);
    op_ici_install(cmd);
	*/
    udp_command_ici = op_ici_create("udp_command_v3");
    op_ici_attr_set(udp_command_ici, "local_port", (int)thePort);
    op_ici_install(udp_command_ici);
	
	// end LP

    op_intrpt_force_remote(UDPC_COMMAND_CREATE_PORT, sim_process->GetUdpProcId()); // JPH 
    return true;       
}  // end OpnetProtoSimProcess::SocketAgent::Bind()




bool OpnetProtoSimProcess::UdpSocketProxy::SendTo(const char*         buffer, 
                                                  unsigned int&       buflen,
                                                  const ProtoAddress& dstAddr)
{
#ifdef OP_DEBUG1
printf("\tOpnetProtoSimProcess::UdpSocketProxy::SendTo: local_port %d, rem_port %d, rem_addr_%u\n",
		proto_socket->GetPort(), dstAddr.GetPort(), dstAddr.SimGetAddress()); // LP 2-10-04
#endif
    // (TBD) error check Opnet API calls ...
    Packet* pkt = op_pk_create(buflen*8);
    char* payload = (char*) op_prg_mem_copy_create((void*)buffer, buflen);
    op_pk_fd_set(pkt, 0, OPC_FIELD_TYPE_STRUCT, payload, 0,
                 op_prg_mem_copy_create, op_prg_mem_free, buflen);
	
	// LP - the following can be set up later  to be created once as a permanent member of
	// the OpnetProtoSimProcess.  Ici parameter can be updated anytime later when it
	// is used.  That will help to reduce the time to create Ici, then deleted at UDP

	// LP 6-21-04 - replaced to reduce memory usage
	/*
    Ici* cmd = op_ici_create("udp_command_v3"); 
    op_ici_attr_set(cmd, "local_port", proto_socket->GetPort());
    op_ici_attr_set(cmd, "rem_port", dstAddr.GetPort());
    op_ici_attr_set(cmd, "rem_addr", dstAddr.SimGetAddress());
    op_ici_install(cmd);
	*/
	
	op_ici_attr_set(udp_command_ici, "local_port", proto_socket->GetPort());
    op_ici_attr_set(udp_command_ici, "rem_port", dstAddr.GetPort());
    op_ici_attr_set(udp_command_ici, "rem_addr", dstAddr.SimGetAddress());
    op_ici_install(udp_command_ici);

	// end LP
    
    sim_process->TransmitPacketMonitor(udp_command_ici, pkt);  // JPH
	
    op_pk_send_forced(pkt, 0);
    return true;
}  // end OpnetProtoSimProcess::UdpSocketProxy::SendTo()



bool OpnetProtoSimProcess::UdpSocketProxy::RecvFrom(char*         buffer, 
                                                    unsigned int& numBytes, 
                                                    ProtoAddress& srcAddr)
{
#ifdef OP_DEBUG1
printf("\tOpnetProtoSimProcess::UdpSocketProxy::RecvFrom\n"); 
#endif
    if (recv_data)
    {
        if (numBytes >= recv_data_len)
        {
            numBytes = recv_data_len;

            memcpy(buffer, recv_data, numBytes);
			op_prg_mem_free (recv_data); // LP 6-25-04 - added
            srcAddr = src_addr;
            recv_data = NULL;
            recv_data_len = 0; 
            return true;
        }
        else
        {
            PLOG(PL_ERROR, "OpnetProtoSimProcess::UdpSocketProxy::RecvFrom buffer too small\n");
            numBytes = 0; 
            return false;    
        }        
    }
    else
    {
        PLOG(PL_WARN, "OpnetProtoSimProcess::UdpSocketProxy::RecvFrom no data ready\n");
        numBytes = 0; 
        return false;       
    }   
}  // end OpnetProtoSimProcess::UdpSocketProxy::RecvFrom()



bool OpnetProtoSimProcess::UdpSocketProxy::JoinGroup(const ProtoAddress& groupAddr)
{
    // (TBD)
	return true;
}  // end OpnetProtoSimProcess::SocketAgent::JoinGroup()


bool OpnetProtoSimProcess::UdpSocketProxy::LeaveGroup(const ProtoAddress& groupAddr)
{
    // (TBD)
	return true;
}  // end OpnetProtoSimProcess::SocketAgent::LeaveGroup()


void OpnetProtoSimProcess::UdpSocketProxy::OnReceive(char*               recvData, 
                                                     unsigned int        recvLen,
                                                     const ProtoAddress& srcAddr,
                                                     const ProtoAddress& dstAddr)
{
#ifdef OP_DEBUG1
printf("\tOpnetProtoSimProcess::UdpSocketProxy::OnReceive - RecvLen = %u\n", recvLen); // LP 6-8-04
#endif
    // (TBD) filter out loopback multicast traffic

	// LP 6-10-04 - replaced
    // recv_data = recvData;
    recv_data = (char*) op_prg_mem_copy_create((void*)recvData, recvLen);
 
    // end LP

    recv_data_len = recvLen;
    src_addr = srcAddr;
    if (proto_socket)
        proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);
}  // end OpnetProtoSimProcess::UdpSocketProxy::OnReceive()




/**********************************************************************************************/
/**************************      TcpSocketProxy     *******************************************/


OpnetProtoSimProcess::TcpSocketProxy::TcpSocketProxy(OpnetProtoSimProcess* simProcess)
 : sim_process(simProcess),
   recv_data(NULL), recv_data_len(0), tcp_command_ici(NULL)
{ 
#ifdef OP_DEBUG2
printf("\topnetProtosimProcess.pr.c - OpnetProtoSimProcess::TcpSocketProxy::TcpSocketProxy()\n");
#endif
}


OpnetProtoSimProcess::TcpSocketProxy::~TcpSocketProxy()
{
   op_ici_destroy (tcp_command_ici);
}


bool OpnetProtoSimProcess::TcpSocketProxy::Bind(UINT16& thePort)
{
	local_port = thePort;
	return true;
}


bool OpnetProtoSimProcess::TcpSocketProxy::Connect(const ProtoAddress& dstAddr)
{
	// Open an active connection to dstAddr.
	ApiT_Tcp_App_Handle* hndl_ptr = &tcp_intf_hndl_copy(*(sim_process->GetTcpAppHandle()));
	IpT_Address rem_addr = dstAddr.SimGetAddress();
	TcpT_Port rem_port = GetPort(); // Assume that the remote port number is the same as the local port number.
	IpT_Address local_addr = GetTcpHostAddress().SimGetAddress();
	TcpT_Port loc_port = GetPort();
	int command = TCPC_COMMAND_OPEN_ACTIVE;
	int ip_qos = 0;
	int tcp_conn_id =
		tcp_connection_with_source_open (hndl_ptr, rem_addr, rem_port, local_addr, loc_port, command, ip_qos);
	if (tcp_conn_id == TCPC_CONN_ID_INVALID)
		return false;
	else
		{
		proto_socket->SetState(ProtoSocket::CONNECTED);
		//tcp_receive_command_send(hndl,1);
		conn_id = tcp_conn_id;
		sim_process->SetTcpAppHandle(*hndl_ptr,conn_id);
		return true;
		}
}


bool OpnetProtoSimProcess::TcpSocketProxy::Accept(ProtoSocket* theSocket)
{
	//ProtoSocket* socket = GetSocket();
	//socket->GetDestination() = GetTcpRemAddress();  // Set src address logged by RECV
	//socket->SetState(ProtoSocket::CONNECTED);
	if (theSocket)
		{
		OpnetProtoSimProcess::TcpSocketProxy* newTcpSocketProxy = new OpnetProtoSimProcess::TcpSocketProxy(sim_process);
		newTcpSocketProxy->conn_id = conn_id;
		newTcpSocketProxy->AttachSocket(*theSocket);
		theSocket->SetHandle(newTcpSocketProxy);
		sim_process->socket_proxy_list.Prepend(*newTcpSocketProxy);
		theSocket->GetDestination() = GetTcpRemAddress();  // Set src address logged by CONNECT
		theSocket->SetState(ProtoSocket::CONNECTED);
		}
	return true;
}


bool OpnetProtoSimProcess::TcpSocketProxy::Listen(UINT16 thePort)
{
	// Open a passive connection.
	ApiT_Tcp_App_Handle* hndl_ptr = &tcp_intf_hndl_copy(*(sim_process->GetTcpAppHandle()));
	IpT_Address rem_addr = 0;
	TcpT_Port rem_port = TCPC_PORT_UNSPEC;
	IpT_Address local_addr = GetTcpHostAddress().SimGetAddress();
	TcpT_Port loc_port = GetPort();
	int command = TCPC_COMMAND_OPEN_PASSIVE;
	int ip_qos = 0;
	int tcp_conn_id =
		tcp_connection_with_source_open (hndl_ptr, rem_addr, rem_port, local_addr, loc_port, command, ip_qos);
	if (tcp_conn_id == TCPC_CONN_ID_INVALID)
		return false;
	else
		{
		proto_socket->SetState(ProtoSocket::LISTENING);
		tcp_receive_command_send(*hndl_ptr,1);
		conn_id = tcp_conn_id;
		sim_process->SetTcpAppHandle(*hndl_ptr,conn_id);
		return true;
		}
}


bool OpnetProtoSimProcess::TcpSocketProxy::SendTo(const char*         buffer, 
                                                  unsigned int&       buflen,
                                                  const ProtoAddress& dstAddr)
{
	ApiT_Tcp_App_Handle* hndl_ptr = sim_process->GetTcpAppHandle(conn_id);
    Packet* pkt = op_pk_create(buflen*8);
    char* payload = (char*) op_prg_mem_copy_create((void*)buffer, buflen);
    op_pk_fd_set(pkt, 0, OPC_FIELD_TYPE_STRUCT, payload, 0,
                 op_prg_mem_copy_create, op_prg_mem_free, buflen);
	Ici* dummy_ici=0;
    sim_process->TransmitPacketMonitor(dummy_ici, pkt);  // JPH
	tcp_data_send(*hndl_ptr,pkt);
	return true;
}


bool OpnetProtoSimProcess::TcpSocketProxy::Send(const char*         buffer, 
                                                  unsigned int&     numBytes)
{
	// Dummy
	return false;
}


bool OpnetProtoSimProcess::TcpSocketProxy::RecvFrom(char*         buffer, 
                                                    unsigned int& numBytes, 
                                                    ProtoAddress& srcAddr)
{
	srcAddr = src_addr;
	return Recv(buffer,numBytes);
}


bool OpnetProtoSimProcess::TcpSocketProxy::Recv(char*         buffer, 
                                                unsigned int& numBytes)
{
    if (recv_data)
    {
        if (numBytes >= recv_data_len)
        {
            numBytes = recv_data_len;
            memcpy(buffer, recv_data+recv_data_read_index, numBytes);
			op_prg_mem_free (recv_data);
            recv_data = NULL;
            recv_data_len = 0; 
        }
        else
        {
            memcpy(buffer, recv_data+recv_data_read_index, numBytes);
			recv_data_read_index+=numBytes;
			recv_data_len-=numBytes;
        }        
        return true;
    }
    else
    {
        PLOG(PL_WARN, "OpnetProtoSimProcess::TcpSocketProxy::RecvFrom no data ready\n");
        numBytes = 0; 
        return false;       
    }   
}


void OpnetProtoSimProcess::TcpSocketProxy::OnReceive(char*               recvData, 
                                                     unsigned int        recvLen,
                                                     const ProtoAddress& srcAddr,
                                                     const ProtoAddress& dstAddr)
{
	ApiT_Tcp_App_Handle* hndl_ptr = sim_process->GetTcpAppHandle(conn_id);
	tcp_receive_command_send(*hndl_ptr,1);
    recv_data = (char*) op_prg_mem_copy_create((void*)recvData, recvLen);
    recv_data_len = recvLen;
	recv_data_read_index = 0;
    //src_addr = srcAddr;  // JPH 5/29/07 src_addr set by OpnetProtoSimProcess::OnAccept
    if (proto_socket)
        proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);
}  // end OpnetProtoSimProcess::TcpSocketProxy::OnReceive()


bool OpnetProtoSimProcess::TcpSocketProxy::JoinGroup(const ProtoAddress& groupAddr)
{
    // definition of virtual function needed
	return true;
}  // end OpnetProtoSimProcess::SocketAgent::JoinGroup()


bool OpnetProtoSimProcess::TcpSocketProxy::LeaveGroup(const ProtoAddress& groupAddr)
{
    // definition of virtual function needed
	return true;
}  // end OpnetProtoSimProcess::SocketAgent::LeaveGroup()


ProtoSimAgent::SocketProxy* OpnetProtoSimProcess::TcpSocketProxy::TcpSockList::FindProxyByConn(int connId)
{
    TcpSocketProxy* next = (TcpSocketProxy*)SocketProxy::List::head;
    while (next)
    {
        if (next->GetConn() == connId)
            return (SocketProxy*)next;
        else
            next = (TcpSocketProxy*)next->GetNext();   
    }
    return NULL;
}  // end OpnetProtoSimProcess::TcpSocketProxy::TcpSockList::FindProxyByConn()


 
