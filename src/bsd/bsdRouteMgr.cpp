#include "protoRouteMgr.h"
#include "protoDebug.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>

// BSD specific includes
#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if_dl.h>

class BsdRouteMgr : public ProtoRouteMgr
{
    public:
        BsdRouteMgr();
        virtual ~BsdRouteMgr();
        
        virtual bool Open(const void* userData = NULL);
        virtual bool IsOpen() const {return (descriptor >= 0);}
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
        
        // (TBD) make this actually do something   
        virtual bool SetForwarding(bool /*state*/) {return true;}
        
        virtual unsigned int GetInterfaceIndex(const char* interfaceName)
        {
            return ProtoSocket::GetInterfaceIndex(interfaceName);
        }
        virtual bool GetInterfaceAddressList(unsigned int        ifIndex, 
                                             ProtoAddress::Type  addrType,
                                             ProtoAddressList& addrList);
        virtual bool GetInterfaceName(unsigned int  interfaceIndex, 
                                      char*         buffer, 
                                      unsigned int  buflen)
        {
            return ProtoSocket::GetInterfaceName(interfaceIndex, buffer, buflen);
        }
        
            
    private:        
        int     descriptor;
        pid_t   pid;
        UINT32  sequence;
        
};  // end class BsdRouteMgr


ProtoRouteMgr* ProtoRouteMgr::Create(Type /*type*/)
{
    // TBD - support alternative route mgr "types" (e.g. ZEBRA)
    return (static_cast<ProtoRouteMgr*>(new BsdRouteMgr));   
}

BsdRouteMgr::BsdRouteMgr()
 : descriptor(-1), sequence(0)
{
    
}

BsdRouteMgr::~BsdRouteMgr()
{
    Close();
}

bool BsdRouteMgr::Open(const void* /*userData*/)
{
    if (IsOpen()) Close();
    if ((descriptor = socket(AF_ROUTE, SOCK_RAW, 0)) < 0)
    {
        PLOG(PL_ERROR, "BsdRouteMgr::Open() socket(AF_ROUTE) error: %s\n",
                strerror(errno));  
        return false; 
    }
    else
    {
        pid = (UINT32)getpid();
        return true;
    }
}  // end BsdRouteMgr::Open()

void BsdRouteMgr::Close()
{
    if (IsOpen())
    {
        close(descriptor);
        descriptor = -1;
    }   
}  // end BsdRouteMgr::Close()

/*
#define ROUNDUP(a, size) (((a) & ((size)-1)) ?                          \
                                (1 + ((a) | ((size)-1)))                \
                                : (a))
                                
#define NEXT_SA(sa)   sa = (struct sockaddr *) ((caddr_t) (sa) + ((sa)->sa_len ?    \
                ROUNDUP((sa)->sa_len, sizeof(u_long))                   \
                : sizeof(u_long)));
*/   
        
// IMPORTANT NOTE:  These "oldie but goodie macros above are wrong on a 64-bit machine!!!
//                  The ones below do the right thing.
                     
#define ROUNDUP(a, size) (((a) & ((size)-1)) ?                          \
                                (1 + ((a) | ((size)-1)))                \
                                : (a))
                                
#define NEXT_SA(sa)   sa = (struct sockaddr *) ((caddr_t) (sa) + ((sa)->sa_len ?    \
                ROUNDUP((sa)->sa_len, sizeof(UINT32))                   \
                : sizeof(UINT32)));

