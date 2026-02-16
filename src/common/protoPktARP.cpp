/**
* @file protoPktARP.cpp
* 
* @brief This class provides access to ARP packets
*/
#include "protoPktARP.h"
#include "protoDebug.h"

ProtoPktARP::ProtoPktARP(void*          bufferPtr, 
                         unsigned int   numBytes, 
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
  : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer();
    else
        InitIntoBuffer();
}

ProtoPktARP::~ProtoPktARP()
{
}

bool ProtoPktARP::InitFromBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
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
            addr.SetRawHostAddress(ProtoAddress::ETH, (char*)GetBuffer(OffsetSenderHardwareAddr()), GetHardwareAddrLen());
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
                addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer(OffsetSenderProtocolAddr()), 4);
            }
            else if (16 == GetProtocolAddrLen())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv6, (char*)GetBuffer(OffsetSenderProtocolAddr()), 16);
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
            addr.SetRawHostAddress(ProtoAddress::ETH, (char*)GetBuffer(OffsetTargetHardwareAddr()), GetHardwareAddrLen());
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
                addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer(OffsetTargetProtocolAddr()), 4);
            }
            else if (16 == GetProtocolAddrLen())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv6, (char*)GetBuffer(OffsetTargetProtocolAddr()), 16);
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

bool ProtoPktARP::InitIntoBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
{
    unsigned int minLength = OFFSET_SNDR_HRD_ADDR;
    if (NULL != bufferPtr) 
    {
        if (numBytes < minLength)
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < minLength) 
    {
        return false;
    }
    SetLength(minLength);
    return true;
}  // end ProtoPktARP::InitIntoBuffer()

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
    memcpy(AccessBuffer(OffsetSenderHardwareAddr()), addr.GetRawHostAddress(), addr.GetLength());
    SetLength(GetLength() + addr.GetLength());
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
    memcpy(AccessBuffer(OffsetSenderProtocolAddr()), addr.GetRawHostAddress(), addr.GetLength());
    SetLength(GetLength() + addr.GetLength());
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
    memcpy(AccessBuffer(OffsetTargetHardwareAddr()), addr.GetRawHostAddress(), addr.GetLength());
    SetLength(GetLength() + addr.GetLength());
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
    memcpy(AccessBuffer(OffsetTargetProtocolAddr()), addr.GetRawHostAddress(), addr.GetLength());
    SetLength(GetLength() + addr.GetLength());
    return true;
}  // end ProtoPktARP::SetTargetProtocolAddress()


/////////////////////////////////////////////////
// ProtoArpTable implementation

ProtoArpTable::MacItem::MacItem(const ProtoAddress& macAddr)
  : mac_addr(macAddr)
{
}

ProtoArpTable::MacItem::~MacItem()
{
    ip_addr_list.Destroy();
}
                        

ProtoArpTable::ProtoArpTable()
{
}

ProtoArpTable::~ProtoArpTable()
{
    mac_list.Destroy();
    ip_list.Destroy();
}

