#include "protoList.h"
#include "protoDebug.h"
#include "protoDispatcher.h"
#include "protoNet.h"

#include <winsock2.h>
#include <WS2tcpip.h>  // for extra socket options
// wcstombs
#include <stdlib.h>
/**
* @file win32Net.cpp
* 
* @brief Win32 (Windows) implementation of ProtoNet 
*/
#pragma comment(lib,"IPHlpApi.Lib")
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
			// Were we given a friendly name or the adapter name?
			char friendlyName[MAX_INTERFACE_NAME_LEN];
			//PLOG(PL_INFO,"PWCHAR string: %ls \n", addrEntry->FriendlyName);
			//PLOG(PL_INFO,"adapterName> %s \n", addrEntry->AdapterName);
			//PLOG(PL_INFO,"interfaceName> %s \n\n", interfaceName);
			int ret = wcstombs(friendlyName, addrEntry->FriendlyName, MAX_INTERFACE_NAME_LEN);
			if (ret == MAX_INTERFACE_NAME_LEN) friendlyName[MAX_INTERFACE_NAME_LEN - 1] = '\0';
            if ((0 == strncmp(interfaceName, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN))
				|
				(0 == strncmp(interfaceName, friendlyName, MAX_INTERFACE_NAME_LEN)))
            {
                // A match was found!
                if (ProtoAddress::ETH == addressType)
                {
                    if (6 == addrEntry->PhysicalAddressLength)
                    {
                        ProtoAddress ethAddr;
                        ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)&addrEntry->PhysicalAddress, 6);
						PLOG(PL_INFO, "Win32Net::GetInterfaceAddressList() ethAddr>%s\n", ethAddr.GetHostString());
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
        //if (!foundAddr)
        //    PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) warning: no matching interface found.  Checking for ip address\n", interfaceName);
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
    if (ifAddr.ConvertFromString(interfaceName))
    {
        char ifName[MAX_ADAPTER_NAME_LENGTH + 4];
        if (!GetInterfaceName(ifAddr, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
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


unsigned int ProtoNet::GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen)
{
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))  
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) new infoBuffer error: %s\n", ::GetErrorString());
            return 0;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return 0;
        }
        while (NULL != adapterInfo)
        {
            if (index == adapterInfo->Index)
            {
				size_t nameLen = strnlen_s(adapterInfo->AdapterName, MAX_ADAPTER_NAME_LENGTH);
				strncpy_s(buffer, buflen, adapterInfo->AdapterName, nameLen);
                delete[] infoBuffer;
                return nameLen;
            }
            adapterInfo = adapterInfo->Next;
        }
        // Assume index == 1 is loopback?
        if (1 == index)
        {
            strncpy_s(buffer, buflen, "lo", 3);
            return 2;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) no matching interface found!\n");
            return 0;
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
            unsigned int nameLen = wcstombs(buffer, ifRow.wszName, buflen);
#else
			size_t nameLen = strnlen_s((char*)ifRow.bDescr, ifRow.dwDescrLen);
            strncpy_s(buffer, buflen, (char*)ifRow.bDescr, ifRow.dwDescrLen);
#endif // if/else _UNICODE
            return nameLen;
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(by index) GetIfEntry(%d) error: %s\n", index, ::GetErrorString());
            return 0;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%d) error: invalid index\n", index);
        return 0;
    }
}  // end ProtoNet::GetInterfaceName(by index)

/**
 * Two different approaches are given here:
 * 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
 *    using the "GetAdaptersAddresses()" call, and
 * 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
 */