bool BsdRouteMgr::GetAllRoutes(ProtoAddress::Type addrType,
                               ProtoRouteTable&   routeTable)
{
    int mib[6];
    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    
    switch (addrType)
    {
        case ProtoAddress::IPv4:
            mib[3] = PF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            mib[3] = PF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() unsupported addr family\n");
            return false;
    }
    mib[4] = NET_RT_DUMP;
    mib[5] = 0;
    
    char* buf;;
    size_t len;
    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
    {
        PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() sysctl() error: %s\n",
                strerror(errno));
        return false;   
    }   
    
    if (!(buf = new char[len]))
    {
        PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() malloc(%d) error: %s\n",
                len, strerror(errno));
        return false;
    }
    
    if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) 
    {
            delete[] buf;
            PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() sysctl() error: %s\n", strerror(errno));
            return false;
    }
    
    char* end = buf + len;
    char* next = buf;
    while (next < end)
    {
        // (void*) casts here to avoid cast-align mis-warning
        struct rt_msghdr* rtm = (struct rt_msghdr*)((void*)next);
        struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
        ProtoAddress dst, gw;
        dst.Invalidate();
        gw.Invalidate();
        int prefixLen = -1;
        for (int i = 0; i < RTAX_MAX; i++)
        {
            if (0 != (rtm->rtm_addrs & (0x01 << i)))
            {
                switch (i)
                {
                    case RTAX_DST:
                    {
                        dst.SetSockAddr(*addr);
                        //TRACE("RTAX_DST: %s\t\t", dst.GetHostString());
                        break;
                    }
                    case RTAX_GATEWAY:
                    {
                        gw.SetSockAddr(*addr);
                        //TRACE("RTAX_GWY: %s\n", gw.GetHostString());
                        break;
                    }
                    case RTAX_NETMASK:
                    {
                        const unsigned char* ptr = (const unsigned char*)(&addr->sa_data[2]);
                        if (NULL != ptr)
                        {
                            unsigned int maskSize = addr->sa_len ? (addr->sa_len - (int)(ptr - (unsigned char*)addr)) : 0;
                            prefixLen = 0;
                            for (unsigned int i = 0; i < maskSize; i++)
                            {
                                if (0xff == *ptr)
                                {
                                    prefixLen += 8;
                                    ptr++;   
                                }
                                else
                                {
                                    unsigned char bit = 0x80;
                                    while (0 != (bit & *ptr))
                                    {
                                        bit >>= 1;
                                        prefixLen += 1;  
                                    }
                                    break;
                                }   
                            }
                        }
                        //TRACE("BsdRouteMgr::GetAllRoutes() recvd RTA_NETMASK: %d\n", prefixLen);
                        break;
                    }
                    case RTAX_GENMASK:
                    {
                        PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() recvd RTA_GENMASK ...\n");
                        break;
                    }
                    default:
                    {
                        PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() recvd unhandled RTA: %d\n", i);
                        break;   
                    }
                }  // end switch(i)
                NEXT_SA(addr);
            }  // end if(mask[i] is set)
        }  // end for(i=0..RTAX_MAX)
        if (dst.IsValid())
        {
            bool setRoute = true;
#ifdef MACOSX
            // Don't fetch cloned routes (TBD - investigate further)
            if (0 != (rtm->rtm_flags & RTF_WASCLONED))
                setRoute = false;
#endif  // MACOSX            
            if (0 == prefixLen)
            {
                // This makes sure default routes w/ valid gateway "trump" device default route
                // (TBD - deal with multiple default route entries)
                if ((NULL != routeTable.GetDefaultEntry()) && !gw.IsValid())
                    setRoute = false;
            }
            if (prefixLen < 0) prefixLen = dst.GetLength() << 3;
            if (0 == (rtm->rtm_flags & RTF_GATEWAY)) gw.Invalidate();
            
            if (setRoute)
            {
                if (!routeTable.SetRoute(dst, prefixLen, gw, rtm->rtm_index))
                {
                    PLOG(PL_ERROR, "BsdRouteMgr::GetAllRoutes() error creating table entry\n");
                    delete[] buf;
                    return false;   
                }
            }
        }
        next += rtm->rtm_msglen;   
    }
    delete[] buf;  
    return true;
    
}  // end BsdRouteMgr::GetAllRoutes()

