/*
 *  TCPSocketAgent.h
 *
 *  Created by Ian Taylor on 01/01/2007.
 *
 */

/**
	A TCPSocketAgent is an agent that is capable of creating Sockets on-the-fly for allowing multiple TCP connections.
*/

#ifndef TCPSocketAgent_h
#define TCPSocketAgent_h

#include "protoDebug.h"

#include <agent.h>
#include <ip.h>
#include <tcp-full.h>
#include <timer-handler.h>

#include <protoAddress.h>
#include <protoSocket.h>

#include "TCPSocketApp.h"
#include "TCPEvent.h"
#include "SimpleList.h"

#define TH_IANS_SEND 0x100

class TCPAgentInterface  { // IF YOU CHANGE THIS INTERFACE - DO AN NRL CLEAN BEFORE RE-RUNNING ....
public:
	virtual nsaddr_t getPort() = 0;
	virtual nsaddr_t getAddress() = 0;
	virtual void setPort(nsaddr_t thePort) = 0;
	virtual void setAddress(nsaddr_t theAddress) = 0;
	virtual nsaddr_t getDestinationPort() = 0;
	virtual nsaddr_t getDestinationAddress() = 0;
	virtual void setDestinationPort(nsaddr_t destPort) = 0;
	virtual void setDestinationAddress(nsaddr_t destAddress) = 0;
	virtual void doConnect() =0;
	
	virtual void listenOn(nsaddr_t port) = 0;
	virtual void close() = 0;
	virtual ~TCPAgentInterface() {}
	virtual void finalAck()=0;
	virtual void sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, Packet *p)=0;
};

class TcpTimer;

class TCPSocketAgent : public Agent, public TCPSocketAgentListener {		
	
public:
	// Allowed TCP protocol types and constructors
	
	TCPSocketAgent() : Agent(PT_TCP) { init(); }
	
	enum TcpProtocol {FULLTCP, RENOFULLTCP, SACKFULLTCP, TAHOEFULLTCP, ERROR};  

	enum TCPSendTriggerMode {EDGE, LEVEL};

	TCPSocketAgent(TcpProtocol theTCPProtocol, nsaddr_t thePort);
	
	TCPSocketAgent(const char *theTCPProtocolString, nsaddr_t thePort);

	TCPSocketAgent(FullTcpAgent *theTCPAgent, nsaddr_t thePort);
		
	~TCPSocketAgent() { // send a DISCONNECTED event - this will be called when a close of something happens
		PLOG(PL_DETAIL, "TCPSocketAgent destructor called \n"); 
		tcpEventReceived(new TCPEvent(TCPEvent::DISCONNECTED, this, NULL, 0));}

	// others

	void setSENDTriggerMode(TCPSendTriggerMode mode) { tcpTriggerMode=mode; }

	TCPSendTriggerMode getSENDTriggerMode() { return tcpTriggerMode; }

	static SimpleList *getSocketConnections();

	bool bind(nsaddr_t thePort);
	
	void setSimulationAgent(Agent *simAgent) { initialSimulationAgent=simAgent; }

	bool connect(nsaddr_t destAddress, nsaddr_t destPort);

	void setConnectedTCPAgent(TCPSocketAgent *remoteTCPSocket) { remoteTCPAgent= remoteTCPSocket; }
	
	TCPSocketAgent *getConnectedTCPAgent() { return remoteTCPAgent; }
	
	// returns the number of bytes sent
	unsigned int send(int nbytes, const char *data);
	
	/**
	 * Adds a listener to this socket - note that TCPSocketAgents only support ONE listener 
	 * for events from this socket.
	 */
	void setListener(TCPSocketAgentListener *listener) { tcpListener_=listener; }
		
	int command(int argc, const char*const* argv);

	FullTcpAgent *getTCPAgent() { return tcpAgent; }
	TCPAgentInterface *getTCPInterface() { return tcpAgentInterface; }
	TCPSocketApp *getTCPApp() { return tcpSocketApp; }

    // Implementation of the TCPSocketAgentListener interface - callback - where events pass through

	virtual bool tcpEventReceived(TCPEvent *event); // basic implementation here - override in your app specific one

	// Proxy methods to the underlying protocol
	
	nsaddr_t getPort() { return tcpAgentInterface ? tcpAgentInterface->getPort() : -1; } 
	nsaddr_t getAddress() { return tcpAgentInterface ? tcpAgentInterface->getAddress(): -1; } 	
	void setPort(nsaddr_t thePort) { port()=thePort; if (tcpAgentInterface) tcpAgentInterface->setPort(thePort); } 
	void setAddress(nsaddr_t theAddress) { if (tcpAgentInterface) tcpAgentInterface->setAddress(theAddress); } 	

	nsaddr_t getDestinationPort() { return tcpAgentInterface ? tcpAgentInterface->getDestinationPort() : -1; } 
	nsaddr_t getDestinationAddress() { return tcpAgentInterface ? tcpAgentInterface->getDestinationAddress(): -1; } 	
	void setDestinationPort(nsaddr_t thePort) { if (tcpAgentInterface) tcpAgentInterface->setDestinationPort(thePort); } 
	void setDestinationAddress(nsaddr_t theAddress) { if (tcpAgentInterface) tcpAgentInterface->setDestinationAddress(theAddress); } 	

	bool setTxBufferSize(unsigned int bufferSize);
	unsigned int getTxBufferSize();
	