unsigned int ProtoNet::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
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
            return 0;
        }
        char* addrBuffer = new char[bufferSize];
        if (NULL == addrBuffer)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() new addrBuffer error: %s\n", ::GetErrorString());
            return 0;
        }
        IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
        if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
            delete[] addrBuffer;
            return 0;
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
						size_t nameLen = strnlen_s(addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
                        strncpy_s(buffer, buflen, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
                        delete[] addrBuffer;
                        return nameLen;
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
							size_t nameLen = strnlen_s(addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
                            strncpy_s(buffer, buflen, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
                            delete[] addrBuffer;
                            return nameLen;
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
            return 0;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceName(%s) GetAdaptersInfo() error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            delete[] infoBuffer;
            return 0;
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
				size_t nameLen = strnlen_s(adapterInfo->AdapterName, MAX_INTERFACE_NAME_LEN);
                strncpy_s(buffer, buflen, adapterInfo->AdapterName, MAX_ADAPTER_NAME_LENGTH);
                delete[] infoBuffer;
                return nameLen;
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
            return 0;
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
                    size_t nameLen = wcstombs(buffer, ifRow.wszName, buflen);
#else
					size_t nameLen = strnlen_s((char*)ifRow.bDescr, ifRow.dwDescrLen);
                    strncpy_s(buffer, buflen, (char*)ifRow.bDescr, ifRow.dwDescrLen);
#endif // if/else _UNICODE
                    return nameLen;
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
                return 0;
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
                            return 0;
                        }
                        // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
                        buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                        size_t nameLen = wcstombs(buffer, ifEntry.wszName, buflen);
						unsigned int tmpbuflen = buflen;
#else
						size_t nameLen = strnlen_s((char*)ifEntry.bDescr, ifEntry.dwDescrLen);
                        strncpy_s(buffer, buflen, (char*)ifEntry.bDescr, ifEntry.dwDescrLen);
						unsigned int tmpbuflen = buflen;
#endif // if/else _UNICODE
                        delete[] tableBuffer;
                        return nameLen;
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
    return 0;
}  // end ProtoNet::GetInterfaceName(by addr)

ProtoNet::InterfaceStatus ProtoNet::GetInterfaceStatus(const char* ifaceName)
{
    // On WIN32, if we can't get an interface index, we assume IFACE_DOUWN
    unsigned int ifaceIndex = GetInterfaceIndex(ifaceName);
    if (0 != ifaceIndex)
        return IFACE_UP;
    else
        return IFACE_DOWN;   
}  // end ProtoNet::GetInterfaceStatus(by name)

ProtoNet::InterfaceStatus ProtoNet::GetInterfaceStatus(unsigned int ifaceIndex)
{
    // On WIN32, if we can't get an interface name, we assume IFACE_DOWN
    char ifaceName[MAX_ADAPTER_NAME_LENGTH + 4];
    if (GetInterfaceName(ifaceIndex, ifaceName, MAX_ADAPTER_NAME_LENGTH + 4))
        return IFACE_UP;
    else
        return IFACE_DOWN;
}  // end ProtoNet::GetInterfaceStatus(by index)
unsigned int ProtoNet::GetInterfaceAddressMask(unsigned int ifaceIndex, const ProtoAddress& ifAddr)
{
	char ifName[MAX_ADAPTER_NAME_LENGTH + 4];
	//unsigned int buflen;
	if (GetInterfaceName(ifaceIndex, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
	{
		return GetInterfaceAddressMask(ifName, ifAddr);
	}
	PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask() no matching interface found for ifaceIndex %s", ifaceIndex);
	return 0;
}

unsigned int ProtoNet::GetInterfaceAddressMask(const char* ifName, const ProtoAddress& ifAddr)
{

	// ifName can be a friendly adapater address, an adapter name, or and
	// adapter ip address.  A separate function looks up by index.

	// We could use GetAdaptersInfo and get mask from adaterInfo->IpAddressList.IpMask.String
	// would need to convert dotted decimal mask to cidr

	// TBD: Implement option 2 ljt?

	// 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
	//    using the "GetAdaptersAddresses()" call, and
	// 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
	// Then, try the "GetAdaptersAddresses()" approach first

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
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask(%s) error: no matching interface adapters found.\n", ifAddr.GetHostString());
			return 0;
		}
		
		char* addrBuffer = new char[bufferSize];
		if (NULL == addrBuffer)
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask() new addrBuffer error: %s\n", ::GetErrorString());
			return 0;
		}
		IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
		if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
			delete[] addrBuffer;
			return 0;
		}
		while (addrEntry)
		{
			// Were we given a friendly name or the adapter name?
			char friendlyName[MAX_INTERFACE_NAME_LEN];
			int ret = wcstombs(friendlyName, addrEntry->FriendlyName, MAX_INTERFACE_NAME_LEN);
			if (ret == MAX_INTERFACE_NAME_LEN) friendlyName[MAX_INTERFACE_NAME_LEN - 1] = '\0';

			if ((0 == strncmp(ifName, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN))
				|| (0 == strncmp(ifName, friendlyName, MAX_INTERFACE_NAME_LEN)))
			{
				// A match was found!
				if (ProtoAddress::ETH != ifAddr.GetType())
				{
					IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
					while (NULL != ipAddr)
					{
						if ((afFamily == AF_UNSPEC) ||
							(afFamily == ipAddr->Address.lpSockaddr->sa_family))
						{
							ProtoAddress tmpAddr;
							tmpAddr.SetSockAddr(*(ipAddr->Address.lpSockaddr));
							if (ifAddr == tmpAddr)
							{
								unsigned int mask =  ipAddr->OnLinkPrefixLength;
								delete[] addrBuffer;
								return  mask;
							}
						}
						ipAddr = ipAddr->Next;
					}
				}
			}
			addrEntry = addrEntry->Next;
		}
		delete[] addrBuffer;
		PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressMask(%s) warning: no matching interface found.  Checking for ip address.\n", ifAddr.GetHostString());
	}
	
	// See if "ifName" is actually an address
	ProtoAddress tmpAddr;
	if (tmpAddr.ConvertFromString(ifName))
	{
		char ifName[MAX_ADAPTER_NAME_LENGTH + 4]; 
		if (!GetInterfaceName(tmpAddr, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask(%s) error: no matching interface found\n", ifAddr.GetHostString());
			return false;
		}
		return GetInterfaceAddressMask(ifName, tmpAddr);
	}
	

	return 0;
#endif // if (WINVER >= 0x0501)
	// TBD: Implement support for earlier versions of windows?

	PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList(%s) warning: no matching interface found (WINVER < 0x0501?)\n", ifAddr.GetHostString());

	return 0;
}; // ProtoNet::GetInterfaceAddressMask

bool ProtoNet::AddInterfaceAddress(unsigned int ifaceIndex, const ProtoAddress& addr, unsigned int maskLen, bool dhcp_enabled)
{
	/*
	NOTE: dhcp interface flag is inconsisten when interface had both ipv4 and ipv6 addresses.
	Do not use in this case.
	*/

	//unsigned int buflen;
	char ifName[MAX_ADAPTER_NAME_LENGTH];
	if (GetInterfaceFriendlyName(ifaceIndex, ifName, MAX_ADAPTER_NAME_LENGTH ))
	{
		return AddInterfaceAddress(ifName, addr, maskLen,dhcp_enabled);
	}
	PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() no matching interface found for ifaceIndex %s", ifaceIndex);
	return false;
};

bool ProtoNet::AddInterfaceAddress(const char* ifaceName, const ProtoAddress& ifaceAddr, unsigned int maskLen, bool dhcp_enabled)
{
	/*
	NOTE: dhcp interface flag is inconsisten when interface had both ipv4 and ipv6 addresses.
	Do not use in this case.
	*/
	char cmd[1024];
	switch (ifaceAddr.GetType())
	{
	case ProtoAddress::IPv4:
		if (dhcp_enabled)
			sprintf(cmd, "netsh interface ipv4 set address \"%s\" dhcp", ifaceName);
		else
			sprintf(cmd, "netsh interface ipv4 add address \"%s\" addr=%s", ifaceName, ifaceAddr.GetHostString());

		break;
#ifdef HAVE_IPV6
	case ProtoAddress::IPv6:
		sprintf(cmd, "netsh interface ipv6 set address interface=\"%s\" address=%s", ifaceName, ifaceAddr.GetHostString());
		break;
#endif //HAVE_IPV6
	default:
	{
		PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: invalid address type\n");
		return false;
	}
	}

	PLOG(PL_INFO, "cmd>%s \n", cmd);
	if (LONG ret = system(cmd))
	{
		PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress(%s) add %s failed failed. netsh call returned: %u", ifaceName, ifaceAddr.GetHostString(), ret);
		return false;
	}
	return true;

}   // end ProtoNet::AddInterfaceAddress()

bool ProtoNet::RemoveInterfaceAddress(unsigned int ifaceIndex, const ProtoAddress& addr, unsigned int maskLen)
{
	char ifName[MAX_ADAPTER_NAME_LENGTH];
	//unsigned int buflen;
	if (GetInterfaceName(ifaceIndex, ifName, MAX_ADAPTER_NAME_LENGTH))
	{
		return RemoveInterfaceAddress(ifName, addr, maskLen);
	}
	PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() no matching interface found for ifaceIndex %s", ifaceIndex);
	return false;
};

bool ProtoNet::RemoveInterfaceAddress(const char* ifaceAddress, const ProtoAddress& ifaceAddr, unsigned int maskLen)
{
	// Get the adapter name from the iface address
	ProtoAddress ifAddr;
	char ifName[MAX_ADAPTER_NAME_LENGTH];
	if (ifAddr.ConvertFromString(ifaceAddress))
	{
		if (!GetInterfaceFriendlyName(ifAddr, ifName, MAX_ADAPTER_NAME_LENGTH))
		{
			PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress(%s) error: no matching interface name found\n", ifaceAddress);
			return false;
		}
		PLOG(PL_INFO, "ProtoNet::RemoveInterfaceAddress() ifaceName>%s\n", ifName);
		
	} 
	else
	{
		// Is it already a friendly name?
		sprintf(ifName, "%s", ifaceAddress);
		PLOG(PL_INFO, "ProtoNet::RemoveInterfaceAddress() ifaceName>%s\n", ifName);
	}
	char cmd[1024];
	// First assign a static address to the interface so we can then delete it

	switch (ifaceAddr.GetType())
	{
	case ProtoAddress::IPv4:
		sprintf(cmd, "netsh interface ipv4 set address name=\"%s\" static %s", ifName, ifaceAddr.GetHostString());
		break;
	case ProtoAddress::IPv6:
		// ipv6 seems to behave differently - is this necessary?
		sprintf(cmd, "netsh interface ipv6 set address interface=\"%s\" %s", ifName, ifaceAddr.GetHostString());
		break;
	default:
		PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() error: invalid address type\n");
		return false;
	}
	PLOG(PL_INFO, "cmd>%s \n", cmd);
	if (LONG ret = system(cmd))
	{
		PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress(%s) Change to static ip %s failed. netsh call returned: %u\n", ifName, ifaceAddr.GetHostString(), ret);
		return false;
	}
	switch (ifaceAddr.GetType())
	{
	case ProtoAddress::IPv4:
		sprintf(cmd, "netsh interface ipv4 delete address \"%s\" addr=%s", ifName, ifaceAddr.GetHostString());
		break;
#ifdef HAVE_IPV6
	case ProtoAddress::IPv6:
		sprintf(cmd, "netsh interface ipv6 delete address interface=\"%s\" address=%s", ifName, ifaceAddr.GetHostString());
		break;
#endif //HAVE_IPV6
	default:
	{
		PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() error: invalid address type\n");
		return false;
	}
	}
	PLOG(PL_INFO, "cmd>%s \n", cmd);
	if (LONG ret = system(cmd))
	{
		PLOG(PL_ERROR,"ProtoNet::RemoveInterfaceAddress(%s) Open %s failed. netsh call returned: %u\n", ifName,ifaceAddr.GetHostString(), ret);
		return false;
	}

	return true;
}   // end ProtoNet::RemoveInterfaceAddress()

/**
* Only available on WINVER > 0x0501
*/
unsigned int ProtoNet::GetInterfaceFriendlyName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{
#if (WINVER >= 0x0501)

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
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName(%s) error: no matching network adapters found.\n", ifAddr.GetHostString());
			return 0;
		}
		char* addrBuffer = new char[bufferSize];
		if (NULL == addrBuffer)
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName() new addrBuffer error: %s\n", ::GetErrorString());
			return 0;
		}
		IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
		if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
			delete[] addrBuffer;
			return 0;
		}
		while (NULL != addrEntry)
		{
			// TODO LJT remove this
			if (ProtoAddress::ETH == ifAddr.GetType())
			{
				if (6 == addrEntry->PhysicalAddressLength)
				{
					ProtoAddress tempAddress;
					tempAddress.SetRawHostAddress(ProtoAddress::ETH, (char*)&addrEntry->PhysicalAddress, 6);
					if (tempAddress.HostIsEqual(ifAddr))
					{
						// Copy the interface name
						size_t nameLen = strnlen_s(addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
						strncpy_s(buffer, buflen, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN);
						delete[] addrBuffer;
						return nameLen;
					}
				}
			}
			else
			{
				IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
				while (NULL != ipAddr)
				{
					if (afFamily == ipAddr->Address.lpSockaddr->sa_family)
					{
						ProtoAddress tempAddress;
						tempAddress.SetSockAddr(*(ipAddr->Address.lpSockaddr));
						if (tempAddress.HostIsEqual(ifAddr))
						{
							// Convert the PWCHAR friendly name 
							char friendlyName[MAX_INTERFACE_NAME_LEN];
							PLOG(PL_INFO, "PWCHAR string: %ls \n", addrEntry->FriendlyName);
							int ret = wcstombs(friendlyName, addrEntry->FriendlyName, MAX_INTERFACE_NAME_LEN);
							if (ret == MAX_INTERFACE_NAME_LEN) friendlyName[MAX_INTERFACE_NAME_LEN - 1] = '\0';

							size_t nameLen = strnlen_s(friendlyName, MAX_INTERFACE_NAME_LEN);
							strncpy_s(buffer, buflen, friendlyName, MAX_INTERFACE_NAME_LEN);
							delete[] addrBuffer;
							return nameLen;
						}
					}
					ipAddr = ipAddr->Next;
				}
				if (NULL != ipAddr) break;
			}
			addrEntry = addrEntry->Next;
		}  // end while(addrEntry)
		delete[] addrBuffer;
    }
	PLOG(PL_WARN, "ProtoNet::GetInterfaceFriendlyName(%s) warning: no matching interface found\n", ifAddr.GetHostString());
	return 0;
