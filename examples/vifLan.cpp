#include "protoApp.h"
#include "protoVif.h"
#include "protoPktIP.h"
#include "protoPktETH.h"
#include "protoPktARP.h"

#include "protoList.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>  // for "isspace()"


class VifItem : public ProtoList::Item
{
    public:
        VifItem(ProtoVif* theVif);
        ~VifItem();
        
        ProtoVif* GetVif() 
            {return vif;}
        
    private:
        ProtoVif* vif;
};  // end class VifItem

class VifList : public ProtoListTemplate<VifItem> {};


VifItem::VifItem(ProtoVif* theVif)
 : vif(theVif)
{
}

VifItem::~VifItem()
{
    if (NULL != vif)
    {
        delete vif;
        vif = NULL;
    }
}

/**
 * @class VifLanApp
 *
 * @brief Example using protoVif
 *
 */
class VifLanApp : public ProtoApp
{
    public:
      VifLanApp();
      ~VifLanApp();

      bool OnStartup(int argc, const char*const* argv);
      bool ProcessCommands(int argc, const char*const* argv) {return false;}
      void OnShutdown();

    private:
      enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
      static const char* const CMD_LIST[];
      static CmdType GetCmdType(const char* string);
      bool OnCommand(const char* cmd, const char* val);        
      void Usage();
            
      void PeekPkt(ProtoPktETH& ethPkt, bool inbound);

      void OnOutboundPkt(ProtoChannel& theChannel,
                         ProtoChannel::Notification notifyType);
      void OnInboundPkt(ProtoChannel& theChannel,
                        ProtoChannel::Notification notifyType);
      
      VifList   vif_list;

}; // end class VifLanApp

void VifLanApp::Usage()
{
    fprintf(stderr, "Usage: vifLan <vifName1> <vifName2> ...\n");
}

const char* const VifLanApp::CMD_LIST[] =
{
    "-help",        // print help info an exit
    "+debug",       // <debugLevel>
    NULL
};

/**
 * This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(VifLanApp) 

VifLanApp::VifLanApp()
{
}

VifLanApp::~VifLanApp()
{
}

VifLanApp::CmdType VifLanApp::GetCmdType(const char* cmd)
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
}  // end VifLanApp::GetCmdType()


bool VifLanApp::OnStartup(int argc, const char*const* argv)
{
    ProtoAddress invalidAddr;
    for (int i = 1; i < argc; i++)
    {
        // Create virtual interface with given name
        ProtoVif* vif = ProtoVif::Create();
        if (NULL == vif)
        {
            PLOG(PL_ERROR, "VifLanApp::OnStartup() new ProtoVif error: %s\n", GetErrorString());
            vif_list.Destroy();
            return false;
        }
        vif->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
        vif->SetListener(this,&VifLanApp::OnOutboundPkt);
        // Open virtual interface with no address assigned
        if (!vif->Open(argv[i], invalidAddr, 0))
        {
            PLOG(PL_ERROR,"VifLanApp::OnStartup() ProtoVif::Open() error\n");
            delete vif;
            vif_list.Destroy();
            return false;
        }       
        VifItem* vifItem = new VifItem(vif);
        if (NULL == vifItem)
        {
            PLOG(PL_ERROR, "VifLanApp::OnStartup() new VifItem error: %s\n", GetErrorString());
            delete vif;
            vif_list.Destroy();
            return false;
        }
        vif_list.Append(*vifItem);
    }
    return true;
}  // end VifLanApp::OnStartup()

void VifLanApp::OnShutdown()
{
    // Closes and deletes all ProtoVifs in vif_list
    vif_list.Destroy();
    PLOG(PL_INFO, "vifLan: Done.\n"); 

}  // end VifLanApp::OnShutdown()


/**
 * @note We offset the buffer here by 2 bytes since 
 * Ethernet header is 14 bytes
 * (i.e. not a multiple of 4 (sizeof(UINT32))
 * This gives us a properly aligned buffer for 
 * 32-bit aligned IP packets
 */
