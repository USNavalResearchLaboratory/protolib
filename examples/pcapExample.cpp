
#include "protoApp.h"
#include "protoPacketeer.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

#include <pcap.h>  // for libpcap routines

/** 
 * @class PcapExample
 *
 * @brief This example performs asynchronous I/O on a libpcap device
 * It also illustrates the use of the ProtoPacketeer class
 * for sending raw link-layer packets (frames)
*
 * The WIN32 "background" option lets it run as a background process
 *
 * Usage: 
 *  pcapExample [listen <interfaceName>][forward <interfaceName>][background (WIN32)]
 */
class PcapExample : public ProtoApp
{
    public:
        PcapExample();
        ~PcapExample();


  /** 
   * @brief Override from ProtoApp or NsProtoSimAgent base
   */
        bool OnStartup(int argc, const char*const* argv);
  /** 
   * @brief Override from ProtoApp or NsProtoSimAgent base
   */
        bool ProcessCommands(int argc, const char*const* argv);
  /** 
   * @brief Override from ProtoApp or NsProtoSimAgent base
   */
        void OnShutdown();

    private:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static const char* const CMD_LIST[];
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        static void DoPcapInputReady(ProtoDispatcher::Descriptor descriptor, 
                                     ProtoDispatcher::Event      theEvent, 
                                     const void*                 userData);
        void OnPcapInputReady();
       
        pcap_t*         pcap_device;  // for receiving raw packets
        ProtoPacketeer* packeteer;    // for sending raw packets
        
}; // end class PcapExample

/**
 * @brief This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(PcapExample) 
        
PcapExample::PcapExample()
 : pcap_device(NULL)
{       
}

PcapExample::~PcapExample()
{
}

void PcapExample::Usage()
{
#ifdef WIN32
    fprintf(stderr, "pcapExample [listen <interfaceName>][forward <interfaceName>][background]\n");
#else
    fprintf(stderr, "pcapExample [listen <interfaceName>][forward <interfaceName>]\n");
#endif // if/else WIN32/UNIX

}  // end PcapExample::Usage()


const char* const PcapExample::CMD_LIST[] =
{
    "+listen",     // recv raw packets on given interface name
    "+forward",    // send "special" recvd packets out given interface
    NULL
};

PcapExample::CmdType PcapExample::GetCmdType(const char* cmd)
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
};  // end PcapExample::GetCmdType()


bool PcapExample::OnStartup(int argc, const char*const* argv)
{
#ifdef _WIN32_WCE
    Sleep(1000); // give our WinCE debug window time to start up
#endif // _WIN32_WCE
   
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "pcapExample: Error! bad command line\n");
        return false;
    }
    return true;
}  // end PcapExample::OnStartup()

void PcapExample::OnShutdown()
{
   if (NULL != pcap_device)
   {
       pcap_close(pcap_device);
       pcap_device = NULL;
   }   
   if (NULL != packeteer)
   {
        packeteer->Close();
        delete packeteer;
        packeteer = NULL;      
    }   
   PLOG(PL_ERROR, "pcapExample: Done.\n"); 
   CloseDebugLog();
}  // end PcapExample::OnShutdown()

bool PcapExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class PcapExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "PcapExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "PcapExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "PcapExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end PcapExample::ProcessCommands()

bool PcapExample::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "PcapExample::ProcessCommand(%s) missing argument\n", cmd);
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
            PLOG(PL_ERROR, "pcapExample: invalid <interfaceName>\n");
            return false;
        }
        if (NULL != pcap_device) pcap_close(pcap_device);
        char errbuf[PCAP_ERRBUF_SIZE+1];
        errbuf[0] = '\0';
        pcap_device = pcap_open_live(ifName, 65535, 1, 0, errbuf);
        if (NULL == pcap_device)
        {
            PLOG(PL_ERROR, "pcapExample: pcap_open_live() error: %s\n", errbuf);
            return false;   
        }
        if (0 != strlen(errbuf))
            PLOG(PL_ERROR, "pcapExample: pcap_open_live() warning: %s\n", errbuf);
        // set non-blocking for async I/O
        if (-1 == pcap_setnonblock(pcap_device, 1, errbuf))
            PLOG(PL_ERROR, "pcapExample: pcap_setnonblock() warning: %s\n", errbuf);
#ifdef WIN32
        ProtoDispatcher::Descriptor pcapDescriptor = pcap_getevent(pcap_device);
#else
        ProtoDispatcher::Descriptor pcapDescriptor = pcap_get_selectable_fd(pcap_device);
#endif // if/else WIN32/UNIX
        if (!dispatcher.InstallGenericInput(pcapDescriptor,
                                            PcapExample::DoPcapInputReady,
                                            this))
        {
            PLOG(PL_ERROR, "pcapExample: error installing pcap input\n");
            pcap_close(pcap_device);
            pcap_device = NULL;
            return false;
        }
    }
    else if (!strncmp("forward", cmd, len))
    {
        if (NULL != packeteer)
        {
            packeteer->Close();
        }
        else if (NULL == (packeteer = ProtoPacketeer::Create()))
        {
            PLOG(PL_ERROR, "pcapExample: error creating packeteer: %s\n", GetErrorString());
            return false;    
        }    
        if (!packeteer->Open(val))
        {
            PLOG(PL_ERROR, "pcapExample: error opening packeteer!\n");   
            return false;
        }   
    }
    return true;
}  // end PcapExample::OnCommand()

void PcapExample::DoPcapInputReady(ProtoDispatcher::Descriptor descriptor, 
                                   ProtoDispatcher::Event      theEvent, 
                                   const void*                 userData)
{
    if (ProtoDispatcher::EVENT_INPUT == theEvent)
        ((PcapExample*)userData)->OnPcapInputReady(); 
}  // end DoPcapInputReady()

void PcapExample::OnPcapInputReady()
{
    struct pcap_pkthdr* hdr;
    const u_char* data; 
    switch (pcap_next_ex(pcap_device, &hdr, &data))
    {
        case 1:     // pkt read
        {
            ProtoAddress src, dst;

            dst.SetRawHostAddress(ProtoAddress::ETH, (char*)data, 6);
            src.SetRawHostAddress(ProtoAddress::ETH, (char*)data+6, 6);
            TRACE("pcapExample: recv'd a packet length:%u (caplen:%lu)\n",
                    hdr->len, hdr->caplen);
            TRACE("             (src:%s dst:", src.GetHostString());
            TRACE("%s)\n", dst.GetHostString());
            // Example of sending a packet using ProtoPacketeer
            if (1514 == hdr->caplen)
            {
                hdr->caplen = 1513;
                packeteer->Send((char*)data, hdr->caplen);
            }
            break;
        }
        case 0:     // no pkt ready?
            break;   
        default:    // error (or EOF for offline)
            PLOG(PL_ERROR, "PcapExample::DoPcapInput() pcap_next_ex() error\n");
            break;
    } 
}  // end PcapExample::OnPcapInputReady()
