/*
 *  TCPSocketAgent.cpp
 *
 *  Created by Ian Taylor on 01/01/2007.
 *
 */
 

#include "TCPSocketAgent.h"
  
// These are the  overloaded TCP classes that just detect the sequence of events when 
// connecting - more intelligent TCPFull classes. They are NOT instantiated directly for use
// with TCPSocketAgent. Use TCPSocketAgent or TCPProtoSocketAgent instead 
  
  
// NOTE: we same name as original TCP for FullTcp so we get initialised correctly. Otherwise
// we do not get the MANY variables deinfed in ./tcl/lib/ns-default.tcl initialised correctly
static class FullTcpWithEventsClass : public TclClass { 
public:
	FullTcpWithEventsClass() : TclClass("Agent/TCP/FullTcp/Events") {} 
	TclObject* create(int argc, const char*const* argv) {  
 		if (argc != 4)
			return NULL;
		return (new TCPFullWithEvents());
	}
} class_full_tcp_with_events;
 
static class TCPSocketAgentClass : public TclClass {
public:
	TCPSocketAgentClass() : TclClass("Agent/TCP/SocketAgent") {}
	TclObject* create(int argc, const char*const *argv) {				
		if (argc == 6) { // with protocol included
			return (new TCPSocketAgent(argv[4], atoi(argv[5])));
		} else if (argc == 4) {
			TCPSocketAgent *tcpsock = new TCPSocketAgent();
 			return (tcpsock);
		} else
 			return NULL;
	}  
} class_tcp_socket_agent; 
 
 
// Constructor Method Implementations

TCPSocketAgent::TCPSocketAgent(const char *theTCPProtocolString, nsaddr_t thePort)  : Agent(PT_TCP) {
	init();
	setTCPAgentProtocol(getTCPProtocolFromString(theTCPProtocolString));
	setPort(thePort);
}

TCPSocketAgent::TCPSocketAgent(TcpProtocol theTCPProtocol, nsaddr_t thePort) : Agent(PT_TCP) { 
	init(); 
	setTCPAgentProtocol(theTCPProtocol); 
	setPort(thePort); 
	}

 
TCPSocketAgent::TCPSocketAgent(FullTcpAgent *theTCPAgent, nsaddr_t thePort) : Agent(PT_TCP), tcpAgent(theTCPAgent) {
	init(); 
	tcpAgent=theTCPAgent;
	setPort(thePort);
	}
	 
void TCPSocketAgent::init() {
//	SetDebugLevel(PL_MAX);
 	PLOG(PL_DEBUG, "TCPSocketAgent::init called\n");
	tcpSocketApp=NULL;
	txBuffer=NULL;
	setTxBufferSize(4096);
	setRxBufferSize(4096);
	tcpAgentIsAttached=false;
	tcpTriggerMode=LEVEL;
	bytesSent_=0;
	sequenceNumber_=0;
	tcpTimer = new TcpTimer(this);
	sendWasCalled_=false;
	outputNotification_=false; // For LEVEL trigger events - stop SEND events is false for when buffer is not full
	PLOG(PL_DEBUG, "TCPSocketAgent::init ended\n");
	state_=SOCKOPEN;
	}


TCPSocketAgent::TcpProtocol TCPSocketAgent::getTCPProtocolFromString(const char *theTCPProtocolString) { 			
	if (strcmp(theTCPProtocolString, "FULLTCP") == 0)
		return FULLTCP;
	else if (strcmp(theTCPProtocolString, "RENOFULLTCP") == 0)
		return RENOFULLTCP;
	else if (strcmp(theTCPProtocolString, "SACKFULLTCP") == 0)
		return SACKFULLTCP;
	else if (strcmp(theTCPProtocolString, "TAHOEFULLTCP") == 0)
		return TAHOEFULLTCP;
	else 
		return ERROR;
	}
  

const char *TCPSocketAgent::getTCPStringForProtocol(TCPSocketAgent::TcpProtocol theTCPProtocol) {
	if (theTCPProtocol == FULLTCP)
		return "FULLTCP";
	else if (theTCPProtocol == RENOFULLTCP)
		return "RENOFULLTCP";
	else if (theTCPProtocol == SACKFULLTCP)
		return "SACKFULLTCP";
	else if (theTCPProtocol == TAHOEFULLTCP)
		return "TAHOEFULLTCP";
	else 
		return "ERROR";
}

