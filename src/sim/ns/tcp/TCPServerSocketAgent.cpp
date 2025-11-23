/*
 *  TCPServerSocketAgent.cpp
 *
 *  Created by Ian Taylor on 01/03/2007.
 *
 */
 
#include "TCPServerSocketAgent.h" 

static class TCPServerSocketAgentClass : public TclClass {
public:
	TCPServerSocketAgentClass() : TclClass("Agent/TCP/ServerSocketAgent") {}
	TclObject* create(int argc, const char*const *argv) {		
		PLOG(PL_DEBUG, "Creating TCPSocketAgent %i arguments\n", argc);
		if (argc == 6) { // with protocol included
			return (new TCPServerSocketAgent(argv[4], atoi(argv[5])));
		} else if (argc == 4) {
			TCPServerSocketAgent *tcpsock = new TCPServerSocketAgent();
			return (tcpsock);
		} else
			return NULL;
	}
} class_tcp_server_socket_agent; 


void TCPServerSocketAgent::recv(Packet *pkt, Handler *handler) {
		int tiflags = hdr_tcp::access(pkt)->flags() ; 		 // tcp flags from packet
		hdr_ip *iph = hdr_ip::access(pkt);

		if (state_==SOCKCLOSED) return;

	PLOG(PL_DEBUG, "TCPServerSocketAgent::recv, node %u recv Called, flags = %u \n", addr(), tiflags);

	// Possible options:
	// #define TH_FIN  0x01        /* FIN: closing a connection */
	// #define TH_SYN  0x02        /* SYN: starting a connection */
	// #define TH_PUSH 0x08        /* PUSH: used here to "deliver" data */
	// #define TH_ACK  0x10        /* ACK: ack number is valid */
	// #define TH_ECE  0x40        /* ECE: CE echo flag */
	// #define TH_CWR  0x80        /* CWR: congestion window reduced */
	
		if (tiflags & TH_FIN) { // Closing connection
	 		PLOG(PL_DEBUG, "TCPServerSocketAgent received a FIN - Close connection request from %i, port %i\n", iph->saddr(), iph->sport());
			SocketListItem *sitem = (SocketListItem *)mySocketList->findProxyByPortAndAddress(iph->saddr(), iph->sport()); 
			TCPSocketAgent* socket = sitem->getSocketAgent();
			socket->getTCPAgent()->recv(pkt,handler); // sent packet to agent for processing
			state_=SOCKCLOSED; // so no more processing ...
			// shutdown();
//			mySocketList->remove(sitem); // remove socket in either case i.e. FIN received (5) or final FIN (9) from linked list connections
		} else if (tiflags & TH_SYN) { // connection request
			synPacket_= pkt;
			handler_=handler;
			// set the senders port and address
			dport() = iph->sport();
	 		daddr() = iph->saddr();  
			
	 		PLOG(PL_DEBUG, "TCPServerSocketAgent received a SYN - connection request from %i, port %i\n", iph->saddr(), iph->sport());
			
			// First check to see if we already have received a connection request by searching the
			// static socket list to see if this client has already made this request.
			SocketListItem *sitem = (SocketListItem *)TCPSocketAgent::getSocketConnections()->findProxyByPortAndAddress(iph->saddr(), iph->sport()); 

			if (sitem!=NULL) { // then there's been an accept already so ignore duplicate
				PLOG(PL_INFO, "TCPServerSocketAgent: Ingoring SYN request\n");
				PLOG(PL_INFO, "Connection attempt has already been made from %i and port %i\n", iph->saddr(), iph->sport() );
				return;
				}
			if (tcpListener_!=NULL)
				tcpListener_->tcpEventReceived(new TCPEvent(TCPEvent::ACCEPT, this, NULL, NULL));
			else
				accept(); // no listener, so accept by default.
		} else { // 
			
			SocketListItem *sitem = (SocketListItem *)mySocketList->findProxyByPortAndAddress(iph->saddr(), iph->sport()); 
			TCPSocketAgent* socket=NULL;
			
			if (sitem!=NULL)
				socket = sitem->getSocketAgent();
			
			if (socket==NULL) {
				PLOG(PL_ERROR, "TCPServerSocketAgent: No socket found for address %i and port %i !!!", iph->saddr(), iph->sport() );
				PLOG(PL_ERROR, "TCPServerSocketAgent: This should not happen - please report bug \n");
				exit(1);
			} 
			
			bool skip=false;
			
			PLOG(PL_DETAIL, "Flags are TH_ACK: %u, TH_ECE: %u, TH_CWR: %u\n", (tiflags & TH_ACK), (tiflags & TH_ECE), (tiflags & TH_CWR));
//			if ((tiflags & TH_ECE))
//				skip=true;
			
			if (!skip) {
				// Demultiplex and find the socket agent to pass the data to
				// its data so pass it to TCP agent
				
				// If this is the same node, just get the data and bypass TCP

				PLOG(PL_DETAIL, "TCPServerSocketAgent: Pointer to Packet is %u\n",  pkt);

				TcpData *tcpData = (TcpData *)pkt->userdata();

				if ((tcpData!=NULL) && (tcpData->size()!=0)) { // if there is data then pass it on 
					PLOG(PL_DEBUG, "TCPServerSocketAgent:: Demultiplexing - passing onto TCP agent to receive data \n");
//					PLOG(PL_DEBUG, "TCPServerSocketAgent: Got Data!!!!: Node Received DATA !!!!!! %s\n", tcpData->getData());
	//				socket->tcpEventReceived(new TCPEvent(TCPEvent::RECEIVE, pkt, tcpData->getData(), tcpData->getDataSize()));
					// delete tcpData;  // no need - the scheduler cleans this up for us...
					
					// do this in the agent.
				} 		
			}
									
//			if (socket->getAddress()==socket->getDestinationAddress())
//				socket->getTCPApp()->recv(-1);
//			else
				socket->getTCPAgent()->recv(pkt,handler);
		} // NOT A SYN OR FIN 
	
		PLOG(PL_DEBUG, "TCPServerSocketAgent:: Packet Handled ok \n");
	}

 /**
  * Returns a new TCPSocketAgent that will deal with the accepted connection
  */
