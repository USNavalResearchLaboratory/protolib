#ifndef _PROTO_ROUTE_MGR
#define _PROTO_ROUTE_MGR

#include "protoRouteTable.h"
#include "protoSocket.h"

/**
 * @class ProtoRouteMgr
 *
 * @brief Base class for providing  a consistent
 * interface to manage operating system (or
 * other) routing engines.
 *
 * Note: Since ProtoRouteTree can handle only one route per destination
 * GetAllRoutes() may miss some routes.  (Our bsdRouteMgr, linuxRouteMgr
 * and win32RouteMgr code has been tuned to deal with this for the
 * moment).  We'll likely extend ProtoRouteTree soon.
 */
class ProtoRouteMgr
{
    public:
        enum Type
        {
            SYSTEM,
            ZEBRA
        };     
            
        
        static ProtoRouteMgr* Create(Type type = SYSTEM);
        
        virtual ~ProtoRouteMgr();
        
        virtual bool Open(const void* userData = NULL) = 0;
        virtual bool IsOpen() const = 0;
        virtual void Close() = 0;
        
        virtual bool GetAllRoutes(ProtoAddress::Type addrType,
                                  ProtoRouteTable&   routeTable) = 0;
        bool DeleteAllRoutes();
        bool DeleteAllRoutes(ProtoAddress::Type addrType, unsigned int maxIterations = 8);
        bool SetRoutes(ProtoRouteTable& routeTable);
        /**
         * @brief Entries in the route table will be updated to reflect the new route table*.
         * Routes which existe in the old route table but not the new one will be removed.
         * Routes which exist in the new route table but not the old will be added.
         * Any routes with differing parameters will be updated.
         * @param oldRouteTable The older set of routes which are to be diff'ed against.
         * @param newRouteTable The new/current routes which are to be added. 
         * @param settedRouteTable If non-null pointer is supplied the list will be populated with routes which were added/updated
         * @param deletedRouteTable If non-null pointer is supplied the list will be populated with routes which were deleted 
         * @return true upon success.
         */
        bool UpdateRoutes(ProtoRouteTable& oldRouteTable, ProtoRouteTable& newRouteTable, ProtoRouteTable* settedRouteTable = NULL, ProtoRouteTable* deletedRouteTable = NULL);
        /**
         * @brief Entries in the route table will be updated to reflect the new route table*.
         * Routes which existe in the old route table but not the new one will be removed.
         * Routes which exist in the new route table but not the old will be added.
         * Any routes with differing parameters will be updated.
         * @param oldRouteTable The older set of routes which are to be diff'ed against.
         * @param newRouteTable The new/current routes which are to be added. 
         * @param settedRouteTable The list will be populated with routes which should be added
         * @param deletedRouteTable The list will be populated with routes which should be deleted
         * @return true upon success.
         */
        bool GetDiff(ProtoRouteTable& oldRouteTable, ProtoRouteTable& newRouteTable, ProtoRouteTable& settedRouteTable, ProtoRouteTable& deletedRouteTable);
	    bool DeleteRoutes(ProtoRouteTable& routeTable);
        /**
         * 
         * @brief will save IPv4 and IPv6 route tables
         * @return true upon success of saving EITHER IPv4 or IPv6 route tables
         */
        bool SaveAllRoutes();
        /**
         * @brief function will read the current route table and save the routing state in a proto route table
         * @param addrType the type of route table to save
         * @return true upon success
         */
        bool SaveAllRoutes(ProtoAddress::Type addrType);
        /**
         * @brief will attempt to restore both IPv4 and IPv6 route tables
         * @return true upon success of restoring EITHER IPv4 or IPv6 route tables.
         */
        bool RestoreSavedRoutes();
        /**
         * @brief if the routes have been saved then the route table will be updated with the saved routes
         * @param addrType the type of route table to save
         * @return will return true upon success.  Will return false if no route state was previously saved.
         */
        bool RestoreSavedRoutes(ProtoAddress::Type addrType);
        
        void ClearSavedRoutes();
        
        virtual bool GetRoute(const ProtoAddress& dst, 
                              unsigned int        prefixLen,
                              ProtoAddress&       gw,
                              unsigned int&       ifIndex,
                              int&                metric) = 0;
        
        virtual bool SetRoute(const ProtoAddress&   dst,
                              unsigned int          prefixLen,
                              const ProtoAddress&   gw,
                              unsigned int          ifIndex,
                              int                   metric) = 0;
        
        virtual bool DeleteRoute(const ProtoAddress&    dst,
                                 unsigned int           prefixLen,
                                 const ProtoAddress&    gw,
                                 unsigned int           ifIndex) = 0;     


        
        bool SetRoute(ProtoRouteTable::Entry& entry)
        {
            return SetRoute(entry.GetDestination(),
                            entry.GetPrefixSize(),
                            entry.GetGateway(),
                            entry.GetInterfaceIndex(),
                            entry.GetMetric());
        }
        
        bool DeleteRoute(ProtoRouteTable::Entry& entry)
        {
            return DeleteRoute(entry.GetDestination(),
                               entry.GetPrefixSize(),
                               entry.GetGateway(),
                               entry.GetInterfaceIndex());
        }
        
        // Here are some "shortcut" set route methods
        bool SetHostRoute(const ProtoAddress& dst,
                          const ProtoAddress& gw,
                          unsigned int        ifIndex,
                          int                 metric)
        {
            return SetRoute(dst, (dst.GetLength() << 3), gw, ifIndex, metric);   
        }
        
        bool SetDirectHostRoute(const ProtoAddress& dst,
                                unsigned int        ifIndex,
                                int                 metric)
        {
            ProtoAddress gw;
            gw.Invalidate();
            return SetRoute(dst, (dst.GetLength() << 3), gw, ifIndex, metric);   
        } 
        
        bool SetNetRoute(const ProtoAddress& dst, 
                         unsigned int        prefixLen,
                         const ProtoAddress& gw,
                         unsigned int        ifIndex,
                         int                 metric)
        {
            return SetRoute(dst, prefixLen, gw, ifIndex, metric);   
        }
        
        bool SetDirectNetRoute(const ProtoAddress& dst, 
                               unsigned int        prefixLen,
                               unsigned int        ifIndex,
                               int                 metric)
        {
            ProtoAddress gw;
            gw.Invalidate();
            return SetRoute(dst, prefixLen, gw, ifIndex, metric);   
        } 

        // Turn IP forward on/off
        virtual bool SetForwarding(bool state) = 0;
        
        // Network interface helper functions
        bool GetInterfaceAddress(unsigned int       ifIndex, 
                                 ProtoAddress::Type addrType,
                                 ProtoAddress&      theAddress)
        {
            ProtoAddressList addrList;
            GetInterfaceAddressList(ifIndex, addrType, addrList);
            return addrList.GetFirstAddress(theAddress);
        }
        virtual unsigned int GetInterfaceIndex(const char* interfaceName) = 0;
        virtual bool GetInterfaceName(unsigned int  interfaceIndex, 
                                      char*         buffer, 
                                      unsigned int  buflen) = 0;
        
        // Retrieve all addresses for a given interface
        // (the retrieved addresses are added to the "addrList" provided)
        virtual bool GetInterfaceAddressList(unsigned int         ifIndex,
                                             ProtoAddress::Type   addrType,
                                             ProtoAddressList&  addrList) = 0;
        
    protected:
        ProtoRouteMgr();
    
    private:
        ProtoRouteTable* savedRoutesIPv4;
        ProtoRouteTable* savedRoutesIPv6;
};  // end class ProtoRouteMgr



#endif // _PROTO_ROUTE_MGR