bool TCPSocketAgent::setTxBufferSize(unsigned int bufferSize) { 
	txBufferSize= bufferSize;  
	createSendBuffer();
	bytesInSendBuffer_=0;
	return true; 
	} 

unsigned int TCPSocketAgent::getTxBufferSize() { 
	return txBufferSize; 
	} 
	
bool TCPSocketAgent::setRxBufferSize(unsigned int bufferSize) { 
	rxBufferSize=bufferSize; 
	return true; 
	} 

unsigned int TCPSocketAgent::getRxBufferSize() { 
	return rxBufferSize; 
	} 

void TCPSocketAgent::createSendBuffer() {
 	PLOG(PL_DEBUG, "TCPSocketAgent::createSendBuffer entering\n");
	if (txBuffer != NULL) delete txBuffer;
	char *databuf = new char[getTxBufferSize()];
	txBuffer = new TcpData();
	txBuffer->setData(databuf, getTxBufferSize());
 	PLOG(PL_DEBUG, "TCPSocketAgent::createSendBuffer leaving\n");	
}

SimpleList *TCPSocketAgent::getSocketConnections() { 
	static SimpleList *socketConns = new SimpleList(); 
	return socketConns; 
}
 
bool TCPSocketAgent::bind(nsaddr_t thePort) { 	
	if (state_!=SOCKOPEN) return false;
 	PLOG(PL_DEBUG, "TCPSocketAgent::bind - Attaching on port %hu\n", thePort);

	if (initialSimulationAgent!=NULL) {
		attachTCPAgentToNode(initialSimulationAgent, thePort);
		PLOG(PL_DEBUG, "TCPSocketAgent: Attach Complete: TCP Agent on node %i, port %i \n", getAddress(), getPort());
		port()=thePort;
		return TRUE;
		}
	else { 
		PLOG(PL_DEBUG, "TCPSocketAgent: Can't attach socket to node: no simulation agent set\n");
		return false;
		}
}

/**
 * Make a connection to the remote TCPSocketAgent - only used from tcl
 */
bool TCPSocketAgent::connect(TCPSocketAgent *remoteTCPSockAgent) { 			
	if (state_!=SOCKOPEN) return false;
	FullTcpAgent *remoteFullTCPAgent = remoteTCPSockAgent->getTCPAgent();	
	connect(remoteFullTCPAgent->addr(), remoteFullTCPAgent->port());
	return true;
} 

/**
  Connect to remote location.  Note this just sets the dst port
*/
bool TCPSocketAgent::connect(nsaddr_t destAddress, nsaddr_t destPort) {
	if (state_!=SOCKOPEN) return false;
	PLOG(PL_DEBUG, "TCPSocketAgent: Node %i, connecting to node address %i, port %i\n", getAddress(), destAddress, destPort);

	tcpAgentInterface->setDestinationPort(destPort);
	tcpAgentInterface->setDestinationAddress(destAddress);  
	
	if (destAddress==getAddress()) { // add it to the list so we can make a connection on local nodes
		SocketListItem *snode = new SocketListItem(this);
		snode->setAddress(getAddress());
		snode->setPort(getPort());
		TCPSocketAgent::getSocketConnections()->prepend(snode);
		}
	
	connect();

	PLOG(PL_DEBUG, "TCPSocketAgent: Node %i connected to address %i, port %i\n", getAddress(), destAddress, destPort);
 
	return true;
}	

	
/**
 * Send data to the TCP server through this socket. This uses the TCPSocket app class to actually
 * transfer the application data between the nodes.
 * Returns the number of bytes sent.
 */	
