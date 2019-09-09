/*
 *  TCPProtoSocketAgent.h
 *
 */

#ifndef TCPProtoSocketAgent_h
#define TCPProtoSocketAgent_h

#include <agent.h>
#include <ip.h>

#include "./tcp/TCPSocketAgent.h"
#include "./tcp/TCPServerSocketAgent.h"
#include "./tcp/TCPSocketFactory.h"
#include "nsProtoSimAgent.h"
#include "protoDebug.h"

/**
 * @class NsTCPProtoSocketAgent
 *
 * @brief TCP implementation of the NSSocketProxy 
 *	 class for providing TCP support within Protolib.  
 *
 *	 This class provides a hub for interfacing with 
 *	 the underlying TCP toolkit for providing access 
 *	 to the TCP support.  NsProtoTCPSocketAgent 
 *   essentially auto-detects which sockets it should 
 *	 be (i.e. a client or a server) and then instantiates 
 *	 the underlying implementations in order to provide 
 *	 that behaviour, which are either a TCPSocketAgent
 *	 or a TCPServerSocketAgent:
 */

class NsTCPProtoSocketAgent : public Agent, public TCPSocketAgentListener, public NsProtoSimAgent::NSSocketProxy {		
	private:
		// either a server or a TCP pipe i.e. one to one connection. Once connected we deal with TCPSockets directly.
		// every socket becomes a TCPSocket after the first connection request. The server side doesn't care whether 
		// its a server or not, it just receives and writes to a TCP pipe.
		
		TCPServerSocketAgent *serverSocket;
		TCPSocketAgent *tcpPipe;

		// for clients, the state changes from NotInitialised to TcpSocket
		// for servers, the state changes from NotInitialised to TcpServerSocket, then finally to TcpSocket once a connection
		// has been made
		enum SocketType {TcpServerSocket, TcpSocket, NotInitialised};  
	
		SocketType socketType_;
		
		bool isOpen;
		bool outputNotification;
						
	public:
		NsTCPProtoSocketAgent();
		        
		bool SendTo(const char* buffer, unsigned int& numBytes, const ProtoAddress& dstAddr);
		
// implemnted from NSSocketProxy for TCP 

		bool Connect(const ProtoAddress& theAddress);
		
		bool Accept(ProtoSocket* theSocket);

		bool Listen(UINT16 thePort);

		bool Bind(UINT16& thePort) {
			port()=thePort;
			return true; 
			} 

		bool SetOutputNotification(bool outputnotify);
					
		// Custom Close method for our TCP sockets ...
		bool Close();
		bool Shutdown();
			
		// What do we do here for groups?
		
		bool JoinGroup(const ProtoAddress& groupAddr) { return true; }
        bool LeaveGroup(const ProtoAddress& groupAddr) { return true; }
		
		void setTCPSocketAgent(TCPSocketAgent *tcpSocketAgent) { tcpPipe=tcpSocketAgent; socketType_=TcpSocket;}

		TCPSocketAgent *getTCPSocketAgent() { return tcpPipe; }
		
		// My callback from the TCPSocketAgents

	    bool tcpEventReceived(TCPEvent *event); // basic implementation here - override in your app specific one

		// override basic implementations to pass along to real agent:
		
		bool SetTxBufferSize(unsigned int bufferSize) { if (tcpPipe) tcpPipe->setTxBufferSize(bufferSize); else return false; return true; } 
		unsigned int GetTxBufferSize() { if (tcpPipe) return tcpPipe->getTxBufferSize(); else return 0; } 
		bool SetRxBufferSize(unsigned int bufferSize) { if (tcpPipe) tcpPipe->setRxBufferSize(bufferSize); else return false; return true; } 
		unsigned int GetRxBufferSize() { if (tcpPipe) return tcpPipe->getRxBufferSize(); else return 0; } 

};  // end class TCPProtoSocketAgent
 
 
 
#endif
