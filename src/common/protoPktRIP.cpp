
#include "protoPktRIP.h"
#include <string.h>  // for memset(), etc

ProtoPktRIP::ProtoPktRIP(void*          bufferPtr, 
                         unsigned int   numBytes,  
                         unsigned int   pktLength,
                         bool           freeOnDestruct)
: ProtoPkt(bufferPtr, numBytes, freeOnDestruct)                       
{
    if (0 != pktLength) 
        InitFromBuffer(pktLength);
    else
        InitIntoBuffer();
}

ProtoPktRIP::~ProtoPktRIP()
{
}

bool ProtoPktRIP::InitIntoBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr) 
    {
        if (numBytes < (4*OFFSET_PAYLOAD))
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < (4*OFFSET_PAYLOAD)) 
    {
        return false;
    }
    SetCommand(INVALID);
    SetVersion(2);
    SetLength(4*OFFSET_PAYLOAD);
    return true;
}  // end ProtoPktRIP::InitIntoBuffer()

bool ProtoPktRIP::AddRouteEntry(const ProtoAddress&  destAddr,
                                UINT32               maskLen,
                                const ProtoAddress&  nextHop,
                                UINT32               metric,
                                UINT16               routeTag)
{
    // We only support IPv4 for now
    if (ProtoAddress::IPv4 != destAddr.GetType() ||
        ProtoAddress::IPv4 != nextHop.GetType())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::AddRouteEntry() error: invalid address type\n");
        return false;
    }
    if (maskLen > 32)
    {
        PLOG(PL_ERROR, "ProtoPktRIP::AddRouteEntry() error: invalid mask length\n");
        return false;
    }
    if (metric > 16)
    {
        PLOG(PL_ERROR, "ProtoPktRIP::AddRouteEntry() error: invalid metric value\n");
        return false;
    }
    // Is there space for another entry?
    unsigned space = GetBufferLength() - GetLength();
    if (space < 20)  // RIP route entries are 20 bytes
    {
        PLOG(PL_WARN, "ProtoPktRIP::AddRouteEntry() warning: insufficient buffer space\n");
        return false;
    }
    ProtoPktRIP::RouteEntry entry((char*)AccessBuffer() + GetLength(), 20);
    entry.SetAddressFamily(IPv4);
    entry.SetRouteTag(routeTag);
    entry.SetAddress(destAddr);
    entry.SetMaskLength(maskLen);
    entry.SetNextHop(nextHop);
    entry.SetMetric(metric);
    SetLength(GetLength() + 20);
    return true;
}  // end ProtoPktRIP::AddRouteEntry()

bool ProtoPktRIP::InitFromBuffer(unsigned int   pktLength,
                                 void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (numBytes < pktLength)  // UDP header size
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    if (GetBufferLength() < pktLength)
    {
        PLOG(PL_ERROR, "ProtoPktRIP::InitFromBuffer() error: insufficient buffer size\n");
        return false;
    }
    if (pktLength < (4*OFFSET_PAYLOAD))
    {
        PLOG(PL_ERROR, "ProtoPktRIP::InitFromBuffer() error: truncated packet? (pktLen:%u offset:%u\n", pktLength, 4*OFFSET_PAYLOAD);
        return false;
    }
    SetLength(pktLength);
    return true;
}  // end ProtoPktRIP::InitFromBuffer()

unsigned int ProtoPktRIP::GetNumEntry() const
{
    unsigned int pktLen = GetLength();
    if (pktLen < (4*OFFSET_PAYLOAD))
        return 0;
    else
        pktLen -= 4*OFFSET_PAYLOAD; 
    return pktLen / 20;  // RIP is 20 bytes per entry
}  // end ProtoPktRIP::GetNumEntry()

bool ProtoPktRIP::AccessRouteEntry(unsigned int index, RouteEntry& entry)
{
    if (index > (GetNumEntry() - 1))
    {
        PLOG(PL_ERROR, "ProtoPktRIP::AccessRouteEntry() error: invalid route entry index\n");
        return false;
    }
    // Compute UINT32* pointer (20 byte entry is 5 UINT32's)
    void* entryBuffer = AccessBuffer32(OFFSET_PAYLOAD + 5*index);
    return entry.InitFromBuffer(20, entryBuffer, 20);
}  // end ProtoPktRIP::AccessRouteEntry()

ProtoPktRIP::RouteEntry::RouteEntry(void*          bufferPtr, 
                                    unsigned int   numBytes,  
                                    bool           initFromBuffer,
                                    bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)                       
{
    if (initFromBuffer) 
        InitFromBuffer(numBytes);
    else
        InitIntoBuffer();
}