unsigned int TCPSocketAgent::send(int nbytes, const char *data) {	
	if (state_!=SOCKOPEN) return 0;
	PLOG(PL_DEBUG, "Entering TCPSocketAgent::send \n");
	PLOG(PL_DETAIL, "TCPSocketAgent::send -> preparing to send %u bytes\n", nbytes);
	PLOG(PL_DETAIL, "TCPSocketAgent::buffer size is %u\n", txBufferSize);
	PLOG(PL_DETAIL, "TCPSocketAgent::bytes in send buffer = %u\n", bytesInSendBuffer_);
	
	if ((bytesInSendBuffer_+nbytes)< txBufferSize) { // ok to send and notify there's more room
		bytesSent_=nbytes;
		bufferIsFull_=false;
	} else { // not enough room so write what I can
		bufferIsFull_=true;
		// bytesSent=0;
		bytesSent_= txBufferSize-bytesInSendBuffer_; 
		PLOG(PL_DETAIL, "TCPSocketAgent::send buffer full, writing %u bytes \n", bytesSent_);
		}
		
	if (bytesSent_!=0) {
		char *dataToSend = txBuffer->getData();
		memcpy(dataToSend+bytesInSendBuffer_, data, bytesSent_); // copy to internal buffer
		PLOG(PL_DETAIL, "TCPSocketAgent: buffering data at point %i, size = %i from %u port %i\n", 
					bytesInSendBuffer_, bytesSent_, getAddress(), getPort());	
		bytesInSendBuffer_+=bytesSent_;		
		}

	sendWasCalled_=true;
	
	PLOG(PL_DEBUG, "TCPSocketAgent::send - setting up for a TIMER CALLBACK for the SEND event \n");
	timerTrggerMode_=SEND;
	tcpTimer->resched(0.0); // zero second timer, but it schedules after the sends
	
	PLOG(PL_DEBUG, "Leaving TCPSocketAgent::send \n");
	return bytesSent_;
} 
 

void TCPSocketAgent::emptySendBuffer() {
	if (state_!=SOCKOPEN) return;
	PLOG(PL_DETAIL, "TCPSocketAgent: Emptying Send Buffer\n");

	if (bytesInSendBuffer_==0) return;
	
	TcpData *toSend = new TcpData();
	char *data=new char[bytesInSendBuffer_];
	memcpy(data,txBuffer->getData(),bytesInSendBuffer_); // to get the correct size
	toSend->setData(data,bytesInSendBuffer_);
	PLOG(PL_DETAIL, "TCPSocketAgent::emptySendBuffer, sending %u bytes\n", bytesInSendBuffer_);
	PLOG(PL_DETAIL, "TCPSocketAgent::emptySendBuffer, sending %s bytes\n", data);

	tcpSocketApp->send(toSend);

//	if (tcpSocketApp!=NULL)  {
//		tcpSocketApp->send(toSend);
/*	} else {
//		tcpAgentInterface->setTcpData(toSend);
//		tcpAgent->send(bytesInSendBuffer_);
		
		//		Packet* p = allocpkt(buflen);
		Packet* p = allocpkt();
		if (p) {		
			// Set packet destination addr/port
			hdr_ip* iph = hdr_ip::access(p);
			iph->daddr() = tcpAgentInterface->getDestinationAddress();
			iph->dport() = tcpAgentInterface->getDestinationPort();
			iph->saddr() = tcpAgentInterface->getAddress();			
			iph->sport() = tcpAgentInterface->getPort();
			
			PLOG(PL_DETAIL, "TCPSocketAgent::emptySendBuffer, sending to %u port %u\n", iph->daddr(), iph->dport());   
			
			hdr_cmn* ch = hdr_cmn::access(p);  
			ch->ptype() = PT_TCP;
			ch->size() = bytesInSendBuffer_;
			
			p->setdata(toSend);
			PLOG(PL_DETAIL, "TCPSocketAgent::emptySendBuffer, Set DATA to %s\n",  ((TcpData *)p->userdata())->getData());
			
			tcpAgent->send(p, 0);
			PLOG(PL_DEBUG, "TCPSocketAgent::emptySendBuffer, Completed send\n");
		}
		else {
			PLOG(PL_ERROR, "TCPSocketAgent::emptySendBuffer() - could not create a new ns-2 Packet - memory issue??? EXITING NOW...");
			exit(0);
		}
		
	}*/
	 
	bytesInSendBuffer_ = 0; // used to be in SEND event and decrease by event->getDataSize(); // decrease space in buffer
 
	PLOG(PL_DETAIL, "TCPSocketAgent: Leaving Send Buffer\n");
} 

bool TCPSocketAgent::shutdown() {
	PLOG(PL_INFO, "TCPSocketAgent::close(): Closing TCP Full agent now\n");
	tcpAgent->close(); // kicks off the process - closes the agent and sends out a FIN packet
	PLOG(PL_INFO, "TCPSocketAgent::close(): Setting timer to detach socket\n");
	return true;
}

/**
 * This is an active close, called from the application.
 * Must close socket but after current data has been sent.
 */ 
