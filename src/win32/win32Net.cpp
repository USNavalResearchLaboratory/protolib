#include "protoNet.h"
#include "protoList.h"
#include "protoDebug.h"
#include "protoDispatcher.h"

#include <winsock2.h>
#include <WS2tcpip.h>  // for extra socket options
/**
* @file win32Net.cpp
* 
* @brief Win32 (Windows) implementation of ProtoNet 
*/

#include <Iphlpapi.h>
#include <Iptypes.h>
/**
 *
 * Two different approaches are given here:
 * 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
 *    using the "GetAdaptersAddresses()" call, and
 * 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
 */

bool ProtoNet::GetInterfaceAddressList(const char*           interfaceName,
                                       ProtoAddress::Type    addressType,
                                       ProtoAddressList&     addrList,
                                       unsigned int*         interfaceIndex)  // optional to fill in (saves lines of code)
{
    ProtoAddressList localAddrList; // used to cache link local addrs
    if (!strcmp(interfaceName, "lo"))  // loopback interface
    {
        // (TBD) should we also test for interfaceName == loopback address string?
        ProtoAddress loopbackAddr;
        if ((ProtoAddress::IPv4 == addressType) || (ProtoAddress::INVALID == addressType))
        {
            loopbackAddr.ResolveFromString("127.0.0.1");
        }
#ifdef HAVE_IPV6
        else if (ProtoAddress::IPv6 == addressType)
        {
            loopbackAddr.ResolveFromString("::1");
        }
#endif // HAVE_IPV6
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unsupported addressType\n");
            return false;
        }
        if (NULL != interfaceIndex) *interfaceIndex = 1; /// (TBD) what should we really set for interfaceIndex ???
        if (addrList.Insert(loopbackAddr))
        {
            return true;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add loopback addr to list\n");
            return false;
        }
    }
    // Two different approaches are given here:
    // 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
    //    using the "GetAdaptersAddresses()" call, and
    // 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
    // Then, try the "GetAdaptersAddresses()" approach first
    bool foundAddr = false;
    ULONG afFamily = AF_UNSPEC;
    switch (addressType)
    {
        case ProtoAddress::IPv4:
            afFamily = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            afFamily = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            break;
    }
    ULONG bufferLength = 0;

#if (WINVER >= 0x0501)
    // On NT4, Win2K and earlier, GetAdaptersAddresses() isn't to be found
    // in the iphlpapi.dll ...
    DWORD addrFlags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersAddresses(afFamily, addrFlags, NULL, NULL, &bufferSize);
    if ((ERROR_BUFFER_OVERFLOW == result) ||
        (ERROR_NO_DATA == result))
    {
        if (ERROR_NO_DATA == result)
        {   
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList(%s) error: no matching interface adapters found.\n", interfaceName);
            return false;
        }
        char* addrBuffer = new char[bufferSize];
        if (NULL == addrBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() new addrBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
        if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
            delete[] addrBuffer;
            return false;
        }
        while (addrEntry)
        {
            if (0 == strncmp(interfaceName, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN))
            {
                // A match was found!
                if (ProtoAddress::ETH == addressType)
                {
                    if (6 == addrEntry->PhysicalAddressLength)
                    {
                        ProtoAddress ethAddr;
                        ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)&addrEntry->PhysicalAddress, 6);
                        if (NULL != interfaceIndex)
                        {
                            if (0 != addrEntry->IfIndex)
                                *interfaceIndex = addrEntry->IfIndex;
                            else
                                *interfaceIndex = addrEntry->Ipv6IfIndex;
                        }
                        delete[] addrBuffer;
                        if (addrList.Insert(ethAddr))
                        {
                            return true;
                        }
                        else
                        {
                            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list\n");
                            return false;
                        }
                    }
                }
                else
                {
                    IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
                    while(NULL != ipAddr)
                    {
                        if ((afFamily == AF_UNSPEC) ||
                            (afFamily == ipAddr->Address.lpSockaddr->sa_family))
                        {
                            if (NULL != interfaceIndex)
                            {
                                if (0 != addrEntry->IfIndex)
                                    *interfaceIndex = addrEntry->IfIndex;
                                else
                                    *interfaceIndex = addrEntry->Ipv6IfIndex;
                            }
                            ProtoAddress ifAddr;
                            ifAddr.SetSockAddr(*(ipAddr->Address.lpSockaddr));
                            // Defer link local address to last
                            if (ifAddr.IsLinkLocal())
                            {
                                if (localAddrList.Insert(ifAddr))
                                    foundAddr = true;
                                else
                                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add addr to local list\n");
                            }
                            else
                            {
                                if (addrList.Insert(ifAddr))
                                {
                                    foundAddr = true;
                                }  
                                else
                                {
                                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add addr to list\n");
                                    delete[] addrBuffer;
                                    return false;
                                }
                            }
                        }
                        ipAddr = ipAddr->Next;
                    }
                }
            }
            addrEntry = addrEntry->Next;
        }
        delete[] addrBuffer;
        if (!foundAddr)
            PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) warning: no matching interface found\n", interfaceName);
    }
    else 
