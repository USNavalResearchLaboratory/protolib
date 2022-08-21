#ifndef _TCPEvent
#define _TCPEvent

#include "app.h"

/**
 *  TCPEvent.h: is a container for events created through a typical TCP session. Some
 * events are generated during the negotiation of a connection, whilst others are generated when
 * data is received or when data is sent and the output buffer is ready for writing to again. The
 * following events are supported: 
 *
 * <ol>
 *  
 * <li> <bf>ACCEPT:</bf> this is generated when a server receives a SYN from a client to request
 * a connection.  An application normally responds by invoking the accept() method call in
 * order to accept the incoming connection request.
 *
 * <li> <bf>CONNECTED:</bf> This is generated on the client once they receive the ACK in response to
 * a SYN connection request.  Once the ACK is returned from the server, it is assumed that the
 * server has accepted the connection. A connected event is also fired after a server has accepted 
 * a connection.
 *
 * <li> <bf>SEND:</bf> a send event is generated for a client or serv er to indicate that the output buffer has
 * been emptied and is ready to accomodate new data i.e. the client or server can now SEND data.
 * 
 * <li> <bf>RECEIVE:</bf> a RECEIVE event is generated when data has been received at the client or server.
 * 
 * <li> <bf>DISCONNECT:</bf> is generated when a socket has been disconnected.
 *
 * </ol>
 *
 *  Created by Ian Taylor on 11/01/2007.
 * 
 */
class TCPEvent {	

	public:	
		enum Event {ACCEPT, CONNECTED, SEND, RECEIVE, DISCONNECTED, SENDACK};  
		
		TCPEvent(Event theType, void *source, void *theData, int dataSize);
		 
		void setSourceObject(void *source) { sourceObject=source; }
		void setFlags(int theflags) { flags=theflags; }
		
		void *getData() { return data;}
		void *getSource() { return sourceObject;}
		int getDataSize() { return dataSize;}
		int getFlags() { return flags;}
		Event getType() { return type;}

	private:
		void *sourceObject;
		void *data;
		Event type;
		int dataSize;
		int flags;
};
	
	
/**
 *  TCPSocketAgentListener.h
 *  FullerTCP
 *
 *  Created by Ian Taylor on 05/01/2007.
 *
 */
class TCPSocketAgentListener {
	public:	
		virtual bool tcpEventReceived(TCPEvent *event)=0;
		virtual ~TCPSocketAgentListener() {}
};


#endif // _TCPEvent
	