bool TCPSocketAgent::closeSocket() {
	timerTrggerMode_=CLOSE;  // close the socket and detach it after everything has been sent out
	tcpTimer->resched(0.0); // zero second timer, but it schedules after the sends	
	return true;
} 
 
// callback from zero timeout from timing function.
void TCPSocketAgent::tcpTimerTriggered() {
	if (state_!=SOCKOPEN) return;
	PLOG(PL_INFO, "tcpTimerTriggered: Timer Triggered\n");

	PLOG(PL_INFO, "tcpTimerTriggered: sendWasCalled %i, outputnotification = %i\n", sendWasCalled_, outputNotification_);
	PLOG(PL_INFO, "tcpTimerTriggered: bufferIsFull_ %i\n", bufferIsFull_);
 
	if (timerTrggerMode_==SEND) { // trigger used for SEND events etc
		if (tcpTriggerMode==LEVEL) {
			if (bufferIsFull_) {
				PLOG(PL_INFO, "tcpTimerTriggered: MODE: Buffer is full\n");
				emptySendBuffer();			
			} else if	((sendWasCalled_) && (tcpListener_) && (outputNotification_)) {
				PLOG(PL_INFO, "tcpTimerTriggered: MODE: Send was called and outputnotification enabled\n");
				sendWasCalled_=false; // reset this so we can detect if app ignores send event
				PLOG(PL_INFO, "BADIDEA: tcpTimerTriggered: Generating SEND Event\n");
				if (bytesSent_>0) tcpListener_->tcpEventReceived(new TCPEvent(TCPEvent::SEND, NULL, NULL, bytesSent_));
				if ((!sendWasCalled_) || (bytesSent_==0)) // if the app ignored the SEND event, or no bytes sent, empty buffer 
					emptySendBuffer();
				}
			else { // no listener or no data sent = no activity so empty anyway
				PLOG(PL_INFO, "tcpTimerTriggered: MODE: Emptying buffer Anyway....\n");
				emptySendBuffer();
			}
			} // LEVEL
		else { // EDGE
			if (bufferIsFull_) {
				emptySendBuffer();
			}
		} // EDGE
	} else if (timerTrggerMode_==CLOSE) { // Socket closing and clean up		
		if (state_==CLOSEWAIT)
			tcpAgentInterface->finalAck(); // FIN recieved
		else {
			dettachFromNsNode();  // detach from agent after packets have been processed.
			if (tcpSocketApp!=NULL) delete tcpSocketApp;
		}			
	} else { //Not possible 
		PLOG(PL_FATAL, "Undefined timerTrggerMode_ mode in TCPSocketAgent::tcpTimerTriggered\n");
	    }
}


// TCPSocketAgentListener Interface implementation - used to receive events from TCP sockets

/**
 * Events from TCP Application received here - this can be reimplemented in a subclass in order to link into
 * whichever the software toolkit or application requires. Conenction events and receive etc defined in TCPEvent
 * are triggered by the underlying TCP agents and channeled through this method for conveneince. 
 */
