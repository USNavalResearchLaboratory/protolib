
//
// Tcp application interface: extension of the TCPApp code to do duplex connections to transmit 
// real application data and also allow multiple TCP connections
// 
// The single connections model underlying TCP connection as a 
// FIFO byte stream, use this to deliver user data
 
#include "agent.h"
#include "app.h"
#include "TCPSocketApp.h"

 
// TCPSocketApp that accept TCP agents as a parameter

static class TcpAppIntClass : public TclClass {
public:
	TcpAppIntClass() : TclClass("Application/TCPSocketApp") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc != 5)
			return NULL;
		Agent *tcp = (Agent *)TclObject::lookup(argv[4]);
		if (tcp == NULL) 
			return NULL;
		return (new TCPSocketApp(tcp));
	}
} class_TCPSocketApp; 

// Socket Apps that don't have any underlying TCP protocol defined - these are used
// in the dynamic scenario, where we generate these.

TCPSocketApp::TCPSocketApp(Agent *tcp) : 
	Application(), curdata_(0), readOffset(0), bytesSent_(0), curbytes_(0) {
	setTCPAgent(tcp);
}


TCPSocketApp::~TCPSocketApp() {
	agent_->attachApp(NULL);
}

void TCPSocketApp::setTCPAgent(Agent *tcp) {
	agent_ = tcp;
	agent_->attachApp(this);
} 


/**
 * This method is on the CLIENT and is called by the receiving socket to get the data that has just been send 
 * across Ns-2 by the underlying TCP transport
 */ 
TcpData* TCPSocketApp::receiveDataFromClient() { 
	PLOG(PL_DEBUG, "LJT Entering TCPSocketApp:: receiveDataFromClient\n");
	
	TcpData* data = (TcpData*)tcpDataList_.getTail();
	if (data!=NULL) tcpDataList_.remove((ListItem *)data); // SimpleList
			 
	PLOG(PL_DEBUG, "LJT Leaving TCPSocketApp:: receiveDataFromClient\n");
	return data; 	
	}
		

AppData* TCPSocketApp::get_data(int&, AppData*) {
	PLOG(PL_FATAL, "TCPSokcetApp: get_data called - aborting ...\n");
		// Not supported
	abort();
	return NULL;
	}

/**   
 * Send with calls to add data to this pipe by adding to a FIFO buffer before allowing
 * the underlying TCP implementation to make a simulated transfer.
 */
void TCPSocketApp::send(AppData *cbk) {
	PLOG(PL_DEBUG, "TCPSocketApp:: Send Called\n");

	TcpData *tcpData = (TcpData*)cbk;
	int nbytes = tcpData->getDataSize();

	PLOG(PL_DEBUG, "TCPSocketApp:: Send Called after cast\n");

	tcpDataList_.prepend(tcpData); // SimpleList

	PLOG(PL_DETAIL, "TCPSocketApp:: Send Called, sending %i bytes\n", tcpData->getDataSize());
	PLOG(PL_DETAIL, "TCPSocketApp sending: %s\n",  tcpData->getData());

// send number of bytes to the underlying protocol i.e. FullTCP or whatever

	PLOG(PL_DEBUG, "TCPSocketApp:: Sending bytes now \n"); 

	Application::send(nbytes);

	if (connectedSocketApp==NULL) {
		PLOG(PL_ERROR, "TCPSocketApp::getDataFromOtherSocket, Destination for SocketApp is NULL -> the setup is incorrect. Check the code\n");
		exit(0);
		}
	
	connectedSocketApp->setBytesSent(nbytes); // so we can check they all are received for SEND event
	
	PLOG(PL_DEBUG, "TCPSocketApp:: Send Called \n"); 

}

void TCPSocketApp::getDataFromOtherSocket() {
	if (connectedSocketApp==NULL) {
		PLOG(PL_ERROR, "TCPSocketApp::getDataFromOtherSocket, Destination for SocketApp is NULL -> the setup is incorrect. Check the code\n");
		exit(0);
		}

	curdata_ = connectedSocketApp->receiveDataFromClient();
	if ((curdata_ == 0) || (curdata_->getDataSize()==0)) {
		PLOG(PL_DEBUG, "TCPSocketApp::getDataFromOtherSocket, received data from TCP but no data to read!\n");
		abort();
	}
}


