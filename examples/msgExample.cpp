// This the the "node test" (nt) program to exercise our node, iface, link data structures


#include "manetMsg.h"

#include <stdio.h>  // for sprintf()
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>  // for isprint

#define PACKET_SIZE_MAX 1024

// These are some _FAKE_ "SMF" message types, TLVs, etc
// for purpose of testing the "ManetMsg" code.
// In reality, "SMF" messages/TLVs are of the NHDP
// protocol with a few actual SMF TLVs

enum SmfMsg
{
    SMF_RESERVED = 0,
    SMF_HELLO    = 1
};

// SMF TLV types
enum SmfTlv
{
    SMF_TLV_RESERVED        = 0,
    SMF_TLV_RELAY_ALGORITHM = 1,
    SMF_TLV_HELLO_INTERVAL  = 2,
    SMF_TLV_RELAY_WILLING   = 3,
    SMF_TLV_LINK_STATUS     = 4,
    SMF_TLV_MPR_SELECT      = 5,
    SMF_TLV_RTR_PRIORITY    = 6
};

// SMF relay algorithm types
enum SmfRelayAlgorithm
{
    SMF_RELAY_CF        = 0,
    SMF_RELAY_SMPR      = 1,
    SMF_RELAY_ECDS      = 2,
    SMF_RELAY_MPR_CDS   = 3
};

// SMF neighbor link states
enum SmfLinkStatus
{
    SMF_LINK_RESERVED  = 0,
    SMF_LINK_LOST      = 1,
    SMF_LINK_HEARD     = 2,
    SMF_LINK_SYMMETRIC = 3
};

void MakeDump(char* buffer, unsigned int buflen);
int ParseDump(char* file);
int ParseBuffer(UINT32* msgBuffer, unsigned int msgLength);

