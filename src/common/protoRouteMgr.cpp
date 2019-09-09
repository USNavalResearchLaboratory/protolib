/**
* @file protoRouteMgr.cpp
* 
* @brief Base class for providing a consistent interface to manage operating system (or other) routing engines 
*/
#include "protoRouteMgr.h"
#include "protoDebug.h"
ProtoRouteMgr::~ProtoRouteMgr()
{
    if(NULL != savedRoutesIPv4)
        delete savedRoutesIPv4;
    if(NULL != savedRoutesIPv6)
        delete savedRoutesIPv6;
}
bool ProtoRouteMgr::DeleteAllRoutes()
{
    return DeleteAllRoutes(ProtoAddress::IPv4) && DeleteAllRoutes(ProtoAddress::IPv6);
}
bool ProtoRouteMgr::DeleteAllRoutes(ProtoAddress::Type addrType, unsigned int maxIterations)
{
    ProtoRouteTable rt;
    // Make multiple passes to get rid of possible
    // multiple routes to destinations
    // (TBD) extend ProtoRouteTable to support multiple routes per destination
    while (maxIterations-- > 0)
    {
        if (!GetAllRoutes(addrType, rt))
        {
            PLOG(PL_ERROR, "ProtoRouteMgr::DeleteAllRoutes() error getting routes\n");
            return false;   
        }
        if (rt.IsEmpty()) break;
        if (!DeleteRoutes(rt))
        {
            PLOG(PL_ERROR, "ProtoRouteMgr::DeleteAllRoutes() error deleting routes\n");
            return false;
        }
        rt.Destroy();
    }

    if (0 == maxIterations)
    {
        PLOG(PL_ERROR, "ProtoRouteMgr::DeleteAllRoutes() couldn't seem to delete everything!\n");
        return false;
    }
    else
    {
        return true;
    }
}  // end ProtoRouteMgr::DeleteAllRoutes()

/**
 * Set direct (interface) routes and gateway routes.
 */