void TCPSocketApp::recv(int tcpDataArrivedSize) {
	PLOG(PL_DEBUG, "TCPSocketApp::recv, tcpDataArrivedSize = %i, cur - %i\n", tcpDataArrivedSize, curbytes_);
	if (curdata_ == 0)
		getDataFromOtherSocket();
	if (curdata_ == 0) {
		fprintf(stderr, "[%g] %s receives a packet but no callback!\n",
				Scheduler::instance().clock(), name_);
		return;
	}
	curbytes_ += tcpDataArrivedSize;
	if (curbytes_ == curdata_->getDataSize()) {
		PLOG(PL_DEBUG, "TCPSocketApp::recv, equal and buffered data size = %i\n", curdata_->getDataSize());
		upcallToApp(curdata_->getData(), curdata_->getDataSize());
		delete curdata_;
		curdata_ = NULL;
		curbytes_ = 0;
	} else if (curbytes_ > curdata_->getDataSize()) {
		PLOG(PL_DEBUG, "TCPSocketApp::recv, not equal and buffered data size = %i\n", curdata_->getDataSize());
		while (curbytes_ >= curdata_->getDataSize()) {
			upcallToApp(curdata_->getData(), curdata_->getDataSize());
			curbytes_ -= curdata_->getDataSize();
			delete curdata_;
			getDataFromOtherSocket();
			if (curdata_ != 0)
				continue;
			if ((curdata_ == 0) && (curbytes_ > 0)) {
				fprintf(stderr, "[%g] %s gets extra data!\n",
						Scheduler::instance().clock(), name_);
				Tcl::instance().eval("[Simulator instance] flush-trace");
				abort();
			} else
				// Get out of the look without doing a check
				break;
		}
	}
}


/**
 * Application Callback HERE:
 * 
 * upcallToApp() is called when data is delivered from the TCP agent. This effectively is the callback 
 * an application gets from an agent when data arrives. Since TCPSocketApp uses the application
 * Ns2 interface, then this method will be called upon receipt of data the the Ns-2 node.   
 */
void TCPSocketApp::upcallToApp(char* data, unsigned int size) 
{
	PLOG(PL_DEBUG, "TCPSocketApp:: upcallToApp called \n");
	
	if (data == NULL)
		return;
			 
	PLOG(PL_DEBUG, "TCPSocketApp:: upcallToApp, receiving size %i \n", size);

	if (target()) {
		TcpData *tcpdata =  new TcpData();
		tcpdata->setData(data,size);
		PLOG(PL_DETAIL, "TCPSocketApp:: upcallToApp, Sending to Target\n");
		send_data(size, tcpdata);
		PLOG(PL_DETAIL, "TCPSocketApp: upcallToApp, finished Sending Data to Target\n");
	} else if (tcpSocketListenerAgent) { // pass it along
		PLOG(PL_DETAIL, "TCPSocketApp:: upcallToApp, Sending to Listener\n");
		char* datasend = new char[size];
		memcpy(datasend, data, size);
		tcpSocketListenerAgent->tcpEventReceived(new TCPEvent(TCPEvent::RECEIVE, NULL, datasend, size));
		PLOG(PL_DETAIL, "TCPSocketApp: upcallToApp, finished Callback\n");
	}
	else {
		PLOG(PL_INFO, "TCPSocketApp: Receiving %i of size %i  at node %i\n", data, size, getTCPAgent()->addr());
	}
}


void TCPSocketApp::resume()
{
	// Do nothing
}

int TCPSocketApp::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();