bool TCPSocketAgent::tcpEventReceived(TCPEvent *event) {
	PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived EVENT\n");
 
	if (event->getType()==TCPEvent::CONNECTED) {
		    PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived CONNECTED Event on node %i\n", getTCPAgent()->addr());
	 					
			// Just set when SYN was received by remote node - find it in the list
			// we just look it up by using our own port and address as an id

			SocketListItem *sitem = (SocketListItem *)TCPSocketAgent::getSocketConnections()->findProxyByPortAndAddress(getAddress(),getPort()); 
			
			if (sitem!=NULL) { // This is the ACK from a SYN meaning that we are connected, so set things up
				TCPSocketAgent* remoteTCPSocketAgent = sitem->getSocketAgent();
				setConnectedTCPAgent(remoteTCPSocketAgent); // connect agents together
				remoteTCPSocketAgent->setConnectedTCPAgent(this); // connect agents together
				TCPSocketApp *remoteTCPSocketApp = remoteTCPSocketAgent->getTCPApp();
 				if (remoteTCPSocketApp!=NULL) remoteTCPSocketApp->connect(tcpSocketApp); // connect the apps together 
	 			TCPSocketAgent::getSocketConnections()->remove(sitem); // remove placeholder, connection is complete 
				PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived Connection setup complete\n");
				}
			else {
				PLOG(PL_FATAL, "TCPSocketAgent::tcpEventReceived - Could not retrieve remoteSocketAgent for connection on %i\n", getTCPAgent()->addr());
		 		exit(0);
				} 
	} else if (event->getType()==TCPEvent::SENDACK) {
		PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived SENDACK Event on node %i\n", getTCPAgent()->addr());		
		tcpAgentInterface->sendpacket(0, 0, TH_IANS_SEND, 0, 0, 0);
		return true;
	} else if (event->getType()==TCPEvent::SEND) {
		PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived SEND Event on node %i\n", getTCPAgent()->addr());		
		// Do nothing, let it be passed along to app
	} else if (event->getType()==TCPEvent::RECEIVE) {
		PLOG(PL_DETAIL, "TCPSocketAgent::tcpEventReceived RECEIVE Event on node %i\n", getTCPAgent()->addr());
		PLOG(PL_DETAIL, "TCPSocketAgent::tcpEventReceived Receiving %s of size %i at node %i\n", (char *)event->getData(), event->getDataSize(), getTCPAgent()->addr());
	} 
	else if (event->getType()==TCPEvent::DISCONNECTED) {
		PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived DISCONNECT Event on node %i\n", getTCPAgent()->addr());
		// States
	 	// if (state_==TCPS_CLOSE_WAIT)
		//		flags=1;
	    //		if (state_==TCPS_FIN_WAIT_2)
	   //		flags=2;
		if (event->getFlags()==1)
			state_=CLOSEWAIT;
		else
			state_=FINWAIT;
	} else {
		PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived UNKNOWN Event on node %i\n", getTCPAgent()->addr());
		return false;
		}  
		     
//	assert(tcpListener_);

	PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived Passing it onto listeners \n");	
	// If a listener exists pass on the event
	if (tcpListener_) {
		event->setSourceObject(this); // state it came from this object, so application can keep track
		PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived listener being invoked now \n");
		tcpListener_->tcpEventReceived(event);
	}

	if (event->getType()==TCPEvent::CONNECTED) { // If we receives a CONNECT, then also generate a SEND to indicate 
												 // the socket can be written to
		if (tcpListener_) 
			tcpListener_->tcpEventReceived(new TCPEvent(TCPEvent::SEND, NULL, NULL, 0));
	}
	
	PLOG(PL_DEBUG, "TCPSocketAgent::tcpEventReceived finished Callback\n");
	
	return true;
}
 
// Private Method Implementations
 
void TCPSocketAgent::setTCPParameter(const char *parName, const char *parValue) {
	Tcl& tcl = Tcl::instance();    
	
//	cout << "Setting parameter " << parName << " to " << parValue << " on agent " << tcpAgent->name() << " on node: " << tcpAgent->addr() << endl;

	tcl.evalf("%s set %s %s", tcpAgent->name(), parName, parValue);
}
 
/**
 * Attaches the tcpAgent i.e. a FullTcp variant to this Ns-2 node.
 */
bool TCPSocketAgent::attachTCPAgentToNode(const char *nodeNameInTCL, nsaddr_t thePort) {
	Tcl& tcl = Tcl::instance();    

	nodeNameInTCL_ = new char[strlen(nodeNameInTCL)];	
	strcpy(nodeNameInTCL_,nodeNameInTCL); // sets the name of the node to its TCL parameter for future use

    if (thePort >= 0) {
		PLOG(PL_DEBUG, "Running: %s attach %s %hu\n", nodeNameInTCL_, tcpAgent->name(), thePort);	    
        tcl.evalf("%s attach %s %hu", nodeNameInTCL_, tcpAgent->name(), thePort);
		}
    else {
    	PLOG(PL_DEBUG, "TCPSocketAgent::attachTCPAgentToNode - Attaching on ANY available port -1 was specified\n");
		PLOG(PL_DEBUG, "Running: %s attach %s\n", nodeNameInTCL_, tcpAgent->name());	    
        tcl.evalf("%s attach %s", nodeNameInTCL_, tcpAgent->name());
	}

	// should I be tracking the return from tcl.eval here?

	tcpAgentIsAttached=true;
    	
	return true; 
	}

/**
 * Atatches the TCP agent to the node that the provided agent is attached to. The provide agent must have
 * been attached before this method is called, otherwise we cannot get a reference to the node. 
 */
