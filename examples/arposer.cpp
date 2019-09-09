#include "protoApp.h"
#include "protoCap.h"
#include "protoPktIP.h"
#include "protoPktETH.h"
#include "protoPktARP.h"
#include "protoNet.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>  // for "isspace()"

/**
 * @class ArposerApp
 *
 * @brief simple app that gratuitously responds to ARP requests
 * with its own MAC address as the response
 *
 */
class ArposerApp : public ProtoApp
{
    public:
      ArposerApp();
      ~ArposerApp();

      bool OnStartup(int argc, const char*const* argv);
      bool ProcessCommands(int argc, const char*const* argv);
      void OnShutdown();
      
      

    private:
      enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
      static const char* const CMD_LIST[];
      static CmdType GetCmdType(const char* string);
      bool OnCommand(const char* cmd, const char* val);        
      void Usage();
            
      
      
      void PeekPkt(ProtoPktETH& ethPkt, bool inbound);

      void OnInboundPkt(ProtoChannel& theChannel,
                        ProtoChannel::Notification notifyType);
      
      ProtoCap*      cap;
      char           if_name[256]; 
        

}; // end class ArposerApp

void ArposerApp::Usage()
{
    fprintf(stderr, "Usage: arposer interface <ifName> [debug <level>][help]\n");
}

const char* const ArposerApp::CMD_LIST[] =
{
    "-help",        // print help info an exit
    "+interface",   // name of interface on which to listen / respond
    "+debug",       // <debugLevel>
    NULL
};

