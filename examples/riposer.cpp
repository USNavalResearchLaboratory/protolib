#include "protoApp.h"
#include "protoSocket.h"
#include "protoPktRIP.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>  // for "isspace()"

/**
 * @class RiposerApp
 *
 * @brief simple app that can transmit unsolicited RIP response
 * messages to advertise a route
 *
 */
class RiposerApp : public ProtoApp
{
    public:
      RiposerApp();
      ~RiposerApp();

      bool OnStartup(int argc, const char*const* argv);
      bool ProcessCommands(int argc, const char*const* argv);
      void OnShutdown();
      
      static const char*  RIP_ADDR;
      static const UINT16 RIP_PORT;
      static const double RIP_TIMEOUT;

    private:
      enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
      static const char* const CMD_LIST[];
      static CmdType GetCmdType(const char* string);
      bool OnCommand(const char* cmd, const char* val);        
      void Usage();
      
      bool OnRipTimeout(ProtoTimer& theTimer);
      void OnRipSocketEvent(ProtoSocket&       theSocket, 
                            ProtoSocket::Event theEvent);
      
      ProtoTimer    rip_timer;
      ProtoSocket   rip_socket;
      ProtoAddress  rip_addr;  // dest group addr for RIP 
      char          rip_iface[256]; 
      ProtoAddress  route_addr;
      unsigned int  route_mask_len;
      ProtoAddress  route_next_hop;      

}; // end class RiposerApp

const char* RiposerApp::RIP_ADDR = "224.0.0.9";
const UINT16 RiposerApp::RIP_PORT = 520;
const double RiposerApp::RIP_TIMEOUT = 30.0;

void RiposerApp::Usage()
{
    fprintf(stderr, "Usage: riposer [interface <ifaceName>][route <addr>/<maskLen>,<nextHop>]\n"
                    "               [debug <level>][help]\n");
}

const char* const RiposerApp::CMD_LIST[] =
{
    "-help",        // print help info an exit
    "+interface",   // name of interface on which to listen / respond
    "+route",       // <addr>/<maskLen>,<nextHop>
    "+debug",       // <debugLevel>
    NULL
};

