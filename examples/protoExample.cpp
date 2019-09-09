#ifdef SIMULATE
#include "nsProtoSimAgent.h"
#else
#include "protoApp.h"
#include "protoNet.h"
#include "protoRouteMgr.h"
#endif  // if/else SIMULATE

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

class ProtoExample :
#ifdef SIMULATE
                     public NsProtoSimAgent
#else
                     public ProtoApp
#endif  // if/else SIMULATE
                     
{
    public:
        ProtoExample();
        ~ProtoExample();

        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        bool OnStartup(int argc, const char*const* argv);
        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        bool ProcessCommands(int argc, const char*const* argv);
        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        void OnShutdown();
        /**
        * Override from ProtoApp or NsProtoSimAgent base
        */
        virtual bool HandleMessage(unsigned int len, const char* txBuffer,const ProtoAddress& srcAddr) {return true;}

    private:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();

        void OnTxTimeout(ProtoTimer& theTimer);
        void OnUdpSocketEvent(ProtoSocket&       theSocket, 
                            ProtoSocket::Event theEvent);
        void OnClientSocketEvent(ProtoSocket&       theSocket, 
                               ProtoSocket::Event theEvent);
        void OnServerSocketEvent(ProtoSocket&       theSocket, 
                               ProtoSocket::Event theEvent);
        static const char* const CMD_LIST[];
        static void SignalHandler(int sigNum);

        // ProtoTimer/ UDP socket demo members
        ProtoTimer          tx_timer;
        ProtoAddress        dst_addr;

        ProtoSocket         udp_tx_socket;
        ProtoSocket         udp_rx_socket;

        // TCP socket demo members
        ProtoSocket         server_socket;
        ProtoSocket         client_socket;
        unsigned int        client_msg_count;
        ProtoSocket::List   connection_list;
        bool                use_html;

}; // end class ProtoExample


// (TBD) Note this #if/else code could be replaced with something like
// a PROTO_INSTANTIATE(ProtoExample) macro defined differently
// in "protoApp.h" and "nsProtoSimAgent.h"
#ifdef SIMULATE
#ifdef NS2
static class NsProtoExampleClass : public TclClass
{
	public:
		NsProtoExampleClass() : TclClass("Agent/ProtoExample") {}
	 	TclObject *create(int argc, const char*const* argv) 
			{return (new ProtoExample());}
} class_proto_example;	
#endif // NS2


#else

// Our application instance 
PROTO_INSTANTIATE_APP(ProtoExample) 

#endif  // SIMULATE

ProtoExample::ProtoExample()
: udp_tx_socket(ProtoSocket::UDP),
  udp_rx_socket(ProtoSocket::UDP),
  server_socket(ProtoSocket::TCP), 
  client_socket(ProtoSocket::TCP), 
  client_msg_count(0), use_html(false)
{    
    tx_timer.SetListener(this, &ProtoExample::OnTxTimeout);
    udp_tx_socket.SetNotifier(&GetSocketNotifier());
    udp_tx_socket.SetListener(this, &ProtoExample::OnUdpSocketEvent);
    udp_rx_socket.SetNotifier(&GetSocketNotifier());
    udp_rx_socket.SetListener(this, &ProtoExample::OnUdpSocketEvent);
    client_socket.SetNotifier(&GetSocketNotifier());
    client_socket.SetListener(this, &ProtoExample::OnClientSocketEvent);
    server_socket.SetNotifier(&GetSocketNotifier());
    server_socket.SetListener(this, &ProtoExample::OnServerSocketEvent);
}

ProtoExample::~ProtoExample()
{
    
}

void ProtoExample::Usage()
{
    fprintf(stderr, "protoExample [send <host>/<port>] [recv [<group>/]<port>]\n"
                    "             connect <host>/<port>] [listen <port>][html]\n");
}  // end ProtoExample::Usage()


const char* const ProtoExample::CMD_LIST[] =
{
    "+send",       // Send UDP packets to destination host/port
    "+recv",       // Recv UDP packets on <port> (<group> addr optional)
    "+connect",    // TCP connect and send to destination host/port 
    "+listen",     // Listen for TCP connections on <port>
    "-html",       // listening TCP server provides canned HTML response to client(s) (use with "listen" option)
    NULL
};

ProtoExample::CmdType ProtoExample::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end ProtoExample::GetCmdType()

