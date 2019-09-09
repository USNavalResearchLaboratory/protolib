/**
 *  TCPServerSocketAgent.h
 *
 *  Created by Ian Taylor on 01/03/2007.
 *  
 * A TCPServerSocketAgent to provides the server capabilities for a TCP connection.
 * The server socket contains capabilities to host multiple connections simulataneously through the
 * use of a demultiplexor that route the messages to and from the various TCPSocket agents. 
 */

#ifndef TCPServerSocketAgent_h
#define TCPServerSocketAgent_h

#include "TCPSocketAgent.h"
#include "protoDebug.h"

#include "SimpleList.h"

class TCPServerSocketAgent: public Agent {		
public:

// Constructors

	TCPServerSocketAgent() : Agent(PT_TCP) { 
		mySocketList=new SimpleList();
		state_=SOCKOPEN;
		}

	TCPServerSocketAgent(const char *theTCPProtocolString, nsaddr_t thePort) : Agent(PT_TCP) { 
		theTCPProtocol_=TCPSocketAgent::getTCPProtocolFromString(theTCPProtocolString); 
		port()=thePort; 
		mySocketList=new SimpleList();
		state_=SOCKOPEN;
		}

	~TCPServerSocketAgent() { delete mySocketList; }
		

	
// Methods

	void setSimulationAgent(Agent *simAgent) { initialSimulationAgent=simAgent; }
	
	bool bind(nsaddr_t thePort);

	bool setTCPAgentProtocol(TCPSocketAgent::TcpProtocol theTCPProtocol) { theTCPProtocol_=theTCPProtocol; return true; }
	
	/**
	 * Adds a listener to this socket - note that TCPSocketAgents only support ONE listener 
	 * for events from this socket.
	 */
	void setListener(TCPSocketAgentListener *listener) { tcpListener_=listener; }

	TCPSocketAgent* accept();
		
	void listen() {} // nothing to do - this is set up by accept. Aserver socket is always listening

	bool closeSocket();
	bool shutdown();

	void recv(Packet *pkt, Handler *handler);
	
	int command(int argc, const char*const* argv);
	
private:		

	/**
	 * Detaches this socket sever agent from the NS2 node.
	 */
	bool dettachFromNsNode();	

    Agent *initialSimulationAgent;

	bool attachTCPServerToNode(Agent *myAgent, nsaddr_t thePort);
	
	bool attachTCPServerToNode(const char *node, nsaddr_t thePort);

	Packet *synPacket_;
	Handler *handler_;
	TCPSocketAgentListener *tcpListener_;
	TCPSocketAgent::TcpProtocol theTCPProtocol_;

	bool tcpAgentIsAttached; // whether the tcp agent has been attached or not
	char *nodeNameInTCL_;
	
	SimpleList *mySocketList;
	
	enum socketState {SOCKOPEN, SOCKCLOSED};
	
	socketState state_;

};

#endif