bool TCPSocketAgent::attachTCPAgentToNode(Agent *myAgent, nsaddr_t thePort) {
	Tcl& tcl = Tcl::instance();    
     
	tcl.evalf("%s set node_", myAgent->name());
	
	const char *nodeName = tcl.result();
		
	return attachTCPAgentToNode(nodeName, thePort);
} 

bool TCPSocketAgent::dettachFromNsNode() {	
	PLOG(PL_DEBUG, "TCPSocketAgent::dettachFromNsNode called\n");
	
	if (!tcpAgentIsAttached) return false;
	else {	
		PLOG(PL_DEBUG, "TCPSocketAgent::Agent Active, Detaching Agent\n");
		
		Tcl& tcl = Tcl::instance();	
		PLOG(PL_DEBUG,  "TCPSocketAgent::Got TCL Instance\n");
		tcl.evalf("Simulator instance");
		PLOG(PL_DEBUG, "TCPSocketAgent::Evaluated TCL instance\n");
        char simName[32];
	    strncpy(simName, tcl.result(), 32);

		PLOG(PL_DEBUG, "Detach, Running: %s detach-agent %s %s\n", simName, nodeNameInTCL_, tcpAgent->name()); 

		tcl.evalf("%s detach-agent %s %s", simName, nodeNameInTCL_, tcpAgent->name()); 
		
		tcpAgentIsAttached=false;

		PLOG(PL_DEBUG, "TCPSocketAgent::dettachFromNsNode Ended\n");
		
		return true;
		}
    }
 

bool TCPSocketAgent::setTCPAgentProtocol(TcpProtocol theTCPProtocol){
	Tcl& tcl = Tcl::instance();    
	
	theTCPProtocol_=theTCPProtocol;
	
	switch (theTCPProtocol) {
		case FULLTCP :  tcl.evalf("eval new Agent/TCP/FullTcp/Events");
			PLOG(PL_DEBUG, "Created FULLTCP Agent\n");
			break;
		case TAHOEFULLTCP : tcl.evalf("eval new Agent/TCP/FullTcp/Tahoe");
			PLOG(PL_DEBUG,"Created FULLTCP Tahoe Agent");
			break;
		case RENOFULLTCP : tcl.evalf("eval new Agent/TCP/FullTcp/Newreno");
			PLOG(PL_DEBUG, "Created FULLTCP Newreno Agent\n");
			break;
		case SACKFULLTCP : tcl.evalf("eval new Agent/TCP/FullTcp/Sack");
			PLOG(PL_DEBUG, "Created FULLTCP Sack Agent\n");
			break;
		default : protocolError("TCPSocketAgent: No TCP Identifier Specified");
	}
	
	const char *tcpVar = tcl.result();
	 
    tcpAgent = (FullTcpAgent *)tcl.lookup(tcpVar);

	// cout << "tcpAgent Name " << tcpAgent->name() << endl;

    if (tcpAgent==NULL) {
		PLOG(PL_ERROR, "TCPSocketAgent: Cannot instantiate TCP Agent %s\n", tcpVar);
		exit(1);
		}
// UNDO HERE 	    	 
	createTCPApplication(tcpAgent);
		
	// hook up the callback mechanism from the TCP agents to the Apps
	
	switch (theTCPProtocol) {
		case FULLTCP : ((TCPFullWithEvents *)tcpAgent)->setTCPSocketAgent(this);
			tcpAgentInterface = (TCPAgentInterface *)((TCPFullWithEvents *)tcpAgent);
	 		break; 
		case TAHOEFULLTCP : PLOG(PL_ERROR, "Tahoe Full TCP, Not supported\n");
			exit(1);
			break;
		case RENOFULLTCP : PLOG(PL_ERROR, "Reno Full TCP, Not supported\n");
			exit(1);
			break;
		case SACKFULLTCP : PLOG(PL_ERROR, "Sack Full TCP, Not supported\n");
			exit(1); 
			break;
		case ERROR : 
			return false;
	}
	
	return true;
 	}

void TCPSocketAgent::protocolError(const char *theTCPProtocolString) {
	PLOG(PL_ERROR, "TCPSocketAgent: Unable to identify TCP Protocol String %s\n", theTCPProtocolString);
	PLOG(PL_ERROR, "TCPSocketAgent: Allowed Types are \n");
	PLOG(PL_ERROR, "FULLTCP, RENOFULLTCP, SACKFULLTCP, TAHOEFULLTCP\n");
	exit(1);
}
 