bool BsdRouteMgr::SetRoute(const ProtoAddress& dst, 
                           unsigned int        prefixLen,
                           const ProtoAddress& gw,
                           unsigned            ifIndex,
                           int                 /*metric*/)
{
    // Construct a RTM_ADD request
    int buffer[256];  // we use "int" for alignment safety   
    memset(buffer, 0, 256*sizeof(int));
    unsigned int sockaddrLen = 0;
    switch (dst.GetType())
    {
        case ProtoAddress::IPv4:
            sockaddrLen = sizeof(struct sockaddr_in);
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            sockaddrLen = sizeof(struct sockaddr_in6);
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() invalid dst address type!\n");
            return false; 
    }
    struct rt_msghdr* rtm = (struct rt_msghdr*)buffer;
    rtm->rtm_msglen = sizeof(struct rt_msghdr);
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type = RTM_ADD;
    rtm->rtm_addrs = RTA_DST;
    rtm->rtm_flags = RTF_UP;
        
    if (prefixLen < (unsigned int)(dst.GetLength() << 3))
    {
        // address bits past mask _must_ be ZERO, so we test for that here
        unsigned int index = prefixLen >> 3;
        const char* ptr = dst.GetRawHostAddress();
        if (0 != ((0x00ff >> (prefixLen & 0x07)) & ptr[index]))
        {
            PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() invalid address for given mask\n");
            return false;   
        }
        while (++index < dst.GetLength())
        {
            if (0 != ptr[index] )
            {
                PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() invalid address for given mask\n");
                return false;
            }  
        }
        rtm->rtm_addrs |= RTA_NETMASK;
#ifdef RTF_MASK
        rtm->rtm_flags |= RTF_MASK;
#endif // RTF_MASK
    }
    else
    {
        rtm->rtm_flags |= RTF_HOST;
    }
    
    if (gw.IsValid()) 
    {
        rtm->rtm_addrs |= RTA_GATEWAY;
        rtm->rtm_flags |= RTF_GATEWAY; 
    }
    else if (ifIndex != 0) 
    {
        rtm->rtm_addrs |= RTA_GATEWAY;
        rtm->rtm_index = ifIndex;
    }
    else
    {
        PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() invalid gateway address\n");
        return false;   
    }
    
    rtm->rtm_pid = pid;
    int seq = sequence++; 
    rtm->rtm_seq = seq;
    
    
    struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
    for (int i = 0; i < RTAX_MAX; i++)
    {
        if (0 != (rtm->rtm_addrs & (0x01 << i)))
        {
            switch (i)
            {
                case RTAX_DST:
                    //TRACE("Adding RTAX_DST...\n");
                    memcpy(addr, &dst.GetSockAddr(), sockaddrLen);
                    rtm->rtm_msglen += sockaddrLen;
                    break;
                case RTAX_GATEWAY:
                    if (0 != (rtm->rtm_flags & RTF_GATEWAY))
                    {
                        //TRACE("Adding RTAX_GATEWAY...\n");
                        memcpy(addr, &gw.GetSockAddr(), sockaddrLen);
                        rtm->rtm_msglen += sockaddrLen;
                    }
                    else
                    {
                        //TRACE("Adding RTAX_GATEWAY (IF)...\n");
                        struct sockaddr_dl* sdl = (struct sockaddr_dl*)((void*)addr);
                        sdl->sdl_len = sizeof(struct sockaddr_dl);
                        sdl->sdl_family = AF_LINK;
                        sdl->sdl_index = ifIndex;
                        sdl->sdl_type = 0;
                        sdl->sdl_nlen = 0;
                        sdl->sdl_alen = 0;
                        sdl->sdl_slen = 0;
                        rtm->rtm_msglen += sizeof(struct sockaddr_dl);
                    }
                    break;  
                case RTAX_NETMASK:
                {
                    if (prefixLen > 0)
                    {
                        unsigned char* ptr = (unsigned char*)(&addr->sa_data[2]);
                        unsigned int numBytes = prefixLen >> 3;
                        memset(ptr, 0xff, numBytes);
                        unsigned int remainder = prefixLen & 0x07;
                        if (remainder)
                        {
                            ptr[numBytes] = 0xff << (8 - remainder);
                            addr->sa_len = 5 + numBytes;
                        }
                        else
                        {
                            addr->sa_len = (ptr - (unsigned char*)addr) + numBytes;
                        }
                        rtm->rtm_msglen += ROUNDUP(addr->sa_len, sizeof(u_long));
                    }
                    else
                    {
                        rtm->rtm_msglen += sizeof(u_long);
                        addr->sa_len = 4;
                    }
                    break;
                }
                default:
                    break;
            }  // end switch(i)
            NEXT_SA(addr);
        }  // end if (rtm_addrs[i])
    }  // end for(i=0..RTAX_MAX)
    
    
    // Send RTM_ADD request to routing socket
    int result = send(descriptor, rtm, rtm->rtm_msglen, 0);
    if ((result < 0) && (EEXIST == errno))
    {
        // route already exists, so "change" it
        rtm->rtm_type = RTM_CHANGE;
        // Send RTM_CHANGE request to routing socket
        result = send(descriptor, rtm, rtm->rtm_msglen, 0);   
    }
    if ((int)rtm->rtm_msglen != result)
    {
        PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() send() error: %s\n",
                strerror(errno));
        return false;   
    } 
    
    // Recv the result
    while (1)
    {
        ProtoAddress destination, gateway;
        destination.Invalidate();
        gateway.Invalidate();
        int msgLen = recv(descriptor, rtm, 256*sizeof(int), 0); 
        if (msgLen < 0)
        {
            if (errno != EINTR)
            {
                PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() recv() error: %s\n", strerror(errno)); 
                return false;  
            }
            else
            {
                continue;
            }
        }
        if ((seq == rtm->rtm_seq) &&
            (pid == rtm->rtm_pid))
        {
            TRACE("matching response ...\n");
            struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
            for (int i = 0; i < RTAX_MAX; i++)
            {
                if (0 != (rtm->rtm_addrs & (0x01 << i)))
                {
                    switch (i)
                    {
                        case RTAX_DST:
                        {
                            destination.SetSockAddr(*addr);
                            //TRACE("RTAX_DST: %s\t\t", dst.GetHostString());
                            break;
                        }
                        case RTAX_GATEWAY:
                        {
                            gateway.SetSockAddr(*addr);
                            //TRACE("RTAX_GWY: %s\n", gw.GetHostString());
                            break;
                        }
                        case RTAX_NETMASK:
                        {
                            const unsigned char* ptr = (const unsigned char*)(&addr->sa_data[2]);
                            if (ptr)
                            {
                                unsigned int maskSize = addr->sa_len ? (addr->sa_len - (int)(ptr - (unsigned char*)addr)) : 0;
                                prefixLen = 0;
                                for (unsigned int i = 0; i < maskSize; i++)
                                {
                                    if (0xff == *ptr)
                                    {
                                        prefixLen += 8;
                                        ptr++;   
                                    }
                                    else
                                    {
                                        unsigned char bit = 0x80;
                                        while (0 != (bit & *ptr))
                                        {
                                            bit >>= 1;
                                            prefixLen += 1;   
                                        }
                                    }   
                                }
                            }
                            //TRACE("RTAX_NMSK: %d\t\t", prefixLen);
                            break;
                        }
                        case RTAX_GENMASK:
                        {
                            TRACE("BsdRouteMgr::SetRoute() recvd RTA_GENMASK ...\n");
                            break;
                        }
                        default:
                        {
                            TRACE("BsdRouteMgr::SetRoute() recvd unhandled RTA: %d\n", i);
                            break;   
                        }
                    }  // end switch(i)
                    NEXT_SA(addr);
                }  // end if(mask(i) is set)
            }  // end for (i=0..RTAX_MAX)
            if (0 != (rtm->rtm_flags & RTF_DONE))
            {
                if (destination.IsValid())
                {
                    //TRACE("BsdRouteMgr::SetRoute() successfully added route\n");   
                    return true;
                }
                else
                {
                    PLOG(PL_ERROR, "BsdRouteMgr::SetRoute() completed with invalid dst\n");
                    return false;
                } 
            }
        }
        else
        {
            TRACE("BsdRouteMgr::SetRoute() recvd non-matching response\n");   
        }
    }  // end while (1)
    return false;
}  // end BsdRouteMgr::SetRoute()