bool ProtoRouteMgr::SetRoutes(ProtoRouteTable& routeTable)
{
    bool result = true;
    ProtoRouteTable::Iterator iterator(routeTable);
    ProtoRouteTable::Entry* entry;
    // First, set direct (interface) routes 
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsDirectRoute())
        {
            if (!SetRoute(entry->GetDestination(),
                          entry->GetPrefixSize(),
                          entry->GetGateway(),
                          entry->GetInterfaceIndex(),
                          entry->GetMetric()))
            {
                PLOG(PL_ERROR, "ProtoRouteMgr::SetAllRoutes() failed to set direct route to: %s\n",
                        entry->GetDestination().GetHostString());
                result = false;   
            }
        }
    }
    // Second, set gateway routes 
    iterator.Reset();
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsGatewayRoute())
        {
            if (!SetRoute(entry->GetDestination(),
                          entry->GetPrefixSize(),
                          entry->GetGateway(),
                          entry->GetInterfaceIndex(),
                          entry->GetMetric()))
            {
                PLOG(PL_ERROR, "ProtoRouteMgr::SetAllRoutes() failed to set gateway route to: %s\n",
                        entry->GetDestination().GetHostString());
                result = false;   
            }
        }
    }
    
    return result;
}  // end ProtoRouteMgr::SetRoutes()
bool
ProtoRouteMgr::UpdateRoutes(ProtoRouteTable& oldRouteTable, ProtoRouteTable& newRouteTable)
{
    //this can be sped up by only going through a single list instead of both and adding routes directly instead of in sets. TBD
    ProtoRouteTable removeRoutes;
    ProtoRouteTable updateRoutes;
    removeRoutes.Init();
    updateRoutes.Init();
    ProtoRouteTable::Iterator it_old(oldRouteTable);
    ProtoRouteTable::Iterator it_new(newRouteTable);
    ProtoRouteTable::Entry* entry;
    
    ProtoAddress gwAddrLookup;
    unsigned int ifaceIndexLookup;
    int metricLookup;
    
    while(NULL != (entry = it_old.GetNextEntry()))
    {
        ProtoAddress dstAddr    = entry->GetDestination();
        ProtoAddress gwAddr     = entry->GetGateway();
        unsigned int prefixLen  = entry->GetPrefixSize();
        unsigned int ifaceIndex = entry->GetInterfaceIndex();
        int          metric     = entry->GetMetric();
        if(!newRouteTable.FindRoute(dstAddr,prefixLen,gwAddrLookup,ifaceIndexLookup,metricLookup))
        {
            if(!removeRoutes.SetRoute(dstAddr,prefixLen,gwAddr,ifaceIndex,metric)) 
            {
                PLOG(PL_ERROR,"ProtoRouteMgr::UpdateRoutes() failed add an old route to the removeRoutes table\n");
                return false;
            }
        }
    }
    while(NULL != (entry = it_new.GetNextEntry()))
    {
        ProtoAddress dstAddr    = entry->GetDestination();
        ProtoAddress gwAddr     = entry->GetGateway();
        unsigned int prefixLen  = entry->GetPrefixSize();
        unsigned int ifaceIndex = entry->GetInterfaceIndex();
        int          metric     = entry->GetMetric();
        if(!oldRouteTable.FindRoute(dstAddr,prefixLen,gwAddrLookup,ifaceIndexLookup,metricLookup))
        {
            if(!updateRoutes.SetRoute(dstAddr,prefixLen,gwAddr,ifaceIndex,metric))
            {
                PLOG(PL_ERROR,"ProtoRouteMgr::UpdateRoutes() failed add an old route to the removeRoutes table in new section\n");
                return false;
            }
        } else {
            if((gwAddrLookup != gwAddr) ||
               (ifaceIndexLookup != ifaceIndex) ||
               (metricLookup != metric))
            {
                if(!updateRoutes.SetRoute(dstAddr,prefixLen,gwAddr,ifaceIndex,metric))
                {
                    PLOG(PL_ERROR,"ProtoRouteMgr::UpdateRoutes() failed add an old route to the removeRoutes table in change section\n");
                    return false;
                }
            }
        }
    }
    if(!DeleteRoutes(removeRoutes)) {
        PLOG(PL_ERROR,"ProtoRouteMgr::UpdateRoutes() failed delete old routes\n");
        return false;
    }
    if(!SetRoutes(updateRoutes)) {
        PLOG(PL_ERROR,"ProtoRouteMgr::UpdateRoutes() failed update routes\n");
        return false;
    }
    return true;
}
/**
 * Deletes gateway and direct (interface) routes and the
 * default entry if one exists.
 */
bool ProtoRouteMgr::DeleteRoutes(ProtoRouteTable& routeTable)
{
    bool result = true;
    ProtoRouteTable::Iterator iterator(routeTable);
    const ProtoRouteTable::Entry* entry;
    // First, delete gateway routes 
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsGatewayRoute())
        {
            if (!DeleteRoute(entry->GetDestination(),
			                 entry->GetPrefixSize(),
                             entry->GetGateway(),
                             entry->GetInterfaceIndex()))
            {
	            PLOG(PL_ERROR, "ProtoRouteMgr::DeleteAllRoutes() failed to delete gateway route to: %s\n",
	                    entry->GetDestination().GetHostString());
	            result = false;   
            }
        }
    }
    // Second, delete direct (interface) routes
    iterator.Reset(); 
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsDirectRoute())
        {
            if (!DeleteRoute(entry->GetDestination(),
			                 entry->GetPrefixSize(),
                             entry->GetGateway(),
                             entry->GetInterfaceIndex()))
            {
	            PLOG(PL_ERROR, "ProtoRouteMgr::DeleteAllRoutes() failed to delete direct route to: %s\n",
	                    entry->GetDestination().GetHostString());
	            result = false;   
            }
        }
    }
    // If there's a default entry delete it, too
    entry = routeTable.GetDefaultEntry();
    if (entry)
    {
        if (!DeleteRoute(entry->GetDestination(),
			             entry->GetPrefixSize(),
                         entry->GetGateway(),
                         entry->GetInterfaceIndex()))
        {
	        PLOG(PL_ERROR, "ProtoRouteMgr::DeleteAllRoutes() failed to delete default route\n");
	        result = false;   
        }
    }
    return result;
}  // end ProtoRouteMgr::DeleteRoutes()

