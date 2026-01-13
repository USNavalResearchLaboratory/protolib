// This program replays IP packets from a pcap trace file using a ProtoCap socket
// Note that it needs root privileges to execute

// Usage:  pcapReplay interface <ifaceName> [input <pcapFile>][dst <addr[/<port>]>][src <addr[/<port>]][edst <macAddr>]

// TBD - add option for specific network address translation rules

#include <protoString.h>    // for ProtoTokenator
#include <protoCap.h>       // for ProtoCap
#include <protoNet.h>       
#include <protoPktETH.h>
#include <protoPktIP.h>
#include <protoApp.h>       // base class for command-line app
#include <protoDebug.h>
#include <stdio.h>
#include <pcap.h>

// Maximum supported frame size
#define FRAME_MAX 8192

class PcapReplay : public ProtoApp
{
    public:
        PcapReplay();
        ~PcapReplay();

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
        bool ParseAddrPort(const char* string, ProtoAddress& addr);
        double ReadFrame();
        void OnTxTimeout(ProtoTimer& theTimer);

        const char*     pcap_file;
        ProtoCap*       tx_cap;
        ProtoTimer      tx_timer;
        ProtoAddress    dst_addr;
        ProtoAddress    dst_mac;
        ProtoAddress    src_addr;
        pcap_t*         pcap_device;
        int             pcap_link_type;
        ProtoTime       last_time;
        UINT32          aligned_buffer[(FRAME_MAX+4)/4];
        void*           ip_buffer;
        UINT16          ip_length;
      
}; // end class PcapReplay

// This macro creates our ProtoApp derived application instance 
PROTO_INSTANTIATE_APP(PcapReplay)
        
PcapReplay::PcapReplay()
 : pcap_file(NULL), tx_cap(NULL), pcap_device(NULL), ip_buffer(NULL), ip_length(0)
   
{       
    tx_timer.SetListener(this, &PcapReplay::OnTxTimeout);
    tx_timer.SetRepeat(-1);
    memset(aligned_buffer, 0, FRAME_MAX+4);
}

PcapReplay::~PcapReplay()
{
}

void PcapReplay::Usage()
{
    fprintf(stderr, "pcapReplay interface <ifaceName> [input <pcapFile>][dst <addr[/<port>]>][edst <dstMac>][src <addr[/<port>]>]\n");
}  // end PcapReplay::Usage()


const char* const PcapReplay::CMD_LIST[] =
{
    "+input",       // <fileName> pcap input file name
    "+interface",   // <ifaceName> interface for transmission
    "+dst",         // <addr[/<port>]> remap packets' destination to given address and optional port
    "+src",         // <addr[/<port>]> remap packets' source to given address and optional port
    "+edst",        // <macAddr>  (colon-delimited)
    NULL
};

PcapReplay::CmdType PcapReplay::GetCmdType(const char* cmd)
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
}  // end PcapReplay::GetCmdType()

bool PcapReplay::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class PcapReplay command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "PcapReplay::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "PcapReplay::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "PcapReplay::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;
}  // end PcapReplay::ProcessCommands()

bool PcapReplay::ParseAddrPort(const char* string, ProtoAddress& addr)
{
    ProtoTokenator tk(string, '/');
    const char* token = tk.GetNextItem();
    if (!addr.ResolveFromString(token))
    {
        PLOG(PL_ERROR, "pcapReplay error: invalid address: %s\n", token);
        return false;
    }
    token = tk.GetNextItem();
    if (NULL != token)
    {
        UINT16 port;
        if (1 != sscanf(token, "%hu", &port))
        {
            PLOG(PL_ERROR, "pcapReplay error: invalid port: %s\n", token);
            return false;
        }
        addr.SetPort(port);
    }
    else
    {
        addr.SetPort(0);
    }
    return true;
}  // end PcapReplay::ParseAddrPort()