int main(int argc, char* argv[])
{

    if (argc > 1)
    {
        return ParseDump(argv[1]);
    }

    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    srand(currentTime.tv_usec);

    // 1) Test the generic pkt, msg, tlv stuff
    TRACE("msgExample: Building a packet ...\n");

    // a) Instantiate an "ManetPkt" from the stack
    UINT32 buffer[PACKET_SIZE_MAX/4];
    memset(buffer, 0, PACKET_SIZE_MAX/4);
    ManetPkt pkt;
    if (!pkt.InitIntoBuffer(buffer, PACKET_SIZE_MAX)) TRACE("ManetPkt::Init() error\n");

    // Add a pkt-tlv
    TRACE("   adding pkt-tlv to packet ...\n");
    ManetTlv* tlv = pkt.AppendTlv(SMF_TLV_RTR_PRIORITY);


    // b) Append a message to the pkt (multiple messages may go in an ManetPkt)
    //    (Note: the msg pointer returned here is invalid after another message is appended!)
    TRACE("   adding message to packet ...\n");
    ManetMsg* msg = pkt.AppendMessage();
    if (NULL == msg)
    {
        TRACE("msgExample: ManetPkt::AppendMessage() error\n");
        return -1;
    }

    // c) Set msg header fields
    msg->SetType(SMF_HELLO);
    ProtoAddress myAddr;
    myAddr.ResolveFromString("192.168.1.1");
    msg->SetOriginator(myAddr);


    TRACE("   adding a msg-tlv : type:%u ...\n", SMF_TLV_RELAY_ALGORITHM);
    // d) Append a message tlv ...
    tlv = msg->AppendTlv(SMF_TLV_RELAY_ALGORITHM);
    UINT8 relayAlgorithm = (UINT8)SMF_RELAY_SMPR;
    tlv->SetValue(relayAlgorithm);  // set TLV value

    // Append another message tlv
    TRACE("   adding another msg-tlv : type:%u ...\n", SMF_TLV_HELLO_INTERVAL);
    tlv = msg->AppendTlv(SMF_TLV_HELLO_INTERVAL);
    tlv->SetValue((UINT16)2);  // set TLV value


    TRACE("   adding an address block ...\n");
    unsigned int numAddrs = 8;
    // e) Add an address block
    ManetAddrBlock* addrBlk = msg->AppendAddressBlock();
    if (NULL == addrBlk)
    {
        TRACE("msgExample: ManetMsg::AppendAddressBlock() error\n");
        return -1;
    }

    TRACE("   setting 2-byte address block head ...\n");
    ProtoAddress prefix;
    prefix.ResolveFromString("192.168.1.0");
    if (!addrBlk->SetHead(prefix, 2))
    {
        TRACE("msgExample: addrBlk.SetHead() error\n");
        return -1;
    }
    TRACE("   setting 1-byte address block tail ...\n");
    prefix.ResolveFromString("192.168.1.1");
    if (!addrBlk->SetTail(prefix, 1))
    {
        TRACE("msgExample: addrBlk.SetHead() error\n");
        return -1;
    }

    // For testing, we add a batch of addresses w/ link status & "willingness" tlv's
    // (Note that we build the tlv's into separate buffers to parallelize addr blk/tlv building)

    // This one will be a "multi-value" tlv with different value for each address
    UINT8 tlvType = (UINT8)SMF_TLV_LINK_STATUS;
    UINT32 linkStatusTlvBuffer[256];
    ManetTlv linkStatusTlv;
    linkStatusTlv.InitIntoBuffer(tlvType, (char*)linkStatusTlvBuffer, 256*sizeof(UINT32));
    linkStatusTlv.SetIndexRange(0, 7, true, numAddrs);

    // This one will be a "single-value" tlv applied to a subset of addresses
    tlvType = (UINT8)SMF_TLV_RELAY_WILLING;
    UINT32 willingnessTlvBuffer[256];
    ManetTlv willingnessTlv;
    willingnessTlv.InitIntoBuffer(tlvType, (char*)willingnessTlvBuffer, 256*sizeof(UINT32));
    willingnessTlv.SetIndexRange(2, 5, false, numAddrs);
    willingnessTlv.SetValue((UINT8)2);

    TRACE("   adding 8 addresses ...\n");
    for (unsigned int i = 0; i < numAddrs; i++)
    {
        char addrString[32];
        sprintf(addrString, "192.168.%d.1", i+2);
        ProtoAddress neighbor;
        neighbor.ResolveFromString(addrString);
        addrBlk->AppendAddress(neighbor);  // should check result
        // We apply a separate tlv value to each address
        // (Note these could be applied to a subset
        //  of the addresses if desired)
        UINT8 linkStatus;
        if (0 == (i & 0x01))
            linkStatus = SMF_LINK_HEARD;
        else
            linkStatus = SMF_LINK_SYMMETRIC;
        linkStatusTlv.SetValue(linkStatus, i, numAddrs);
    }

    TRACE("   adding addr-blk-tlvs ...\n");
    // Copy the "pre-built" tlv's into the address block
    if (!addrBlk->AppendTlv(linkStatusTlv))
    {
        TRACE("msgExample: ManetAddrBlk::AppendTlv(linkStatusTlv) error\n");
        return -1;
    }
    if (!addrBlk->AppendTlv(willingnessTlv))
    {
        TRACE("msgExample: ManetAddrBlk::AppendTlv(willingnessTlv) error\n");
        return -1;
    }
    //  Add a non-indexed, single-value tlv to address block
    tlv = addrBlk->AppendTlv(SMF_TLV_MPR_SELECT);
    if (NULL == tlv)
    {
        TRACE("msgExample: ManetAddrBlk::AppendTlv() error\n");
        return -1;
    }
    if (!tlv->SetValue((UINT8)1))  // set value to TRUE
    {
        TRACE("msgExample: ManetTlv::SetValue() error\n");
        return -1;
    }

    // Add _another_ address block for testing purposes
    TRACE("   adding another address block ...\n");
    addrBlk = msg->AppendAddressBlock();
    if (NULL == addrBlk)
    {
        TRACE("msgExample: ManetMsg::AppendAddressBlock() error\n");
        return -1;
    }
    prefix.ResolveFromString("192.168.2.0");
    TRACE("   setting 3-byte addr-blk head\n");
    if (!addrBlk->SetHead(prefix, 3))
    {
        TRACE("msgExample: addrBlk.SetHead() error\n");
        return -1;
    }
    // For testing, we add a batch of addresses
    TRACE("   adding 4 addresses to addr-blk\n");
    for (int i = 2; i < 6; i++)
    {
        char addrString[32];
        sprintf(addrString, "192.168.2.%d", i);
        ProtoAddress neighbor;
        neighbor.ResolveFromString(addrString);
        addrBlk->AppendAddress(neighbor);
    }

    // f) Finally, "pack" the packet to finalize structure
    TRACE("   finalizing packet ...\n");
    pkt.Pack();
    TRACE("msgExample: Packet build completed, len:%u\n\n\n", pkt.GetLength());

    // OK, let's parse a "received" packet using sent "buffer" and "pktLen"
    TRACE("msgExample: Parsing \"recvPkt\" ...\n");
    UINT16 pktLen = pkt.GetLength();

    MakeDump((char*)buffer, pktLen);

    int result = ParseBuffer(buffer, pktLen);

    TRACE("msgExample: \"recvPkt\" packet parsing completed.\n\n");

    MakeDump((char*)buffer, pktLen);

    return result;
}  // end main()

// Output buffer in INRIA PacketBB "hex dump" format
void MakeDump(char* buffer, unsigned int buflen)
{
    TRACE("msgExample:  outputting \"hex dump\" of packet ...\n");
    for (unsigned int i = 0; i< buflen; i++)
    {
        printf("%02x ", (UINT8)buffer[i]);
        if (0 == (i+1)%4) printf("\n");
    }
    printf("\n");
}  // end MakeDump();

// Parse file containing PacketBB packet in INRIA PacketBB "hex dump" format
int ParseDump(char* file)
{
    FILE* infile = fopen(file, "r");
    if (NULL == infile)
    {
        perror("msgExample: fopen() error");
        return - 1;
    }

    UINT32 msgBuffer[1024];
    unsigned int msgLength = 0;

    unsigned int val;
    while (1 == fscanf(infile, "%x", &val))
        ((char*)msgBuffer)[msgLength++] = (char)val;

    int result = ParseBuffer(msgBuffer, msgLength);

    fclose(infile);
    return result;
}  // end ParseDump()