#endif // if (WINVER >= 0x0501)
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList(%s) new infoBuffer error: %s\n", interfaceName, ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList(%s) new infoBuffer error: %s\n", interfaceName, ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            if (0 == strncmp(interfaceName, adapterInfo->AdapterName, MAX_ADAPTER_NAME_LENGTH+4))
            {
                if (interfaceIndex) *interfaceIndex = adapterInfo->Index;
                if (ProtoAddress::ETH == addressType)
                {
                    if (6 == adapterInfo->AddressLength)
                    {
                        ProtoAddress ethAddr;
                        ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)adapterInfo->Address, 6);
                        if (addrList.Insert(ethAddr))
                        {
                            delete[] infoBuffer;
                            return true;
                        }
                        else
                        {
                            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list\n");
                            delete[] infoBuffer;
                            return false;
                        }
                    }
                    else
                    {
                        PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) error: non-Ethernet interface\n", interfaceName);
                        delete[] infoBuffer;
                        return false;
                    }
                }
                else if (ProtoAddress::IPv6 == addressType)
                {
                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList(%s) error: IPv6 not supported\n", interfaceName);
                    delete[] infoBuffer;
                    return false;
                }
                else // ProtoAddress::IPv4 == addressType
                {
                    ProtoAddress ifAddr;
                    if (ifAddr.ResolveFromString(adapterInfo->IpAddressList.IpAddress.String))
                    {
                        // (TBD) Do we need to check for loopback or link local here???
                        if (addrList.Insert(ifAddr))
                        {
                            foundAddr = true;
                        }
                        else
                        {
                            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add  addr to list\n"); 
                            delete[] infoBuffer;
                            return false;
                        }
                    }
                }
            }
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
    }
    else if (ProtoAddress::ETH == addressType)
    {
        // Since "GetAdaptersInfo() didn't work (probably NT4), try this as a backup
        DWORD ifCount;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        for (DWORD i = 1; i <= ifCount; i++)
        {
            MIB_IFROW ifRow;
            ifRow.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifRow))
            {
                PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            // We use "bDescr" field because "wszName" doesn't seem to work (on non-Unicode)?
#ifdef _UNICODE
            // First, we need to convert the "interfaceName" to wchar_t
            wchar_t wideIfName[MAX_INTERFACE_NAME_LEN];
            mbstowcs(wideIfName, interfaceName, MAX_INTERFACE_NAME_LEN);
            if (0 == wcsncmp(ifRow.wszName, wideIfName, MAX_INTERFACE_NAME_LEN))
#else
            if (0 == strncmp((char*)ifRow.bDescr, interfaceName, ifRow.dwDescrLen))
#endif // if/else _UNICODE
            {
                if (6 == ifRow.dwPhysAddrLen)
                {
                    ProtoAddress ethAddr;
                    ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)ifRow.bPhysAddr, 6);
                    if (NULL != interfaceIndex) *interfaceIndex = i;
                    if (addrList.Insert(ethAddr))
                    {
                        return true;
                    }
                    else
                    {
                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list\n"); 
                        return false;
                    }
                }
            }
        }
        PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) GetIfEntry() warning: no matching ETH interface found\n", 
                interfaceName);
    }
    else if ((ProtoAddress::IPv4 == addressType) || (ProtoAddress::INVALID == addressType))
    {
        // Since GetAdaptersAddresses() failed, try the other approach iff IPv4 == addressType
        DWORD ifCount;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        // Second, iterate through addresses looking for a name match
        bool foundMatch = false;
        MIB_IFROW ifEntry;    
        for (DWORD i = 1; i <= ifCount; i++)
        {
            ifEntry.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifEntry))
            {   
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
            wchar_t wideIfName[MAX_INTERFACE_NAME_LEN];
            mbstowcs(wideIfName, interfaceName, MAX_INTERFACE_NAME_LEN);
            if (0 == wcsncmp(wideIfName, ifEntry.wszName, MAX_INTERFACE_NAME_LEN))
#else
            if (0 == strncmp(interfaceName, (char*)ifEntry.bDescr, ifEntry.dwDescrLen))
#endif // if/else _UNICODE
            {
                foundMatch = true;
                break;
            }
        }
        if (foundMatch)
        {
            DWORD ifIndex = ifEntry.dwIndex;
            ULONG bufferSize = 0;
            if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
            {
                char* tableBuffer = new char[bufferSize];
                if (NULL == tableBuffer)
                {   
                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() new tableBuffer error: %s\n", ::GetErrorString());
                    return false;
                }
                MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
                if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
                {
                    for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
                    {
                        MIB_IPADDRROW* entry = &(addrTable->table[i]);
                        if (ifIndex == entry->dwIndex)
                        {
                            ProtoAddress ifAddr;
                            ifAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr, 4);
                            if (NULL != interfaceIndex) *interfaceIndex = ifIndex;
                            foundAddr = true;
                            if (!addrList.Insert(ifAddr))
                            {
                                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add addr to list\n"); 
                                delete[] tableBuffer;
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() warning GetIpAddrTable() error: %s\n", ::GetErrorString());
                }
                delete[] tableBuffer;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() warning GetIpAddrTable() error 1: %s\n", ::GetErrorString());
            }
        }
        else
        {
            PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) warning: no matching IPv4 interface found\n",
                    interfaceName);
        }  // end if/else (foundMatch)
    }
    else
    {
        PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList() warning: GetAdaptersAddresses() error: %s\n", ::GetErrorString());
    }
    // Add any link local addrs found to addrList
    ProtoAddressList::Iterator iterator(localAddrList);
    ProtoAddress localAddr;
    while (iterator.GetNextAddress(localAddr))
    {
        if (addrList.Insert(localAddr))
        {
            foundAddr = true;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() warning: unable to add local addr to list\n"); 
            break;
        }   
    }
    if (foundAddr) return true;
    // As a last resort, check if the "interfaceName" is actually an address string
    ProtoAddress ifAddr;
    if (ifAddr.ResolveFromString(interfaceName))
    {
        char ifName[256];
        if (!GetInterfaceName(ifAddr, ifName, 256))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList(%s) error: no matching interface name found\n", interfaceName);
            return false;
        }
        bool result = GetInterfaceAddressList(ifName, addressType, addrList, interfaceIndex);
        return result;
    }
    else
    {
        char* typeString = NULL;
        switch (addressType)
        {   
            case ProtoAddress::IPv4:
                typeString = "IPv4";
                break;
#ifdef HAVE_IPV6
            case ProtoAddress::IPv6:
                typeString = "IPv6";
                break;
#endif // HAVE_IPV6
            case ProtoAddress::ETH:
                typeString = "Ethernet";
                break;
            default:
                typeString = "UNSPECIFIED";
                break;
        }
        PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) error: no matching %s interface found\n", interfaceName, typeString);
        return false;
    }
}  // end ProtoNet::GetInterfaceAddressList()