/**
 * Creates the TCP application for sending data between TCP nodes. 
 */
bool TCPSocketAgent::createTCPApplication(FullTcpAgent *theTCPAgent) {
	
	Tcl& tcl = Tcl::instance();    

	tcl.evalf("eval new Application/TCPSocketApp %s", theTCPAgent->name());
 	
	const char *tcpVar = tcl.result();
	
    tcpSocketApp = (TCPSocketApp *)tcl.lookup(tcpVar);

	// Create Socket app and binds the TCPSocketApp with the underlying TCP protocol

	// cout << "Created Socket App " << tcpSocketApp->name() << " on node " << theTCPAgent->addr() << endl;
		
	tcpSocketApp->setTCPSocketAgentListener(this); // add ourselves as a listener for events - i.e. we get notified
 	
	return true;
    }
 	
	
int TCPSocketAgent::command(int argc, const char*const* argv) {
	Tcl& tcl = Tcl::instance();    
	PLOG(PL_DEBUG, "Node = %s, command = %s\n", name(), argv[1]);
	
    for (int i =0; i<argc; ++i)
		PLOG(PL_DEBUG," arg[%i] = %s," , i, argv[i]);

	PLOG(PL_DEBUG,"\n");

	if (argc==2) {
		if (strcmp(argv[1], "listen") == 0) {
			listen(port());
			return (TCL_OK);
		} else if (strcmp(argv[1], "detach-from-node") == 0) {
			dettachFromNsNode(); 
			return (TCL_OK);
			}
	} else if (argc==3) {	
		if (strcmp(argv[1], "setProtocol") == 0) {
			setTCPAgentProtocol(getTCPProtocolFromString(argv[2])); 
			return (TCL_OK);
		} else if (strcmp(argv[1], "attach-to-node") == 0) {
			attachTCPAgentToNode(argv[2],0); 
			return (TCL_OK);
		} else if (strcmp(argv[1], "tcp-connect") == 0) {
			TCPSocketAgent *tcpSocket = (TCPSocketAgent *)TclObject::lookup(argv[2]);
			if (tcpSocket == NULL) {
				tcl.resultf("%s: connected to null object.", name_);
				return (TCL_ERROR);
			}
			connect(tcpSocket);
 		
			return (TCL_OK);
		}
	} else if (argc==4) {
		// Pass all the set commands to the TCL agent e.g. $tcp1 set window_ 100
		if (strcmp(argv[1], "set-var") == 0) { 
			setTCPParameter(argv[2], argv[3]);
		
			return (TCL_OK);
		} else if (strcmp(argv[1], "tcp-connect") == 0) {
		//	cout << "Trying to connect " << argv[2] << " port " << argv[2] << endl;
			nsaddr_t address = atoi(argv[2]);
			nsaddr_t port = atoi(argv[3]);
			connect(address,port);
			return (TCL_OK);
		} else if (strcmp(argv[1], "send") == 0) {
			const char *bytes = argv[3];
			int size = atoi(argv[2]);
		
//			cout << "Sending " << bytes << ", size " << size << " from node " << getTCPAgent()->addr() << endl;
		
			send(size,bytes); // the socketapp actually sends the num bytes to the protocol through our previous app 
							  // attachment layer. We just have to pass the data across.
		
			return (TCL_OK);
		}
	}
	
	return Agent::command(argc, argv);
}    



/**
 * Override the TCP full and add detection of Connect SYN and ACK packets so we can notify our 
 * application of the progress of a connection.  
 States:
 define TCPS_CLOSED             0       closed 
 #define TCPS_LISTEN             1       listening for connection 
 #define TCPS_SYN_SENT           2       active, have sent syn 
 #define TCPS_SYN_RECEIVED       3       have sent and received syn 
 #define TCPS_ESTABLISHED        4       established 
 
 TCPS_CLOSE_WAIT			5	rcvd fin, waiting for app close - this is when the other side of the connection sends a FIN 	
 TCPS_FIN_WAIT_2         9   have closed, fin is acked - this is when the FIN has been acked by the sender 
 
 other internal states for close but not when receiving, 
 
 TCPS_FIN_WAIT_1         6        have closed, sent fin 
 TCPS_CLOSING            7        closed xchd FIN; await FIN ACK 
 TCPS_LAST_ACK           8        had fin and close; await FIN ACK 
 
 tcp_flags
 
 #define TH_FIN  0x01        FIN: closing a connection 
 #define TH_SYN  0x02        SYN: starting a connection 
 #define TH_PUSH 0x08        PUSH: used here to "deliver" data 
 #define TH_ACK  0x10        ACK: ack number is valid 
 #define TH_ECE  0x40        ECE: CE echo flag 
 #define TH_CWR  0x80        CWR: congestion window reduced 
 */