#else
	PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName() not available on WINVER < 0x0501\n");
	return 0;
#endif // if (WINVER >= 0x0501)

}  // end ProtoNet::GetInterfaceFriendlyName(by addr)
/**
* Only available on WINVER > 0x0501
*/
unsigned int ProtoNet::GetInterfaceFriendlyName(unsigned int index, char* buffer, unsigned int buflen)
{
#if (WINVER >= 0x0501)

	ULONG bufferLength = 0;
	if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
	{
		char * infoBuffer = new char[bufferLength]; 
		if (NULL == infoBuffer)
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName(by index) new infoBuffer error: %s\n", GetErrorString());
			return 0;
		}

		IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer;
		if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName(by index) GetAdaptersInfo() error: %s\n", ::GetErrorString());
			delete[] infoBuffer;
			return 0;
		}
		ProtoAddress ifAddr;
		while (NULL != adapterInfo)
		{
			if (index == adapterInfo->Index)
			{
				ifAddr.ResolveFromString(adapterInfo->IpAddressList.IpAddress.String);
				return GetInterfaceFriendlyName(ifAddr, buffer, MAX_ADAPTER_NAME_LENGTH);
			}
			adapterInfo = adapterInfo->Next;
		}
		// Assume index == 1 is loopback?
		if (1 == index)
		{
			strncpy_s(buffer, buflen, "lo", 3);
			return 2;
		}
		else
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceFirendlyName(by index) no matching interface found!\n");
			return 0;
		}
	}
	else
	{
		PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName(%d) error: invalid index\n", index);
		return 0;
	}

