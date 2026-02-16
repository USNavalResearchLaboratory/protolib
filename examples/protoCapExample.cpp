
#include "protoApp.h"
#include "protoCap.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>


/**
 * @class ProtoCapExample
 *
 * @brief This example uses a ProtoCap class instance to sniff for
 * raw packets on an interface as specified by the "listen"
 * command.  Received packets may optionally be "forwarded"
 * to another (or, dangerously, the same) interface.
 *
 * Usage: 
 *  protoCapExample [listen <interfaceName>][forward <interfaceName>][background (WIN32)]
 */
class ProtoCapExample : public ProtoApp
{
    public:
        ProtoCapExample();
        ~ProtoCapExample();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv);
        void OnShutdown();

    private:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static const char* const CMD_LIST[];
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        void OnReceive(ProtoChannel&              theChannel, 
                       ProtoChannel::Notification notifyType);              
       
        bool OnStatTimeout(ProtoTimer& theTimer);

        char            rcvr_interface[64];
        char            sndr_interface[64];
        ProtoCap*       cap_rcvr;    // used for raw packet reception (and forwarding for same interface)
        ProtoCap*       cap_sndr;    // used for forwarding to different interface
        ProtoCap*       cap_fwdr;    // pointer to which cap is forwarding (NULL if not forwarding)
        bool            forward;     // "true" for forwarding, "false" for bridging

        // Stats
        unsigned int    recv_count;
        unsigned int    send_count;
        unsigned int    serr_count;  // send error count

        ProtoTimer      stat_timer;  // Stats are printed on timeout
      
}; // end class ProtoCapExample

// This macro creates our ProtoApp derived application instance 
PROTO_INSTANTIATE_APP(ProtoCapExample) 
        
ProtoCapExample::ProtoCapExample()
 : cap_rcvr(NULL), cap_sndr(NULL), cap_fwdr(NULL),
   forward(false), recv_count(0), send_count(0), serr_count(0)
   
{       
    stat_timer.SetListener(this, &ProtoCapExample::OnStatTimeout);
    stat_timer.SetInterval(1.0);
    stat_timer.SetRepeat(-1);
}

ProtoCapExample::~ProtoCapExample()
{
}

void ProtoCapExample::Usage()
{
#ifdef WIN32
    fprintf(stderr, "protoCapExample [listen <interfaceName> [{forward | bridge} <interfaceName>]][background]\n");
#else
    fprintf(stderr, "protoCapExample [listen <interfaceName> [{forward | bridge} <interfaceName>]]\n");
#endif // if/else WIN32/UNIX
}  // end ProtoCapExample::Usage()


const char* const ProtoCapExample::CMD_LIST[] =
{
    "+listen",    // recv raw packets on given interface name
    "+forward",   // resend all recvd packets out given interface with my src MAC addr
    "+bridge",    // resend all recvd packets unaltered out given interface
    NULL
};

ProtoCapExample::CmdType ProtoCapExample::GetCmdType(const char* cmd)
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
}  // end ProtoCapExample::GetCmdType()