void VifLanApp::OnOutboundPkt(ProtoChannel&              theChannel,
                              ProtoChannel::Notification notifyType)
{
    const int BUFFER_MAX = 4096;
    UINT32 alignedBuffer[BUFFER_MAX/sizeof(UINT32)];
    // offset by 2-bytes so IP content is 32-bit aligned
    UINT16* ethBuffer = ((UINT16*)alignedBuffer) + 1; 
    unsigned int numBytes = BUFFER_MAX - 2;
    ProtoVif& vif = static_cast<ProtoVif&>(theChannel);
    while (vif.Read((char*)ethBuffer, numBytes))
    {
        if (numBytes == 0) break;  // no more packets to receive
        
        
        
        
        ProtoPktETH ethPkt((UINT32*)ethBuffer, BUFFER_MAX - 2);
        if (!ethPkt.InitFromBuffer(numBytes))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnOutboundPkt() error: bad Ether frame\n");
            continue;
        }
        // Uncomment this to get printout of transmitted packets
        //PeekPkt(ethPkt, false);
        
        // Iterate through the vif_list and "output" this packet to 
        // the _other_ interfaces on our virtual LAN
        VifList::Iterator iterator(vif_list);
        VifItem* nextItem;
        while (NULL != (nextItem = iterator.GetNextItem()))
        {
            ProtoVif* nextVif = nextItem->GetVif();
            if (&vif == nextVif) continue;
            if (!nextVif->Write((char*)ethBuffer, numBytes))
            {
                PLOG(PL_ERROR, "VifLanApp::OnOutboundPkt() error: error writing packet\n");
            }
        }
        numBytes = BUFFER_MAX;
    }
} // end VifLanApp::OnOutboundPkt


// Method useful for debugging
void VifLanApp::PeekPkt(ProtoPktETH& ethPkt, bool inbound)
{
    switch (ethPkt.GetType())
    {
        case ProtoPktETH::IP:
        case ProtoPktETH::IPv6:
        {
            unsigned int payloadLength = ethPkt.GetPayloadLength();
            ProtoPktIP ipPkt;
            if (!ipPkt.InitFromBuffer(payloadLength, (UINT32*)ethPkt.AccessPayload(), payloadLength))
            {
                PLOG(PL_ERROR, "vifLan::PeekPkt() error: bad %sbound IP packet\n",
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
                    PLOG(PL_ERROR,"VifLanApp::PeekPkt() Error: Invalid IP pkt version.\n");
                    break;
                }
            }
            PLOG(PL_ALWAYS, "VifLanApp::PeekPkt() %sbound  packet IP dst>%s ",
                    inbound ? "in" : "out", dstAddr.GetHostString());
            PLOG(PL_ALWAYS," src>%s length>%d\n", srcAddr.GetHostString(), ipPkt.GetLength());
            break;
        }
        case ProtoPktETH::ARP:
        {
            ProtoPktARP arp;
            if (!arp.InitFromBuffer((UINT32*)ethPkt.AccessPayload(), ethPkt.GetPayloadLength()))
            {
                PLOG(PL_ERROR, "VifLanApp::PeekPkt() received bad ARP packet?\n");
                break;
            } 
            PLOG(PL_ALWAYS,"VifLanApp::PeekPkt() %sbound ARP ", 
                  inbound ? "in" : "out");
            switch(arp.GetOpcode())
            {
                case ProtoPktARP::ARP_REQ:
                    PLOG(PL_ERROR, "request ");
                    break;
                case ProtoPktARP::ARP_REP:
                    PLOG(PL_ERROR, "reply ");
                    break;
                default:
                    PLOG(PL_ERROR, "??? ");
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
                PLOG(PL_ERROR, "eth:%s ", addr.GetHostString()); 
            }
            PLOG(PL_ALWAYS, "\n");
            break;
        }
        default:
            PLOG(PL_ERROR, "VifLanApp::PeekPkt() unknown %s packet type\n", inbound ? "inbound" : "outbound");
            break;
    }
}  // end VifLanApp::PeekPkt()