bool ProtoArpTable::AddEntry(const ProtoAddress& ipAddr, const ProtoAddress& macAddr)
{
    // One MAC addr entry per IP address (do we already know this IP address)
    IPItem* ipItem = ip_list.FindItem(ipAddr);
    if (NULL == ipItem)
    {
        // It's a new IP address, is it for an existing MAC addr?
        MacItem* macItem = mac_list.FindItem(macAddr);
        if (NULL == macItem)
        {
            if (NULL == (macItem = new MacItem(macAddr)))
            {
                PLOG(PL_ERROR, "ProtoArpTable::AddEntry() new MacItem error: %s\n", GetErrorString());
                return false;
            }
            if (!mac_list.Insert(*macItem))
            {
                PLOG(PL_ERROR, "ProtoArpTable::AddEntry() error: unable to insert MacItem\n");
                delete macItem;
                return false;
            }   
        }
        // Create new IPItem
        if (NULL == (ipItem = new IPItem(ipAddr, macItem)))
        {
            PLOG(PL_ERROR, "ProtoArpTable::AddEntry() new IPItem error: %s\n", GetErrorString());
        }
        else if (!ip_list.Insert(*ipItem))  // attempt insertion
        {
            PLOG(PL_ERROR, "ProtoArpTable::AddEntry() unab IPItem error: %s\n", GetErrorString());
            delete ipItem;
            ipItem = NULL;
        }
        else if (!macItem->AddAddress(ipAddr))  // add IP address to macItem
        {
            PLOG(PL_ERROR, "ProtoArpTable::AddEntry() error: unable to add IP address\n");
            ip_list.Remove(*ipItem);
            delete ipItem;
            ipItem = NULL;
        }
        if (NULL == ipItem)
        {
            // something went wrong
            macItem->RemoveAddress(ipAddr);
            if (macItem->AccessAddressList().IsEmpty())
            {
                mac_list.Remove(*macItem);
                delete macItem;
            }
            return false;
        } 
    }
    else
    {
        // It's an existing IPItem, update stuff if different MAC addr
        if (!macAddr.HostIsEqual(ipItem->GetMacAddr()))
        {
            // Remove the record for this IP address and re-add with new MAC
            DeleteIPItem(ipItem);
            return  AddEntry(ipAddr, macAddr);
        }
    }
    return true;
}  // end ProtoArpTable::AddEntry()

void ProtoArpTable::DeleteIPItem(IPItem* ipItem)
{
    if (NULL != ipItem)
    {
        ip_list.Remove(*ipItem);
        MacItem* macItem = ipItem->GetMacItem();
        macItem->RemoveAddress(ipItem->GetAddress());
        if (macItem->AccessAddressList().IsEmpty())
            delete macItem;
        delete ipItem;
    }
}  // end ProtoArpTable:RemoveIPItem()

void ProtoArpTable::DeleteMacItem(MacItem* macItem)
{
    if (NULL != macItem)
    {
        ProtoAddressList::Iterator iterator(macItem->AccessAddressList());
        ProtoAddress ipAddr;
        while (iterator.GetNextAddress(ipAddr))
        {
            IPItem* ipItem = ip_list.FindItem(ipAddr);
            ASSERT(NULL != ipItem);
            ip_list.Remove(*ipItem);
            delete ipItem;
        }
        delete macItem;
    }
}  // end ProtoArpTable::DeleteMacItem()

void ProtoArpTable::RemoveEntryByIP(const ProtoAddress& ipAddr)
{
    IPItem* ipItem = ip_list.FindItem(ipAddr);
    if (NULL != ipItem) DeleteIPItem(ipItem);
}  // end ProtoArpTable::RemoveEntryByIP()


void ProtoArpTable::RemoveEntryByMAC(const ProtoAddress& macAddr)
{
    MacItem* macItem = mac_list.FindItem(macAddr);
    if (NULL != macItem) DeleteMacItem(macItem);
}  // end ProtoArpTable::RemoveEntryByMAC()

bool ProtoArpTable::GetMacAddress(const ProtoAddress& ipAddr, ProtoAddress& macAddr)
{
    IPItem* ipItem = ip_list.FindItem(ipAddr);
    if (NULL == ipItem) 
    {
        macAddr.Invalidate();
        return false;
    }
    else
    {
        macAddr = ipItem->GetMacAddr();
        return true;
    }
}  // end ProtoArpTable::GetMacAddress()

bool ProtoArpTable::GetAddressList(const ProtoAddress& macAddr, ProtoAddressList addrList)
{
    MacItem* macItem = mac_list.FindItem(macAddr);
    if (NULL == macItem)
    {
        PLOG(PL_WARN, "ProtoArpTable::GetAddressList() warning: unknown MAC address\n");
        return false;
    }
    ProtoAddressList::Iterator iterator(macItem->AccessAddressList());
    ProtoAddress ipAddr;
    while (iterator.GetNextAddress(ipAddr))
    {
        if (!addrList.Insert(ipAddr))
        {
            PLOG(PL_ERROR, "ProtoArpTable::GetAddressList() error: %s\n", GetErrorString());
            return false;
        }
    }
    return true;
}  // end ProtoArpTable::GetIPAddresses()


