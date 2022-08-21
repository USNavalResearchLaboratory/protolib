
#include "protoApp.h"
#include "protoCap.h"

#include "protoPktETH.h"
#include "protoPktIP.h"
#include "protoPktIGMP.h"
#include "protoAddress.h"
#include "protoString.h"  // for ProtoTokenator
#include "protoNet.h"

#include <stdlib.h>  // for atoi(), rand(), RAND_MAX
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>
#include <math.h>      // for log()

/**
 * @class JoinApp
 *
 * @brief This example sends repeating IGMP JOIN messages on a specified
 * interface for a set of IP multicast group addresses.
 *
 * Usage: 
 *  join [interval <sec>] interface <interfaceName> <group1>[,<group2>,<group2> ...]
 */
class JoinApp : public ProtoApp
{
    public:
        JoinApp();
        ~JoinApp();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv);
        void OnShutdown();
        
        static double ExponentialRand(double mean)
            {return(-log(((double)rand())/((double)RAND_MAX))*mean);}

    private:
            
        enum {STRETCH_MAX = 4};
    
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static const char* const CMD_LIST[];
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        bool OnIgmpTimeout(ProtoTimer& theTimer);
        
        //char                        iface_name[64];
        ProtoCap*                   proto_cap;    
        ProtoAddress                iface_addr;
        UINT16                      ip_id;
        
        ProtoTimer                  igmp_timer; 
        double                      join_interval;  // Average group join interval
        
        ProtoAddressList            group_list;
        unsigned int                group_count;
        ProtoAddressList::Iterator  group_iterator;
      
}; // end class JoinApp

// This macro creates our ProtoApp derived application instance 
PROTO_INSTANTIATE_APP(JoinApp) 
        
JoinApp::JoinApp()
 : proto_cap(NULL), ip_id(0), join_interval(10.0), group_count(0), group_iterator(group_list)
   
{       
    igmp_timer.SetListener(this, &JoinApp::OnIgmpTimeout);
    igmp_timer.SetInterval(0.0);  // Interval will be dynamically adjusted on timeouts
    igmp_timer.SetRepeat(-1);
}

JoinApp::~JoinApp()
{
}

void JoinApp::Usage()
{
    fprintf(stderr, "join [interval <sec>] interface <interfaceName> <group1>[,<group2>,<group2> ...]\n");
}  // end JoinApp::Usage()


const char* const JoinApp::CMD_LIST[] =
{
    "+interface",    // recv raw packets on given interface name
    "+interval",     // average IGMP JOIN interval
    "+join",
    NULL
};

JoinApp::CmdType JoinApp::GetCmdType(const char* cmd)
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
}  // end JoinApp::GetCmdType()

bool JoinApp::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class JoinApp command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                // Assume it's a group address
                return OnCommand("join", argv[i]);
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "JoinApp::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "JoinApp::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end JoinApp::ProcessCommands()

