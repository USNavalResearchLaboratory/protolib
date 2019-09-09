#include "protoNet.h"
#include "protoDebug.h"

unsigned int ProtoNet::GetInterfaceCount() 
{
    return GetInterfaceIndices(NULL, 0);
}  // end ProtoNet::GetInterfaceCount()

unsigned int ProtoNet::GetInterfaceIndex(const ProtoAddress& ifAddr)
{
    
    char ifName[256];
    ifName[255] = '\0';
    if (GetInterfaceName(ifAddr, ifName, 255))
    {
        return GetInterfaceIndex(ifName);
    }
    else
    {
        return 0;
    }
}  // end ProtoNet::GetInterfaceIndex(by address)

bool ProtoNet::GetInterfaceAddressList(unsigned int         ifIndex,
				                       ProtoAddress::Type   addrType,
				                       ProtoAddressList&    addrList)
{
    char ifName[256];
    ifName[255] = '\0';
	if (GetInterfaceName(ifIndex, ifName, 255))
    {
        return GetInterfaceAddressList(ifName, addrType, addrList);
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: invalid interface index?!\n");
        return false;
    }
}  // end GetInterfaceAddressList(by index)

// given addrType, returns addrList with addresses found added
// TBD - we may want to move this implementation to "unixNet.cpp" and create a separate
// Win32-specific implementation using the Windows "GetAdaptersAddresses()" call which
// does.

bool ProtoNet::GetHostAddressList(ProtoAddress::Type  addrType,
				                  ProtoAddressList&   addrList)
{
    // First determine how many interfaces there are
    unsigned int ifCount =  GetInterfaceCount();
    if (0 == ifCount)
    {
        PLOG(PL_WARN, "ProtoNet::GetHostAddressList() warning: no interfaces?!\n");
        return true;
    }
    // Then allocate a buffer of the appropriate size
    unsigned int* ifIndices = new unsigned int[ifCount];
    if (NULL == ifIndices)
    {
        PLOG(PL_ERROR, "ProtoNet::GetHostAddressList() new ifIndices[] error: %s\n", GetErrorString());
        return false;
    }
    // Now call with a buffer to get this list of indices
    ifCount =  GetInterfaceIndices(ifIndices, ifCount);
    for (unsigned int i = 0; i < ifCount; i++)
	{   
        if (!GetInterfaceAddressList(ifIndices[i], addrType, addrList))
        {
            PLOG(PL_DEBUG, "ProtoNet::GetHostAddressList() error: unable to get addresses for iface index %d\n", ifIndices[i]);
        }
    }
    delete[] ifIndices;
    return true;  // all interfaces found & list returned in addrList
}  // end ProtoNet::GetHostAddressList()

// given "addrType", searches through interface list, returns first non-loopback address found
// TBD - should we _try_ to find a non-link-local addr as well?
#ifndef WIN32
// FindLocalAddress defined in win32Net.cpp
bool ProtoNet::FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress)
{
    ProtoAddressList addrList;
    if (GetHostAddressList(addrType, addrList))
    {
        ProtoAddressList::Iterator iterator(addrList);
        while (iterator.GetNextAddress(theAddress))
        {
            if (!theAddress.IsLoopback())
			    return true;
        }
    }
    return false;
}  // end ProtoNet::FindLocalAddress()
#endif
bool ProtoNet::GetInterfaceAddress(const char*         ifName, 
				                   ProtoAddress::Type  addrType,
				                   ProtoAddress&       theAddress,
                                   unsigned int*       ifIndex)
{
    ProtoAddressList addrList;
    GetInterfaceAddressList(ifName, addrType, addrList, ifIndex);
    return addrList.GetFirstAddress(theAddress);   
}  // end ProtoNet::GetInterfaceAddress()

bool ProtoNet::GetInterfaceAddress(unsigned int        ifIndex, 
				                   ProtoAddress::Type  addrType,
				                   ProtoAddress&       theAddress)
{
    ProtoAddressList addrList;
    GetInterfaceAddressList(ifIndex, addrType, addrList);
    return addrList.GetFirstAddress(theAddress);   
}  // end ProtoNet::GetInterfaceAddress()

#ifndef WIN32  
unsigned int ProtoNet::GetInterfaceAddressMask(unsigned int ifIndex, const ProtoAddress& ifAddr)
{
#ifndef WIN32
    char ifName[256];
    ifName[255] = '\0';
	if (GetInterfaceName(ifIndex, ifName, 255))
    {
        return GetInterfaceAddressMask(ifName, ifAddr);
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask() error: invalid interface index?!\n");
        return 0;
    }
#else
	PLOG(PL_ERROR,"ProtoNet::GetInterfaceAddressMask() error: function not implemented for WIN32\n");
	return 0;
#endif
}  // end ProtoNet::GetInterfaceAddressMask()

bool ProtoNet::AddInterfaceAddress(unsigned int ifIndex, const ProtoAddress& addr, unsigned int maskLen)
{
#ifndef WIN32
    char ifName[256];
    ifName[255] = '\0';
	if (GetInterfaceName(ifIndex, ifName, 255))
    {
        return AddInterfaceAddress(ifName, addr, maskLen);
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: invalid interface index?!\n");
        return false;
    }
#else
	PLOG(PL_ERROR,"ProtoNet::AddInterfaceAddress() error: function not implemented in WIN32\n");
	return false;
#endif
}  // end ProtoNet::AddInterfaceAddress()

bool ProtoNet::RemoveInterfaceAddress(unsigned int ifIndex, const ProtoAddress& addr, unsigned int maskLen)
{
#ifndef WIN32
    char ifName[256];
    ifName[255] = '\0';
	if (GetInterfaceName(ifIndex, ifName, 255))
    {
        return RemoveInterfaceAddress(ifName, addr, maskLen);
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: invalid interface index?!\n");
        return false;
    }
#else
	PLOG(PL_ERROR,"ProtoNet::RemoveInterfaceAddress() error: function not implemented in WIN32\n");
	return false;
#endif
}   // end ProtoNet::RemoveInterfaceAddress()
#endif // !WIN32   

ProtoNet::Monitor::Monitor()
{
    // Enable input notification by default
    StartInputNotification();
}

ProtoNet::Monitor::~Monitor()
{
    Close();
}

ProtoNet::Monitor::Event::Event()
 : event_type(UNKNOWN_EVENT), iface_index(0)
{
    strcpy(iface_name, "???");
    iface_name[IFNAME_MAX] = '\0';
}


ProtoNet::Monitor::Event::~Event()
{
}