unsigned int ProtoNet::GetInterfaceIndices(unsigned int* indexArray, unsigned int indexArraySize)
{
    unsigned int indexCount = 0;
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndices() new infoBuffer error: %s\n", ::GetErrorString());
            return 0;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndices() GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return 0;
        }
        while (NULL != adapterInfo)
        {
            if ((NULL != indexArray) && (indexCount < indexArraySize))
                indexArray[indexCount] = adapterInfo->Index;
            indexCount++;
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
        return indexCount;
    }
    else 
    {
        // TBD - I think this is broken.  I think we need to use GetIfTable() instead!
        // Note our use of GetIfEntry() is broken everywhere.  Fix this!!!
        DWORD ifCount = 0;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndices() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        // Second, iterate through addresses looking for a name match
        MIB_IFROW ifEntry;    
        for (DWORD i = 1; i <= ifCount; i++)
        {
            ifEntry.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifEntry))
            {   
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndices() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            if ((NULL != indexArray) && (indexCount < indexArraySize))
                indexArray[indexCount] = i;
            indexCount++;
        }
        return indexCount;
    }
}  // end ProtoNet::GetInterfaceIndices()

bool ProtoNet::FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress)
{
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() new infoBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            char ifName[MAX_ADAPTER_NAME_LENGTH + 4];
            if (GetInterfaceName(adapterInfo->Index, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
            {
                if (GetInterfaceAddress(ifName, addrType, theAddress))
                {
                    if (!theAddress.IsLoopback())
                    {
                        delete[] infoBuffer;
                        return true;
                    }
                }   
            }
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
        PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() no IPv%d address assigned\n",
                (addrType == ProtoAddress::IPv6) ? 6 : 4);
        return false;
    }
    else if (ProtoAddress::IPv4 == addrType)
    {
        DWORD ifCount = 0;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        // Second, iterate through addresses looking for a name match
        MIB_IFROW ifEntry;    
        for (DWORD i = 1; i <= ifCount; i++)
        {
            ifEntry.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifEntry))
            {   
                PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            char ifName[MAX_INTERFACE_NAME_LEN];
            if (GetInterfaceName(i, ifName, MAX_INTERFACE_NAME_LEN))
            {
                if (GetInterfaceAddress(ifName, addrType, theAddress))
                {
                    if (!theAddress.IsLoopback())
                    {
                        return true;
                    }
                }   
            }
        }
        // (TBD) should we set loopback addr if nothing else?
        PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() no IPv4 address assigned\n");
        return false;
    }
    else
    {
        // (TBD) should we set loopback addr if nothing else?
        PLOG(PL_ERROR, "ProtoNet::FindLocalAddress() IPv6 not supported for this \n");
        return false;    
    }    
}  // end ProtoNet::FindLocalAddress()