bool JoinApp::OnCommand(const char* cmd, const char* val)
{
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        fprintf(stderr, "join::command \"%s\" missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("interface", cmd, len))
    {
        // Make sure "val" is an interface name
        int ifIndex = ProtoSocket::GetInterfaceIndex(val);
        char ifName[256];

        ifName[255] = '\0';
        if (!ProtoSocket::GetInterfaceName(ifIndex, ifName, 255))
        {
            fprintf(stderr, "join: invalid <interfaceName>\n");
            return false;
        }
        if (!ProtoNet::GetInterfaceAddress(ifName, ProtoAddress::IPv4, iface_addr))
        {
            fprintf(stderr, "join error: interface %s has no IP address!\n", ifName);
            return false;   
        }
        if (!proto_cap->Open(ifName))
        {
            fprintf(stderr, "join: ProtoCap::Open() error: %s\n", GetErrorString());
            return false;   
        }
        //strncpy(iface_name, ifName, 64);
        //iface_name[63] = '\0';
        if (group_count > 0) ActivateTimer(igmp_timer);
    }
    else if (!strncmp("interval", cmd, len))
    {
        float interval;
        if (1 != scanf(val, "%f", &interval))
        {
            fprintf(stderr, "join error: invalid \"interval\" value\n");
            Usage();
            return false;
        }   
        join_interval = interval;
        if (igmp_timer.IsActive())
        {
            double scaledInterval = join_interval / (double)group_count;
            double nextInterval;
            do
            {
                nextInterval = ExponentialRand(scaledInterval);
            } while (nextInterval > STRETCH_MAX*scaledInterval);
            double elapsed = igmp_timer.GetInterval() - igmp_timer.GetTimeRemaining();
            if (elapsed > nextInterval)
                igmp_timer.SetInterval(0.0);
            else
                igmp_timer.SetInterval(nextInterval - elapsed);
            igmp_timer.Reschedule();
        } 
    }
    else if (!strncmp("join", cmd, len))
    {
        ProtoTokenator tk(val, ',');
        const char* addrText;
        while (NULL != (addrText = tk.GetNextItem()))
        {
            ProtoAddress groupAddr;
            if (!groupAddr.ConvertFromString(addrText))
            {
                fprintf(stderr, "join error: invalid IP address: %s\n", addrText);
                Usage();
                return false;
            }
            else if (!groupAddr.IsMulticast())
            {
                fprintf(stderr, "join error: invalid multicast address: %s\n", addrText);
                Usage();
                return false;
            }
            group_list.Insert(groupAddr);
            group_count++;           
            if (igmp_timer.IsActive())
            {
                // Join new group immediately
                igmp_timer.SetInterval(0.0);
                igmp_timer.Reschedule();
            }    
            else if (proto_cap->IsOpen())
            {
                ActivateTimer(igmp_timer);
            }           
        }
    }
    else
    {
        fprintf(stderr, "join: invalid command\n");
        return false;
    }
    return true;
}  // end JoinApp::OnCommand()


bool JoinApp::OnStartup(int argc, const char*const* argv)
{
    // Create out proto_cap instance and initialize ...
    if (!(proto_cap = ProtoCap::Create()))
    {
        fprintf(stderr, "join: new ProtoCap error: %s\n", GetErrorString());
        return false;
    }  
    
    if (!ProcessCommands(argc, argv))
    {
        //fprintf(stderr, "join error: bad command line\n");
        return false;
    }
    
    if ((0 == group_count) || (!proto_cap->IsOpen()))
    {
        fprintf(stderr, "join: missing required command-line parameter(s)\n");
        Usage();
        return false;
    }
    
    return true;
}  // end JoinApp::OnStartup()

void JoinApp::OnShutdown()
{
   if (igmp_timer.IsActive())
       igmp_timer.Deactivate();
   group_list.Destroy();
   if (NULL != proto_cap)
   {
       proto_cap->Close();
       delete proto_cap;
       proto_cap = NULL;
   }
}  // end JoinApp::OnShutdown()

bool JoinApp::OnIgmpTimeout(ProtoTimer& /*theTimer*/)
{
    ProtoAddress groupAddr;
    if (!group_iterator.GetNextAddress(groupAddr))
    {
        group_iterator.Reset();
        group_iterator.GetNextAddress(groupAddr);
    }
    ASSERT(groupAddr.IsValid());
    ASSERT(groupAddr.IsMulticast());
    
    
    // Build up an IGMP/IP/ETH frame to send
    const int BUFFER_MAX = 4096;
    const int ETH_MAX = BUFFER_MAX - 2;
    UINT32 alignedBuffer[BUFFER_MAX/sizeof(UINT32)];
    // offset by 2-bytes so IP content will be 32-bit aligned
    UINT16* ethBuffer = ((UINT16*)alignedBuffer) + 1; 
    ProtoPktETH ethPkt((UINT32*)ethBuffer, ETH_MAX);  // TBD - suppress warning here
    
    // Set Ethernet destination MAC addr to IGMP addr (224.0.0.22)
    ProtoAddress igmpAddr;
    igmpAddr.ConvertFromString("224.0.0.22");
    ProtoAddress igmpMacAddr;
    igmpMacAddr.GetEthernetMulticastAddress(igmpAddr);
    
    ethPkt.SetDstAddr(igmpMacAddr);
    ethPkt.SetType(ProtoPktETH::IP);
    
    const int IP_MAX = ETH_MAX - 14;
    UINT32* ipBuffer = alignedBuffer + 4;  // 16-byte offset given the 2-byte ethBuffer offset
    ProtoPktIPv4 ipPkt;
    ipPkt.InitIntoBuffer(ipBuffer, IP_MAX);
    ipPkt.SetTOS(0);
    ipPkt.SetID(ip_id++);
    ipPkt.SetTTL(1);
    ipPkt.SetProtocol(ProtoPktIP::IGMP);
    ipPkt.SetSrcAddr(iface_addr);
    ipPkt.SetDstAddr(igmpAddr);
    
    // Generate an IGMPv3 group join message (TBD - make version configurable?)
    ProtoPktIGMP igmpMsg(ipPkt.AccessPayload(), IP_MAX - 20);
    igmpMsg.InitIntoBuffer(ProtoPktIGMP::REPORT_V3, 3);
    ProtoPktIGMP::GroupRecord groupRecord;
    igmpMsg.AttachGroupRecord(groupRecord);
    groupRecord.SetType(ProtoPktIGMP::GroupRecord::CHANGE_TO_EXCLUDE_MODE);
    groupRecord.SetGroupAddress(&groupAddr);
    igmpMsg.AppendGroupRecord(groupRecord);
    
    ipPkt.SetPayloadLength(igmpMsg.GetLength());
    
    ethPkt.SetPayloadLength(ipPkt.GetLength());
    
    // Now send the IGMP/IP/Ethernet frame
    
    unsigned int numBytes = ethPkt.GetLength();
    proto_cap->Send(ethPkt.GetBuffer(), numBytes);
    
    //TRACE("sending JOIN for group %s ...\n", groupAddr.GetHostString());
    
    // Schedule next timeout
    double scaledInterval = join_interval / (double)group_count;
    double nextInterval;
    do
    {
        nextInterval = ExponentialRand(scaledInterval);
    } while (nextInterval > STRETCH_MAX*scaledInterval);
    igmp_timer.SetInterval(nextInterval);
    
    return true;
}  // end JoinApp::OnIgmpTimeout()