// Currently deletes _all_ routes regardless of gateway or index
bool BsdRouteMgr::DeleteRoute(const ProtoAddress& dst, 
                              unsigned int        prefixLen,
                              const ProtoAddress& /*gateway*/,
                              unsigned int        ifIndex)
{
    ProtoAddress gw;
    int metric;
    while (GetRoute(dst, prefixLen, gw, ifIndex, metric))
    {
    // Construct a RTM_DELETE request
    int buffer[256];  // we use "int" for alignment safety   
    memset(buffer, 0, 256*sizeof(int));
    unsigned int sockaddrLen = 0;
    switch (dst.GetType())
    {
        case ProtoAddress::IPv4:
            sockaddrLen = sizeof(struct sockaddr_in);
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            sockaddrLen = sizeof(struct sockaddr_in6);
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "BsdRouteMgr::DeleteRoute() invalid dst address type!\n");
            return false; 
    }
    struct rt_msghdr* rtm = (struct rt_msghdr*)buffer;
    rtm->rtm_msglen = sizeof(struct rt_msghdr);
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type = RTM_DELETE;
    rtm->rtm_addrs = RTA_DST;
    rtm->rtm_flags = 0;
        
    if (prefixLen < (unsigned int)(dst.GetLength() << 3))
    {
        // address bits past mask _must_ be ZERO, so we test for that here
        unsigned int index = prefixLen >> 3;
        const char* ptr = dst.GetRawHostAddress();
        if (0 != ((0x00ff >> (prefixLen & 0x07)) & ptr[index]))
        {
            PLOG(PL_ERROR, "BsdRouteMgr::DeleteRoute() invalid address for given mask\n");
            return false;   
        }
        while (++index < dst.GetLength())
        {
            if (0 != ptr[index] )
            {
                PLOG(PL_ERROR, "BsdRouteMgr::DeleteRoute() invalid address for given mask\n");
                return false;
            }  
        }
        rtm->rtm_addrs |= RTA_NETMASK;
    }
    else
    {
        rtm->rtm_flags |= RTF_HOST;
    }
    
    // (TBD) IPv6 host flags???    
    if (gw.IsValid()) 
    {
        rtm->rtm_addrs |= RTA_GATEWAY;
        rtm->rtm_flags |= RTF_GATEWAY; 
    }
    else if (ifIndex > 0)
    {
        rtm->rtm_addrs |= RTA_GATEWAY;
    }
    
    rtm->rtm_pid = pid;
    int seq = sequence++; 
    rtm->rtm_seq = seq;
    
    struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
    for (int i = 0; i < RTAX_MAX; i++)
    {
        if (0 != (rtm->rtm_addrs & (0x01 << i)))
        {
            switch (i)
            {
                case RTAX_DST:
                    memcpy(addr, &dst.GetSockAddr(), sockaddrLen);
                    rtm->rtm_msglen += sockaddrLen;
                    break;
                case RTAX_GATEWAY:
                    if (0 != (rtm->rtm_flags & RTF_GATEWAY))
                    {
                        //TRACE("Adding RTAX_GATEWAY...\n");
                        memcpy(addr, &gw.GetSockAddr(), sockaddrLen);
                        rtm->rtm_msglen += sockaddrLen;
                    }
                    else
                    {
                        //TRACE("Adding RTAX_GATEWAY (IF)...\n");
                        struct sockaddr_dl* sdl = (struct sockaddr_dl*)((void*)addr);
                        sdl->sdl_len = sizeof(struct sockaddr_dl);
                        sdl->sdl_family = AF_LINK;
                        sdl->sdl_index = ifIndex;
                        sdl->sdl_type = 0;
                        sdl->sdl_nlen = 0;
                        sdl->sdl_alen = 0;
                        sdl->sdl_slen = 0;
                        rtm->rtm_msglen += sizeof(struct sockaddr_dl);
                    }
                    break;  
                case RTAX_NETMASK:
                {
                    unsigned char* ptr = (unsigned char*)(&addr->sa_data[2]);
                    if (prefixLen > 0)
                    {
                        unsigned int numBytes = prefixLen >> 3;
                        memset(ptr, 0xff, numBytes);
                        unsigned int remainder = prefixLen & 0x07;
                        if (remainder)
                        {
                            ptr[numBytes] = 0xff << (8 - remainder);
                            addr->sa_len = 5 + numBytes;
                        }
                        else
                        {
                            addr->sa_len = (ptr - (unsigned char*)addr) + numBytes;
                        }
                        rtm->rtm_msglen += ROUNDUP(addr->sa_len, sizeof(u_long));
                    }
                    else
                    {
                        rtm->rtm_msglen += sizeof(u_long);
                        addr->sa_len = 4;
                        
                    }
                    break;
                }
                default:
                    break;
            }  // end switch(i)
            NEXT_SA(addr);
        }  // end if (rtm_addrs[i])
    }  // end for(i=0..RTAX_MAX)
    
    // Send RTM_ADD request to netlink socket
    int result = send(descriptor, rtm, rtm->rtm_msglen, 0);
    if ((result < 0) && (EEXIST == errno))
    {
        // route already exists, so "change" it
        rtm->rtm_type = RTM_CHANGE;
        // Send RTM_CHANGE request to netlink socket
        result = send(descriptor, rtm, rtm->rtm_msglen, 0);   
    }
    if ((int)rtm->rtm_msglen != result)
    {
        // This will occur when there is no matching route to delete
        PLOG(PL_WARN, "BsdRouteMgr::DeleteRoute() send() warning: %s\n",
                strerror(errno));
        return false;   
    } 
    
    // Recv the result
    bool complete = false;
    while (!complete)
    {
        ProtoAddress destination, gateway, genmask;
        destination.Invalidate();
        gateway.Invalidate();
        genmask.Invalidate();
        int msgLen = recv(descriptor, rtm, 256*sizeof(int), 0); 
        if (msgLen < 0)
        {
            if (errno != EINTR)
            {
                PLOG(PL_ERROR, "BsdRouteMgr::DeleteRoute() recv() error: %s\n", strerror(errno)); 
                return false;  
            }
            else
            {
                continue;
            }
        }
        if ((seq == rtm->rtm_seq) &&
            (pid == rtm->rtm_pid))
        {
            struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
            for (int i = 0; i < RTAX_MAX; i++)
            {
                if (0 != (rtm->rtm_addrs & (0x01 << i)))
                {
                    switch (i)
                    {
                        case RTAX_DST:
                        {
                            destination.SetSockAddr(*addr);
                            //TRACE("RTAX_DST: %s\t\t", destination.GetHostString());
                            break;
                        }
                        case RTAX_GATEWAY:
                        {
                            gateway.SetSockAddr(*addr);
                            //TRACE("RTAX_GWY: %s\n", gateway.GetHostString());
                            break;
                        }
                        case RTAX_NETMASK:
                        {
                            const unsigned char* ptr = (const unsigned char*)(&addr->sa_data[2]);
                            if (ptr)
                            {
                                unsigned int maskSize = addr->sa_len ? (addr->sa_len - (int)(ptr - (unsigned char*)addr)) : 0;
                                prefixLen = 0;
                                for (unsigned int i = 0; i < maskSize; i++)
                                {
                                    if (0xff == *ptr)
                                    {
                                        prefixLen += 8;
                                        ptr++;   
                                    }
                                    else
                                    {
                                        unsigned char bit = 0x80;
                                        while (0 != (bit & *ptr))
                                        {
                                            bit >>= 1;
                                            prefixLen += 1;   
                                        }
                                    }   
                                }
                            }
                            //TRACE("RTAX_NMSK: %d\t\t", prefixLen);
                            break;
                        }
                        case RTAX_GENMASK:
                        {
                            TRACE("BsdRouteMgr::DeleteRoute() recvd RTA_GENMASK ...\n");
                            break;
                        }
                        default:
                        {
                            TRACE("BsdRouteMgr::DeleteRoute() recvd unhandled RTAX: %d\n", i);
                            break;   
                        }
                    }  // end switch(i)
                    NEXT_SA(addr);
                }  // end if(mask(i) is set)
            }  // end for (i=0..RTAX_MAX)
            if (0 != (rtm->rtm_flags & RTF_DONE))
            {
                if (destination.IsValid())
                {
                    complete = true;
                    break;
                }
                else
                {
                    PLOG(PL_ERROR, "BsdRouteMgr::DeleteRoute() completed with invalid dst\n");
                    return false;
                } 
            }
        }
        else
        {
            TRACE("BsdRouteMgr::DeleteRoute() recvd non-matching response\n");   
        }  // if/else (matchingResponse)
    }  // end while (!complete)
    }
    return true;
}  // end BsdRouteMgr::DeleteRoute()

