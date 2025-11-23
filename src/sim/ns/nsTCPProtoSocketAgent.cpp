/*
 *  TCPProtoSocketAgent.cpp
 *
 */

#include "nsTCPProtoSocketAgent.h"
   
/////////////////////////////////////////////////////////////////////////
// NsProtoSimAgent::UdpSocketAgent implementation
//
static class ProtoSocketTcpAgentClass : public TclClass {
public:
	ProtoSocketTcpAgentClass() : TclClass("Agent/ProtoSocket/TCP") {}
	TclObject* create(int, const char*const*) {
		return (new NsTCPProtoSocketAgent());
	}
} class_proto_socket_tcp_agent;

NsTCPProtoSocketAgent::NsTCPProtoSocketAgent()
 : Agent(PT_TCP)
{
	socketType_=NotInitialised;
	outputNotification=false; // by default
	// printf("NsTCPProtoSocketAgent: Created ok\n");
	//SetDebugLevel(7);
}
 
bool NsTCPProtoSocketAgent::Listen(UINT16 thePort) {
	if (socketType_==NotInitialised) { // must be a server socket, port is set from the NsProtoSimAgent bind operation

		PLOG(PL_DEBUG, "TCPProtoSocketAgent: Listening on %i\n", thePort);
		
		Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());	
		
		serverSocket = TCPSocketFactory::createServerSocketAgent(simAgent);
		
		if (serverSocket==NULL) return FALSE;
		
		serverSocket->setTCPAgentProtocol(TCPSocketAgent::FULLTCP);
		
		serverSocket->bind(thePort);		
		socketType_=TcpServerSocket;
		
//		printf("TCPProtoSocketAgent: Calling listen on server socket\n");

		isOpen=true;
		
		// start listening for connections
		serverSocket->listen();
		
		serverSocket->setListener(this);
		

//		printf("TCPProtoSocketAgent: Server initialsed and listening\n");

		}
	else {
		return false;
	}
 
	return true;
	}

/**
 *
 * Enables the output notification for a socket i.e. whether SEND events are generated in trigger mode.
 */
bool NsTCPProtoSocketAgent::SetOutputNotification(bool outputnotify) {
	outputNotification=outputnotify;
	if (socketType_==TcpSocket) 
		tcpPipe->setOutputNotification(outputnotify);
	return outputnotify;
	}

bool NsTCPProtoSocketAgent::Connect(const ProtoAddress& theAddress) { 
	PLOG(PL_DEBUG, "TCPProtoSocketAgent: Connect Entering\n");
	
	if (socketType_==NotInitialised) { // must be a client socket, port is set from the NsProtoSimAgent bind operation		
		PLOG(PL_DETAIL, "TCPProtoSocketAgent: Creating a new socket on %i, port %i\n", addr(), port());
		Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
		tcpPipe = TCPSocketFactory::createSocketAgent(simAgent); // sets simAgent as agent to bind to
		if (tcpPipe==NULL) return FALSE;
		tcpPipe->setTCPAgentProtocol(TCPSocketAgent::FULLTCP);

		tcpPipe->bind(port());
		tcpPipe->setListener(this); // notifications sent here

		socketType_=TcpSocket;
		isOpen=true;
		
		PLOG(PL_DETAIL, "TCPProtoSocketAgent: Conencting now to address %u, port %i\n", theAddress.SimGetAddress(), theAddress.GetPort());
		// connect to server

		tcpPipe->setOutputNotification(outputNotification);
		tcpPipe->connect(theAddress.SimGetAddress(), theAddress.GetPort());
		}
	else {
		return false;
	}
			 
	return true; 
	} 

 /** 
  * Here, we create a new protosocket to
  * deal with the new connection and tell this TCPProtoSocketAgent to reply to the requesting
  * client. This allows us to spwan off new agents to deal with multiple clients.
  * NOTE that this assumes that the user provides a socket to deal with the connection !
  */ 
bool NsTCPProtoSocketAgent::Accept(ProtoSocket* theSocket) { 
	PLOG(PL_DEBUG, "TCPProtoSocketAgent::Accept Entering \n");

	if (socketType_==NotInitialised) return FALSE;
			
	theSocket->SetNotifier(proto_socket->GetNotifier());

    // Note that the first argument does not matter because the third is false i.e. bindOnCreate is false
	// so port is not used in Open
	theSocket->Open(0, ProtoAddress::SIM, FALSE); //creates a new nsTCPProtoSocketAgent
 
	NsTCPProtoSocketAgent *theProtoSocket = (NsTCPProtoSocketAgent *) static_cast<ProtoSimAgent::SocketProxy*>(theSocket->GetHandle());
	
	TCPSocketAgent* socket = serverSocket->accept(); // create new socket agent for this connection

 	socket->setListener(theProtoSocket); // set the listener for RECEIVE events to this class 
	
	theProtoSocket->setTCPSocketAgent(socket);  // set the socket agent in the new TCPProtoSocketAgent to be 
	
	// Need to set the destination...
	
	ProtoAddress destination;
	destination.SimSetAddress(socket->getDestinationAddress());
	destination.SetPort(socket->getDestinationPort());
	theSocket->SetDestination(destination);

	PLOG(PL_DEBUG, "TCPProtoSocketAgent::Accept Exiting \n");
	
	return true;
	} 
 