#else
	PLOG(PL_ERROR, "ProtoNet::GetInterfaceFriendlyName(by index) not available on WINVER < 0x0501\n");
	return 0;
#endif // if (WINVER >= 0x0501)

}  // end ProtoNet::GetInterfaceFriendlyName(by index)

bool ProtoNet::GetInterfaceAddressDhcp(unsigned int ifaceIndex, const ProtoAddress& ifAddr)
{
	char ifName[MAX_ADAPTER_NAME_LENGTH + 4];
	//unsigned int buflen;
	if (GetInterfaceName(ifaceIndex, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
	{
		return GetInterfaceAddressDhcp(ifName, ifAddr);
	}
	PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressDhcp() no matching interface found for ifaceIndex %s", ifaceIndex);
	return false;
}

bool ProtoNet::GetInterfaceAddressDhcp(const char* ifName, const ProtoAddress& ifAddr)
{
	// 1) IP_ADAPTER_ADDRESSES only seems to support Dhcpv4Enabled - not sure
	//    how to detect ipv6 ljt
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
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressDhcp(%s) error: no matching interface adapters found.\n", ifAddr.GetHostString());
			return 0;
		}

		char* addrBuffer = new char[bufferSize];
		if (NULL == addrBuffer)
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressDhcp() new addrBuffer error: %s\n", ::GetErrorString());
			return 0;
		}
		IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
		if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressDhcp() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
			delete[] addrBuffer;
			return 0;
		}
		while (addrEntry)
		{
			if (0 == strncmp(ifName, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN))
			{
				// A match was found!
				if (ProtoAddress::ETH != ifAddr.GetType())
				{
					IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
					while (NULL != ipAddr)
					{
						if ((afFamily == AF_UNSPEC) ||
							(afFamily == ipAddr->Address.lpSockaddr->sa_family))
						{
							ProtoAddress tmpAddr;
							tmpAddr.SetSockAddr(*(ipAddr->Address.lpSockaddr));
							if (ifAddr == tmpAddr)
							{
								unsigned int mask = ipAddr->OnLinkPrefixLength;
								delete[] addrBuffer;
								// Apparantly windows doesn't automatically convert 0/1 to bool properly??
								if (addrEntry->Dhcpv4Enabled)
								{
									return true;
								}
								else
								{
									return false;
								}
								return  addrEntry->Dhcpv4Enabled;
							}
						}
						ipAddr = ipAddr->Next;
					}
				}
			}
			addrEntry = addrEntry->Next;
		}
		delete[] addrBuffer;
		PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressDhcp(%s) warning: no matching interface found.  Checking for ip address.\n", ifAddr.GetHostString());
	}

	// See if "ifName" is actually an address
	ProtoAddress tmpAddr;
	if (tmpAddr.ConvertFromString(ifName))
	{
		char ifName[MAX_ADAPTER_NAME_LENGTH + 4];
		if (!GetInterfaceName(tmpAddr, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddresDhcp(%s) error: no matching interface found\n", ifAddr.GetHostString());
			return false;
		}
		return GetInterfaceAddressDhcp(ifName, tmpAddr);
	}


	return false;
#endif // if (WINVER >= 0x0501)
	// TBD: Implement support for earlier versions of windows?

	PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressDhcp(%s) warning: no matching interface found (WINVER < 0x0501?)\n", ifAddr.GetHostString());

	return false;
}; // ProtoNet::GetInterfaceAddressMask