unsigned int ProtoNet::GetInterfaceIndex(const char* interfaceName)
{
    ProtoAddress theAddress;
    unsigned int theIndex;
    if (GetInterfaceAddress(interfaceName, theAddress.GetType(), theAddress, &theIndex))
    {
        return theIndex;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndex(%s) error: no matching interface found.\n", interfaceName);
        return 0;
    }
}  // end ProtoNet::GetInterfaceIndex()

bool ProtoNet::GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen)
{
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))  
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) new infoBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            if (index == adapterInfo->Index)
            {
                strncpy_s(buffer, buflen, adapterInfo->AdapterName, MAX_ADAPTER_NAME_LENGTH);
                delete[] infoBuffer;
                return true;
            }
            adapterInfo = adapterInfo->Next;
        }
        // Assume index==1 is loopback?
        if (1 == index)
        {
            strncpy_s(buffer, buflen, "lo", 3);
            return true;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) no matching interface found!\n");
            return false;
        }
    }
    else if (0 != index)
    {
        // This should work on any Win32 that doesn't support GetAdaptersAddresses() ?? 
        MIB_IFROW ifRow;
        ifRow.dwIndex = index;
        if (NO_ERROR == GetIfEntry(&ifRow))
        {
            ProtoAddress temp;
            temp.SetRawHostAddress(ProtoAddress::ETH, (char*)ifRow.bPhysAddr, 6);
            // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
            buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
            wcstombs(buffer, ifRow.wszName, buflen);
#else
            strncpy_s(buffer, buflen, (char*)ifRow.bDescr, ifRow.dwDescrLen);
#endif // if/else _UNICODE
            return true;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) GetIfEntry(%d) error: %s\n", index, ::GetErrorString());
            return false;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%d) error: invalid index\n", index);
        return false;
    }
}  // end ProtoNet::GetInterfaceName(by index)