bool ProtoExample::OnStartup(int argc, const char*const* argv)
{
#ifdef _WIN32_WCE
    if (!OnCommand("recv", "224.225.1.2/5002"))
    {
        TRACE("Error with \"recv\" command ...\n");
    }
    if (!OnCommand("send", "224.225.1.2/5002"))
    {
        TRACE("Error with \"send\" command ...\n");
    }
#endif // _WIN32_WCE
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "protoExample: Error! bad command line\n");
        return false;
    }  
    
#ifndef SIMULATE     

    // Here's some code to test the ProtoSocket routines for network interface info       
    ProtoAddress localAddress;
    char nameBuffer[256];
    nameBuffer[255] = '\0';

    // ljt test
    ProtoAddress dst;
    dst.ResolveFromString("fe80::426c:8fff:fe31:4f8e");
    if (dst.ResolveToName(nameBuffer,255))
        TRACE("protoExample: LJT ipv6 host name: %s\n", nameBuffer);

    
    if (localAddress.ResolveLocalAddress())
    {
        TRACE("protoExample: local default IP address: %s\n", localAddress.GetHostString());
        if (localAddress.ResolveToName(nameBuffer, 255))
            TRACE("protoExample: local default host name: %s\n", nameBuffer);
        else
            TRACE("protoExample: unable to resolve local default IP address to name\n");
    }      
    else
    {
        TRACE("protoExample: unable to determine local default IP address\n");
    }
    
    // Get all of our ifaces and dump some info (name, MAC addr, IP addrs, etc)
    unsigned int ifaceCount = ProtoNet::GetInterfaceCount();
    if (ifaceCount > 0)
    {
        // Allocate array to hold the indices
        unsigned int* indexArray = new unsigned int[ifaceCount];
        if (NULL == indexArray)
        {
            PLOG(PL_ERROR, "protoExample: new indexArray[%u] error: %s\n", ifaceCount, GetErrorString());
            return false;
        }
        if (ProtoNet::GetInterfaceIndices(indexArray, ifaceCount) != ifaceCount)
        {
            PLOG(PL_ERROR, "protoExample: GetInterfaceIndices() error?!\n");
            delete[] indexArray;
            return false;
        }
        for (unsigned int i = 0; i < ifaceCount; i++)
        {
            unsigned int index = indexArray[i];
            TRACE("protoExample: interface index %u ", index);
            // Get the iface name
            if (ProtoNet::GetInterfaceName(index, nameBuffer, 255))
                TRACE("has name \"%s\" ", nameBuffer);
            else
                TRACE("name \"unknown\" ");
            // Get MAC addr for the iface
            ProtoAddress ifaceAddr;
            if (ProtoNet::GetInterfaceAddress(nameBuffer, ProtoAddress::ETH, ifaceAddr))
                TRACE("with MAC addr %s\n", ifaceAddr.GetHostString());
            else
                TRACE("with no MAC addr\n");
            
            ProtoAddressList addrList;
            // Get IPv4 addrs for the iface
            if (ProtoNet::GetInterfaceAddressList(nameBuffer, ProtoAddress::IPv4, addrList))
            {
                TRACE("          IPv4 addresses:");
                ProtoAddressList::Iterator iterator(addrList);
                while (iterator.GetNextAddress(ifaceAddr))
                {
					TRACE(" %s/%d", ifaceAddr.GetHostString(),ProtoNet::GetInterfaceAddressMask(nameBuffer, ifaceAddr));
                    if (ifaceAddr.IsLinkLocal()) TRACE(" (link local)");
                }
                TRACE("\n");
            }
            addrList.Destroy();
            // Get IPv6 addrs for the iface
            if (ProtoNet::GetInterfaceAddressList(nameBuffer, ProtoAddress::IPv6, addrList))
            {
                TRACE("          IPv6 addresses:");
                ProtoAddressList::Iterator iterator(addrList);
                while (iterator.GetNextAddress(ifaceAddr))
                {

                    TRACE(" %s/%d", ifaceAddr.GetHostString(), ProtoNet::GetInterfaceAddressMask(nameBuffer, ifaceAddr));

					if (ifaceAddr.IsLinkLocal()) TRACE(" (link local)");
                }
                TRACE("\n");
            }
            addrList.Destroy();

			// Test our interface add routine
            /*
			char * ifaceName = "192.168.1.6";
			ProtoAddress tmpAddr;
			tmpAddr.ConvertFromString("192.168.1.6");
			int maskLen = 24;
			bool result = ProtoNet::AddInterfaceAddress(ifaceName, tmpAddr, maskLen);
            */


#ifndef WIN32
            // This code should work for Linux and BSD (incl. Mac OSX)
            // Get IPv4 group memberships for interfaces
            if (ProtoNet::GetGroupMemberships(nameBuffer, ProtoAddress::IPv4, addrList))
            {
                TRACE("          IPv4 memberships:");
                ProtoAddressList::Iterator iterator(addrList);
                ProtoAddress groupAddr;
                while (iterator.GetNextAddress(groupAddr))
                {
                    TRACE(" %s", groupAddr.GetHostString());
                }       
                TRACE("\n");
            }
            addrList.Destroy();
            if (ProtoNet::GetGroupMemberships(nameBuffer, ProtoAddress::IPv6, addrList))
            {
                TRACE("          IPv6 memberships:");
                ProtoAddressList::Iterator iterator(addrList);
                ProtoAddress groupAddr;
                while (iterator.GetNextAddress(groupAddr))
                {
                    TRACE(" %s", groupAddr.GetHostString());
                }       
                TRACE("\n");
            }
            addrList.Destroy();
#endif // WIN32
        }
        delete[] indexArray;
    }
    else
    {
        TRACE("protoExample: host has no network interfaces?!\n");
    }  
    // Here's some code to get the system routing table
    ProtoRouteTable routeTable;
    ProtoRouteMgr* routeMgr = ProtoRouteMgr::Create();
    if (NULL == routeMgr)
    {
        PLOG(PL_ERROR, "ProtoExample::OnStartup() error creating route manager\n");
        return false;
    }
    if (!routeMgr->Open())
    {
        PLOG(PL_ERROR, "ProtoExample::OnStartup() error opening route manager\n");
        return false;    
    }
    if (!routeMgr->GetAllRoutes(ProtoAddress::IPv4, routeTable))
        PLOG(PL_ERROR, "ProtoExample::OnStartup() warning getting system routes\n");     
    // Display whatever routes we got
    ProtoRouteTable::Iterator iterator(routeTable);
    ProtoRouteTable::Entry* entry;
    PLOG(PL_ALWAYS, "IPv4 Routing Table:\n");
    PLOG(PL_ALWAYS, "%16s/Prefix %-12s   ifIndex   Metric\n", "Destination", "Gateway");
    while (NULL != (entry = iterator.GetNextEntry()))
    {
        PLOG(PL_ALWAYS, "%16s/%-3u     ",
                entry->GetDestination().GetHostString(), entry->GetPrefixSize());
        const ProtoAddress& gw = entry->GetGateway();
        PLOG(PL_ALWAYS, "%-16s %-02u     %d\n",
                gw.IsValid() ? gw.GetHostString() : "0.0.0.0", 
                entry->GetInterfaceIndex(),
                entry->GetMetric());
    }
    //ProtoAddress dst;
    //dst.ResolveFromString("10.1.2.3");
    //ProtoAddress gw;
    //routeMgr->SetRoute(dst, 32, gw, 4, 0);
    routeMgr->Close();
            
    fprintf(stderr, "\nprotoExample: entering main loop (<CTRL-C> to exit)...\n");