bool PcapReplay::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "PcapReplay::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("input", cmd, len))
    {
        pcap_file = val;
    }
    else if (!strncmp("interface", cmd, len))
    {
        // Make sure "val" is a valid interface name
        int ifIndex = ProtoNet::GetInterfaceIndex(val);
        char ifName[256];
        ifName[255] = '\0';
        if (!ProtoNet::GetInterfaceName(ifIndex, ifName, 255))
        {
            PLOG(PL_ERROR, "pcapReplay: invalid <interfaceName>\n");
            return false;
        }
        if (NULL == tx_cap)
        {
            if (NULL == (tx_cap = ProtoCap::Create()))
            {
                PLOG(PL_ERROR, "pcapReplay: new ProtoCap error: %s\n", GetErrorString());
                return false;
            }
        } else if (tx_cap->IsOpen())
        {
            tx_cap->Close();
        }
        if (!tx_cap->Open(ifName))
        {
            PLOG(PL_ERROR, "pcapReplay: ProtoCap::Open() error.\n");
            return false;   
        }
    }
    else if (!strncmp("dst", cmd, len))
    {
        if (!ParseAddrPort(val, dst_addr)) 
        {
            PLOG(PL_ERROR, "pcapReplay error: invalid destination address: \"%s\"\n", val);
            return false;
        }
    }
    else if (!strncmp("edst", cmd, len))
    {
        if (!dst_mac.ResolveEthFromString(val))
        {
            PLOG(PL_ERROR, "pcapReplay error: invalid mac address: \"%s\"\n", val);
            return false;
        }
    }
    else if (!strncmp("src", cmd, len))
    {
        if (!ParseAddrPort(val, src_addr))
        {
            PLOG(PL_ERROR, "pcapReplay error: invalid source address: \"%s\"\n", val);
            return false;
        }
    }
    else
    {
        PLOG(PL_ERROR, "pcapReplay:: invalid command\n");
        return false;
    }
    return true;
}  // end PcapReplay::OnCommand()

bool PcapReplay::OnStartup(int argc, const char*const* argv)
{
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "protoCapExample: Error! bad command line\n");
        return false;
    }
    if (NULL == tx_cap)
    {
        PLOG(PL_ERROR, "pcapReplay error: no transmit 'interface' specified?!\n");
        Usage();
        return false;
    }
    FILE* infile = stdin;  // by default
    if (NULL != pcap_file)
    {
        if (NULL == (infile = fopen(pcap_file, "r")))
        {
            PLOG(PL_ERROR, "pcapReplay error: invalid pcap file \"%s\"\n", pcap_file);
            return false;
        }
    }
    char pcapErrBuf[PCAP_ERRBUF_SIZE+1];
    pcapErrBuf[PCAP_ERRBUF_SIZE] = '\0';
    pcap_device = pcap_fopen_offline(infile, pcapErrBuf);
    if (NULL == pcap_device)
    {
        fprintf(stderr, "pcap2mgen: pcap_fopen_offline() error: %s\n", pcapErrBuf);
        if (stdin != infile) fclose(infile);
        return false;
    }
    pcap_link_type = pcap_datalink(pcap_device);
    tx_timer.SetInterval(0.0);
    ActivateTimer(tx_timer);
    return true;
}  // end PcapReplay::OnStartup()


double PcapReplay::ReadFrame()
{
    // TBD - read/buffer next pcap item and set interval timer
    pcap_pkthdr hdr;
    const u_char* pktData;
    // TBD - make this a loop to seek IP packets?
    if (NULL != (pktData = pcap_next(pcap_device, &hdr)))
    {
        if (hdr.len > hdr.caplen)
        {
            PLOG(PL_ERROR, "pcapRelay error: pcap snap length was too small!\n");
            return -1.0;
        }
        unsigned int numBytes = hdr.caplen;
        if (numBytes > FRAME_MAX)
        {
            PLOG(PL_ERROR, "pcapRelay error: encountered oversized frame length  %u\n", numBytes);
            return -1.0;
        }
        if (DLT_LINUX_SLL == pcap_link_type)
        {
             memcpy(aligned_buffer, pktData, numBytes);
             ip_buffer = aligned_buffer + 4;  // assumes 16-byte header (i.e. 6-byte link address on SLL frame)
             ip_length = numBytes - 16;
        }
        else  // assume DLT_EN10MB
        {
            UINT16* ethBuffer = ((UINT16*)aligned_buffer) + 1; 
            memcpy(ethBuffer, pktData, numBytes);
            ProtoPktETH ethPkt(ethBuffer, FRAME_MAX);
            if (!ethPkt.InitFromBuffer(hdr.len))
            {
                fprintf(stderr, "pcapRelay error: invalid Ether frame in pcap file\n");
                return -1.0;
            }    
            ip_buffer = ethPkt.AccessPayload();
            ip_length = ethPkt.GetPayloadLength();
        }
        // Now determine interval for tx_timer
        ProtoTime thisTime(hdr.ts);
        double interval;
        if (last_time.IsValid())
        {
            interval = ProtoTime::Delta(thisTime, last_time);
            if (interval < 0.0)
            {
                PLOG(PL_WARN, "pcapRelay warning: negative pcap interval detected?!\n");
                interval = 0.0;
            }
        }
        else
        {
            interval = 0.0;  // first frame
        }
        last_time = thisTime;
        return interval;
    }
    else
    {
        // end of pcap input reached
        return -1.0;
    }
    
}  // end PcapRelay::ReadFrame()

