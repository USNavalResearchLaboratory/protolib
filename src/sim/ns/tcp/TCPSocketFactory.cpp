/*
 *  TCPSocketFactory.cpp
 *  FullerTCP
 *
 *  Created by Ian Taylor on 07/03/2007.
 *
 */

#include "TCPSocketFactory.h"

Agent *TCPSocketFactory::instantiateAgent(const char *tclAgent) {
	Tcl& tcl = Tcl::instance();    
 
	tcl.evalf("eval new %s", tclAgent);
	
	const char *tcpVar = tcl.result();
 
	if (tcpVar==NULL) return NULL;
		
	return (Agent *)tcl.lookup(tcpVar);
}
 
Agent *TCPSocketFactory::instantiateAgent(const char *tclAgent, const char *par1) {
	Tcl& tcl = Tcl::instance();    

	tcl.evalf("eval new %s %s", tclAgent, par1);
	
	const char *tcpVar = tcl.result();
 
	if (tcpVar==NULL) return NULL;
		
	return (Agent *)tcl.lookup(tcpVar);
}

Agent *TCPSocketFactory::instantiateAgent(const char *tclAgent, const char *par1, const char *par2) {
	Tcl& tcl = Tcl::instance();    

	tcl.evalf("eval new %s %s %s", tclAgent, par1, par2);
	
	const char *tcpVar = tcl.result();
 
	if (tcpVar==NULL) return NULL;
		
	return (Agent *)tcl.lookup(tcpVar);
}

// CLIENTS

TCPSocketAgent *TCPSocketFactory::createSocketAgent(Agent *forMyAgent){
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/SocketAgent");
	if (socket==NULL)
		return NULL;
	else {
		TCPSocketAgent *socketAgent = (TCPSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
} 
 	 
TCPSocketAgent *TCPSocketFactory::createSocketAgent(Agent *forMyAgent, nsaddr_t thePort) {
	char portString[16];
 		
	sprintf(portString, "%i", thePort);
		
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/SocketAgent FULLTCP", portString);
	if (socket==NULL)
		return NULL;
	else {
		TCPSocketAgent *socketAgent = (TCPSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}
		
TCPSocketAgent *TCPSocketFactory::createSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol, nsaddr_t thePort) {
	const char *protocol;
	char portString[16];

	protocol = TCPSocketAgent::getTCPStringForProtocol(theTCPProtocol);
	sprintf(portString, "%i", thePort);
		
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/SocketAgent", protocol, portString);
	if (socket==NULL)
		return NULL;
	else {
		TCPSocketAgent *socketAgent = (TCPSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}


TCPSocketAgent *TCPSocketFactory::createSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol) {
	const char *protocol;

	protocol = TCPSocketAgent::getTCPStringForProtocol(theTCPProtocol);
		
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/SocketAgent", protocol);

	if (socket==NULL)
		return NULL;
	else {
		TCPSocketAgent *socketAgent = (TCPSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}


// SERVERS

TCPServerSocketAgent *TCPSocketFactory::createServerSocketAgent(Agent *forMyAgent) {
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/ServerSocketAgent");
	if (socket==NULL)
		return NULL;
	else {
		TCPServerSocketAgent *socketAgent = (TCPServerSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}
		
TCPServerSocketAgent *TCPSocketFactory::createServerSocketAgent(Agent *forMyAgent, nsaddr_t thePort) {
	char portString[16];
		
	sprintf(portString, "%i", thePort);
		
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/ServerSocketAgent FULLTCP", portString);
	if (socket==NULL)
		return NULL;
	else {
		TCPServerSocketAgent *socketAgent = (TCPServerSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}
		
TCPServerSocketAgent *TCPSocketFactory::createServerSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol, nsaddr_t thePort) {
	char portString[16];
	const char *protocol;

 	protocol = TCPSocketAgent::getTCPStringForProtocol(theTCPProtocol);
	sprintf(portString, "%i", thePort);
		
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/ServerSocketAgent", protocol, portString);
	if (socket==NULL)
		return NULL;
	else {
		TCPServerSocketAgent *socketAgent = (TCPServerSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}

TCPServerSocketAgent *TCPSocketFactory::createServerSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol) {
	const char *protocol;

	protocol = TCPSocketAgent::getTCPStringForProtocol(theTCPProtocol);
		
	Agent* socket = TCPSocketFactory::instantiateAgent("Agent/TCP/ServerSocketAgent", protocol);
	
	if (socket==NULL)
		return NULL;
	else {
		TCPServerSocketAgent *socketAgent = (TCPServerSocketAgent *)socket;
		socketAgent->setSimulationAgent(forMyAgent);
		return socketAgent;
	}
}