TCPSocketAgent* TCPServerSocketAgent::accept() { 
	PLOG(PL_DEBUG, "TCPServerSocketAgent::Accept CALLED - we are on node %i, accepting connection from node %i, port %i\n", addr(), daddr(), dport());
	
	Tcl& tcl = Tcl::instance();    
 
	tcl.evalf("eval new Agent/TCP/SocketAgent");
	
	const char *tcpVar = tcl.result();
  
	if (tcpVar==NULL) return NULL;
		
	TCPSocketAgent *socket = (TCPSocketAgent *)tcl.lookup(tcpVar);
	socket->setSimulationAgent(initialSimulationAgent);
	 	  
	socket->setTCPAgentProtocol(theTCPProtocol_); // creates the underlying TCP protocol for the agent
		
	socket->setDestinationPort(dport()); 
	socket->setDestinationAddress(daddr());

  	socket->attachTCPAgentToNode(this, -1); // attaches the socket to this node, on a port negative 1 - unlikely to be taken ...
	
	socket->getTCPInterface()->listenOn(port()); // se we can receive data

	// Add the receiving socket to the static socket list with the sender's port and address as the ID.
	PLOG(PL_DETAIL, "TCPServerSocketAgent: add REMOTE socket to list for client's address %i and port %i\n", socket->getDestinationAddress(), socket->getDestinationPort());
	SocketListItem *snode = new SocketListItem(socket);
	snode->setAddress(socket->getDestinationAddress());
	snode->setPort(socket->getDestinationPort());
	TCPSocketAgent::getSocketConnections()->prepend(snode);

	PLOG(PL_DETAIL, "TCPServerSocketAgent: added REMOTE socket to list for client's address %i and port %i\n", snode->getAddress(), snode->getPort());

	// Adds this socket to the list of sockets that this server socket is managing for demulpiplexing later.
	mySocketList->prepend(snode);

	// THIS IS JUST FOR LOCAL CONNECITONS ...
	TCPSocketAgent *remoteSocket=NULL; 
    if (socket->getAddress() == socket->getDestinationAddress()) {  
		PLOG(PL_WARN, "TCPServerSocketAgent::LOCAL SOCKET DETECTED - NS-2 cannot run LOCAL sockets \n");
		SocketListItem *sitem = (SocketListItem *)TCPSocketAgent::getSocketConnections()->findProxyByPortAndAddress(socket->getAddress(), socket->getPort()); 
		remoteSocket = sitem->getSocketAgent();
		TCPSocketAgent::getSocketConnections()->remove(sitem);
		}
	// END LOCAL CONNECITONS ...
		
	PLOG(PL_INFO, "TCPServerSocketAgent::Forwarding SYN to Agent \n");

    // here only pass it to the TCP agent if we are not on the same node. If we are the TCPAgent breaks
	// so we bypass the protocol.
	
    if (socket->getAddress() != socket->getDestinationAddress())
		socket->getTCPAgent()->recv(synPacket_, handler_); // send the SYN packet to the TCP protocol so it can respond
														// as if it received it in the first place ....	 	
	else // need to notify connected socket...
		remoteSocket->tcpEventReceived(new TCPEvent(TCPEvent::CONNECTED, NULL, NULL, 0));

	return socket;
	}