void PcapReplay::OnTxTimeout(ProtoTimer& theTimer)
{
    if (0 != ip_length)
    {
        ProtoPktIP ipPkt;
        if (ipPkt.InitFromBuffer(ip_length, ip_buffer, ip_length))
        {
            if (dst_addr.IsValid())
                ipPkt.SetDstAddr(dst_addr);
            // TBD - support port number translation
            if (0 != dst_addr.GetPort())
                PLOG(PL_WARN, "pcapRelay warning: port translation not yet supported!\n");
            if (src_addr.IsValid())
                ipPkt.SetSrcAddr(src_addr);
            if (0 != src_addr.GetPort())
                PLOG(PL_WARN, "pcapRelay warning: port translation not yet supported!\n");
            // Fix UDP checksum  
            ProtoPktIP::Protocol protocol;
            switch (ipPkt.GetVersion())
            {
                case 4:
                {
                    ProtoPktIPv4 ip4Pkt(ipPkt);
                    protocol = ip4Pkt.GetProtocol();;
                    break;
                }
                case 6:
                {
                    ProtoPktIPv6 ip6Pkt(ipPkt);
                    protocol = ip6Pkt.GetNextHeader();;
                    break;
                }
                default:
                    PLOG(PL_WARN, "pcapRelay warning: unknown IP version!\n");
                    protocol = ProtoPktIP::RESERVED;
                    break;
            }
            if (ProtoPktIP::UDP == protocol)
            {
                ProtoPktUDP udpPkt;
                if (udpPkt.InitFromPacket(ipPkt))
                    udpPkt.FinalizeChecksum(ipPkt);
                else
                    PLOG(PL_WARN, "pcapRelay warning: invalid UDP packet?!\n");
            }
                
            // Set appropriate Ethernet header
            UINT16* ethBuffer = (UINT16*)ip_buffer - 7;
            ProtoPktETH ethPkt(ethBuffer, ip_length + 14);
            ethPkt.SetDstAddr(dst_mac);
            ethPkt.SetType(ProtoPktETH::IP);
            ethPkt.SetPayloadLength(ip_length);
            unsigned int numBytes = ethPkt.GetLength();
            if (!tx_cap->Forward((char*)ethPkt.GetBuffer(), numBytes))
            {
                PLOG(PL_ERROR, "pcapRelay error: unable to forward frame!\n");
            }
        }
        else
        {
            PLOG(PL_ERROR, "pcapRelay error: bad IP packet!\n");
        }
    }
    double interval = ReadFrame();  // read next pcap frame
    if (interval < 0.0)
        Stop();
    else
        theTimer.SetInterval(interval);
}  // end PcapReplay::OnTxTimeout()

void PcapReplay::OnShutdown()
{
    if (tx_timer.IsActive()) tx_timer.Deactivate();
    if (NULL != tx_cap)
    {
        tx_cap->Close();
        delete tx_cap;
        tx_cap = NULL;
    }
    if (NULL != pcap_device)
    {
        pcap_close(pcap_device);
        pcap_device = NULL;
    }
    PLOG(PL_ERROR, "PcapReplay: Done.\n"); 
    CloseDebugLog();
}  // end PcapReplay::OnShutdown()