bool ProtoNet::GetInterfaceIpAddress(unsigned int index,ProtoAddress& ifAddr)
{
#if (WINVER >= 0x0501)  // TBD Should work on older?

	ULONG bufferLength = 0;
	if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
	{
		char * infoBuffer = new char[bufferLength];
		if (NULL == infoBuffer)
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceIpAddress(by index) new infoBuffer error: %s\n", GetErrorString());
			return false;
		}

		IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer;
		if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
		{
			PLOG(PL_ERROR, "ProtoNet::GetInterfaceIpAddress(by index) GetAdaptersInfo() error: %s\n", GetErrorString());
			delete[] infoBuffer;
			return false;
		}
		while (NULL != adapterInfo)
		{
			if (index == adapterInfo->Index)
			{
				ifAddr.ResolveFromString(adapterInfo->IpAddressList.IpAddress.String);
				return true;
			}
			adapterInfo = adapterInfo->Next;
		}
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceIpAddress(%d) error: invalid index\n", index);
		return false;
	}
	else
	{
		PLOG(PL_ERROR, "ProtoNet::GetInterfaceIpAddress(by index) GetAdaptersInfo() error: %s\n", GetErrorString());
		return false;
	}

#else
	PLOG(PL_ERROR, "ProtoNet::GetInterfaceIpAddress(by index) not available on WINVER < 0x0501\n");
	return false;
#endif // if (WINVER >= 0x0501)

}  // end ProtoNet::GetInterfaceIpAddress(by index)