/**
 * This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(ArposerApp) 

ArposerApp::ArposerApp()
  : cap(NULL)
{
    if_name[0] = '\0';
}

ArposerApp::~ArposerApp()
{
}

ArposerApp::CmdType ArposerApp::GetCmdType(const char* cmd)
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
}  // end ArposerApp::GetCmdType()


bool ArposerApp::OnStartup(int argc, const char*const* argv)
{
    // Seed rand() with time of day usec
    // (comment out for repeatable results)
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    srand((unsigned int)currentTime.tv_usec);
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "ArposerApp::OnStartup() error processing command line options\n");
        Usage();
        return false;   
    }
    
    // Check for valid parameters.
    if (0 == strlen(if_name))
    {
        PLOG(PL_ERROR, "ArposerApp::OnStartup() error: missng required 'interface' command!\n");
        Usage();
        return false;
    }
    
    // Create our cap instance and initialize ...
    if (!(cap = ProtoCap::Create()))
    {
        PLOG(PL_ERROR, "ArposerApp::OnStartup() new ProtoCap error: %s\n", GetErrorString());
        return false;
    }  
    cap->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    cap->SetListener(this,&ArposerApp::OnInboundPkt);
    if (!cap->Open(if_name))
    {
       PLOG(PL_ERROR,"ArposerApp::OnStartup() ProtoCap::Open(\"%s\") error\n", if_name);
       Usage();
       return false;
    }
    
    PLOG(PL_INFO, "arposer: running on interface: %s\n", if_name);

    return true;
}  // end ArposerApp::OnStartup()

void ArposerApp::OnShutdown()
{
    if (NULL != cap)
    {
        cap->Close();
        delete cap;
        cap = NULL;
    }
    
    PLOG(PL_INFO, "arposer: Done.\n"); 

}  // end ArposerApp::OnShutdown()

bool ArposerApp::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a arposer command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "ArposerApp::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                Usage();
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "ArposerApp::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    Usage();
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "ArposerApp::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    Usage();
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end ArposerApp::ProcessCommands()

bool ArposerApp::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "ArposerApp::ProcessCommand(%s) missing argument\n", cmd);
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
        strncpy(if_name, val, 255);
        if_name[255] = '\0';
    }
    else if (!strncmp("debug", cmd, len))
    {
        SetDebugLevel(atoi(val));
    }
    else
    {
        PLOG(PL_ERROR, "arposer error: invalid command\n");
        Usage();
        return false;
    }
    return true;
}  // end ArposerApp::OnCommand()


void ArposerApp::PeekPkt(ProtoPktETH& ethPkt, bool inbound)
{
    switch (ethPkt.GetType())
    {
        case ProtoPktETH::IP:
        case ProtoPktETH::IPv6:
        {
            unsigned int payloadLength = ethPkt.GetPayloadLength();
            ProtoPktIP ipPkt;
            // The void* cast here suppresses the alignment warning we get otherwise.  This cast is OK
            // because of how we set up the "alignedBuffer" elsewhere
            if (!ipPkt.InitFromBuffer(payloadLength, (UINT32*)((void*)ethPkt.AccessPayload()), payloadLength))
            {
                PLOG(PL_ERROR, "arposer::PeekPkt() error: bad %sbound IP packet\n",
                        inbound ? "in" : "out");
                break;
            }
            ProtoAddress dstAddr; 
            ProtoAddress srcAddr;
            switch (ipPkt.GetVersion())
            {
                case 4:
                {
                    ProtoPktIPv4 ip4Pkt(ipPkt);
                    ip4Pkt.GetDstAddr(dstAddr);
                    ip4Pkt.GetSrcAddr(srcAddr);
                    break;
                } 
                case 6:
                {
                    ProtoPktIPv6 ip6Pkt(ipPkt);
                    ip6Pkt.GetDstAddr(dstAddr);
                    ip6Pkt.GetSrcAddr(srcAddr);
                    break;
                }
                default:
                {
                    PLOG(PL_ERROR,"ArposerApp::PeekPkt() Error: Invalid IP pkt version.\n");
                    break;
                }
            }
            PLOG(PL_ALWAYS, "ArposerApp::PeekPkt() %sbound  packet IP dst>%s ",
                    inbound ? "in" : "out", dstAddr.GetHostString());
            PLOG(PL_ALWAYS, "src>%s length>%d\n", srcAddr.GetHostString(), ipPkt.GetLength());
            break;
        }
        case ProtoPktETH::ARP:
        {
            ProtoPktARP arp;
            // The void* cast here suppresses the alignment warning we get otherwise.  This cast is OK
            // because of how we set up the "alignedBuffer" elsewhere
            if (!arp.InitFromBuffer((UINT32*)((void*)ethPkt.AccessPayload()), ethPkt.GetPayloadLength()))
            {
                PLOG(PL_ERROR, "ArposerApp::PeekPkt() received bad ARP packet?\n");
                break;
            } 
            PLOG(PL_ALWAYS,"ArposerApp::PeekPkt() %sbound ARP ", 
                  inbound ? "in" : "out");
            switch(arp.GetOpcode())
            {
                case ProtoPktARP::ARP_REQ:
                    PLOG(PL_ALWAYS, "request ");
                    break;
                case ProtoPktARP::ARP_REP:
                    PLOG(PL_ALWAYS, "reply ");
                    break;
                default:
                    PLOG(PL_ALWAYS, "??? ");
                    break;
            }
            ProtoAddress addr;
            arp.GetSenderHardwareAddress(addr);
            PLOG(PL_ALWAYS, "from eth:%s ", addr.GetHostString());
            arp.GetSenderProtocolAddress(addr);
            PLOG(PL_ALWAYS, "ip:%s ", addr.GetHostString());
            arp.GetTargetProtocolAddress(addr);
            PLOG(PL_ALWAYS, "for ip:%s ", addr.GetHostString());
            if (ProtoPktARP::ARP_REP == arp.GetOpcode())
            {

                arp.GetTargetHardwareAddress(addr);
                PLOG(PL_ALWAYS, "eth:%s ", addr.GetHostString()); 
            }
            PLOG(PL_ALWAYS, "\n");
            break;
        }
        default:
            PLOG(PL_ERROR, "ArposerApp::PeekPkt() unknown %s packet type\n", inbound ? "inbound" : "outbound");
            break;
    }
}  // end ArposerApp::PeekPkt()


/**
 * @note We offset the buffer here by 2 bytes since 
 * Ethernet header is 14 bytes
 * (i.e. not a multiple of 4 (sizeof(UINT32))
 * This gives us a properly aligned buffer for 
 * 32-bit aligned IP packets
 */