ProtoPktRIP::RouteEntry::~RouteEntry()
{
}

bool ProtoPktRIP::RouteEntry::InitIntoBuffer(void*          bufferPtr, 
                                             unsigned int   numBytes, 
                                             bool           freeOnDestruct)
{
    if (NULL != bufferPtr) 
    {
        if (numBytes < 20)
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 20) 
    {
        return false;
    }
    memset(AccessBuffer(), 0, 20);
    SetLength(20);
    return true;
}  // end ProtoPktRIP::RouteEntry::InitIntoBuffer()

bool ProtoPktRIP::RouteEntry::SetAddress(const ProtoAddress& addr)
{
    if (ProtoAddress::IPv4 != addr.GetType())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::SetAddress() error: invalid address type\n");
        return false;
    }
    memcpy(AccessBuffer(OFFSET_ADDR), addr.GetRawHostAddress(), 4);
    return true;
}  // end ProtoPktRIP::RouteEntry::SetAddress()

bool ProtoPktRIP::RouteEntry::SetMask(const ProtoAddress& addr)
{
    if (ProtoAddress::IPv4 != addr.GetType())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::SetMask() error: invalid mask address\n");
        return false;
    }
    memcpy(AccessBuffer32(OFFSET_MASK), addr.GetRawHostAddress(), 4);
    return true;
}  // end ProtoPktRIP::RouteEntry::SetMaskLength()

bool ProtoPktRIP::RouteEntry::SetMaskLength(UINT8 maskLen)
{
    if (maskLen > 32)
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::SetMaskLength() error: invalid mask length\n");
        return false;
    }
    ProtoAddress maskAddr;
    maskAddr.GeneratePrefixMask(ProtoAddress::IPv4, maskLen);
    return SetMask(maskAddr);
}

bool ProtoPktRIP::RouteEntry::SetNextHop(const ProtoAddress& nextHop)
{
    if (ProtoAddress::IPv4 != nextHop.GetType())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::SetAddress() error: invalid address type\n");
        return false;
    }
    memcpy(AccessBuffer32(OFFSET_NHOP), nextHop.GetRawHostAddress(), 4);
    return true;
}  // end ProtoPktRIP::RouteEntry::SetNextHop()

bool ProtoPktRIP::RouteEntry::InitFromBuffer(unsigned int   pktLength,
                                             void*          bufferPtr, 
                                             unsigned int   numBytes, 
                                             bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (numBytes < pktLength)  // UDP header size
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    if (GetBufferLength() < pktLength)
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::InitFromBuffer() error: insufficient buffer size\n");
        return false;
    }
    if (pktLength < 20)
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::InitFromBuffer() error: truncated packet?\n");
        return false;
    }
    SetLength(20);
    return true;
}  // end ProtoPktRIP::RouteEntry::InitFromBuffer()

bool ProtoPktRIP::RouteEntry::GetAddress(ProtoAddress& addr) const
{
    if (IPv4 != GetAddressFamily())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::GetAddress() error: invalid address family: %d\n", GetAddressFamily());
        return false;
    }
    addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer32(OFFSET_ADDR), 4);
    return true;
}  // end ProtoPktRIP::RouteEntry::GetAddress()

bool ProtoPktRIP::RouteEntry::GetMask(ProtoAddress& addr) const
{
    if (IPv4 != GetAddressFamily())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::GetMask() error: invalid address family\n");
        return 0;
    }
    addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer32(OFFSET_MASK), 4);
    return true;
}  // end ProtoPktRIP::RouteEntry::GetMask()

UINT8 ProtoPktRIP::RouteEntry::GetMaskLength() const
{
    ProtoAddress maskAddr;
    if (!GetMask(maskAddr))
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::GetMaskLength() error: invalid address family\n");
        return 0;
    }
    return maskAddr.GetPrefixLength();
}  // end ProtoPktRIP::RouteEntry::GetMaskLength()

bool ProtoPktRIP::RouteEntry::GetNextHop(ProtoAddress& addr) const
{
    if (IPv4 != GetAddressFamily())
    {
        PLOG(PL_ERROR, "ProtoPktRIP::RouteEntry::GetNextHop() error: invalid address family\n");
        return 0;
    }
    addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer32(OFFSET_NHOP), 4);
    return true;
}  // end ProtoPktRIP::RouteEntry::GetNextHop()


