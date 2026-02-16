/*
 *  TCPSocketExample.h
 *
 *  Created by Ian Taylor on 01/03/2007.
 *
 */

#include <agent.h>
#include <ip.h>

#include "TCPSocketAgent.h"
#include "TCPServerSocketAgent.h"

#include "TCPSocketFactory.h"

// C++ example ...  

class TCPSocketExample : public Agent, TCPSocketAgentListener {
public:
	int CLIENT_CONNECTIONS; 
	int SERVER_CONNECTIONS; 
	int PORT;

	TCPSocketAgent **clients;
	TCPServerSocketAgent *server;
	TCPSocketAgent **serversockets;
	
	int connectTo; // address that the server is on
	
	int sockno;
	int transmits;
	int leftoverread;
	unsigned int *startByte;
	int msgsize;

	TCPSocketAgent::TCPSendTriggerMode sendMode;
		
	TCPSocketExample() : Agent(PT_TCP) { 
		CLIENT_CONNECTIONS=1; 
		SERVER_CONNECTIONS=1; 
		PORT=55; 
		sockno=0; 
		transmits=0;
		SetDebugLevel(PL_DETAIL);
		startByte=0;
		sendMode=TCPSocketAgent::LEVEL;
		leftoverread=0;
		msgsize=53;
		}
	
	~TCPSocketExample() {
		PLOG(PL_INFO, "TCPSocketExample: Destructing\n"); 
		free(startByte);
		for (int i=0; i<CLIENT_CONNECTIONS; ++i) 
			delete clients[i];
		delete[] clients;
		delete server;
		for (int i=0; i<SERVER_CONNECTIONS; ++i) 
			delete serversockets[i];
		delete[] serversockets;
		}
	
	void createServer() {
		server = TCPSocketFactory::createServerSocketAgent(this); // must set yoursef as the simulation agent
		
		if (server->bind(PORT)==FALSE) PLOG(PL_ERROR, "Problem with Binding node to port\n");
 		 
		server->setTCPAgentProtocol(TCPSocketAgent::FULLTCP);
		server->listen();
		server->setListener(this);

		PLOG(PL_INFO, "TCPSocketExample: Created server on port %i\n", server->port()); 

		PLOG(PL_INFO, "TCPSocketExample: Creating Server Sockets\n"); 
		
		serversockets = new TCPSocketAgent *[SERVER_CONNECTIONS];
		
		PLOG(PL_INFO, "TCPSocketExample: Created Server, ok\n"); 
	}
 
	void createClients() {
		clients = new TCPSocketAgent *[CLIENT_CONNECTIONS];
		startByte = (unsigned int*)malloc(CLIENT_CONNECTIONS*sizeof(unsigned int));
			
		for (int i=0; i<CLIENT_CONNECTIONS; ++i) {
			clients[i] = TCPSocketFactory::createSocketAgent(this);
			clients[i]->setTCPAgentProtocol(TCPSocketAgent::FULLTCP);
			clients[i]->bind(PORT+i);
			clients[i]->setListener(this);
			clients[i]->setTxBufferSize(200);
			startByte[i] = 0;
			clients[i]->setSENDTriggerMode(sendMode);
			clients[i]->setOutputNotification(false);
		}
	PLOG(PL_INFO, "TCPSocketExample: Created Clients\n"); 
	}
 	
	void connectClients() {
		for (int i=0; i<CLIENT_CONNECTIONS; ++i) 
			clients[i]->connect(connectTo, PORT);
		PLOG(PL_INFO, "TCPSocketExample: Connected Clients\n"); 
		}
		
	void sendData() {
		PLOG(PL_INFO, "TCPSocketExample: Sending Data\n"); 
		unsigned int bytesSent; // anything greater than 
		unsigned int bytesSending;
		bool continueloop=false;
		
		char *message;
		
		for (int i=0; i<CLIENT_CONNECTIONS; ++i) { 
			do {
				++transmits;
				if (transmits>10) break;
				message= new char[msgsize]; // max buffer size
				sprintf(message, "Hello another long message %2i from node %i and port %i", transmits, addr(), clients[i]->port());
				bytesSending= msgsize - startByte[i]; // send actual length
				PLOG(PL_INFO, "TCPSocketExample: StartByte = %u \n", startByte[i]);
				PLOG(PL_INFO, "TCPSocketExample: Sending %u bytes = %s\n", bytesSending, (message+startByte[i])); 
				bytesSent = clients[i]->send(bytesSending, (const char *)(message+startByte[i]));
				PLOG(PL_INFO, "TCPSocketExample: Sent %u bytes\n", bytesSent); 
				if (bytesSent<bytesSending) { 
					startByte[i]=bytesSent;
					--transmits;
					PLOG(PL_INFO, "TCPSocketExample: Out of space in send buffer - startByte = %u \n", startByte);
				} else startByte[i]=0;			
				delete[] message;
				
				if ((sendMode ==TCPSocketAgent::EDGE) || (!clients[i]->isOutputNotificationEnabled())) {
					if (bytesSent==bytesSending)
						continueloop=true;
					else
						continueloop=false; 
					}
				else {
					continueloop=false;
				}
			} while (continueloop);
		}
	PLOG(PL_INFO, "TCPSocketExample: send, leaving ...\n");
	}

