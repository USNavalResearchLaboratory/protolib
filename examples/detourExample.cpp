
#include "protoApp.h"
#include "protoDetour.h"
#include "protoPktIP.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>
#include "protoBase64.h"

/**
 * @class DetourExample
 *
 * @brief An example using a ProtoDetour class instance to intercept
 * outbound packets
 */
class DetourExample : public ProtoApp
{
     public:
         DetourExample();
        ~DetourExample();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv);
        void OnShutdown();

    private:
        void DetourEventHandler(ProtoChannel&               theChannel,
                                ProtoChannel::Notification  theNotification);
            
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static const char* const CMD_LIST[];
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        ProtoDetour*    detour;
        bool            ipv6_mode;
        bool            allow;  // toggled variable for test
        
}; // end class DetourExample

const char* const DetourExample::CMD_LIST[] =
{
    "-ipv6",     // IPv6 test (instead of IPv4)
    NULL
};

// This macro creates our ProtoApp derived application instance 
PROTO_INSTANTIATE_APP(DetourExample) 
        
DetourExample::DetourExample()
 : detour(NULL), ipv6_mode(false), allow(true)
   
{       
   
}

DetourExample::~DetourExample()
{
}

void DetourExample::Usage()
{
#ifdef WIN32
    fprintf(stderr, "detourExample [ipv6][background]\n");
#else
    fprintf(stderr, "detourExample\n");
#endif // if/else WIN32/UNIX
}  // end DetourExample::Usage()


DetourExample::CmdType DetourExample::GetCmdType(const char* cmd)
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
}  // end DetourExample::GetCmdType()


bool DetourExample::OnStartup(int argc, const char*const* argv)
{
    SetDebugLevel(3);
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "detourExample::OnStartup() error processing command line options\n");
        return false;   
    }
    
    // Create our cap_rcvr instance and initialize ...
    if (!(detour = ProtoDetour::Create()))
    {
        PLOG(PL_ERROR, "detourExample::OnStartup() new ProtoDetour error: %s\n", GetErrorString());
        return false;
    }  
     
    detour->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    detour->SetListener(this, &DetourExample::DetourEventHandler);
    
    // Set detour filter for outbound multicast packets
    ProtoAddress srcFilter;
    ProtoAddress dstFilter;
    unsigned int dstFilterMask;
    if (ipv6_mode)
    {
        srcFilter.Reset(ProtoAddress::IPv6);   // unspecified address 
        dstFilter.ResolveFromString("ff00::");
        dstFilterMask = 8;
    }
    else
    {
        srcFilter.Reset(ProtoAddress::IPv4);  // unspecified address
        dstFilter.ResolveFromString("239.0.0.0");
        dstFilterMask = 4;
    }
    
    if (!detour->Open(ProtoDetour::INPUT, srcFilter, 0, dstFilter, dstFilterMask))
    {
        PLOG(PL_ERROR, "detourExample::OnStartup() ProtoDetour::Open() error\n");
    }
    
#ifdef NEVER //HAVE_SCHED
    // Boost process priority for real-time operation
    // (This _may_ work on Linux-only at this point)
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority =  sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &schp))
            PLOG(PL_ERROR, "detourExample: Warning! Couldn't set any real-time priority: %s\n", GetErrorString());
    }
#endif // HAVE_SCHED

#ifdef WIN32
#ifndef _WIN32_WCE
    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS))
	    PLOG(PL_ERROR, "detourExample: Warning! SetPriorityClass() error: %s\n", GetErrorString());
#endif // !_WIN32_WCE
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
	    PLOG(PL_ERROR, "detourExample: Warning! SetThreadPriority() error: %s\n", GetErrorString());
#endif // WIN32

    TRACE("startup returning control to dispatcher ...\n");
    
    return true;
}  // end DetourExample::OnStartup()

void DetourExample::OnShutdown()
{
   if (NULL != detour)
   {
       detour->Close();
       delete detour;
       detour = NULL;
   }
   PLOG(PL_ERROR, "detourExample: Done.\n"); 
   CloseDebugLog();
}  // end DetourExample::OnShutdown()