//	cout << "Command: " << argv[1] << endl;

	if (strcmp(argv[1], "connect") == 0) {
		connectedSocketApp = (TCPSocketApp *)TclObject::lookup(argv[2]);
		if (connectedSocketApp == NULL) {
			tcl.resultf("%s: connected to null object.", name_);
			return (TCL_ERROR);
		}
		connectedSocketApp->connect(this);
		return (TCL_OK);
	} else if (strcmp(argv[1], "send") == 0) {
		
		const char *bytes = argv[3];
		int size = atoi(argv[2]);
		
//		cout << "Sending " << bytes << ", size " << size << " from node " << getTCPAgent()->addr() << endl;
		
		if (argc > 3) {
			TcpData *tmp = new TcpData();
			tmp->setData(bytes, size);
			send(tmp);
		}
		 
		return (TCL_OK);

	} else if (strcmp(argv[1], "dst") == 0) {
		tcl.resultf("%s", connectedSocketApp->name());
		return TCL_OK;
	}
	return Application::command(argc, argv);
}
	
	
	
	
	
	
	
	/** void TCPSocketApp::recv(int tcpDataArrivedSize) {	
	 
	 
	 PLOG(PL_DEBUG, "TCPSocketApp:: Recv Called, %i bytes have arrived from TCP. Reading now\n", tcpDataArrivedSize);
	 int anythingLeftToRead=0;
	 char *writeBuffer = new char[tcpDataArrivedSize]; // buffer to read data into
	 int writeOffset=0; 
	 int readingNow=0;
	 int datasize=0;
	 char *databuf=0;
	 do {
	 PLOG(PL_DEBUG, "TCPSocketApp:: Left to read = %i bytes\n", anythingLeftToRead);
	 if (curdata_==NULL)  // get the next data from the FIFO
	 getDataFromOtherSocket();
	 
	 if (curdata_==NULL) break;
	 
	 datasize=curdata_->getDataSize();
	 databuf=curdata_->getData(); // current buffer to read from
	 
	 if (anythingLeftToRead>0) 
	 readingNow=anythingLeftToRead;
	 else
	 readingNow=tcpDataArrivedSize;
	 
	 if (readOffset+readingNow > datasize) {
	 readingNow = datasize-readOffset;
	 anythingLeftToRead=tcpDataArrivedSize-readingNow; // left over to read				
	 } else
	 anythingLeftToRead=0;				
	 
	 PLOG(PL_DETAIL, "TCPSocketApp: reading %u bytes at position %u, Total buffer is %i\n", readingNow, readOffset, datasize);
	 PLOG(PL_DETAIL, "TCPSocketApp: Writing %u bytes at position %i, total size of buffer = %i bytes\n", readingNow, writeOffset, tcpDataArrivedSize);
	 
	 if ((writeOffset+readingNow) > tcpDataArrivedSize)
	 break;
	 
	 memcpy(writeBuffer+writeOffset, databuf+readOffset, readingNow);
	 
	 writeOffset+=readingNow; // so that all bytes are read
	 readOffset+=readingNow;
	 
	 if (anythingLeftToRead>0) { // ran out of data, move on to next data
	 delete curdata_;
	 curdata_=NULL;
	 readOffset=0;
	 }
	 
	 PLOG(PL_DETAIL, "TCPSocketApp: readOffset %u, WriteOffset %i, total = %u\n", readOffset, writeOffset, tcpDataArrivedSize);
	 } while(anythingLeftToRead>0); // Got everything
	 
	 PLOG(PL_DETAIL, "TCPSocketApp::recv upcall to app\n");
	 
	 PLOG(PL_DETAIL, "TCPSocketApp::recv TcpDataArrived = %i\n", tcpDataArrivedSize);
	 
	 bytesSent_-=tcpDataArrivedSize; // bytesSent_ is the original bytes sent from the sender.
	 
	 upcallToApp(writeBuffer, tcpDataArrivedSize); // always read in all the data, even if it is over several buffers
	 
	 if (bytesSent_<0) {
	 PLOG(PL_FATAL, "TCPSocketApp::FATAL, ran out of data in buffer - BytesSent = %i\n", bytesSent_);
	 PLOG(PL_DETAIL, "TCPSocketApp::recv TcpDataArrived = %i\n", tcpDataArrivedSize);
	 PLOG(PL_DETAIL, "TCPSocketApp: Read now %u  read offset %u, Total read buffer %i\n", readingNow, readOffset, datasize);
	 PLOG(PL_DETAIL, "TCPSocketApp: Write Offset %i, total size of buffer = %i bytes\n", writeOffset, tcpDataArrivedSize);	
	 abort();
	 }
	 
	 PLOG(PL_DETAIL, "TCPSocketApp::recv bytesSent = %i\n", bytesSent_);
	 PLOG(PL_DETAIL, "TCPSocketApp::recv listener = %i\n", connectedSocketApp->tcpSocketListenerAgent);
	 
	 if (bytesSent_==0) { // data has been read in full from sender, so:
	 if (connectedSocketApp->tcpSocketListenerAgent) {  // data is removed from data queue, so notify on SENDER there is more room
	 PLOG(PL_DETAIL, "TCPSocketApp::recv generating SEND event\n");
	 tcpSocketListenerAgent->tcpEventReceived(new TCPEvent(TCPEvent::SENDACK, NULL, NULL, tcpDataArrivedSize));
	 }
	 }	
	 
	 PLOG(PL_DETAIL, "TCPSocketApp:: Recv Done \n");
	 }  */
   