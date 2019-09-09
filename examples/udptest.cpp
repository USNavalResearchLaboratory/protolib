#include "protoApp.h"
#include "protoSocket.h"

class UdpTest : public ProtoApp
{
    public:
        UdpTest();
        ~UdpTest();

        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        bool OnStartup(int argc, const char*const* argv);
        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        bool ProcessCommands(int argc, const char*const* argv) {return true;}
        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        void OnShutdown();
     
        

    private:
        void OnTxTimeout(ProtoTimer& theTimer);
        void OnClientSocketEvent(ProtoSocket&       theSocket, 
                               ProtoSocket::Event   theEvent);
        void OnServerSocketEvent(ProtoSocket&       theSocket, 
                               ProtoSocket::Event   theEvent);

        // ProtoTimer/ UDP socket demo members
        ProtoTimer          tx_timer;
        ProtoSocket         server_socket;
        ProtoSocket         client_socket;
}; // end class UdpTest



// Our application instance 
PROTO_INSTANTIATE_APP(UdpTest) 

UdpTest::UdpTest()
: server_socket(ProtoSocket::UDP), 
  client_socket(ProtoSocket::UDP)
{    
    tx_timer.SetListener(this, &UdpTest::OnTxTimeout);
    client_socket.SetNotifier(&GetSocketNotifier());
    client_socket.SetListener(this, &UdpTest::OnClientSocketEvent);
    server_socket.SetNotifier(&GetSocketNotifier());
    server_socket.SetListener(this, &UdpTest::OnServerSocketEvent);
}

UdpTest::~UdpTest()
{
    
}

bool UdpTest::OnStartup(int argc, const char*const* argv)
{
	if (argc > 1)
    {
        // Act as client (sender)
        client_socket.Bind(6000 + argc - 2);
        tx_timer.SetInterval(1.0);
        tx_timer.SetRepeat(-1.0);
        ActivateTimer(tx_timer);
        TRACE("udptest: Client startup ..\n");
    }    
    else
    {
        // Act as server (receiver)
        /*server_socket.Open(0, ProtoAddress::IPv4, false);
        server_socket.SetReuse(true);
        server_socket.Bind(5000);*/
        
        ProtoAddress addr;
        addr.ConvertFromString("127.0.0.1");
        addr.SetPort(6000);
        client_socket.Open(0, ProtoAddress::IPv4, false);
        client_socket.SetReuse(true);
        client_socket.Bind(5000);
        client_socket.Connect(addr);

		// Toggle server
		/*server_socket.Close();*/
		server_socket.Open(0, ProtoAddress::IPv4, false);
		server_socket.SetReuse(true);
		server_socket.Bind(5000);
		TRACE("udptest: Server startup ..\n");
    }
    return true;
}  // end UdpTest::OnStartup()

void UdpTest::OnShutdown()
{
    if (tx_timer.IsActive()) tx_timer.Deactivate();
    client_socket.Close();
    server_socket.Close();
    
}  // end UdpTest::OnShutdown()


void UdpTest::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    ProtoAddress dst;
    dst.ConvertFromString("127.0.0.1");
    dst.SetPort(5000);
    char buffer[256];
	sprintf(buffer, "Hi, this is client %d\n", client_socket.GetPort());
    unsigned int buflen = strlen(buffer) + 1;
    client_socket.SendTo(buffer, buflen, dst);
    TRACE("udptest: Client %d sent message to server ..\n", client_socket.GetPort());

}  // end UdpTest::OnTxTimeout()


void UdpTest::OnClientSocketEvent(ProtoSocket&       theSocket, 
                                  ProtoSocket::Event theEvent)
{
    if (ProtoSocket::RECV == theEvent)
    {
        char buffer[1024];
        unsigned int buflen = 1024;
        ProtoAddress src;
        theSocket.RecvFrom(buffer, buflen, src);
		if (buflen > 0)
			TRACE("udptest: server's client socket received: %s", buffer);
    }
    
}  // end UdpTest::OnClientSocketEvent()


void UdpTest::OnServerSocketEvent(ProtoSocket&       theSocket, 
                                 ProtoSocket::Event theEvent)
{
     if (ProtoSocket::RECV == theEvent)
    {
        char buffer[1024];
        unsigned int buflen = 1024;
        ProtoAddress src;
        theSocket.RecvFrom(buffer, buflen, src);
		if (buflen > 0)
			TRACE("udptest: server's server socket received: %s", buffer);
    }
}  // end UdpTest::OnServerSocketEvent() 
       

