
#include "protoRouteMgr.h"
#include "protoDebug.h"

#include <Iphlpapi.h>
#include <Iprtrmib.h>
#define MPR50 1
#ifndef _WIN32_WCE
#include <Routprot.h>
#endif // !_WIN32_WCE

class Win32RouteMgr : public ProtoRouteMgr
{
    public:
        Win32RouteMgr();
        ~Win32RouteMgr();

        virtual bool Open(const void* userData = NULL);
        virtual bool IsOpen() const {return true;}
        virtual void Close();

        virtual bool GetAllRoutes(ProtoAddress::Type addrType,
                                  ProtoRouteTable&   routeTable);

        virtual bool GetRoute(const ProtoAddress& dst, 
                              unsigned int        prefixLen,
                              ProtoAddress&       gw,
                              unsigned int&       ifIndex,
                              int&                metric);
        
        virtual bool SetRoute(const ProtoAddress& dst,
                              unsigned int        prefixLen,
                              const ProtoAddress& gw,
                              unsigned int        ifIndex,
                              int                 metric);
        
        virtual bool DeleteRoute(const ProtoAddress& dst,
                                 unsigned int        prefixLen,
                                 const ProtoAddress& gw,
                                 unsigned int        ifIndex);  
        
        virtual bool SetForwarding(bool state);
        
        // Network interface helper functions
        virtual unsigned int GetInterfaceIndex(const char* interfaceName)
        {
            return ProtoSocket::GetInterfaceIndex(interfaceName);
        }
        virtual bool GetInterfaceAddressList(unsigned int        ifIndex, 
                                             ProtoAddress::Type  addrType,
                                             ProtoAddressList&   addrList)
        {
            char ifName[256];
            bool result = ProtoSocket::GetInterfaceName(ifIndex, ifName, 255);
            return result ? ProtoSocket::GetInterfaceAddressList(ifName, addrType, addrList) : false;
        }
        virtual bool GetInterfaceName(unsigned int  ifIndex, 
                                      char*         buffer, 
                                      unsigned int  buflen)
        {
            return ProtoSocket::GetInterfaceName(ifIndex, buffer, buflen);
        }

    private:
        

};  // end class Win32RouteMgr

ProtoRouteMgr* ProtoRouteMgr::Create(Type /*type*/)
{
    // TBD - support alternative route mgr "types" (e.g. ZEBRA)
    return static_cast<ProtoRouteMgr*>(new Win32RouteMgr);
}

Win32RouteMgr::Win32RouteMgr()
{
}

Win32RouteMgr::~Win32RouteMgr()
{
}

bool Win32RouteMgr::Open(const void* /*userData*/)
{
    return true;

}  // end Win32RouteMgr::Open()

void Win32RouteMgr::Close()
{
   
}  // end Win32RouteMgr::Close()


bool Win32RouteMgr::SetForwarding(bool state)
{
    MIB_IPSTATS ipStats;

    ipStats.dwDefaultTTL = MIB_USE_CURRENT_TTL;
    ipStats.dwForwarding = state ? MIB_IP_FORWARDING : MIB_IP_NOT_FORWARDING;
    if (NO_ERROR == SetIpStatistics(&ipStats))
    {
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "Win32RouteMgr::SetForwarding() SetIpStatistics() error: %s\n", GetErrorString());
        return false;
    }
}  // end Win32RouteMgr::SetForwarding()

bool Win32RouteMgr::GetRoute(const ProtoAddress& dst, 
                             unsigned int        prefixLen,
                             ProtoAddress&       gw,
                             unsigned int&       ifIndex,
                             int&                metric)
{
    ProtoRouteTable routeTable;

    if (!GetAllRoutes(dst.GetType(), routeTable))
    {
        PLOG(PL_ERROR, "Win32RouteMgr::GetRoute() error getting system route table\n");
        return false;
    }
    return routeTable.FindRoute(dst, prefixLen, gw, ifIndex, metric);
}  // end Win32RouteMgr::GetRoute()

