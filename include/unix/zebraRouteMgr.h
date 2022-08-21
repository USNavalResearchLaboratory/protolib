#include <stdio.h>
#include "protoRouteMgr.h"
#include "protoPipe.h"
#include "protoDebug.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>

// linux specific includes
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace ZebraNameSpace
{
    const int ZEBRA_VERSION = 1;
    const int ZEBRA_MAX_PACKET_SIZE = 4096;
    const int ZEBRA_PORT = 2600;
    const int ZEBRA_MODE = ProtoAddress::IPv4;
    const int ZEBRA_HEADER_SIZE            = 6;

    /* Zebra API message flag. */
    const int ZAPI_MESSAGE_NEXTHOP  = 0x01;
    const int ZAPI_MESSAGE_IFINDEX  = 0x02;
    const int ZAPI_MESSAGE_DISTANCE = 0x04;
    const int ZAPI_MESSAGE_METRIC   = 0x08;

    /* Zebra message types. */
    const int ZEBRA_INTERFACE_ADD               = 1;
    const int ZEBRA_INTERFACE_DELETE            = 2;
    const int ZEBRA_INTERFACE_ADDRESS_ADD       = 3;
    const int ZEBRA_INTERFACE_ADDRESS_DELETE    = 4;
    const int ZEBRA_INTERFACE_UP                = 5;
    const int ZEBRA_INTERFACE_DOWN              = 6;
    const int ZEBRA_IPV4_ROUTE_ADD              = 7;
    const int ZEBRA_IPV4_ROUTE_DELETE           = 8;
    const int ZEBRA_IPV6_ROUTE_ADD              = 9;
    const int ZEBRA_IPV6_ROUTE_DELETE           = 10;
    const int ZEBRA_REDISTRIBUTE_ADD            = 11;
    const int ZEBRA_REDISTRIBUTE_DELETE         = 12;
    const int ZEBRA_REDISTRIBUTE_DEFAULT_ADD    = 13;
    const int ZEBRA_REDISTRIBUTE_DEFAULT_DELETE = 14;
    const int ZEBRA_IPV4_NEXTHOP_LOOKUP         = 15;
    const int ZEBRA_IPV6_NEXTHOP_LOOKUP         = 16;
    const int ZEBRA_IPV4_IMPORT_LOOKUP          = 17;
    const int ZEBRA_IPV6_IMPORT_LOOKUP          = 18;
    const int ZEBRA_INTERFACE_RENAME            = 19;
    const int ZEBRA_ROUTER_ID_ADD               = 20;
    const int ZEBRA_ROUTER_ID_DELETE            = 21;
    const int ZEBRA_ROUTER_ID_UPDATE            = 22;
    const int ZEBRA_LINKMETRICS_SUBSCRIBE       = 23;
    const int ZEBRA_LINKMETRICS_UNSUBSCRIBE     = 24;
    const int ZEBRA_LINKMETRICS_METRICS         = 25;
    const int ZEBRA_LINKMETRICS_STATUS          = 26;
    const int ZEBRA_LINKMETRICS_METRICS_RQST    = 27;
    const int ZEBRA_MESSAGE_MAX                 = 28;
    const int ZEBRA_HEADER_MARKER               = 255;

    /* Zebra route's types. */
    const int ZEBRA_ROUTE_SYSTEM              = 0;
    const int ZEBRA_ROUTE_KERNEL              = 1;
    const int ZEBRA_ROUTE_CONNECT             = 2;
    const int ZEBRA_ROUTE_STATIC              = 3;
    const int ZEBRA_ROUTE_RIP                 = 4;
    const int ZEBRA_ROUTE_RIPNG               = 5;
    const int ZEBRA_ROUTE_OSPF                = 6;
    const int ZEBRA_ROUTE_OSPF6               = 7;
    const int ZEBRA_ROUTE_ISIS                = 8;
    const int ZEBRA_ROUTE_BGP                 = 9;
    const int ZEBRA_ROUTE_HSLS                = 10;
    const int ZEBRA_ROUTE_MAX                 = 11;

    /* Zebra's family types. */
    const int ZEBRA_FAMILY_IPV4               = 1;
    const int ZEBRA_FAMILY_IPV6               = 2;
    const int ZEBRA_FAMILY_MAX                = 3;

    /* Error codes of zebra. */
    const int ZEBRA_ERR_NOERROR              =  0;
    const int ZEBRA_ERR_RTEXIST              = -1;
    const int ZEBRA_ERR_RTUNREACH            = -2;
    const int ZEBRA_ERR_EPERM                = -3;
    const int ZEBRA_ERR_RTNOEXIST            = -4;
    const int ZEBRA_ERR_KERNEL               = -5;

    /* Zebra message flags */
    const int ZEBRA_FLAG_INTERNAL          = 0x01;
    const int ZEBRA_FLAG_SELFROUTE         = 0x02;
    const int ZEBRA_FLAG_BLACKHOLE         = 0x04;
    const int ZEBRA_FLAG_IBGP              = 0x08;
    const int ZEBRA_FLAG_SELECTED          = 0x10;
    const int ZEBRA_FLAG_CHANGED           = 0x20;
    const int ZEBRA_FLAG_STATIC            = 0x40;
    const int ZEBRA_FLAG_REJECT            = 0x80;

    /* Zebra nexthop flags. */
    const int ZEBRA_NEXTHOP_IFINDEX           = 1;
    const int ZEBRA_NEXTHOP_IFNAME            = 2;
    const int ZEBRA_NEXTHOP_IPV4              = 3;
    const int ZEBRA_NEXTHOP_IPV4_IFINDEX      = 4;
    const int ZEBRA_NEXTHOP_IPV4_IFNAME       = 5;
    const int ZEBRA_NEXTHOP_IPV6              = 6;
    const int ZEBRA_NEXTHOP_IPV6_IFINDEX      = 7;
    const int ZEBRA_NEXTHOP_IPV6_IFNAME       = 8;
    const int ZEBRA_NEXTHOP_BLACKHOLE         = 9;

#ifndef INADDR_LOOPBACK
    const int INADDR_LOOPBACK = 0x7f000001;      /* Internet address 127.0.0.1.  */
#endif

    /* Address family numbers from RFC1700. */
    const int AFI_IP                   = 1;
    const int AFI_IP6                  = 2;
    const int AFI_MAX                  = 3;

    /* Subsequent Address Family Identifier. */
    const int SAFI_UNICAST             = 1;
    const int SAFI_MULTICAST           = 2;
    const int SAFI_UNICAST_MULTICAST   = 3;
    const int SAFI_MPLS_VPN            = 4;
    const int SAFI_MAX                 = 5;

    /* Filter direction.  */
    const int FILTER_IN                = 0;
    const int FILTER_OUT               = 1;
    const int FILTER_MAX               = 2;

    /* Default Administrative Distance of each protocol. */
    const int ZEBRA_KERNEL_DISTANCE_DEFAULT     = 0;
    const int ZEBRA_CONNECT_DISTANCE_DEFAULT    = 0;
    const int ZEBRA_STATIC_DISTANCE_DEFAULT     = 1;
    const int ZEBRA_RIP_DISTANCE_DEFAULT      = 120;
    const int ZEBRA_RIPNG_DISTANCE_DEFAULT    = 120;
    const int ZEBRA_OSPF_DISTANCE_DEFAULT     = 110;
    const int ZEBRA_OSPF6_DISTANCE_DEFAULT    = 110;
    const int ZEBRA_ISIS_DISTANCE_DEFAULT     = 115;
    const int ZEBRA_IBGP_DISTANCE_DEFAULT     = 200;
    const int ZEBRA_EBGP_DISTANCE_DEFAULT     =  20;
}
using namespace ZebraNameSpace;