/**
 * Two different approaches are given here:
 * 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
 *    using the "GetAdaptersAddresses()" call, and
 * 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
 */
bool ProtoNet::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{
    /* (TBD) Do the approaches below provide the loopback address?
    if (ifAddr.IsLoopback())
    {
        strncpy_s(buffer, buflen, "lo", 3);
        return true;
    }*/
   
    
    // Try the "GetAdaptersAddresses()" approach first
    ULONG afFamily = AF_UNSPEC;
    switch (ifAddr.GetType())
    {
        case ProtoAddress::IPv4:
            afFamily = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            afFamily = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            break;
    }
    ULONG bufferLength = 0;
#if (WINVER >= 0x0501)
    // On NT4 and earlier, GetAdaptersAddresses() isn't to be found
    // in the iphlpapi.dll ...
    DWORD addrFlags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersAddresses(afFamily, addrFlags, NULL, NULL, &bufferSize);
    if ((ERROR_BUFFER_OVERFLOW == result) ||
        (ERROR_NO_DATA == result))
    {
        if (ERROR_NO_DATA == result)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%s) error: no matching network adapters found.\n", ifAddr.GetHostString());
            return false;
        }
        char* addrBuffer = new char[bufferSize];
        if (NULL == addrBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() new addrBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
        if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
            delete[] addrBuffer;
            return false;
        }
        while (NULL != addrEntry)
        {
            if (ProtoAddress::ETH == ifAddr.GetType())
            {
                if (6 == addrEntry->PhysicalAddressLength)
                {
                    ProtoAddress tempAddress;
                    tempAddress.SetRawHostAddress(ProtoAddress::ETH, (char*)&addrEntry->PhysicalAddress, 6);
                    if (tempAddress.HostIsEqual(ifAddr))
                    {
                        // Copy the interface name
                        strncpy_s(buffer, buflen, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
                        delete[] addrBuffer;
                        return true;
                    }
                }
            }
            else
            {
                IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
                while(NULL != ipAddr)
                {
                    if (afFamily == ipAddr->Address.lpSockaddr->sa_family)
                    {
                        ProtoAddress tempAddress;
                        tempAddress.SetSockAddr(*(ipAddr->Address.lpSockaddr));
                        if (tempAddress.HostIsEqual(ifAddr))
                        {
                            strncpy_s(buffer, buflen, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
                            delete[] addrBuffer;
                            return true;
                        }
                    }
                    ipAddr = ipAddr->Next;
                }
                if (NULL != ipAddr) break;
            }
            addrEntry = addrEntry->Next;
        }  // end while(addrEntry)
        delete[] addrBuffer;
        PLOG(PL_WARN, "ProtoNet::GetInterfaceName(%s) warning: no matching interface found\n", ifAddr.GetHostString());
    }
    else 
#endif // if (WINVER >= 0x0501)
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%s) new infoBuffer error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%s) GetAdaptersInfo() error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            ProtoAddress tempAddr;
            tempAddr.Invalidate();
            if (ProtoAddress::ETH == ifAddr.GetType())
            {
                if (6 == adapterInfo->AddressLength)
                    tempAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)adapterInfo->Address, 6);
            }
            else if (ProtoAddress::IPv4 == ifAddr.GetType())
            {
                tempAddr.ResolveFromString(adapterInfo->IpAddressList.IpAddress.String);
            }
            if (tempAddr.IsValid() && tempAddr.HostIsEqual(ifAddr))
            {
                strncpy_s(buffer, buflen, adapterInfo->AdapterName, MAX_ADAPTER_NAME_LENGTH);
                delete[] infoBuffer;
                return true;
            }
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%s) error: no matching interface found\n", ifAddr.GetHostString());
    }
    else if (ProtoAddress::ETH == ifAddr.GetType())
    {
        DWORD ifCount;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        for (DWORD i = 1; i <= ifCount; i++)
        {
            MIB_IFROW ifRow;
            ifRow.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifRow))
            {
                PLOG(PL_WARN, "ProtoNet::GetInterfaceName() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            if (6 == ifRow.dwPhysAddrLen)
            {
                ProtoAddress tempAddress;
                tempAddress.SetRawHostAddress(ProtoAddress::ETH, (char*)ifRow.bPhysAddr, 6);
                if (tempAddress.HostIsEqual(ifAddr))
                {
                    // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
                    buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                    wcstombs(buffer, ifRow.wszName, buflen);
#else
                    strncpy_s(buffer, buflen, (char*)ifRow.bDescr, ifRow.dwDescrLen);
#endif // if/else _UNICODE
                    return true;
                }
            }
        }
        PLOG(PL_WARN, "ProtoNet::GetInterfaceName(%s) GetIfEntry() error: no matching Ethernet interface found\n", 
                ifAddr.GetHostString());
    }
    else if (ProtoAddress::IPv4 == ifAddr.GetType())
    {
        PLOG(PL_WARN, "ProtoNet::GetInterfaceName() warning GetAdaptersAddresses() error: %s\n", ::GetErrorString());
        // Since GetAdaptersAddresses() failed, try the other approach iff IPv4 == addressType
        // Iterate through addresses looking for an address match
        ULONG bufferSize = 0;
        if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
        {
            char* tableBuffer = new char[bufferSize];
            if (NULL == tableBuffer)
            {   
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() new tableBuffer error: %s\n", ::GetErrorString());
                return false;
            }
            MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
            if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
            {
                for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
                {
                    MIB_IPADDRROW* entry = &(addrTable->table[i]);
                    ProtoAddress tempAddress;
                    tempAddress.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr, 4);
                    if (tempAddress.HostIsEqual(ifAddr))
                    {
                        MIB_IFROW ifEntry;  
                        ifEntry.dwIndex = entry->dwIndex;
                        if (NO_ERROR != GetIfEntry(&ifEntry))
                        {   
                            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                            return false;
                        }
                        // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
                        buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                        wcstombs(buffer, ifEntry.wszName, buflen);
#else
                        strncpy_s(buffer, buflen, (char*)ifEntry.bDescr, ifEntry.dwDescrLen);
#endif // if/else _UNICODE
                        delete[] tableBuffer;
                        return true;
                    }
                }
            }
            else
            {
                PLOG(PL_WARN, "ProtoNet::GetInterfaceName(%s) warning GetIpAddrTable() error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            }
            delete[] tableBuffer;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%s) warning GetIpAddrTable() error 2: %s\n", ifAddr.GetHostString(), ::GetErrorString());            
        }
        PLOG(PL_WARN, "ProtoNet::GetInterfaceName(%s) warning: no matching IPv4 interface found\n",
                ifAddr.GetHostString());
    }
    else
    {
        PLOG(PL_WARN, "ProtoNet::GetInterfaceName() warning GetAdaptersAddresses() error: %s\n", ::GetErrorString());
    }
    return false;
}  // end ProtoNet::GetInterfaceName(by addr)