bool BsdRouteMgr::GetRoute(const ProtoAddress& dst,
                           unsigned int        prefixLen, 
                           ProtoAddress&       gw,
                           unsigned int&       ifIndex,
                           int&                metric)
{
    // Init return values
    gw.Invalidate();
    ifIndex = 0; 
    
    // Construct a RTM_GET request
    int buffer[256];  // we use "int" for alignment safety   
    memset(buffer, 0, 256*sizeof(int));
    unsigned int sockaddrLen = 0;
    switch (dst.GetType())
    {
        case ProtoAddress::IPv4:
            sockaddrLen = sizeof(struct sockaddr_in);
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            sockaddrLen = sizeof(struct sockaddr_in6);
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "BsdRouteMgr::GetRoute() invalid dst address type!\n");
            return false; 
    }
    struct rt_msghdr* rtm = (struct rt_msghdr*)buffer;
    rtm->rtm_msglen = sizeof(struct rt_msghdr);
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type = RTM_GET;
    rtm->rtm_addrs = RTA_DST;
    rtm->rtm_flags = 0;
    
    if (prefixLen < (unsigned int)(dst.GetLength() << 3))
    {
        // address bits past mask _must_ be ZERO, so we test for that here
        unsigned int index = prefixLen >> 3;
        const char* ptr = dst.GetRawHostAddress();
        if (0 != ((0x00ff >> (prefixLen & 0x07)) & ptr[index]))
        {
            PLOG(PL_ERROR, "BsdRouteMgr::GetRoute() invalid address for given mask\n");
            return false;   
        }
        while (++index < dst.GetLength())
        {
            if (0 != ptr[index] )
            {
                PLOG(PL_ERROR, "BsdRouteMgr::GetRoute() invalid address for given mask\n");
                return false;
            }  
        }
        rtm->rtm_addrs |= RTA_NETMASK;
    }
    else
    {
        rtm->rtm_flags |= RTF_HOST;
    }
    
    rtm->rtm_pid = pid;
    int seq = sequence++; 
    rtm->rtm_seq = seq;
    
    struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
    for (int i = 0; i < RTAX_MAX; i++)
    {
        if (0 != (rtm->rtm_addrs & (0x01 << i)))
        {
            switch (i)
            {
                case RTAX_DST:
                    memcpy(addr, &dst.GetSockAddr(), sockaddrLen);
                    rtm->rtm_msglen += sockaddrLen;
                    break;
                case RTAX_NETMASK:
                {
                    unsigned char* ptr = (unsigned char*)(&addr->sa_data[2]);
                    if (prefixLen > 0)
                    {
                        unsigned int numBytes = prefixLen >> 3;
                        memset(ptr, 0xff, numBytes);
                        unsigned int remainder = prefixLen & 0x07;
                        if (remainder)
                        {
                            ptr[numBytes] = 0xff << (8 - remainder);
                            addr->sa_len = 5 + numBytes;
                        }
                        else
                        {
                            addr->sa_len = (ptr - (unsigned char*)addr) + numBytes;
                        }
                        rtm->rtm_msglen += ROUNDUP(addr->sa_len, sizeof(u_long));
                    }
                    else
                    {
                        rtm->rtm_msglen += sizeof(u_long);
                        addr->sa_len = 4;
                        
                    }
                    break;
                }
                default:
                    break;
            }  // end switch(i)
            NEXT_SA(addr);
        }  // end if (rtm_addrs[i])
    }  // end for(i=0..RTAX_MAX)
        
    // Send request to netlink socket
    int result = send(descriptor, rtm, rtm->rtm_msglen, 0);
    if ((int)rtm->rtm_msglen != result)
    {
        PLOG(PL_ERROR, "BsdRouteMgr::GetRoute() send() error: %s\n", strerror(errno));
        return false;   
    }  
    
    // Recv the result
    while (1)
    {
        int msgLen = recv(descriptor, rtm, 256*sizeof(int), 0); 
        if (msgLen < 0)
        {
            if (errno != EINTR)
            {
                PLOG(PL_ERROR, "BsdRouteMgr::GetRoute() recv() error: %s\n", strerror(errno)); 
                return false;  
            }
            else
            {
                continue;
            }
        }
        if ((RTM_GET == rtm->rtm_type) &&
            (seq == rtm->rtm_seq) &&
            (pid == rtm->rtm_pid))
        {
            struct sockaddr* addr = (struct sockaddr*)(rtm + 1);
            ProtoAddress destination;
            destination.Invalidate();
            for (int i = 0; i < RTAX_MAX; i++)
            {
                if (0 != (rtm->rtm_addrs & (0x01 << i)))
                {
                    switch (i)
                    {
                        case RTAX_DST:
                        {
                            destination.SetSockAddr(*addr);
                            //TRACE("RTAX_DST: %s\t\t", dst.GetHostString());
                            break;
                        }
                        case RTAX_GATEWAY:
                        {
                            switch (addr->sa_family)
                            {   
#ifdef HAVE_IPV6
                                case AF_INET6:                  
#endif // HAVE_IPV6  
                                case AF_INET:
                                    gw.SetSockAddr(*addr);
                                    //TRACE("RTAX_GWY: %s\n", gw.GetHostString());
                                    break;
                                case AF_LINK:
                                    ifIndex = ((struct sockaddr_dl*)((void*)addr))->sdl_index;
                                    //TRACE("RTAX_GWY: ifIndex:%d\n", ifIndex);
                                    break;
                                default:
                                    PLOG(PL_ERROR, "BsdRouteMgr::GetRoute() recvd unknown sa_family\n");
                                    break;
                            }
                            break;
                        }
                        case RTAX_NETMASK:
                        {
                            const unsigned char* ptr = (const unsigned char*)(&addr->sa_data[2]);
                            if (ptr)
                            {
                                unsigned int maskSize = addr->sa_len ? (addr->sa_len - (int)(ptr - (unsigned char*)addr)) : 0;
                                unsigned int prefixLen = 0;
                                for (unsigned int i = 0; i < maskSize; i++)
                                {
                                    if (0xff == *ptr)
                                    {
                                        prefixLen += 8;
                                        ptr++;   
                                    }
                                    else
                                    {
                                        unsigned char bit = 0x80;
                                        while (0 != (bit & *ptr))
                                        {
                                            bit >>= 1;
                                            prefixLen += 1;   
                                        }
                                    }
                                    break;   
                                }
                                //TRACE("BsdRouteMgr::GetRoute() recvd RTAX_NMSK: %d\n", prefixLen);
                            }
                            break;
                        }
                        case RTAX_GENMASK:
                        {
                            TRACE("BsdRouteMgr::GetRoute() recvd RTA_GENMASK ...\n");
                            break;
                        }
                        default:
                        {
                            TRACE("BsdRouteMgr::GetRoute() recvd unhandled RTA: %d\n", i);
                            break;   
                        }
                    }  // end switch(i)
                    NEXT_SA(addr);
                }  // end if (mask[i] is set)
            }  // end for(i=0..RTAX_MAX)
            if (0 != (rtm->rtm_flags & RTF_DONE))
            {
                if (destination.IsValid())
                {
                    //if (!gw.IsValid()) gw.Reset(destination.GetType());
                    //if (prefixLen < 0) prefixLen = dst.GetLength() << 3;
                    // (TBD) get actual metric
                    metric = -1;
                    return true;
                }
                else
                {
                    return false;   
                }
            }
        }
        else
        {
            TRACE("BsdRouteMgr::GetRoute() recvd non-matching response\n");   
        }
    }  // end while (1)
    return false;
}  // end BsdRouteMgr::GetRoute()