bool DetourExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class DetourExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "DetourExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "DetourExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "DetourExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end DetourExample::ProcessCommands()

bool DetourExample::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "DetourExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("ipv6", cmd, len))
    {
        ipv6_mode = true;
    }
    else
    {
        PLOG(PL_ERROR, "detourExample:: invalid command\n");
        return false;
    }
    return true;
}  // end DetourExample::OnCommand()

void DetourExample::DetourEventHandler(ProtoChannel&               theChannel, 
                                       ProtoChannel::Notification  theNotification)
{
    if (ProtoChannel::NOTIFY_INPUT == theNotification)
    {
        UINT32 buffer[8192/4];
        unsigned int numBytes = 8192; 
        if (detour->Recv((char*)buffer, numBytes))
        {
            TRACE("detour recv'd packet ...\n");
            if (0 != numBytes)
            {
                allow = true;
                if (allow)
                {
		  TRACE("detour allowing packet len:%u\n", numBytes);
                    
                    ProtoPktIP ipPkt(buffer, numBytes);
                    ipPkt.InitFromBuffer(numBytes);
                    
                    // This some example code that "translates" the dst addr
                    // of certain outbound IP packets 224.0.0.251 -> 224.1.2.3
                    // (These are MDNS packets, by the way)
                    ProtoAddress dstAddr;
                    ipPkt.GetDstAddr(dstAddr);
                    ProtoAddress matchAddress;
                    matchAddress.ResolveFromString("224.0.0.251");
                    ProtoAddress goalAddress;
                    goalAddress.ResolveFromString("224.1.2.3");
                    if (dstAddr.HostIsEqual(matchAddress))
                    {
                        ipPkt.SetDstAddr(goalAddress);
                        // Need to update UDP checksum if UDP packet
                        // (yucky cross-layer UDP checksum)
                        ProtoPktUDP udpPkt;
                        if (udpPkt.InitFromPacket(ipPkt))
                            udpPkt.FinalizeChecksum(ipPkt);
                    }
                    
                    // Some UDP packet manipulation code
                    ProtoPktUDP udpPkt;
                    if (udpPkt.InitFromPacket(ipPkt))// && (5000 == udpPkt.GetDstPort()))
                    {
                        if (udpPkt.ChecksumIsValid(ipPkt))
                        {
                            TRACE("   UDP with valid checksum %04x (computed: %04x)\n", 
                                    udpPkt.GetChecksum(), udpPkt.ComputeChecksum(ipPkt)); 
                        }
                        else
                        {
                            TRACE("   UDP with INVALID checksum %04x (computed: %04x)\n", 
                                    udpPkt.GetChecksum(), udpPkt.ComputeChecksum(ipPkt)); 
                            
                            
                            ProtoAddress srcAddr;
                            ipPkt.GetSrcAddr(srcAddr);
                            TRACE("   src:%s\n", srcAddr.GetHostString());
                            TRACE("   dst:%s\n", dstAddr.GetHostString());
                            ProtoPktIPv4 ipv4Pkt(ipPkt);
                            TRACE("   proto:%d\n", ipv4Pkt.GetProtocol());
                            
                            /*
                            char base64Buffer[8192];
                            unsigned int length = ProtoBase64::Encode((char*)buffer, numBytes, base64Buffer, 8192);
                            TRACE("packet(base64)=\n %s\n", base64Buffer);
                            */
                            for (unsigned int i = 0; i < numBytes; i++)
                            {
                                if (0 == i)
                                    ;
                                else if (0 == (i%16))
                                    TRACE("\n");
                                else if (0 == (i%2)) 
                                    TRACE(" "); 
                                unsigned char* ptr = (unsigned char*)buffer;
                                TRACE("%02x", ptr[i]);
                            }
                        }
                        
                    }
                    
                    
                    detour->Allow((char*)buffer, numBytes);
                    //allow = false;  // uncomment this to drop every other packet
                }
                else
                {    
                    TRACE("detour dropping/injecting packet ...\n");
                    detour->Drop();
                    detour->Inject((char*)buffer, numBytes);
                    allow = true;
                }
            }
            numBytes = 8192;
        }                  
    }
}  // end DetourExample::DetourEventHandler()