class Win32NetMonitor : public ProtoNet::Monitor
{
public:
	Win32NetMonitor();
	~Win32NetMonitor();

	bool Open();
	void Close();
	bool GetNextEvent(Event& theEvent);
	bool GetEvent(PMIB_IPINTERFACE_ROW row,
		MIB_NOTIFICATION_TYPE notificationType);
	static bool FindIPAddr(NET_IFINDEX InterfaceIndex);
	const char* GetNotificationType(int type);
	HANDLE GetEventHandle() { return input_handle; }

private:
	// We cache mib changes to a linked list for
	// retrieval by the GetNextEvent() method
	class EventItem : public Event, public ProtoList::Item
	{
	public:
		EventItem();
		~EventItem();
	};  // end class Win32NetMontior::EventItem

	class EventList : public ProtoListTemplate<EventItem> {};
	EventList	event_list;
	EventList	event_pool;

	typedef CRITICAL_SECTION    Mutex;
	Mutex                       lock;
    
	static void Init(Mutex& m) {InitializeCriticalSection(&m);}
    static void Destroy(Mutex& m) {DeleteCriticalSection(&m);}
    static void Lock(Mutex& m) {EnterCriticalSection(&m);}
	static void Unlock(Mutex& m) {LeaveCriticalSection(&m);}