#endif // !SIMULATE
    return true;
}  // end ProtoExample::OnStartup()

void ProtoExample::OnShutdown()
{
    if (tx_timer.IsActive()) tx_timer.Deactivate();
    if (udp_tx_socket.IsOpen()) udp_tx_socket.Close();
    if (udp_rx_socket.IsOpen()) udp_rx_socket.Close();
    if (server_socket.IsOpen()) server_socket.Close();
    if (client_socket.IsOpen()) client_socket.Close();
    connection_list.Destroy();   
#ifndef SIMULATE
    PLOG(PL_ERROR, "protoExample: Done.\n");
#endif // SIMULATE
    CloseDebugLog();
}  // end ProtoExample::OnShutdown()

bool ProtoExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class ProtoExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
#ifndef SIMULATE
                PLOG(PL_ERROR, "ProtoExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
#endif // SIMULATE
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "ProtoExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "ProtoExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end ProtoExample::ProcessCommands()

bool ProtoExample::OnCommand(const char* cmd, const char* val)
{

    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "ProtoExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("send", cmd, len))
    {
        if (!udp_tx_socket.Open(0, ProtoAddress::IPv4, false))
        {
            PLOG(PL_ERROR, "ProtoExample::ProcessCommand(send) error opening udp_tx_socket\n");
            return false;    
        }    
        char string[256];
        strncpy(string, val, 256);
        string[255] ='\0';
        char* ptr = strchr(string, '/');
        if (ptr) 
        {
            *ptr++ = '\0';
            if (!dst_addr.ResolveFromString(string))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(send) error: invalid <host>\n");
                return false;
            }
            dst_addr.SetPort(atoi(ptr));
            udp_tx_socket.Connect(dst_addr);
            tx_timer.SetInterval(1.0);
            tx_timer.SetRepeat(-1);
            OnTxTimeout(tx_timer);
            if (!tx_timer.IsActive()) ActivateTimer(tx_timer);
        }
        else
        {
            PLOG(PL_ERROR, "ProtoExample::ProcessCommand(send) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("recv", cmd, len))
    {     
        char string[256];
        strncpy(string, val, 256);
        string[255] ='\0';
        char* portPtr = strchr(string, '/');
        ProtoAddress groupAddr;
        if (portPtr) 
        {
            *portPtr++ = '\0';
            if (!groupAddr.ResolveFromString(string))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(recv) error: invalid <groupAddr>\n");
                return false;
            }
        }
        else
        {
            portPtr = string; 
        } 
        UINT16 thePort;
        if (1 == sscanf(portPtr, "%hu", &thePort))
        {
            
            if (!udp_rx_socket.Open(0, ProtoAddress::IPv4, false))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(recv) error opening udp_rx_socket\n");
                return false;   
            }
            udp_rx_socket.EnableRecvDstAddr();  // for testing this feature
            if (groupAddr.IsValid() && groupAddr.IsMulticast())
            {
                
                bool result = udp_rx_socket.SetReuse(true);
                TRACE("set port reuse result %d...\n", result);
            }
            if (!udp_rx_socket.Bind(thePort))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(recv) error binding udp_rx_socket\n");
                return false;   
            }
            if (groupAddr.IsValid() && groupAddr.IsMulticast())
            {
                if (!(udp_rx_socket.JoinGroup(groupAddr)))
                {
                    PLOG(PL_ERROR, "ProtoExample::ProcessCommand(recv) error joining group\n");
                    udp_rx_socket.Close();
                    return false;  
                }
            }            
        }
        else
        {
            PLOG(PL_ERROR, "ProtoExample::ProcessCommand(recv) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("connect", cmd, len))
    {
        char string[256];
        strncpy(string, val, 256);
        string[255] ='\0';
        char* ptr = strchr(string, '/');
        if (ptr) 
        {
            *ptr++ = '\0';
            ProtoAddress server;
            if (!server.ResolveFromString(string))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(connect) error: invalid <host>\n");
                return false;
            }
            server.SetPort(atoi(ptr));
            TRACE("calling connect ...\n");
            if (!client_socket.Connect(server))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(connect) error connecting\n");
                return false;
            }
            client_msg_count = 5;  // (Send 5 messages, then disconnect)
            TRACE("   (completed. starting output notification ...\n");
            client_socket.StartOutputNotification();
        }
        else
        {
            PLOG(PL_ERROR, "ProtoExample::ProcessCommand(connect) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("listen", cmd, len))
    {
        UINT16 thePort;
        if (1 == sscanf(val, "%hu", &thePort))
        {
            if (!server_socket.Listen(thePort))
            {
                PLOG(PL_ERROR, "ProtoExample::ProcessCommand(listen) error listening\n");
                return false;   
            }            
        }
        else
        {
            PLOG(PL_ERROR, "ProtoExample::ProcessCommand(connect) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("html", cmd, len))
    {
        use_html = true;
    }
    else if (!strncmp("background", cmd, len))
    {
        // do nothing (this command was scanned immediately at startup)
    }
    return true;
}  // end ProtoExample::OnCommand()

void ProtoExample::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    const char* string = "Hello there UDP peer, how are you doing?";
    unsigned int len = strlen(string) + 1;
    unsigned int numBytes = len;
    
    if (!udp_tx_socket.SendTo(string, numBytes, dst_addr))
    {
        PLOG(PL_ERROR, "ProtoExample::OnTxTimeout() error sending to %s/%hu ...\n",
                dst_addr.GetHostString(), dst_addr.GetPort());   
    }
    else if (len != numBytes)
    {
        PLOG(PL_ERROR, "ProtoExample::OnTxTimeout() incomplete SendTo()\n");                
        TRACE("   (only sent %lu of %lu bytes)\n", numBytes, len);
    }

}  // end ProtoExample::OnTxTimeout()

void ProtoExample::OnUdpSocketEvent(ProtoSocket&       theSocket, 
                                    ProtoSocket::Event theEvent)
{
    if (&theSocket == &udp_tx_socket)
        TRACE("ProtoExample::OnUdpTxSocketEvent(");
    else
        TRACE("ProtoExample::OnUdpRxSocketEvent(");
    switch (theEvent)
    {
        case ProtoSocket::INVALID_EVENT:
            TRACE("ERROR) ...\n");
            break;
        case ProtoSocket::CONNECT:
            TRACE("CONNECT) ...\n");
            break;  
        case ProtoSocket::ACCEPT:
            TRACE("ACCEPT) ...\n");
            break; 
        case ProtoSocket::SEND:
        {
            TRACE("SEND) ...\n");
            break; 
        }
        case ProtoSocket::RECV:
        {
            static unsigned long count = 0;
            TRACE("RECV) ...\n");
            ProtoAddress srcAddr, dstAddr;
            char buffer[1024];
            unsigned int len = 1024;
            // This RecvFrom() call w/ dstAddr may not be suppoted on WIN32 yet
            if (theSocket.RecvFrom(buffer, len, srcAddr, dstAddr))
            {
                buffer[len] = '\0';
                if (len)
                {
                    char dstAddrString[32];
                    dstAddr.GetHostString(dstAddrString, 32);
                    TRACE("ProtoExample::OnUdpSocketEvent(%lu) received \"%s\" from %s to dest %s\n", 
                           count++, buffer, srcAddr.GetHostString(), dstAddrString);
                }
                else
                {
                    TRACE("ProtoExample::OnUdpSocketEvent() received 0 bytes\n");
                }
            }
            else
            {
                PLOG(PL_ERROR, "ProtoExample::OnUdpSocketEvent() error receiving!\n");
            }
            break; 
        }
        case ProtoSocket::DISCONNECT:
            TRACE("DISCONNECT) ...\n");
            break;   
        
        case ProtoSocket::EXCEPTION:
            TRACE("EXCEPTION) ...\n");
            break;  
        
        case ProtoSocket::ERROR_:
            TRACE("ERROR_) ...\n");
            break;  
    }
}  // end ProtoExample::OnUdpSocketEvent()


void ProtoExample::OnClientSocketEvent(ProtoSocket&       theSocket, 
                                       ProtoSocket::Event theEvent)
{
    TRACE("ProtoExample::OnClientSocketEvent(");
    switch (theEvent)
    {
        case ProtoSocket::INVALID_EVENT:
            TRACE("ERROR) ...\n");
            break;
        case ProtoSocket::CONNECT:
            TRACE("CONNECT) ...\n");
            break;  
        case ProtoSocket::ACCEPT:
            TRACE("ACCEPT) ...\n");
            break; 
        case ProtoSocket::SEND:
        {
            TRACE("SEND) ...\n");
            if (0 == client_msg_count)
            {
                TRACE("protoExample: client message transmission completed.\n");
                client_socket.StopOutputNotification();  // don't send any more
                // Uncomment this to have the client cleanly disconnect after transmission
                // (otherwise, linger for server response until user "Ctrl-C" exit)
                client_socket.Shutdown();
				return;
            }         
            const char* string = "Hello there ProtoServer, how are you doing?";
            unsigned int len = strlen(string) + 1;
            unsigned int numBytes = len;
			client_msg_count--;
            if (!client_socket.Send(string, numBytes))
            {
                PLOG(PL_ERROR, "ProtoExample::OnClientSocketEvent() error sending\n");   
            }
            else if (len != numBytes)
            {
                PLOG(PL_WARN, "ProtoExample::OnClientSocketEvent() incomplete Send()\n");                
				TRACE("   (only sent %lu of %lu bytes (msgCount:%d))\n", numBytes, len, client_msg_count);
			} 
            else
            {
                TRACE("sent %u bytes to ProtoServer ...\n", len);
            }  
			
            break; 
        }
        case ProtoSocket::RECV:
        {
            TRACE("RECV) ...\n");
            char buffer[1024];
            unsigned int len = 1024;
            theSocket.Recv(buffer, len);
            if (0 != len)
            {
                buffer[len-1] = '\0';
                TRACE("ProtoExample::OnClientSocketEvent() received %u bytes \"%s\" from %s\n", 
                       len, buffer, theSocket.GetDestination().GetHostString());
            }
            else
            {
                TRACE("ProtoExample::OnClientSocketEvent() received 0 bytes\n");
            }
            break;  
        }
        case ProtoSocket::DISCONNECT:
            TRACE("DISCONNECT) ...\n");
            client_socket.Close();
            break;    
        
        case ProtoSocket::EXCEPTION:
            TRACE("EXCEPTION) ...\n");
            break;  
        
        case ProtoSocket::ERROR_:
            TRACE("ERROR_) ...\n");
            break;   
    }
}  // end ProtoExample::OnClientSocketEvent()

const char* PROTO_HTML = 
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\n"
    "<html>\n"
    "<head>\n"
    "<title>protoExample</title>\n"
    "</head>\n"
    "<body>\n"
    "<p>This is a canned response from your friendly, neighborhood <b>protoExample</b> server.</p>\n"
    "</body>\n"
    "</html>\n";
  
void ProtoExample::OnServerSocketEvent(ProtoSocket&       theSocket, 
                                       ProtoSocket::Event theEvent)
{
    TRACE("ProtoExample::OnServerSocketEvent(");
    switch (theEvent)
    {
        case ProtoSocket::INVALID_EVENT:
            TRACE("INVALID_EVENT) ...\n");
            break;
        case ProtoSocket::CONNECT:
            TRACE("CONNECT) ...\n");
            break;  
        case ProtoSocket::ACCEPT:
        {
            TRACE("ACCEPT) ...\n");
            ProtoSocket* connectedSocket = new ProtoSocket(ProtoSocket::TCP);
			if (NULL == connectedSocket)
            {
                PLOG(PL_ERROR, "ProtoExample::OnServerSocketEvent(ACCEPT) new ProtoSocket error\n");
                break;
            }
            connectedSocket->SetListener(this, &ProtoExample::OnServerSocketEvent); 

            if (!server_socket.Accept(connectedSocket))
            {
                PLOG(PL_ERROR, "ProtoExample::OnServerSocketEvent(ACCEPT) error accepting connection\n");
                delete connectedSocket;
                break;
            }

            if (!connection_list.AddSocket(*connectedSocket))
            {
                PLOG(PL_ERROR, "ProtoExample::OnServerSocketEvent(ACCEPT) error adding client to list\n");
                delete connectedSocket;
            }
            PLOG(PL_ERROR, "protoExample: accepted connection from %s\n",
                    connectedSocket->GetDestination().GetHostString());
            break; 
        }
        case ProtoSocket::SEND:
            TRACE("SEND) ...\n");
            break; 
        case ProtoSocket::RECV:
        {
            TRACE("RECV) ...\n");
            char buffer[1024];
            unsigned int len = 1024;
            if (theSocket.Recv(buffer, len))
            {
                if (0 != len)
                {
                    buffer[len-1] = '\0';
                    TRACE("ProtoExample::OnServerSocketEvent() received %u bytes \"%s\" from %s\n", 
                           len, buffer, theSocket.GetDestination().GetHostString());
                    
                    // Talk back to client
                    const char* string = "Hi ProtoClient, I'm fine.  How about you?";
                    if (use_html) string = PROTO_HTML;
                    len = strlen(string) + 1;
                    unsigned int numBytes = len;
                    if (!theSocket.Send(string, numBytes))
                    {
                        PLOG(PL_ERROR, "ProtoExample::OnServerSocketEvent() error sending\n");   
                    }
                    else if (len != numBytes)
                    {
                        PLOG(PL_ERROR, "ProtoExample::OnServerSocketEvent() incomplete Send()\n");                
                        TRACE("   (only sent %lu of %lu bytes)\n", numBytes, len);
                    } 
                    else
                    {
                        TRACE("sent %u bytes back to ProtoClient ...\n", len);
                    }  
                    if (use_html) theSocket.Shutdown();
                }
                else
                {
                    TRACE("ProtoExample::OnServerSocketEvent() received 0 bytes\n");
                }
                
            }
            else
            {
                PLOG(PL_ERROR, "ProtoExample::OnServerSocketEvent() error receiving\n");
            }
            break; 
        }
        case ProtoSocket::DISCONNECT:
            TRACE("DISCONNECT) ...\n");
            if(&theSocket == &server_socket)
            {
                TRACE("server_socket disconnected!\n");
                break;
            }
            theSocket.Close();
            connection_list.RemoveSocket(theSocket);
            break;   
        
        case ProtoSocket::EXCEPTION:
            TRACE("EXCEPTION) ...\n");
            break;  
        
        case ProtoSocket::ERROR_:
            TRACE("ERROR_) ...\n");
            break;  
    }
}  // end ProtoExample::OnServerSocketEvent() 
       