bool BsdRouteMgr::GetInterfaceAddressList(unsigned int        ifIndex, 
                                          ProtoAddress::Type  addrType,
                                          ProtoAddressList&   addrList)
{
    ProtoAddressList localAddrList;  // keep link & site local addrs separate and add at end
    // Init return values
    int mib[6];
    mib[0] = CTL_NET;
    mib[1] = AF_ROUTE;
    mib[2] = 0;
    
    switch (addrType)
    {
        case ProtoAddress::IPv4:
            mib[3] = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            mib[3] = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() unsupported addr family\n");
            break;
    }
    mib[4] = NET_RT_IFLIST;
    mib[5] = ifIndex;
    
    char* buf;;
    size_t len;
    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
    {
        PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() sysctl() error: %s\n",
                strerror(errno));
        return false;   
    }   
    
    if (!(buf = new char[len]))
    {
        PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() malloc(%d) error: %s\n",
                len, strerror(errno));
        return false;
    }
        
    if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) 
    {
        delete[] buf;
        PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() sysctl() error: %s\n", strerror(errno));
        return false;
    }
    
    char* end = buf + len;
    char* next = buf;
    while (next < end)
    {
        struct if_msghdr* ifm = (struct if_msghdr*)((void*)next);
        switch (ifm->ifm_type)
        {
            case RTM_IFINFO:
            {
                // TRACE("received RTM_IFINFO message ...\n");
                // (this can give us the the link layer addr)
                break;   
            }
            case RTM_NEWADDR:
            {
                //TRACE("received RTM_NEWADDR message ...\n");
                struct ifa_msghdr* ifam = (struct ifa_msghdr*)ifm;
                struct sockaddr* addr = (struct sockaddr*)(ifam + 1);
                for (int i = 0; i < RTAX_MAX; i++)
                {
                    if (0 != (ifam->ifam_addrs & (0x01 << i)))
                    {
                        switch (i)
                        {  
                            case RTAX_IFA:
                                //TRACE("received RTAX_IFA ...index:%d\n", ifam->ifam_index);
                                if (ifIndex == ifam->ifam_index)
                                {
                                    ProtoAddress addrTemp;
                                    addrTemp.SetSockAddr(*addr);
                                    if (addrTemp.IsValid())
                                    {
                                        if (addrTemp.IsLinkLocal() || addrTemp.IsSiteLocal())
                                        {
                                            if (!localAddrList.Insert(addrTemp))
                                                PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() error: unable to add addr to local list\n");
                                        }
                                        else
                                        {
                                            if (!addrList.Insert(addrTemp))
                                                PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() error: unable to add addr to list\n");
                                        }   
                                    }
                                }
                                break;
                            case RTAX_BRD:
                                //TRACE("received RTAX_BRD ...\n");
                                break;    
                            case RTAX_NETMASK:
                                //TRACE("received RTAX_NETMASK ...\n");
                                break;       
                            default:
                                //TRACE("unhandled RTAX type:%d\n", i);
                                break;
                        }
                        NEXT_SA(addr);
                    }
                }  // end for(i=0..RTAX_MAX)
                break;   
            }
            default:
                PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() warning: unhandled IFM message type:%d\n", ifm->ifm_type);
                break;
            
        }
        next += ifm->ifm_msglen;   
    }
    delete[] buf;   
    
    ProtoAddressList::Iterator iterator(localAddrList);
    ProtoAddress localAddr;
    while (iterator.GetNextAddress(localAddr))
    {
        if (!addrList.Insert(localAddr))
            PLOG(PL_ERROR, "BsdRouteMgr::GetInterfaceAddressList() error: unable to add local addr to list\n");
    }         
    if (addrList.IsEmpty())
        PLOG(PL_WARN, "BsdRouteMgr::GetInterfaceAddressList() warning: no addresses found\n");
    return true;
}  // end BsdRouteMgr::GetInterfaceAddressList()