	void recv(char *databuf, int size, unsigned int from) {
		int offset=0;
		int toread;
		PLOG(PL_INFO, "TCPSocketExample: Data Arrived %i bytes\n", size);
		do {
			if (leftoverread>0) 
				toread=leftoverread;
			else
				toread=msgsize;

			if (offset+toread > size) {
				toread = size-offset;
				leftoverread=msgsize-toread;				
			} else
				leftoverread=0;				

			PLOG(PL_INFO, "TCPSocketExample: Reading %i bytes\n", toread);

			char *message = new char[toread];
			memcpy(message, databuf+offset, toread);
			PLOG(PL_INFO, "TCPSocketExample: Received data \"%s\" of size %i at node %i\n", message,toread,from);
			offset+=toread;
			delete message;			
		} while(offset<size);
	}
	
	virtual bool tcpEventReceived(TCPEvent *event) {
		PLOG(PL_INFO, "TCPSocketExample: An Event !, Type %i \n", event->getType()); 

		if (event->getType()==TCPEvent::SEND) {
			PLOG(PL_INFO, "TCPSocketExample: SEND Received - buffer ready to write\n");
			sendData();
			return true;
		}
		else if (event->getSource()==server) {
			if (event->getType()==TCPEvent::ACCEPT) {
				PLOG(PL_INFO, "TCPSocketExample: ACCEPT Notification\n");
	 			serversockets[sockno] = server->accept(); // accept the connection and get the socket responsible for it
				serversockets[sockno]->setListener(this); // set the listener for RECEIVE events to this class 
	 			++sockno; 
				return true;
			}		
		} else 	if (event->getType()==TCPEvent::RECEIVE) {
			PLOG(PL_INFO, "TCPSocketExample: Received Data\n");
			recv((char *)event->getData(), event->getDataSize(), this->addr());
			return true;
		} else	if (event->getType()==TCPEvent::CONNECTED) {
			PLOG(PL_INFO, "TCPSocketExample: CONECTED to Server\n");
			return true;
		} else	if (event->getType()==TCPEvent::DISCONNECTED) {
			PLOG(PL_INFO, "TCPSocketExample: Socket has been successfully closed\n");
			return true;
		}
 	
	return false;
	}
		
	int command(int argc, const char*const* argv) {
		PLOG(PL_INFO, "Node = %s, command = %s\n", name(), argv[1]);
	
		for (int i =0; i<argc; ++i)
			PLOG(PL_INFO," arg[%i] = %s," , i, argv[i]);

		PLOG(PL_INFO,"\n");

		if (argc==2) {
			if (strcmp(argv[1], "createClients") == 0) {
				createClients();
				return (TCL_OK);
			} else if (strcmp(argv[1], "createServer") == 0) {
				createServer();
				return (TCL_OK);
			} else if (strcmp(argv[1], "connectClients") == 0) {
				connectClients();
				return (TCL_OK);
			} else if (strcmp(argv[1], "sendData") == 0) {
				// sendData();
				return (TCL_OK);
			} else if (strcmp(argv[1], "close") == 0) {
				server->close();
				return (TCL_OK);
			}
		} else if (argc==3) {
			if (strcmp(argv[1], "setClientConnections") == 0) {
				CLIENT_CONNECTIONS = atoi(argv[2]); 
				return (TCL_OK);
			}
			else if (strcmp(argv[1], "setServerConnections") == 0) {
				SERVER_CONNECTIONS = atoi(argv[2]); 
				return (TCL_OK);
			}
			else if (strcmp(argv[1], "setConnectTo") == 0) {
				connectTo = atoi(argv[2]); 
				return (TCL_OK);
			}
		}
	
		return Agent::command(argc, argv);
	} 
};

static class TCPSocketExampleClass : public TclClass { 
public:
	TCPSocketExampleClass() : TclClass("Agent/TCP/TCPSocketExample") {} 
	TclObject* create(int argc, const char*const* argv) {  
		if (argc != 4)
			return NULL;
		return (new TCPSocketExample());
	}
} class_TCP_Socket_Example;