	HANDLE notification_handle;  // handle to subsequently stop notifications

}; // end class Win32NetMonitor

/// This is the implementation of the ProtoNet::Monitor::Create()
/// static method (our win32-specific factory)
ProtoNet::Monitor* ProtoNet::Monitor::Create()
{
	return static_cast<ProtoNet::Monitor*>(new Win32NetMonitor);
} // end ProtoNet::Monitor::Create()

Win32NetMonitor::Win32NetMonitor()
{
    Init(lock);
}

Win32NetMonitor::~Win32NetMonitor()
{
}

// Static callback function for NotifyIpInterfaceChange API
static void WINAPI IpInterfaceChangeCallback(PVOID callerContext,
	PMIB_IPINTERFACE_ROW row,
	MIB_NOTIFICATION_TYPE notificationType)
{
	Win32NetMonitor* monitor = (Win32NetMonitor*)callerContext;
	if (!monitor) 
	{
		PLOG(PL_ERROR,"IpInterfaceChangeCallback() Error: No callerContext.\n");
		return;
	}

	if (row)
	{
		// Get complete information for MIP_IPINTERFACE_ROW
		GetIpInterfaceEntry(row);
		// Add an event to our list for the notification
		if (!monitor->GetEvent(row,notificationType))
		{	
			PLOG(PL_ERROR,"MonitorEventHandler() GetEvent error\n");
			return;
		}

    }
	if (!SetEvent(monitor->GetEventHandle()))
		PLOG(PL_ERROR,"win32Net::MonitorEventHandler() Error setting event handle.\n");

}
bool Win32NetMonitor::Open()
{
 	// Not using a manual reset event?
	if (NULL == (input_handle = CreateEvent(NULL,FALSE,FALSE,NULL)))
    {
        input_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR,"Win32Monitor::Open() CreateEvent(event_handle) error: %s\n", ::GetErrorString());
        Close(); 
        return false;
    }
	// Initiate notifications ...
	notification_handle = NULL;
	if (!NotifyIpInterfaceChange(
				AF_UNSPEC,  // AF_INET
				(PIPINTERFACE_CHANGE_CALLBACK)IpInterfaceChangeCallback,
				this,
				false, // initialNofification 
				&notification_handle) == NO_ERROR)
	{
		PLOG(PL_ERROR,"Win32NetMonitor::Open() NotifyIpInterfaceChange failed\n");
		return false;
	}

	if (!ProtoNet::Monitor::Open())
	{
		Close();
		return false;
	}
	
	return true;
}