bool TCPServerSocketAgent::bind(nsaddr_t thePort) { 
	PLOG(PL_DEBUG, "TCPServerSocketAgent: Attempting to Attach on port %i \n", thePort);

	if (initialSimulationAgent!=NULL) {
		attachTCPServerToNode(initialSimulationAgent, thePort); 
		PLOG(PL_DEBUG, "TCPServerSocketAgent: Attach Complete, TCP ServerAgent on node %i, port %i \n", addr(), port());
		return TRUE;
		}
	else {
		PLOG(PL_ERROR, "TCPServerSocketAgent: Can't attach server to node: no simulation agent set\n");
		return false;
		}
	}


/**
 * Atatches the TCP agent to the node that the provided agent is attached to. The provide agent must have
 * been attached before this method is called, otherwise we cannot get a reference to the node. 
 */
bool TCPServerSocketAgent::attachTCPServerToNode(Agent *myAgent, nsaddr_t thePort) {
	Tcl& tcl = Tcl::instance();    
     
	tcl.evalf("%s set node_", myAgent->name());
	
	const char* nodeName = tcl.result();

	attachTCPServerToNode(nodeName, thePort);
	return true;
} 
	
/**
 * Overridden to become a dummy TCPSocketAgent that muli-plexes messages to real socket Agents.
 */
bool TCPServerSocketAgent::attachTCPServerToNode(const char *nodeNameInTCL, nsaddr_t thePort) {
    
	Tcl& tcl = Tcl::instance();	
    // find out the name of this node in tcl space
	
	// tcl.evalf("%s set node_", node->name());
    // const char* nodeName = tcl.result();

	// cout << "Node name for " << name() << " on node: " << nodeNameInTCL_ <<  endl;

	nodeNameInTCL_ = new char[strlen(nodeNameInTCL)];
	
	strcpy(nodeNameInTCL_,nodeNameInTCL); // sets the name of the node to its TCL parameter for future use

//    if (thePort > 0)
		PLOG(PL_DEBUG, "Running: %s attach %s %hu\n", nodeNameInTCL_, name(), thePort);	    
        tcl.evalf("%s attach %s %hu", nodeNameInTCL_, name(), thePort);
//    else
  //      tcl.evalf("%s attach %s", nodeNameInTCL, name());
  
  	tcpAgentIsAttached=true;
	
	return true;  
	}