bool NsTCPProtoSocketAgent::SendTo(const char* buffer, unsigned int& numBytes, const ProtoAddress& dstAddr) {
	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Sending Data\n");
	
	if (socketType_==NotInitialised) return FALSE;

	unsigned int bytesSent = tcpPipe->send(numBytes,buffer); 
	numBytes=bytesSent;
	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Completed send\n");
	return true;
}	
 

/**
 * Catches events thrown from the underlying TCP agents. 
 */ 
bool NsTCPProtoSocketAgent::tcpEventReceived(TCPEvent *event) {

	// check how these map ...

	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Getting Callback\n");
	
	if (event->getType()==TCPEvent::ACCEPT) 
    {
        if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);  
	} 
    else if (event->getType()==TCPEvent::CONNECTED) 
    {
	    //	printf("Connected event in ProtoSocketAgent\n");
		if (proto_socket) 
			proto_socket->OnNotify(ProtoSocket::NOTIFY_OUTPUT);
			
	} 
    else if (event->getType()==TCPEvent::SEND) 
    {
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_OUTPUT);  
	} 
    else if (event->getType()==TCPEvent::RECEIVE) 
    {

		recv_data = (char *)event->getData();
		recv_data_len = event->getDataSize();
		recv_data_offset=0; // reset offset to 0

		PLOG(PL_DEBUG,"NsTCPProtoSocketAgent::tcpEventReceived: Got data of length %i\n", recv_data_len);
		PLOG(PL_DETAIL, "NsTCPProtoSocketAgent::tcpEventReceived: Destination port and Address are %i and %i\n", tcpPipe->getDestinationPort(), 
							tcpPipe->getDestinationAddress()); 

		src_addr.SimSetAddress(tcpPipe->getDestinationAddress());
		src_addr.SetPort(tcpPipe->getDestinationPort());

		PLOG(PL_DETAIL, "NsTCPProtoSocketAgent::tcpEventReceived event being sent to socket ...\n ");
 
		if (proto_socket) 
			proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);
		PLOG(PL_DETAIL, "NsTCPProtoSocketAgent::tcpEventReceived event processed ... \n");
				
	} 
    else if (event->getType()==TCPEvent::DISCONNECTED) 
    {
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_NONE);  
	} 
    else 
    {
		PLOG(PL_ERROR, "NsTCPProtoSocketAgent::tcpEventReceived UNKNOWN Event on node %i\n", addr());
		return false;
	}
	
	PLOG(PL_DEBUG, "NsTCPProtoSocketAgent::tcpEventReceived finished Callback\n");

	delete event;
	return true;
} 

/**
 * Close and detach the relevant agents from the sockets that have been created by this 
 * NS TCP Proto Sokcet Agent.  This agent is NOT attached but others it communicates with are...
 */
bool NsTCPProtoSocketAgent::Shutdown() {
	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Shutdown\n");

	if (!isOpen) return false;

	if (socketType_==NotInitialised) return TRUE;
	
	if (socketType_==TcpSocket)
		tcpPipe->shutdown();
	else
		serverSocket->shutdown();
	
	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Socket shutdown initiated\n");
	
	return true;
}  // end NsProtoSimAgent::UdpSocketAgent::Close()


/**
 * Close and detach the relevant agents from the sockets that have been created by this 
 * NS TCP Proto Sokcet Agent.  This agent is NOT attached but others it communicates with are...
 */
bool NsTCPProtoSocketAgent::Close() {
	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Closing\n");
	bool state=false;
	if (!isOpen) return true;

	if (socketType_==NotInitialised) return TRUE; // is closed already

	if (socketType_==TcpSocket)
		state=tcpPipe->closeSocket();
	else
		state=serverSocket->closeSocket();
	
	PLOG(PL_DEBUG, "NsTCPPrototSocketAgent: Socket Closed\n");

	isOpen=false;
	
	return state;
}  // end NsProtoSimAgent::UdpSocketAgent::Close()

