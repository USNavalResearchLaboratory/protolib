#include "protoNet.h"
#include "protoDebug.h"



unsigned int ProtoNet::GetInterfaceCount() 
{
    return GetInterfaceIndices(NULL, 0);
}  // end ProtoNet::GetInterfaceCount()

// given addrType, returns addrList with addresses found added

// TBD - we may want to move this implementation to "unixNet.cpp" and create a separate
// Win32-specific implementation using the Windows "GetAdaptersAddresses()" call which
// does.

bool ProtoNet::GetHostAddressList(ProtoAddress::Type  addrType,
				                  ProtoAddressList&   addrList)
{
    // First determine how many interfaces there are
    unsigned int ifCount =  GetInterfaceIndices(NULL, 0);
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
        char ifName[256];
        ifName[255] = '\0';
		if (GetInterfaceName(ifIndices[i], ifName, 255))
        {
            if (!GetInterfaceAddressList(ifName, addrType, addrList))
            {
                PLOG(PL_DEBUG, "ProtoNet::GetHostAddressList() error: unable to get addresses for iface index %d\n", ifIndices[i]);
            }
        }
        else
        {
            PLOG(PL_DEBUG, "ProtoNet::GetHostAddressList() error: unable to get name for iface index %d\n", ifIndices[i]);
        } 
    }
    delete[] ifIndices;
    return true;  // all interfaces found & list returned in addrList
}  // end ProtoNet::GetHostAddressList()

bool ProtoNet::GetInterfaceAddress(const char*         ifName, 
				                   ProtoAddress::Type  addrType,
				                   ProtoAddress&       theAddress,
                                   unsigned int*       ifIndex)
{
    ProtoAddressList addrList;
    GetInterfaceAddressList(ifName, addrType, addrList, ifIndex);
    return addrList.GetFirstAddress(theAddress);   
}  // end ProtoNet::GetInterfaceAddress()

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
}


ProtoNet::Monitor::Event::~Event()
{
}