/**
 * Must shutdown socket but after current data has been sent, also needs to notfiy all connected 
 * sockets that this has closed.
 */
bool TCPServerSocketAgent::shutdown() {
	// Tell every socket to shutdown, which sends off a SYN packet to the other side of the connection.
	SocketListItem* cur = (SocketListItem*)mySocketList->getHead();
	if (cur!=NULL) {
		SocketListItem* next;
		// detach all socket agents added to this node.
		while (cur) {
			next = (SocketListItem*)cur->getNext();
			TCPSocketAgent *attachedSocket = cur->getSocketAgent();
			PLOG(PL_DEBUG, "TCPServerSocketAgent -> shutting down %s\n", attachedSocket->getTCPAgent()->name());
			attachedSocket->shutdown();
			cur = next; 
		}   
	}  
	return true; 
	state_=SOCKCLOSED;
}

/**
 * Must close socket but after current data has been sent, also needs to notfiy all connected 
 * sockets that this has closed.
 */
bool TCPServerSocketAgent::closeSocket() {
	// Tell every socket to close
	SocketListItem* cur = (SocketListItem*)mySocketList->getHead();
	if (cur!=NULL) {
		SocketListItem* next;
		// detach all socket agents added to this node.
		while (cur) {
			next = (SocketListItem*)cur->getNext();
			TCPSocketAgent *attachedSocket = cur->getSocketAgent();
			PLOG(PL_DEBUG, "TCPServerSocketAgent -> closing %s\n", attachedSocket->getTCPAgent()->name());
			attachedSocket->close();
			delete cur;
			cur = next; 
		}   
	}  
	PLOG(PL_DEBUG, "TCPServerSocketAgent::all sockets closed - detaching now\n");
	dettachFromNsNode();
	return true; 
}

/**
 * Detaches this TCP socket server agent from the node.
 */
bool TCPServerSocketAgent::dettachFromNsNode() {	
	PLOG(PL_DEBUG, "TCPServerSocketAgent::dettachFromNsNode called\n");
	
	if (!tcpAgentIsAttached) return false;
	else {	
		PLOG(PL_DEBUG, "TCPServerSocketAgent::Agent Active, Detaching Agent\n");
		
		Tcl& tcl = Tcl::instance();	
        tcl.evalf("Simulator instance");
        char simName[32];
	    strncpy(simName, tcl.result(), 32);

		PLOG(PL_DEBUG, "Detach, Running: %s detach-agent %s %s\n", simName, nodeNameInTCL_, name()); 

		tcl.evalf("%s detach-agent %s %s", simName, nodeNameInTCL_, name()); 

    	PLOG(PL_DEBUG, "TCPSocketServerAgent::dettachFromNsNode Finished\n");
		
		return true;
		}
    }
		
int TCPServerSocketAgent::command(int argc, const char*const* argv) {	
	PLOG(PL_DEBUG, "Node = %s, command = %s\n", name(), argv[1]);
	
    for (int i =0; i<argc; ++i)
		PLOG(PL_DEBUG," arg[%i] = %s," , i, argv[i]);

	PLOG(PL_DEBUG,"\n");

	if (argc==2) {
		if (strcmp(argv[1], "listen") == 0) {
			//
			return (TCL_OK);
		} else if (strcmp(argv[1], "detach-from-node") == 0) {
			dettachFromNsNode(); 
			return (TCL_OK);
			}
	} else if (argc==3) {
		if (strcmp(argv[1], "setProtocol") == 0) {
			theTCPProtocol_=TCPSocketAgent::getTCPProtocolFromString(argv[2]); 
			return (TCL_OK);
		} else if (strcmp(argv[1], "attach-to-node") == 0) {
			attachTCPServerToNode(argv[2], 0); 
			return (TCL_OK);
		}
	}
	
	return Agent::command(argc, argv);
} 

