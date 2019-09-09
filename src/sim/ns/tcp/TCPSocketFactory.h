/*
 *  TCPSocketFactory.h
 *  FullerTCP
 *
 *  Created by Ian Taylor on 07/03/2007.
 *
 */

#include "TCPSocketAgent.h"
#include "TCPServerSocketAgent.h"

class TCPSocketFactory {

private:
	TCPSocketFactory() {}
	~TCPSocketFactory() {}
	
public:	
	static TCPSocketAgent *createSocketAgent(Agent *forMyAgent);
	static TCPSocketAgent *createSocketAgent(Agent *forMyAgent, nsaddr_t thePort);
	static TCPSocketAgent *createSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol);
	static TCPSocketAgent *createSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol, nsaddr_t thePort);

	static TCPServerSocketAgent *createServerSocketAgent(Agent *forMyAgent);
	static TCPServerSocketAgent *createServerSocketAgent(Agent *forMyAgent, nsaddr_t thePort);
	static TCPServerSocketAgent *createServerSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol);
	static TCPServerSocketAgent *createServerSocketAgent(Agent *forMyAgent, TCPSocketAgent::TcpProtocol theTCPProtocol, nsaddr_t thePort);

    static Agent *instantiateAgent(const char *tclAgentName);	
	static Agent *instantiateAgent(const char *tclAgent, const char *par1);
	static Agent *instantiateAgent(const char *tclAgent, const char *par1, const char *par2);
};