bool ProtoCapExample::OnStartup(int argc, const char*const* argv)
{

#ifdef _WIN32_WCE
    Sleep(1000); // give our WinCE debug window time to start up
#endif // _WIN32_WCE

    // Get interface name of localAddress
    char ifName[256];
    ProtoAddress localAddress;
    if (localAddress.ResolveLocalAddress())
    {
        ProtoSocket dummySocket(ProtoSocket::UDP);
        unsigned int index = dummySocket.GetInterfaceIndex(localAddress.GetHostString());
        TRACE("protoCapExample: got interface index:%d for interface: %s\n", 
               index, localAddress.GetHostString());

        ifName[255] = '\0';
        if (dummySocket.GetInterfaceName(index, ifName, 255))
        {
            TRACE("              (real interface name: \"%s\")\n", ifName);
        }

        ProtoAddress ethAddr;
        if (dummySocket.GetInterfaceAddress(ifName, ProtoAddress::ETH, ethAddr))
        {
            TRACE("              (interface MAC addr: \"%s\")\n", ethAddr.GetHostString());
        }

        //index = dummySocket.GetInterfaceIndex(ifName);
        //TRACE("protoCapExample: got interface index:%d for interface: %s\n", index, ifName);

        /*ProtoAddress ifAddr;
        if (dummySocket.GetInterfaceAddress(buffer, ProtoAddress::IPv4, ifAddr))
        {
            TRACE("protoCapExample: got IPv4 address:%s for interface: %s\n", ifAddr.GetHostString(), buffer);
        }

        if (dummySocket.GetInterfaceAddress(buffer, ProtoAddress::IPv6, ifAddr))
        {
            TRACE("protoCapExample: got IPv6 address:%s for interface: %s\n", ifAddr.GetHostString(), buffer);
        }*/
    }
    
    // Create out cap_rcvr instance and initialize ...
    if (!(cap_rcvr = ProtoCap::Create()))
    {
        PLOG(PL_ERROR, "protoCapExample::OnStartup() new ProtoCap cap_rcvr error: %s\n", GetErrorString());
        return false;
    }  
    cap_rcvr->SetListener(this, &ProtoCapExample::OnReceive); 
    cap_rcvr->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    
    // Create out cap_sndr instance ...
    if (!(cap_sndr = ProtoCap::Create()))
    {
        PLOG(PL_ERROR, "protoCapExample::OnStartup() new ProtoCap cap_sndr error: %s\n", GetErrorString());
        return false;
    }  
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "protoCapExample: Error! bad command line\n");
        return false;
    }
    
#ifdef HAVE_SCHED
    // Boost process priority for real-time operation
    // (This _may_ work on Linux-only at this point)
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority =  sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &schp))
            PLOG(PL_ERROR, "protoCapExample: Warning! Couldn't set any real-time priority: %s\n", GetErrorString());
    }
#endif // HAVE_SCHED

#ifdef WIN32
#ifndef _WIN32_WCE
    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS))
	    PLOG(PL_ERROR, "protoCapExample: Warning! SetPriorityClass() error: %s\n", GetErrorString());
#endif // !_WIN32_WCE
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
	    PLOG(PL_ERROR, "protoCapExample: Warning! SetThreadPriority() error: %s\n", GetErrorString());
#endif // WIN32

    return true;
}  // end ProtoCapExample::OnStartup()

void ProtoCapExample::OnShutdown()
{
   
    if (NULL != cap_rcvr)
   {
       cap_rcvr->Close();
       delete cap_rcvr;
       cap_rcvr = NULL;
   }
   if (NULL != cap_sndr)
   {
       cap_sndr->Close();
       delete cap_sndr;
       cap_sndr = NULL;
   }
   PLOG(PL_ERROR, "protoCapExample: Done.\n"); 
   CloseDebugLog();
}  // end ProtoCapExample::OnShutdown()

bool ProtoCapExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class ProtoCapExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "ProtoCapExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "ProtoCapExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "ProtoCapExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end ProtoCapExample::ProcessCommands()