void ArposerApp::OnInboundPkt(ProtoChannel&              theChannel,
                                 ProtoChannel::Notification notifyType)
{
    ProtoTime currentTime;
    if (ProtoChannel::NOTIFY_INPUT != notifyType) return;
    while(1) 
    {
        ProtoCap::Direction direction;

        const int BUFFER_MAX = 4096;
        UINT32 alignedBuffer[BUFFER_MAX/sizeof(UINT32)];
        // offset by 2-bytes so IP content is 32-bit aligned
        UINT16* ethBuffer = ((UINT16*)alignedBuffer) + 1; 
        unsigned int numBytes = BUFFER_MAX - 2;
            
        if (!cap->Recv((char*)ethBuffer, numBytes, &direction))
        {
            PLOG(PL_ERROR, "ArposerApp::OnInboundPkt() ProtoCap::Recv() error\n");
            break;
        }

        if (numBytes == 0) break;  // no more packets to receive
        
        if ((ProtoCap::OUTBOUND != direction)) 
        {
            // Map ProtoPktETH instance into buffer and init for processing
            // (void* cast here is OK since ProtoPktETH is OK w/ UINT16*
            ProtoPktETH ethPkt((UINT32*)((void*)ethBuffer), BUFFER_MAX - 2);
            if (!ethPkt.InitFromBuffer(numBytes))
            {
                PLOG(PL_ERROR, "ArposerApp::OnInboundPkt() error: bad Ether frame\n");
                continue;
            }
            
            // In "MNE" environment, ignore packets from blocked MAC sources
            ProtoAddress srcMacAddr;
            ethPkt.GetSrcAddr(srcMacAddr);
            
            // Only handle ARP packets (skip others)
            if (ProtoPktETH::ARP == ethPkt.GetType())
            {
                
                ProtoPktARP arp;
                if (!arp.InitFromBuffer((UINT32*)((void*)ethPkt.AccessPayload()), ethPkt.GetPayloadLength()))
                {
                    PLOG(PL_ERROR, "ArposerApp::PeekPkt() received bad ARP packet?\n");
                    break;
                } 
                // Skip ARP replies (only respond to ARP requests)
                if (ProtoPktARP::ARP_REQ != arp.GetOpcode())
                    continue;
                
                PLOG(PL_ALWAYS,"ArposerApp::PeekPkt() inbound ARP ");
                switch(arp.GetOpcode())
                {
                    case ProtoPktARP::ARP_REQ:
                        PLOG(PL_ALWAYS, "request ");
                        break;
                    case ProtoPktARP::ARP_REP:
                        PLOG(PL_ALWAYS, "reply ");
                        break;
                    default:
                        PLOG(PL_ALWAYS, "??? ");
                        break;
                }
                ProtoAddress senderMAC;
                arp.GetSenderHardwareAddress(senderMAC);
                PLOG(PL_ALWAYS, "from eth:%s ", senderMAC.GetHostString());
                ProtoAddress senderIP;
                arp.GetSenderProtocolAddress(senderIP);
                PLOG(PL_ALWAYS, "ip:%s ", senderIP.GetHostString());
                ProtoAddress targetIP;
                arp.GetTargetProtocolAddress(targetIP);
                PLOG(PL_ALWAYS, "for ip:%s ", targetIP.GetHostString());
                if (ProtoPktARP::ARP_REP == arp.GetOpcode())
                {
                    ProtoAddress targetMAC;
                    arp.GetTargetHardwareAddress(targetMAC);
                    PLOG(PL_ALWAYS, "eth:%s ", targetMAC.GetHostString()); 
                }
                PLOG(PL_ALWAYS, "\n");
                
                // Build our ARP reply
                arp.InitIntoBuffer((UINT32*)((void*)ethPkt.AccessPayload()), BUFFER_MAX - 16);
                arp.SetOpcode(ProtoPktARP::ARP_REP);
                arp.SetSenderHardwareAddress(cap->GetInterfaceAddr());
                arp.SetSenderProtocolAddress(targetIP);   // target IP from above
                arp.SetTargetHardwareAddress(senderMAC);  // hw addr of requestor
                arp.SetTargetProtocolAddress(senderIP);   // IP addr of requestor
                
                // Send the reply
                ethPkt.SetSrcAddr(cap->GetInterfaceAddr());
                ethPkt.SetDstAddr(srcMacAddr);
                ethPkt.SetPayloadLength(arp.GetLength());
                
                numBytes = ethPkt.GetLength();
                cap->Send((char*)ethBuffer, numBytes);  // tbd - check result
            }
            
        }
    }  // end while(1)
    
} // end ArposerApp::OnInboundPkt

