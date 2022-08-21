
/**
  TCPSocketApp: A class to use the ADU NS-2 functionality to implement
  a bi directional TCP pipe for transfering data over an NS-2 TCP connection.
  The connection is abstracted as a virtual pipe because rather than extending
  a TCP protocol to implement these extensions, we implement the sending of data
  as a separate entity which simply uses an external buffer at the application-level
  to buffer the data. TCPSocketApp then waits until NS-2 simulates the 
  transfer before retrieving the data from the pipe at thje receiver side of
  the connection.
  
  TCPSocketApp subclasses from Application as the pipe is modelled as a
  connection between two applications wishing to transfer data using a TCP
  protocol. We use the exisisting NS-2 mechanism for representing application
  data (ADU - Application Data Unit) and implement other necessities for 
  TCP real-world applications, ike the ability to send data ion both 
  directions. This class basically extends the existing tcpapp code in NS-2
  to enhance that functionality.    
  
  TCPSocketApp model the TCP connection as a FIFO byte stream. It shouldn't 
  be used if this assumption is violated.
*/
 
#ifndef ns_TCPSocketApp_h
#define ns_TCPSocketApp_h

#include "protoDebug.h"

#include "app.h"
#include "TCPData.h"
#include "SimpleList.h"

#include "TCPEvent.h"

#include <tcp-full.h>

class TCPSocketApp : public Application {
public:	
	TCPSocketApp(Agent *tcp);
	~TCPSocketApp();

// These are the key methods here, that interface with TCP socket agent
	void send(AppData *data);
	void setBytesSent(int bytes) { bytesSent_+=bytes; }
	void recv(int nbytes);

	void setTCPAgent(Agent *tcp);
	
	void setTCPSocketAgentListener(TCPSocketAgentListener *listener) { tcpSocketListenerAgent = listener; }

	FullTcpAgent *getTCPAgent() { return (FullTcpAgent *)agent_; }
		
	void connect(TCPSocketApp *socketApp) { // connect both ways for two way transfer
									  connectedSocketApp = socketApp; socketApp->connectedSocketApp=this; }

	void upcallToApp(char *data, unsigned int size); 
	AppData* get_data(int&, AppData*);

	// Do nothing when a connection is terminated
	virtual void resume();


protected:
	virtual int command(int argc, const char*const* argv);
	TcpData* receiveDataFromClient(); // calls client to retrieve the actual application data

	// We don't have start/stop
	virtual void start() { abort(); } 
	virtual void stop() { abort(); }

private:

	void getDataFromOtherSocket();
	TCPSocketApp *connectedSocketApp;
	SimpleList tcpDataList_;
	TcpData *curdata_;
	int readOffset;
	int bytesSent_;
	int curbytes_;
	TCPSocketAgentListener *tcpSocketListenerAgent;
};

#endif // ns_TCPSocketApp_h