bool ProtoCapExample::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "ProtoCapExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("listen", cmd, len))
    {
        // Make sure "val" is an interface name
        int ifIndex = ProtoSocket::GetInterfaceIndex(val);
        char ifName[256];

        ifName[255] = '\0';
        if (!ProtoSocket::GetInterfaceName(ifIndex, ifName, 255))
        {
            PLOG(PL_ERROR, "protoCapExample: invalid <interfaceName>\n");
            return false;
        }
        if (!cap_rcvr->Open(ifName))
        {
            PLOG(PL_ERROR, "protoCapExample: ProtoCap::Open() error.\n");
            return false;   
        }
        strncpy(rcvr_interface, ifName, 64);
        rcvr_interface[62] = '\0';
        TRACE("protoCapExample: listening to interface: %s...\n", ifName);
        if (!stat_timer.IsActive()) ActivateTimer(stat_timer);
    }
    else if (!strncmp("forward", cmd, len))
    {
        if (!cap_rcvr->IsOpen())
        {
            PLOG(PL_ERROR, "protoCapExample: command line error: must specify \"listen\" first\n");
            Usage();
            return false;
        }
        // Make sure "val" is an interface name
        int ifIndex = ProtoSocket::GetInterfaceIndex(val);
        char ifName[256];
        ifName[255] = '\0';
        if (!ProtoSocket::GetInterfaceName(ifIndex, ifName, 255))
        {
            PLOG(PL_ERROR, "protoCapExample: invalid <interfaceName>\n");
            return false;
        }
        if (!strncmp(ifName, rcvr_interface, 64))
        {
            cap_fwdr = cap_rcvr;
        }
        else
        {
            if (!cap_sndr->Open(ifName))
            {
                PLOG(PL_ERROR, "protoCapExample: ProtoCap::Open() error.\n");
                return false;   
            }
            cap_fwdr = cap_sndr;
        }
        strncpy(sndr_interface, ifName, 64);
        sndr_interface[62] = '\0';
        forward = true;
        TRACE("protoCapExample: forwarding to interface: %s...\n", ifName);        
    }
    
    else if (!strncmp("bridge", cmd, len))
    {
        if (!cap_rcvr->IsOpen())
        {
            PLOG(PL_ERROR, "protoCapExample: command line error: must specify \"listen\" first\n");
            Usage();
            return false;
        }
        // Make sure "val" is an interface name
        int ifIndex = ProtoSocket::GetInterfaceIndex(val);
        char ifName[256];
        ifName[255] = '\0';
        if (!ProtoSocket::GetInterfaceName(ifIndex, ifName, 255))
        {
            PLOG(PL_ERROR, "protoCapExample: invalid <interfaceName>\n");
            return false;
        }
        if (!strncmp(ifName, rcvr_interface, 64))
        {
            cap_fwdr = cap_rcvr;
        }
        else
        {
            if (!cap_sndr->Open(ifName))
            {
                PLOG(PL_ERROR, "protoCapExample: ProtoCap::Open() error.\n");
                return false;   
            }
            cap_fwdr = cap_sndr;
        }
        strncpy(sndr_interface, ifName, 64);
        sndr_interface[62] = '\0';
        forward = false;
        TRACE("protoCapExample: bridging to interface: %s...\n", ifName);    
    }
    else
    {
        PLOG(PL_ERROR, "protoCapExample:: invalid command\n");
        return false;
    }
    return true;
}  // end ProtoCapExample::OnCommand()

void ProtoCapExample::OnReceive(ProtoChannel&              theChannel, 
                                ProtoChannel::Notification notifyType)
{
    if (ProtoChannel::NOTIFY_INPUT == notifyType)
    {
        char buffer[4096];
        unsigned int numBytes;
        do
        {
            numBytes = 4096;
            ProtoCap::Direction direction;
            if (!cap_rcvr->Recv(buffer, numBytes, &direction))
            {
                PLOG(PL_ERROR, "protoCapExample::OnReceive() ProtoCap::Recv() error\n");
                break;
            } 
            else if (0 != numBytes)
            {
                recv_count++;
                if (cap_fwdr)
                {
		            // Linux PF_PACKET doesn't like to send 802.3 frames
		            UINT16 type;
		            memcpy(&type, buffer+12, 2);
		            type = ntohs(type);
                    if ((ProtoCap::INBOUND == direction) && (0 != numBytes) && (type > 0x05dc))
                    {  
                        if (forward)
                        {
                            if (cap_fwdr->Forward(buffer, numBytes))
                                send_count++;
                            else
                                serr_count++;
                        }
                        else  // we're bridging
                        {
                            if (cap_fwdr->Send(buffer, numBytes))
                                send_count++;
                            else
                                serr_count++;
                        }
                    }
                }
            }
        } while (0 != numBytes);
    }
    else
    {
        TRACE("ProtoCapExample::OnReceive() unhandled notifyType: %d\n", notifyType);   
    }

}  // end ProtoCapExample::OnReceive()

bool ProtoCapExample::OnStatTimeout(ProtoTimer& /*theTimer*/)
{
    TRACE("stats: rcvd>%lu sent>%lu serr>%lu\n", recv_count, send_count, serr_count);
    return true;
}  // end ProtoCapExample::OnStatTimeout()