/**
 * This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(RiposerApp) 

RiposerApp::RiposerApp()
 : rip_socket(ProtoSocket::UDP)
{
    rip_iface[0] = '\0';
    rip_timer.SetListener(this, &RiposerApp::OnRipTimeout);
    rip_timer.SetInterval(RIP_TIMEOUT);
    rip_timer.SetRepeat(-1.0);  // forever
    rip_socket.SetNotifier(&GetSocketNotifier());
    rip_socket.SetListener(this, &RiposerApp::OnRipSocketEvent);
    rip_addr.ConvertFromString(RIP_ADDR);  // default RIP group addr/port
    rip_addr.SetPort(RIP_PORT);
}

RiposerApp::~RiposerApp()
{
}

RiposerApp::CmdType RiposerApp::GetCmdType(const char* cmd)
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
}  // end RiposerApp::GetCmdType()


bool RiposerApp::OnStartup(int argc, const char*const* argv)
{
    // Seed rand() with time of day usec
    // (comment out for repeatable results)
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    srand((unsigned int)currentTime.tv_usec);
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "RiposerApp::OnStartup() error processing command line options\n");
        Usage();
        return false;   
    }
    
    // Open up the RIP UDP socket (port 520) and join the RIP_ADDR
    if (!rip_socket.Open(rip_addr.GetPort()))
    {
        PLOG(PL_ERROR, "RiposerApp::OnStartup() error: unable to open rip socket\n");
        return false;
    }
    // Check if specific iface was set
    const char* mcastIfaceName = NULL;
    if (0 != strlen(rip_iface))
        mcastIfaceName = rip_iface;
    if (!rip_socket.JoinGroup(rip_addr, mcastIfaceName))
    {
        PLOG(PL_ERROR, "RiposerApp::OnStartup() error: unable to join rip multicast group\n");
        return false;
    }
    if (NULL != mcastIfaceName)
    {
        if (!rip_socket.SetMulticastInterface(mcastIfaceName))
        {
            PLOG(PL_ERROR, "RiposerApp::OnStartup() error: unable to join set rip multicast interface\n");
            return false;
        }
    }
    
    if (route_addr.IsValid())
    {
        // We've been instructed advertise a route, so startup "rip_timer"
        OnRipTimeout(rip_timer);
        rip_timer.SetInterval(RIP_TIMEOUT);
        ActivateTimer(rip_timer);
    }
    
    PLOG(PL_INFO, "riposer: running on interface: %s\n", mcastIfaceName ? mcastIfaceName : "default");

    return true;
}  // end RiposerApp::OnStartup()

void RiposerApp::OnShutdown()
{
    if (rip_timer.IsActive())
        rip_timer.Deactivate();
    if (rip_socket.IsOpen())
        rip_socket.Close();
    
    PLOG(PL_INFO, "riposer: Done.\n"); 

}  // end RiposerApp::OnShutdown()

bool RiposerApp::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a riposer command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "RiposerApp::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                Usage();
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "RiposerApp::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    Usage();
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "RiposerApp::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    Usage();
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end RiposerApp::ProcessCommands()

bool RiposerApp::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "RiposerApp::ProcessCommand(%s) missing argument\n", cmd);
        Usage();
        return false;
    }
    else if (!strncmp("help", cmd, len))
    {
        Usage();
        exit(0);
    }
    else if (!strncmp("interface", cmd, len))
    {
        strncpy(rip_iface, val, 255);
        rip_iface[255] = '\0';
    }
    else if (!strncmp("route", cmd, len))
    {
        // route <dstAddr>/<maskLen>,<nextHop>
        // 1) get <dstAddr>
        const char* txtPtr = val;
        const char* delimiter = strchr(txtPtr, '/');
        if (NULL == delimiter)
        {
            PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: missing <maskLen>\n");
            return false;
        }
        size_t txtLen = delimiter - txtPtr;
        char tempTxt[256];
        if (txtLen > 255) txtLen = 255;
        strncpy(tempTxt, txtPtr, txtLen);
        tempTxt[txtLen] = '\0';  // null terminate
        if (!route_addr.ConvertFromString(tempTxt))
        {
            PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: invalid <dstAddr>\n");
            return false;
        }
        if (ProtoAddress::IPv4 != route_addr.GetType())
        {
            PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: non-IPv4 <dstAddr>\n");
            return false;
        }
        // 2) get <maskLen>
        txtPtr = delimiter + 1;
        delimiter = strchr(txtPtr, ',');
        if (NULL != delimiter)
            txtLen = delimiter - txtPtr;
        else
            txtLen = strlen(txtPtr);
        if (txtLen > 255) txtLen = 255;
        strncpy(tempTxt, txtPtr, txtLen);
        tempTxt[txtLen] = '\0';
        if (1 != sscanf(tempTxt, "%u", &route_mask_len))
        {
            PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: invalid <maskLen>\n");
            return false;
        }
        // 3) get <nextHop>
        if (NULL != delimiter)
        {
            txtPtr = delimiter + 1;
            if (!route_next_hop.ConvertFromString(txtPtr))
            {
                PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: invalid <nextHop>\n");
                return false;
            }
            if (ProtoAddress::IPv4 != route_next_hop.GetType())
            {
                PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: non-IPv4 <nextHop>\n");
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "RiposerApp::OnCommand(route) error: missing <nextHop>\n");
            return false;
        }
    }
    else if (!strncmp("debug", cmd, len))
    {
        SetDebugLevel(atoi(val));
    }
    else
    {
        PLOG(PL_ERROR, "riposer error: invalid command\n");
        Usage();
        return false;
    }
    return true;
}  // end RiposerApp::OnCommand()

bool RiposerApp::OnRipTimeout(ProtoTimer& theTimer)
{
    // Send unsolicited RIP response with our route advertisement
    UINT32 buffer[256];
    ProtoPktRIP response(buffer, 256);  // zero inits buffer 
    response.SetCommand(ProtoPktRIP::RESPONSE);
    response.SetVersion(ProtoPktRIP::VERSION);
    response.AddRouteEntry(route_addr, route_mask_len, route_next_hop);
    unsigned int numBytes = response.GetLength();
    if (!rip_socket.SendTo((char*)response.GetBuffer(), numBytes, rip_addr))
    {
        PLOG(PL_ERROR, "RiposerApp::OnRipTimer() error: sendto() error: %s\n", GetErrorString());
    }
    return true;
}  // end RiposerApp::OnRipTimeout()


void RiposerApp::OnRipSocketEvent(ProtoSocket&       theSocket, 
                                    ProtoSocket::Event theEvent)
{
    if (ProtoSocket::RECV == theEvent)
    {
        UINT32 buffer[8192];
        unsigned int len = 8192;
        ProtoAddress srcAddr;
        if (theSocket.RecvFrom((char*)buffer, len, srcAddr))
        {        
            if (0 == len) return;  // false alarm
            // TBD - print out more detail RIP packet information ...
            TRACE("riposer: received a %u byte RIP packet from %s/%hu\n", 
                  len, srcAddr.GetHostString(), srcAddr.GetPort());
            
            ProtoPktRIP rip(buffer, len, len);
            
            
            TRACE("riposer: received RIP message length:%d version:%d ", len, rip.GetVersion());
            switch (rip.GetCommand())
            {
                case ProtoPktRIP::REQUEST:
                    TRACE("REQUEST\n");
                    break;
                case ProtoPktRIP::RESPONSE:
                    TRACE("RESPONSE\n");
                    break;
                default:
                    TRACE("INVALID command type:%s\n", rip.GetCommand());
                    break;
            }
            unsigned int entryCount = rip.GetNumEntry();
            for (unsigned int i = 0; i < entryCount; i++)
            {
                ProtoPktRIP::RouteEntry entry;
                rip.AccessRouteEntry(i, entry);
                switch (entry.GetAddressFamily())
                {
                    case ProtoPktRIP::IPv4:
                    {
                        ProtoAddress addr;
                        entry.GetAddress(addr);
                        char routeAddr[64];
                        addr.GetHostString(routeAddr, 64);
                        entry.GetNextHop(addr);
                        char nextHop[64];
                        addr.GetHostString(nextHop, 64);
                        TRACE("   route entry %s/%u via %s\n", routeAddr, entry.GetMaskLength(), nextHop);
                        break;
                    }
                    case ProtoPktRIP::AUTH:
                        TRACE("   AUTH entry\n");
                        break;
                    default:
                        TRACE("   UNKNOWN address family: %d\n", entry.GetAddressFamily());
                }
            }
        }
    }   
}  // end RiposerApp::OnRipSocketEvent()


