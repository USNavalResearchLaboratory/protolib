/**
* @file protoPktARP.cpp
* 
* @brief This class provides access to ARP packets
*/
#include "protoPktARP.h"
#include "protoDebug.h"

ProtoPktARP::ProtoPktARP(UINT32*        bufferPtr, 
                         unsigned int   numBytes, 
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
{
}

ProtoPktARP::~ProtoPktARP()
{
}

bool ProtoPktARP::InitFromBuffer(UINT32* bufferPtr, 
                                 unsigned int numBytes, 
                                 bool freeOnDestruct)
{
    unsigned int minLength = OFFSET_SNDR_HRD_ADDR;
    if (NULL != bufferPtr)
    {
        if (numBytes < minLength)  // UDP header size
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    if (GetBufferLength() < minLength)
    {
        PLOG(PL_ERROR, "ProtoPktARP::InitFromBuffer() error: insufficient buffer size\n");
        return false;
    }
    minLength += 2*(GetHardwareAddrLen() + GetProtocolAddrLen());
    if (GetBufferLength() < minLength)
    {
        PLOG(PL_ERROR, "ProtoPktARP::InitFromBuffer() error: invalid packet\n");
        return false;
    }
    SetLength(minLength);
    return true;
}  // end ProtoPktARP::InitFromBuffer()

bool ProtoPktARP::GetSenderHardwareAddress(ProtoAddress& addr) const
{
    switch (GetHardwareType())
    {
        case ETHERNET:
        case IEEE802:
            addr.SetRawHostAddress(ProtoAddress::ETH, ((char*)buffer_ptr) + OffsetSenderHardwareAddr(), GetHardwareAddrLen());
            break;
        default:
            PLOG(PL_ERROR, "ProtoPktARP::GetSenderHardwareAddress() error: unsupported hardware type\n");
            return false;
    }
    return true;
}  // end ProtoPktARP::GetSenderHardwareAddress()
        
bool ProtoPktARP::GetSenderProtocolAddress(ProtoAddress& addr) const
{
    switch (GetEtherType())
    {
        case ProtoPktETH::IP:
        case ProtoPktETH::IPv6:
            if (4 == GetProtocolAddrLen())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv4, ((char*)buffer_ptr) + OffsetSenderProtocolAddr(), 4);
            }
            else if (16 == GetProtocolAddrLen())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv6, ((char*)buffer_ptr) + OffsetSenderProtocolAddr(), 16);
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPktARP::GetSenderProtocolAddress() error: invalid IP address length\n");
                return false;
            }
            break;
        default:
            PLOG(PL_ERROR, "ProtoPktARP::GetSenderProtocolAddress() error: unsupported hardware type\n");
            return false;
    }
    return true;
}  // end ProtoPktARP::GetSenderProtocolAddress()

bool ProtoPktARP::GetTargetHardwareAddress(ProtoAddress& addr) const
{
    switch (GetHardwareType())
    {
        case ETHERNET:
        case IEEE802:
            addr.SetRawHostAddress(ProtoAddress::ETH, ((char*)buffer_ptr) + OffsetTargetHardwareAddr(), GetHardwareAddrLen());
            break;
        default:
            PLOG(PL_ERROR, "ProtoPktARP::GetTargetHardwareAddress() error: unsupported hardware type\n");
            return false;
    }
    return true;
}  // end ProtoPktARP::GetTargetHardwareAddress()

bool ProtoPktARP::GetTargetProtocolAddress(ProtoAddress& addr) const
{
    switch (GetEtherType())
    {
        case ProtoPktETH::IP:
        case ProtoPktETH::IPv6:
            if (4 == GetProtocolAddrLen())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv4, ((char*)buffer_ptr) + OffsetTargetProtocolAddr(), 4);
            }
            else if (16 == GetProtocolAddrLen())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv6, ((char*)buffer_ptr) + OffsetTargetProtocolAddr(), 16);
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPktARP::GetTargetProtocolAddress() error: invalid IP address length\n");
                return false;
            }
            break;
        default:
            PLOG(PL_ERROR, "ProtoPktARP::GetTargetProtocolAddress() error: unsupported hardware type\n");
            return false;
    }
    return true;
}  // end ProtoPktARP::GetTargetProtocolAddress()

bool ProtoPktARP::SetSenderHardwareAddress(const ProtoAddress& addr)
{
    // TBD - support more types and verify that target/sender hw addr type are equal!
    if (ProtoAddress::ETH != addr.GetType()) 
    {
        PLOG(PL_ERROR, "ProtoPktARP::SetSenderHardwareAddress() error: unsupported address type\n");
        return false;
    }
    SetHardwareType(ETHERNET);
    SetHardwareAddrLen(addr.GetLength());
    memcpy(((char*)buffer_ptr) + OffsetSenderHardwareAddr(), addr.GetRawHostAddress(), addr.GetLength());
    return true;
}  // end ProtoPktARP::SetSenderHardwareAddress()

bool ProtoPktARP::SetSenderProtocolAddress(const ProtoAddress& addr)
{
    ProtoPktETH::Type etherType;
    switch (addr.GetType()) 
    {
        case ProtoAddress::IPv4:
            etherType = ProtoPktETH::IP;
            break;
        case ProtoAddress::IPv6:
            etherType = ProtoPktETH::IPv6;
            break;
        default:
            PLOG(PL_ERROR, "ProtoPktARP::SetSenderProtocolAddress() error: unsupported address type\n");
            return false;
    }
    SetEtherType(etherType);
    SetProtocolAddrLen(addr.GetLength());
    memcpy(((char*)buffer_ptr) + OffsetSenderProtocolAddr(), addr.GetRawHostAddress(), addr.GetLength());
    return true;
}  // end ProtoPktARP::SetSenderProtocolAddress()

bool ProtoPktARP::SetTargetHardwareAddress(const ProtoAddress& addr)
{
    // TBD - support more types and verify that target/sender hw addr type are equal!
    if (ProtoAddress::ETH != addr.GetType()) 
    {
        PLOG(PL_ERROR, "ProtoPktARP::SetTargetHardwareAddress() error: unsupported address type\n");
        return false;
    }
    SetHardwareType(ETHERNET);
    SetHardwareAddrLen(addr.GetLength());
    memcpy(((char*)buffer_ptr) + OffsetTargetHardwareAddr(), addr.GetRawHostAddress(), addr.GetLength());
    return true;
}  // end ProtoPktARP::SetTargetHardwareAddress()

bool ProtoPktARP::SetTargetProtocolAddress(const ProtoAddress& addr)
{
    ProtoPktETH::Type etherType;
    switch (addr.GetType()) 
    {
        case ProtoAddress::IPv4:
            etherType = ProtoPktETH::IP;
            break;
        case ProtoAddress::IPv6:
            etherType = ProtoPktETH::IPv6;
            break;
        default:
            PLOG(PL_ERROR, "ProtoPktARP::SetTargetProtocolAddress() error: unsupported address type\n");
            return false;
    }
    SetEtherType(etherType);
    SetProtocolAddrLen(addr.GetLength());
    memcpy(((char*)buffer_ptr) + OffsetTargetProtocolAddr(), addr.GetRawHostAddress(), addr.GetLength());
    return true;
}  // end ProtoPktARP::SetTargetProtocolAddress()