void TCPFullWithEvents::recv(Packet *pkt, Handler *handler) {		
	bool passtoTCP=true;
	
	PLOG(PL_DEBUG, "TCPFullwithEvents, node %u RECV, processing packet ....\n", getAddress());
	
	hdr_tcp *tcph = hdr_tcp::access(pkt); // tcp header
	int tiflags = tcph->flags() ; 		 // tcp flags from packet
	
	PLOG(PL_DEBUG, "TCPFullwithEvents, Flags = %u ....\n", tiflags);
	
/*	if (pkt->userdata()!=NULL) { // Data receive by normal non server agent
		PLOG(PL_DEBUG, "TCPFullwithEvents, PUSH State\n");
		tcpdata_ = pkt;
		passtoTCP=true;			
	} else */
	if (tiflags & TH_FIN) { // Other side is closing the connection
		PLOG(PL_DEBUG, "TCPFullwithEvents, FIN State\n");
		if (state_==0) return;
		PLOG(PL_DEBUG, "TcpFullWithEvents: Node %u Received a FIN, state is: %i\n", getAddress(), state_);
		int flags=0; // general - i.e. active close from application
		// pass state across to TCPSocket so it knows what to do
		if (state_==TCPS_CLOSE_WAIT)
			flags=1;
		if (state_==TCPS_FIN_WAIT_2)
			flags=2;
		TCPEvent *event = new TCPEvent(TCPEvent::DISCONNECTED, NULL, NULL, 0);
		event->setFlags(flags);
 		FullTcpAgent::recv(pkt, handler); // get TCP agent to process request before shutting down the socket.
		if (tcpsocket_!=NULL)
			tcpsocket_->tcpEventReceived(event);
		passtoTCP=false;
	} else if ((state_ == TCPS_SYN_SENT) && (tiflags & TH_ACK)) {
		PLOG(PL_DEBUG, "TCPFullwithEvents, node %u CONNECTED Event\n", getAddress());
		if (tcpsocket_!=NULL)
			tcpsocket_->tcpEventReceived(new TCPEvent(TCPEvent::CONNECTED, NULL, NULL, 0));
	} else if (tiflags & TH_ACK) { 
		PLOG(PL_DEBUG, "TCPFullwithEvents, ACK State\n");
		int seqno = tcph->seqno();
		int ackno = tcph->ackno();
		PLOG(PL_DETAIL, "ACK Received by node %u with sequence number %i, ackno %i\n", getAddress(), seqno, ackno);
		if (awaitingFinalAck) {
			PLOG(PL_DETAIL, "Final ACK received, shuttung down socket on node %u\n", getAddress());
			TCPEvent *event = new TCPEvent(TCPEvent::DISCONNECTED, NULL, NULL, 0);
			event->setFlags(2);
			FullTcpAgent::recv(pkt, handler); // get TCP agent to process request before shutting down the socket.
			if (tcpsocket_!=NULL)
				tcpsocket_->tcpEventReceived(event);
			passtoTCP=false;
		}
	} else if (tiflags & TH_IANS_SEND) { // My SEND packet
		PLOG(PL_DEBUG, "TCPFullwithEvents, GOT A SEND\n");
		passtoTCP=false;
		tcpsocket_->tcpEventReceived(new TCPEvent(TCPEvent::SEND, NULL, NULL, 0));
	} 
	
	if (passtoTCP) {
		PLOG(PL_DEBUG, "TCPFullwithEvents, passing to tcp agent to handle request\n", getAddress());
		FullTcpAgent::recv(pkt, handler);
	}
	
	PLOG(PL_DEBUG, "TCPFullwithEvents, node %u: done\n", getAddress());
}


void TCPFullWithEvents::sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, Packet *p) {
	PLOG(PL_DETAIL, "TCPFullWithEvents: SENDING PACKET from %u with sequence number %i, ackno %i\n", getAddress(), seqno, ackno);	
	
	FullTcpAgent::sendpacket(seqno, ackno, pflags, datalen, reason, p);		
} 