int ParseBuffer(UINT32* msgBuffer, unsigned int msgLength)
{
    TRACE("msgExample: Parsing %u-byte packet buffer ...\n", msgLength);
    // Parse/display Manet PacketBB Pkt
    ManetPkt recvPkt;  // "received" packet in "buffer" already ...
    if (!recvPkt.InitFromBuffer(msgLength, msgBuffer, msgLength))
    {
        TRACE("msgExample: recvPkt.InitFromBuffer() error\n");
        return -1;
    }

    // Iterate through any packet TLVs
    ManetPkt::TlvIterator pktTlvIterator(recvPkt);

    ManetTlv recvTlv;
    while (pktTlvIterator.GetNextTlv(recvTlv))
    {
        TRACE("  found pkt-tlv, type:%u len:%lu\n", recvTlv.GetType(), recvTlv.GetTlvLength());
    }

    // Iterate through messages ...
    ManetPkt::MsgIterator iterator(recvPkt);
    ManetMsg recvMsg;

    while (iterator.GetNextMessage(recvMsg))
    {
        TRACE("  found message, type:%d len:%d ", recvMsg.GetType(), recvMsg.GetLength());
        if (recvMsg.HasOriginator())
        {
            ProtoAddress origin;
            recvMsg.GetOriginator(origin);
            TRACE("originator:%s", origin.GetHostString());
        }
        TRACE("\n");

        // Iterate through any message tlv's ...
        ManetMsg::TlvIterator iterator(recvMsg);
        while (iterator.GetNextTlv(recvTlv))
        {
            TRACE("    got msg-tlv, type:%u len:%lu\n", recvTlv.GetType(), recvTlv.GetTlvLength());
        }

        // Iterate through any address blocks in the message
        ManetMsg::AddrBlockIterator addrBlkIterator(recvMsg);
        ManetAddrBlock recvAddrBlock;
        while (addrBlkIterator.GetNextAddressBlock(recvAddrBlock))
        {
            TRACE("    got addr block len:%u w/ %u addresses (len:%u)\n",
                  recvAddrBlock.GetLength(), recvAddrBlock.GetAddressCount(), recvAddrBlock.GetAddressLength());
            // Iterate through addresses in this block
            unsigned int addrCount = recvAddrBlock.GetAddressCount();
            for (unsigned int i = 0; i < addrCount; i++)
            {
                ProtoAddress addr;
                if (recvAddrBlock.GetAddress(i, addr))
                {
                    TRACE("      addr(%u): %s\n", i, addr.GetHostString());
                }
                else
                {
                    TRACE("msgExample: ManetAddrBlock::GetAddress(%u) error\n", i);
                    return -1;
                }
            }

            // Iterate through any tlv's associated with this block
            ManetAddrBlock::TlvIterator iterator(recvAddrBlock);
            while (iterator.GetNextTlv(recvTlv))
            {
                UINT8 start, stop;
                if (recvTlv.HasIndex())
                {
                    start = recvTlv.GetIndexStart();
                    stop = recvTlv.GetIndexStop(addrCount);

                }
                else
                {
                    start = 0;
                    stop = addrCount - 1;
                }
                if (recvTlv.IsMultiValue())
                {
                    TRACE("      found indexed, multi-value addr-blk-tlv, type:%d len:%u\n",
                          recvTlv.GetType(), recvTlv.GetLength());
                }
                else
                {
                    TRACE("      found %sindexed single-value addr-blk-tlv, type:%d len:%u indices:%u-%u\n",
                          recvTlv.HasIndex() ? "" : "non-", recvTlv.GetType(), recvTlv.GetLength(),
                          start, stop);
                    start = stop;  // to force print out of _single_ value
                }
                for (UINT8 index = start; index <= stop; index++)
                {
                    switch (recvTlv.GetValueLength(addrCount))
                    {
                        case 1:
                        {
                            UINT8 val;
                            recvTlv.GetValue(val, index, addrCount);
                            TRACE("        value[%u] = 0x%02x\n", index, val);
                            break;
                        }
                        case 2:
                        {
                            UINT16 val;
                            recvTlv.GetValue(val, index, addrCount);
                            TRACE("        value[%u] = 0x%04x\n", index, val);
                            break;
                        }
                        case 4:
                        {
                            UINT32 val;
                            recvTlv.GetValue(val, index, addrCount);
                            TRACE("        value[%u] = 0x%08x\n", index, val);
                            break;
                        }
                        default:
                        {
                            UINT16 valueLength = recvTlv.GetValueLength(addrCount);
                            UINT8* vptr = (UINT8*)recvTlv.GetValuePtr();//valueLength, index, addrCount);
                            TRACE("        value[%u] = 0x\n", index);
                            for (unsigned int i = 0; i < valueLength; i++)
                                TRACE("%02x", *vptr++);
                            TRACE("\n");
                            break;
                        }
                    }
                }
            }
        }
    }
    TRACE("msgExample: packet parsing completed.\n");
    return 0;
}  // end ParseBuffer()