	bool setRxBufferSize(unsigned int bufferSize);
	unsigned int getRxBufferSize(); 
	
	void setOutputNotification(bool notification) { outputNotification_=notification; }
	bool isOutputNotificationEnabled() { return outputNotification_; }

	static TcpProtocol getTCPProtocolFromString(const char *theTCPProtocolString); 			
	static const char * getTCPStringForProtocol(TcpProtocol theTCPProtocol); 			

	bool setTCPAgentProtocol(TcpProtocol theTCPProtocol);
	bool attachTCPAgentToNode(Agent *myAgent, nsaddr_t thePort);

	bool closeSocket();
	bool shutdown(); // tells socket to shutdown

	// for timer callback to trigger SEND callback
	void tcpTimerTriggered(); 
	
protected:			
	// The TCP Protocol Agent
	FullTcpAgent *tcpAgent;
	
	TCPAgentInterface *tcpAgentInterface;
	
	// The TCP Socket App
	TCPSocketApp *tcpSocketApp;
	
	// The TCP Socket agent this agent is connected to
	
	TCPSocketAgent *tcpSocketConnected;
	TcpProtocol theTCPProtocol_;
	
	TcpTimer *tcpTimer;
	
	long sequenceNumber_;

private:

	/** Removes (detaches in Ns2) the TCP agent from the ns-2 node.
	*/
	bool dettachFromNsNode();	

	bool attachTCPAgentToNode(const char *nodeNameInTCL, nsaddr_t thePort);

	void listen(nsaddr_t port) { if(tcpAgentInterface) tcpAgentInterface->listenOn(port); }

	bool connect(TCPSocketAgent *tcpSocket); // for connection to another TCP agent from tcl - internal use only
	void connect() { if(tcpAgentInterface) tcpAgentInterface->doConnect(); }
	 
	bool createTCPApplication(FullTcpAgent *theTCPAgent);
	void setTCPParameter(const char *parName, const char *parValue);
	
	void protocolError(const char *theTCPProtocolString);

	void init();

	void emptySendBuffer(); // empty send buffer after client has finished
	void createSendBuffer();
	
	unsigned int txBufferSize;
	unsigned int rxBufferSize; 
	TcpData *txBuffer;
	
	char *nodeNameInTCL_;
	TCPSocketAgentListener *tcpListener_;
    Agent *initialSimulationAgent;
	bool tcpAgentIsAttached; // whether the tcp agent has been attached or not
	
	TCPSocketAgent *remoteTCPAgent;
	
	unsigned int bytesInSendBuffer_;		
	TCPSendTriggerMode tcpTriggerMode;
	unsigned int bytesSent_; // used in level trigger mode
	bool bufferIsFull_;
	bool sendWasCalled_;
	bool outputNotification_;
	
	enum timerMode {CLOSE, SEND, SENDEVENT};
	timerMode timerTrggerMode_; 	
	enum socketState {SOCKOPEN, CLOSEWAIT, FINWAIT};
	socketState state_;
};	


class TcpTimer : public TimerHandler { 
private:
	TCPSocketAgent *tcpsocket_; 
public: 
	TcpTimer(TCPSocketAgent *tcpsocket) : TimerHandler() { tcpsocket_ = tcpsocket; } 
protected: 
	virtual void expire(Event *e) { tcpsocket_->tcpTimerTriggered(); }
}; 

// Our specific socket list mode for use with the SimpleList class (see SimpleList.cpp)

class SocketListItem : public ListItem {
public:
	SocketListItem(TCPSocketAgent *agent) : ListItem() { socket=agent; }		
	TCPSocketAgent* getSocketAgent() {return socket;}
private:
	TCPSocketAgent *socket;
};


class TCPFullWithEvents : public FullTcpAgent, TCPAgentInterface {

	
private:
	TCPSocketAgentListener *tcpsocket_;
	bool sendEvent;
	bool awaitingFinalAck;

	
public:

	Packet *tcpdata_;

	TCPFullWithEvents() : FullTcpAgent(), sendEvent(false), awaitingFinalAck(false), tcpdata_(0) {}

	void doConnect() { connect();  }
	
	void listenOn(nsaddr_t onPort) { port()=onPort; 
										listen(); }
	void close() { 
		PLOG(PL_INFO, "TCPFullWithEvents::close() Entering\n");
		FullTcpAgent::close(); 
		}

	void setDestinationPort(nsaddr_t destPort) { dport()=destPort; }
	void setDestinationAddress(nsaddr_t destAddress) { daddr()=destAddress; }

	void setPort(nsaddr_t thePort) { port()=thePort; }
	void setAddress(nsaddr_t theAddress) { addr()=theAddress; }

	nsaddr_t getPort() { return port(); }
	nsaddr_t getAddress() { return addr(); }

	nsaddr_t getDestinationPort() { return dport(); }
	nsaddr_t getDestinationAddress() { return daddr(); }

	void setTCPSocketAgent(TCPSocketAgentListener *tcpsocket) { tcpsocket_=tcpsocket; }
	
	void finalAck() {
		awaitingFinalAck=true;
		}

	~TCPFullWithEvents() { cancel_timers(); rq_.clear(); }
	
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

	 */
	void recv(Packet *pkt, Handler *handler);

	
	/*
	 * Send packet across the network
	*/
	void sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, Packet *p);
	
};


#endif 
