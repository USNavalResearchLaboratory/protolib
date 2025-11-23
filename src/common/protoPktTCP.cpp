#include "protoPktTCP.h"

ProtoPktTCP::ProtoPktTCP(void*          bufferPtr, 
                         unsigned int   numBytes, 
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (initFromBuffer)
            InitFromBuffer(); 
        else 
            InitIntoBuffer();
    }
}

ProtoPktTCP::~ProtoPktTCP()
{
}

bool ProtoPktTCP::InitFromPacket(ProtoPktIP& ipPkt)
{
    switch (ipPkt.GetVersion())
    {
        case 4:
        {
            // (TBD) support IPv4 extended headers
            ProtoPktIPv4 ip4Pkt(ipPkt);
            if (ProtoPktIP::TCP == ip4Pkt.GetProtocol())
            {
                return InitFromBuffer(ip4Pkt.AccessPayload(), ip4Pkt.GetPayloadLength(), false);
            }
            else
            {
                return false;  // not a TCP packet
            }
            break;
        }
        case 6:
        {
            ProtoPktIPv6 ip6Pkt(ipPkt);
            if (ip6Pkt.HasExtendedHeader())
            {
                unsigned int extHeaderLength = 0;
                ProtoPktIPv6::Extension::Iterator extIterator(ip6Pkt);
                ProtoPktIPv6::Extension ext;
                while (extIterator.GetNextExtension(ext))
                {
                    extHeaderLength += ext.GetLength();
                    if (ProtoPktIP::TCP == ext.GetNextHeader())
                    {
                        void* tcpBuffer = (char*)ip6Pkt.AccessPayload() + extHeaderLength;
                        unsigned int tcpLength = ip6Pkt.GetPayloadLength() - extHeaderLength;
                        return InitFromBuffer(tcpBuffer, tcpLength, false);
                    }
                }
                return false;  // not a TCP packet
            }
            else if (ProtoPktIP::TCP == ip6Pkt.GetNextHeader())
            {
                return InitFromBuffer(ip6Pkt.AccessPayload(), ip6Pkt.GetPayloadLength(), false);
            }
            else
            {
                return false;  // not a TCP packet
            }   
            break;
        }
        default:
            PLOG(PL_ERROR, "ProtoPktTCP::InitFromPacket() error: bad IP packet version: %d\n", ipPkt.GetVersion());
            return false;
    }
    return true;
}  // end ProtoPktTCP::InitFromPacket()

bool ProtoPktTCP::InitFromBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    UINT16 totalLen = GetPayloadLength() + (OffsetPayload() << 2);
    if (totalLen > GetBufferLength())
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    else
    {
        // (TBD) We could validate the checksum, too?
        ProtoPkt::SetLength(totalLen);
        return true;
    }
}  // end bool ProtoPktTCP::InitFromBuffer()

bool ProtoPktTCP::InitIntoBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct) 
{
    if (NULL != bufferPtr)
    {
        if (numBytes < 20)  // TCP header size
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    if (GetBufferLength() < 20) return false;
    SetDataOffset(5);
    ClearFlags();
    SetChecksum(0);
    return true;
}  // end ProtoPktTCP::InitIntoBuffer()
                
UINT16 ProtoPktTCP::ComputeChecksum(ProtoPktIP& ipPkt) const
{
    UINT32 sum = 0;
    // 1) Calculate pseudo-header part
    switch(ipPkt.GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ipv4Pkt(ipPkt);
            // a) src/dst addr pseudo header portion
            const UINT16* ptr = (const UINT16*)ipv4Pkt.GetSrcAddrPtr();
            int addrEndex = ProtoPktIPv4::ADDR_LEN;  // note src + dst = 2 addresses
            for (int i = 0; i < addrEndex; i++)
                sum += GetUINT16(ptr++);
            // b) protocol & "total length" pseudo header portions
            sum += (UINT16)ipv4Pkt.GetProtocol();
            sum += (UINT16)GetLength(); // TCP length
            break;
        }
        case 6:
        {
            ProtoPktIPv6 ipv6Pkt(ipPkt);
            // a) src/dst addr pseudo header portion
            const UINT16* ptr = (const UINT16*)ipv6Pkt.GetSrcAddrPtr();
            int addrEndex = ProtoPktIPv6::ADDR_LEN;  // note src + dst = 2 addresses
            for (int i = 0; i < addrEndex; i++)
                sum += GetUINT16(ptr++);
            sum += (UINT16)GetLength(); // TCP length
            sum += (UINT16)ipv6Pkt.GetNextHeader();
            break;
        }
        default:
            return 0;   
    }
    // 2) TCP header part, sans "checksum" field
    unsigned int i;
    for (i = 0; i < OFFSET_CHECKSUM; i++)
        sum += GetWord16(i);
    // 3) TCP payload part (note adjustment for odd number of payload bytes)
    unsigned int dataEndex = GetLength();
    if (0 != (dataEndex & 0x01))
        sum += ((UINT16)GetUINT8(dataEndex-1) << 8);
    dataEndex >>= 1;  // convert from bytes to UINT16 index
    for (i = (OFFSET_CHECKSUM+1); i < dataEndex; i++)
        sum += GetWord16(i);
    
    // 4) Carry as needed
    while (0 != (sum >> 16))
        sum = (sum & 0x0000ffff) + (sum >> 16);
    
    sum = ~sum;
    
    // 5) ZERO check/correct as needed
    if (0 == sum) sum = 0x0000ffff;
    return (UINT16)sum;
}  // end ProtoPktTCP::CalculateChecksum()