bool Win32RouteMgr::SetRoute(const ProtoAddress& dst,
                             unsigned int        prefixLen,
                             const ProtoAddress& gw,
                             unsigned int        ifIndex,
                             int                 metric)
{
    if (ProtoAddress::IPv4 != dst.GetType())
    {
        PLOG(PL_ERROR, "Win32RouteMgr::SetRoute() error: IPv6 not yet supported\n");
        return false;
    }
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "Win32RouteMgr::SetRoute() error: ifIndex not specified!\n");
        return false;
    }
	int oldmetric = 0;
	ProtoAddress oldgw;
	if(GetRoute(dst,prefixLen,oldgw,ifIndex,oldmetric))
		DeleteRoute(dst,prefixLen,oldgw,ifIndex);
	MIB_IPFORWARDROW entry;
    memset(&entry, 0, sizeof(MIB_IPFORWARDROW));
	entry.dwForwardDest = htonl(dst.IPv4GetAddress());
    ProtoAddress mask;
    mask.Reset(dst.GetType(), false);
    mask.ApplyPrefixMask(prefixLen);
	entry.dwForwardMask = htonl(mask.IPv4GetAddress());
    if (gw.IsValid())
        entry.dwForwardNextHop = htonl(gw.IPv4GetAddress());
    else
        entry.dwForwardNextHop = htonl(dst.IPv4GetAddress());
	entry.dwForwardIfIndex = ifIndex;
 	entry.dwForwardProto = PROTO_IP_NETMGMT;
	
    // First, attempt to set as an existing entry
    if (NO_ERROR != SetIpForwardEntry(&entry))
    {
        if (NO_ERROR != CreateIpForwardEntry(&entry))
        {
            PLOG(PL_ERROR, "Win32RouteMgr::SetRoute() CreateIpForwardEntry() error\n");
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return true;
    }
}  // end Win32RouteMgr::SetRoute()

bool Win32RouteMgr::DeleteRoute(const ProtoAddress& dst,
                                unsigned int        prefixLen,
                                const ProtoAddress& gw,
                                unsigned int        ifIndex)
{
    if (ProtoAddress::IPv4 != dst.GetType())
    {
        PLOG(PL_ERROR, "Win32RouteMgr::DeleteRoute() error: IPv6 not yet supported\n");
        return false;
    }
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "Win32RouteMgr::DeleteRoute() error: ifIndex not specified!\n");
        return false;
    }
	MIB_IPFORWARDROW entry;
    memset(&entry, 0, sizeof(MIB_IPFORWARDROW));
	entry.dwForwardDest = htonl(dst.IPv4GetAddress());
    ProtoAddress mask;
    mask.Reset(dst.GetType(), false);
    mask.ApplyPrefixMask(prefixLen);
	entry.dwForwardMask = htonl(mask.IPv4GetAddress());
    if (gw.IsValid())
    {
	    entry.dwForwardNextHop = htonl(gw.IPv4GetAddress());
    }
    else
    {
		entry.dwForwardNextHop = htonl(dst.IPv4GetAddress());
	}
	entry.dwForwardIfIndex = ifIndex;
 	entry.dwForwardProto = PROTO_IP_NETMGMT;
	
	if (NO_ERROR != DeleteIpForwardEntry(&entry))
    {
        PLOG(PL_ERROR, "Win32RouteMgr::DeleteRoute() DeleteIpForwardEntry() error\n");
        return false;
    }
    else
    {
        return true;
    }
}  // end Win32RouteMgr::DeleteRoute()


bool Win32RouteMgr::GetAllRoutes(ProtoAddress::Type addrType,
                                 ProtoRouteTable&   routeTable)
{
    unsigned long bufferSize = 0;
    GetIpForwardTable((MIB_IPFORWARDTABLE*)NULL, &bufferSize, FALSE);
    char* buffer = new char[bufferSize];
    if (NULL == buffer)
    {
        PLOG(PL_ERROR, "Win32RouteMgr::GetAllRoutes() new buffer[%lu] error\n", bufferSize);
        return false;
    }
    MIB_IPFORWARDTABLE* fwdTable = (MIB_IPFORWARDTABLE*)buffer;
    if (NO_ERROR != GetIpForwardTable(fwdTable, &bufferSize, FALSE))
    {
        PLOG(PL_ERROR, "Win32RouteMgr::GetAllRoutes() new MIB_IPFORWARDTABLE error\n");
        delete[] buffer;
        return false;
    }
    bool result = true;
    DWORD numEntries = fwdTable->dwNumEntries;
    for (DWORD i = 0 ; i < numEntries; i++)
    {
        MIB_IPFORWARDROW& entry = fwdTable->table[i];
        ProtoAddress destination;
        destination.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry.dwForwardDest, sizeof(DWORD));
        ProtoAddress gateway;
        gateway.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry.dwForwardNextHop, sizeof(DWORD));
        ProtoAddress mask;
        mask.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry.dwForwardMask, sizeof(DWORD));
        unsigned int prefixLen = mask.GetPrefixLength();
        if (!routeTable.SetRoute(destination, prefixLen, gateway, entry.dwForwardIfIndex, entry.dwForwardMetric1))
        {
            PLOG(PL_ERROR, "Win32RouteMgr::GetAllRoutes() error creating table entry\n");
            result = false;
            break;
        }
    }
    delete[] buffer;
    return result;
}  // end Win32RouteMgr::GetAllRoutes()