void Win32NetMonitor::Close()
{
	if (IsOpen())
	{
		ProtoNet::Monitor::Close();	
		input_handle = INVALID_HANDLE;
	}
	if (notification_handle != INVALID_HANDLE)
		CancelMibChangeNotify2(notification_handle);

	event_list.Destroy();
	event_pool.Destroy();

	Unlock(lock);
	Destroy(lock);
}

const char* Win32NetMonitor::GetNotificationType(int type)
    {
        static const char* names[] = {
        "ParameterNotification",
        "AddInstance",
        "DeleteInstance",
        "InitialNotification" 
        };

        const char* name = "";
        if (type >=0 && type < sizeof(names)) {
            name = names[type];
        }
        return name;
    }
bool Win32NetMonitor::GetNextEvent(Event& theEvent)
{
	// 0) Initialize event instance
	theEvent.SetType(Event::UNKNOWN_EVENT);
	theEvent.SetInterfaceIndex(0);
	theEvent.AccessAddress().Invalidate();

	// 1) Get next event from list
	Lock(lock);
	EventItem* eventItem = event_list.RemoveHead();
	if (eventItem == NULL)
	{
		Unlock(lock);
		theEvent.SetType(Event::NULL_EVENT);
		return true;
	}
	theEvent = static_cast<Event&>(*eventItem);
	event_pool.Append(*eventItem);
	Unlock(lock);
	return true;
}

bool Win32NetMonitor::GetEvent(PMIB_IPINTERFACE_ROW row,
	MIB_NOTIFICATION_TYPE notificationType)
{
	EventItem* eventItem = event_pool.RemoveHead();
	if (NULL == eventItem) eventItem = new EventItem();
	if (NULL == eventItem)
	{
		PLOG(PL_ERROR,"Win32NetMonitor::GetEvent() new EventItem error: %s\n", GetErrorString());
		return false;
	}

	eventItem->SetInterfaceIndex(row->InterfaceIndex);

	switch (notificationType)
    {
	case 0:
		//eventItem->SetType(Event::IFACE_STATE);
		// not interested in windows state changes at the moment
		eventItem->SetType(Event::UNKNOWN_EVENT);
		break;
	case 1:
		eventItem->SetType(Event::IFACE_UP);
		break;
	case 2:
		eventItem->SetType(Event::IFACE_DOWN);
		break;
	case 3:
		eventItem->SetType(Event::UNKNOWN_EVENT);
		break;
	default:
		eventItem->SetType(Event::UNKNOWN_EVENT);
		PLOG(PL_ERROR,"Win32NetMonitor::GetEvent() warning: unhandled network event: %d\n",notificationType);
		break;
	}


	// Iterate through addresses looking for our interface index
	ULONG bufferSize = 0;
    ULONG index = 0;
    if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
    {
            char* tableBuffer = new char[bufferSize];
			if (NULL == tableBuffer)
	        {   
				PLOG(PL_ERROR, "Win32NetMonitor::GetEvent() new tableBuffer error: %s\n", ::GetErrorString());
				return false;
			}
			MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
		    if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
			{
				 for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
				{

				   MIB_IPADDRROW* entry = &(addrTable->table[i]);
				   if (entry->dwIndex == row->InterfaceIndex)
				   {
					  if (row->Family == AF_INET)
					     eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr,4);
					  else
					     eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv6, (char*)&entry->dwAddr,16);
					  // TBD Ignore link local addr new/delete events?
					  
				   }

			    }

			}
	}
    else
        {
            PLOG(PL_WARN, "Win32NetMonitor::GetEvent(%u) warning GetIpAddrTable() error: %s\n", row->InterfaceIndex, GetErrorString());
        }


	Lock(lock);
	event_list.Append(*eventItem);
	Unlock(lock);
	return true;

} // end Win32NetMonitor::GetNextEvent();

Win32NetMonitor::EventItem::EventItem()
{
}

Win32NetMonitor::EventItem::~EventItem()
{
}
 