class ZebraRouteMgr : public ProtoRouteMgr
{
    public:
        ZebraRouteMgr();
        ~ZebraRouteMgr();

        virtual bool Open(const void* userData = NULL);
        virtual bool IsOpen() const;
        virtual void Close();

        virtual bool GetAllRoutes(ProtoAddress::Type addrType,
                                  ProtoRouteTable&   routeTable);

        virtual bool GetRoute(const ProtoAddress&   dst,
                              unsigned int          prefixLen,
                              ProtoAddress&         gw,
                              unsigned int&         ifIndex,
                              int&                  metric);

        virtual bool SetRoute(const ProtoAddress&   dst,
                              unsigned int          prefixLen,
                              const ProtoAddress&   gw,
                              unsigned int          ifIndex = 0,
                              int                   metric = -1);

        virtual bool DeleteRoute(const ProtoAddress& dst,
                                 unsigned int        prefixLen,
                                 const ProtoAddress& gw,
                                 unsigned int        ifIndex = 0);

        virtual bool SetForwarding(bool state);

        virtual unsigned int GetInterfaceIndex(const char* interfaceName)
            {return ProtoSocket::GetInterfaceIndex(interfaceName);}

        virtual bool GetInterfaceAddressList(unsigned int        ifIndex,
                                            ProtoAddress::Type  addrType,
                                            ProtoAddressList& addrList);

        virtual bool GetInterfaceName(unsigned int  interfaceIndex,
                                      char*         buffer,
                                      unsigned int  buflen)
            {return ProtoSocket::GetInterfaceName(interfaceIndex, buffer, buflen);}
    private:

        void OnClientSocketEvent(ProtoSocket&       theSocket,
                           ProtoSocket::Event theEvent);
        void PrintBuffer(char* buffer,int len,int dlevel);
        ProtoPipe zPipe;
        unsigned char ibuf[ZEBRA_MAX_PACKET_SIZE];
        unsigned char obuf[ZEBRA_MAX_PACKET_SIZE];

        int     descriptor;
        pid_t   pid;
        UINT32  sequence;

};  // end class ZebraRouteMgr