bool
ProtoRouteMgr::SaveAllRoutes()
{
    return SaveAllRoutes(ProtoAddress::IPv4) || SaveAllRoutes(ProtoAddress::IPv6);
}
bool
ProtoRouteMgr::SaveAllRoutes(ProtoAddress::Type addrType) 
{
    switch (addrType) 
    {
        case ProtoAddress::IPv4:
            if (NULL == savedRoutesIPv4) 
            {
                savedRoutesIPv4 = new ProtoRouteTable();
                if (NULL == savedRoutesIPv4) 
                {
                    PLOG(PL_ERROR, "ProtoRouteMgr::SaveAllRoutes() failed allocating a ProtoRouteTable\n");
                    return false;
                }
            }
            savedRoutesIPv4->Init();
            if (!GetAllRoutes(ProtoAddress::IPv4, *savedRoutesIPv4)) 
            {
                PLOG(PL_ERROR, "ProtoRouteMgr::SaveAllRoutes() failed getting all of the IPv4 routes");
                return false;
            }
            break;
        case ProtoAddress::IPv6:
            if (NULL == savedRoutesIPv6) 
            {
                savedRoutesIPv6 = new ProtoRouteTable();
                if (NULL == savedRoutesIPv6) 
                {
                    PLOG(PL_ERROR, "ProtoRouteMgr::SaveAllRoutes() failed allocating a ProtoRouteTable\n");
                    return false;
                }
            }
            savedRoutesIPv6->Init();
            if (!GetAllRoutes(ProtoAddress::IPv4, *savedRoutesIPv6)) 
            {
                PLOG(PL_ERROR, "ProtoRouteMgr::SaveAllRoutes() failed getting all of the IPv6 routes");
                return false;
            }
            break;
        default:
            PLOG(PL_ERROR, "ProtoRouteMgr::SaveAllRoutes() only supports saving route tables of types IPv6 and IPv4\n");
            return false;
    }
    return true;
}
bool
ProtoRouteMgr::RestoreSavedRoutes()
{
    bool returnvalue = false;
    if(NULL != savedRoutesIPv6)
    {
        returnvalue = RestoreSavedRoutes(ProtoAddress::IPv6);
    }
    if(NULL != savedRoutesIPv4)
    {
        returnvalue = returnvalue || RestoreSavedRoutes(ProtoAddress::IPv4);
    }
    if(!returnvalue)
    {
        PLOG(PL_ERROR,"ProtoRouteMgr::RestoreSavedRoutes() couldn't restore routes, did you save any first?");
    }
    return returnvalue;
}
bool
ProtoRouteMgr::RestoreSavedRoutes(ProtoAddress::Type addrType)
{
    switch(addrType)
    {
        case ProtoAddress::IPv4:
            if(NULL == savedRoutesIPv4)
            {
                PLOG(PL_ERROR,"ProtoRouteMgr::RestoreSavedRoutes() can not restore IPv4 routes as none have been saved.\n");
                return false;
            }
            return SetRoutes(*savedRoutesIPv4);
            break;
        case ProtoAddress::IPv6:
            if(NULL == savedRoutesIPv6)
            {
                PLOG(PL_ERROR,"ProtoRouteMgr::RestoreSavedRoutes() can not restore IPv6 routes as none have been saved.\n");
                return false;
            }
            return SetRoutes(*savedRoutesIPv6);
            break;
        default:
            PLOG(PL_ERROR, "ProtoRouteMgr::RestoreSavedRoutes() only supports restoring route tables of types IPv6 and IPv4\n");
            return false;
    }
    return true;
}
