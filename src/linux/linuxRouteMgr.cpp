#include <stdio.h>
#include "protoRouteMgr.h"
#include "protoDebug.h"
#include "unix/zebraRouteMgr.h"

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

class LinuxRouteMgr : public ProtoRouteMgr
{
    public:
        LinuxRouteMgr();
        ~LinuxRouteMgr();
        
        virtual bool Open(const void* userData = NULL);
        virtual bool IsOpen() const {return (descriptor >= 0);}
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
        static bool NetlinkAddAttr(struct nlmsghdr* msg, 
                                   unsigned int     maxLen, 
                                   int              type, 
                                   const void*      data, 
                                   int              len);
        bool NetlinkCheckResponse(UINT32 seq);
                    
        int     descriptor;
        UINT32  port_id;  // netlink port id
        UINT32  sequence; // netlink request/response sequence number
        
};  // end class LinuxRouteMgr

ProtoRouteMgr* ProtoRouteMgr::Create(Type theType)
{
    // TBD - support alternative route mgr "types" (e.g. ZEBRA)
    ProtoRouteMgr* returnMgr = NULL;
    switch(theType)
    {
        case ZEBRA:
          returnMgr = (ProtoRouteMgr*)(new ZebraRouteMgr);
          break;
        case SYSTEM:
          returnMgr = (ProtoRouteMgr*)(new LinuxRouteMgr);
          break;
        default:
          return NULL;
    }
    return returnMgr;
}  // end ProtoRouteMgr::Create()

LinuxRouteMgr::LinuxRouteMgr()
 : descriptor(-1), port_id(0), sequence(0)
{
}

LinuxRouteMgr::~LinuxRouteMgr()
{
    Close();
}

bool LinuxRouteMgr::SetForwarding(bool state)
{
    int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
    if (fd < 0)
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::SetForwarding() open(/proc/sys) error: %s\n",
                strerror(errno));
        return false;   
    }
    char value = state ? '1' : '0';
    if (1 == write(fd, &value, 1))
    {
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::SetForwarding() write(/proc/sys) error: %s\n",
            strerror(errno));
        return false;      
    }
}  // end LinuxRouteMgr::SetForwarding()

bool LinuxRouteMgr::Open(const void* /*userData*/)
{
    if (IsOpen()) Close();
    // (TBD) We may need to dictate NETLINK_ROUTE6 for IPv6 ops ???
    if ((descriptor = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0)
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::Open() socket(NETLINK_ROUTE) error: %s\n",
                strerror(errno));  
        return false; 
    }
    else
    {
        // Here we use "bind()" to get a unique netlink port id (pid) from the kernel
        // (with localAddr.nl_pid passed into bind() set as '0', kernel assigns us one
        struct sockaddr_nl localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.nl_family = AF_NETLINK;
	    if (0 > bind(descriptor, (struct sockaddr*) &localAddr, sizeof(localAddr)))
        {
            PLOG(PL_ERROR, "ProtoNetlink::Open() bind() error: %s\n", GetErrorString());
            Close();
            return false;
        }
        // Get socket name so we know our port number (i.e. netlink pid)
        socklen_t addrLen = sizeof(localAddr);
        if (getsockname(descriptor, (struct sockaddr*)&localAddr, &addrLen) < 0) 
        {    
            PLOG(PL_ERROR, "ProtoNetlink::Open()  getsockname() error: %s\n", GetErrorString());
            Close();
            return false;
        }
        port_id = localAddr.nl_pid;
        return true;
    }
}  // end LinuxRouteMgr::Open()

void LinuxRouteMgr::Close()
{
    if (IsOpen())
    {
        close(descriptor);
        descriptor = -1;
    }   
}  // end LinuxRouteMgr::Close()

// Add an "attribute" (field) to netlink msg payload
bool LinuxRouteMgr::NetlinkAddAttr(struct nlmsghdr*    msg, 
                                   unsigned int        maxLen, 
                                   int                 type, 
                                   const void*         data, 
                                   int                 len)
{
    int rtaLen = RTA_LENGTH(len);
    if ((NLMSG_ALIGN(msg->nlmsg_len) + rtaLen) > maxLen) return false;
    struct rtattr* rta = (struct rtattr*)(((char*)msg) + NLMSG_ALIGN(msg->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = rtaLen;
    memcpy(RTA_DATA(rta), data, len);
    msg->nlmsg_len = NLMSG_ALIGN(msg->nlmsg_len) + rtaLen;
    return true;
}  // end LinuxRouteMgr::NetlinkAddAttr()

bool LinuxRouteMgr::NetlinkCheckResponse(UINT32 seq)
{
    while (1)
    {
        char buffer[1024];
        int msgLen = recv(descriptor, buffer, 1024, 0); 
        if (msgLen < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::NetlinkCheckResponse() recv() error: %s\n", strerror(errno)); 
                return false;  
            }
        } 
        struct nlmsghdr* msg = (struct nlmsghdr*)buffer;
        for (; 0 != NLMSG_OK(msg, (unsigned int)msgLen); msg = NLMSG_NEXT(msg, msgLen))
        {
// Justin Below is a semi-hack to get SMF to function correctly in core namespaces.  
// If when the msg->nlmsg_pid gets fixed this ifdef can be removed
#ifndef CORE_NAMESPACES
          if ((msg->nlmsg_pid == port_id) && (msg->nlmsg_seq == seq))
#else
    //DMSG(0,"J Namspaces comment in NetlinkCheckResponse\n");
          if (msg->nlmsg_seq == seq)
#endif // if/else !CORE_NAMESPACES
            {
                switch(msg->nlmsg_type)
                {
                    case NLMSG_ERROR:
                    {
                        struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                        if (0 != errorMsg->error)
                        {
                            PLOG(PL_DEBUG, "LinuxRouteMgr::NetlinkCheckResponse() recvd NLMSG_ERROR "
                                           "error seq:%d code:%d...\n", msg->nlmsg_seq, errorMsg->error);
                            return false;
                        }
                        else
                        {
                            return true;
                        }
                        break;
                    }   
                    default:
                        PLOG(PL_ERROR, "LinuxRouteMgr::NetlinkCheckResponse() recvd unexpected "
                                       "matching message type:%d\n", msg->nlmsg_type);
                        // Assume success ???
                        return true;
                        break;
                }
            }
        }
    }  // end while (1)
}  // end LinuxRouteMgr::NetlinkCheckResponse()

bool LinuxRouteMgr::SetRoute(const ProtoAddress&   dst,
                             unsigned int          prefixLen,
                             const ProtoAddress&   gw,
                             unsigned int          ifIndex,
                             int                   metric)
{
    // First, delete any pre-existing route(s) to this destination
    // (TBD) try to do a "make before break" routing change
    PLOG(PL_DEBUG, "LinuxRouteMgr::SetRoute() setting route to %s/%d via ",
            dst.GetHostString(), prefixLen);
    if (gw.IsValid())
        PLOG(PL_DEBUG, "gateway:%s\n", gw.GetHostString());
    else
        PLOG(PL_DEBUG, "direct if:%d\n", ifIndex);
    if (!DeleteRoute(dst, prefixLen, gw, ifIndex))
    {    
        // Limited to PL_DEBUG since route may _not_ pre-exist.  TBD - improve this by checking first???
        PLOG(PL_DEBUG, "LinuxRouteMgr::SetRoute() error deleting _possible_ pre-existing route to %s/%d\n",
                dst.GetHostString(), prefixLen);
    }    
    
    struct
    {
        struct nlmsghdr msg;
        struct rtmsg    rt;
        char            buf[1024];
    } req;
    memset(&req, 0, sizeof(req));
    
    // netlink message header
    req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.msg.nlmsg_type = RTM_NEWROUTE;
    req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE  | NLM_F_ACK;
    UINT32 seq = sequence++;
    req.msg.nlmsg_seq = seq; 
    req.msg.nlmsg_pid = port_id;
    
    // route add request
    switch (dst.GetType())
    {
        case ProtoAddress::IPv4:
            req.rt.rtm_family = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            req.rt.rtm_family = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() invalid destination address!\n");
            return false;
            break;
    }
    
    unsigned int addrBits = dst.GetLength() << 3;
    req.rt.rtm_dst_len = addrBits;
    req.rt.rtm_src_len = 0;
    req.rt.rtm_table = RT_TABLE_MAIN;
    req.rt.rtm_protocol = RTPROT_BOOT;
    if (gw.IsValid())
        req.rt.rtm_scope = RT_SCOPE_UNIVERSE; 
    else
        req.rt.rtm_scope = RT_SCOPE_LINK;
    if (dst.IsMulticast()) 
        req.rt.rtm_type = RTN_MULTICAST;
    else
        req.rt.rtm_type = RTN_UNICAST;
    
    req.rt.rtm_flags = 0;
    
    // Set destination rtattr
    if (dst.IsValid())
    {
        
        if (prefixLen < addrBits)
        {
            // address bits past mask _must_ be ZERO, so we test for that here
            unsigned int index = prefixLen >> 3;
            const char* bytes = dst.GetRawHostAddress();
            if (0 != ((0x00ff >> (prefixLen & 0x07)) & bytes[index]))
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() invalid address for given mask\n");
                return false;   
            }
            while (++index < dst.GetLength())
            {
                if (0 != bytes[index] )
                {
                    PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() invalid address for given mask\n");
                    return false;
                }  
            }     
            req.rt.rtm_dst_len = prefixLen;       
        }
        else if (prefixLen > addrBits)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() invalid mask.\n");
            return false;
        }
        if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_DST, dst.GetRawHostAddress(), dst.GetLength()))
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error setting RTA_DST attr.\n");
            return false;
        }
    }
    else
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error: invalid destination address\n");
        return false;
    }
    
    // Set gateway rtattr
    if (gw.IsValid())
    {
        if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_GATEWAY, gw.GetRawHostAddress(), gw.GetLength()))
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error adding RTA_GATEWAY attr.\n");
            return false;
        }
    }
    else if (ifIndex == 0)
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error: invalid gateway address\n");
        return false;   
    }
    
    if (ifIndex != 0)
    {
        UINT32 value = ifIndex;
        if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_OIF, &value, sizeof(UINT32)))
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error adding RTA_OIF attr.\n");
            return false;
        }  
    }
    
    // Set default route metric values
    if (metric < 0)
    {
        if (gw.IsValid())
            metric = 2;
        else
            metric = 1;
    }
    
    if (metric >= 0)
    {
        UINT32 value = metric; 
        if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_PRIORITY, &value, sizeof(UINT32)))
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error adding RTA_PRIORITY attr.\n");
            return false;
        }  
    }
    
    // Send request to netlink socket
    int result = send(descriptor, &req, req.msg.nlmsg_len, 0);
    if ((int)req.msg.nlmsg_len != result)
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() send() error: %s\n",
                strerror(errno));
        return false;   
    } 
     
    // Check response for error code    
    if (NetlinkCheckResponse(seq))
    {
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::SetRoute() error setting route\n");
        return false;
    }    
}  // end LinuxRouteMgr::SetRoute()

bool LinuxRouteMgr::DeleteRoute(const ProtoAddress& dst,
                                unsigned int        prefixLen,
                                const ProtoAddress& /*gateway*/,
                                unsigned int        ifIndex)
{
    ProtoAddress gw;
    unsigned int debugCount = 1;
    PLOG(PL_DEBUG, "LinuxRouteMgr::DeleteRoute() %u) getting route(s) to dst>%s/%d ",
            debugCount++, dst.GetHostString(), prefixLen);
    int metric;
    while (GetRoute(dst, prefixLen, gw, ifIndex, metric))
    {   
        PLOG(PL_DEBUG, "LinuxRouteMgr::DeleteRoute() %u) deleting dst>%s/%d ",
                debugCount++, dst.GetHostString(), prefixLen);
        if (gw.IsValid())
            PLOG(PL_DEBUG, "gw>%s (idx>%d)\n", gw.GetHostString(), ifIndex);
        else
            PLOG(PL_DEBUG, "direct idx>%d\n", ifIndex);      
        
        struct
        {
            struct nlmsghdr msg;
            struct rtmsg    rt;
            char            buf[1024];
        } req;
        memset(&req, 0, sizeof(req));

        // netlink message header
        req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
        req.msg.nlmsg_type = RTM_DELROUTE;
        req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_MATCH;
        UINT32 seq = sequence++;
        req.msg.nlmsg_seq = seq; 
        req.msg.nlmsg_pid = port_id;

        // route delete request
        switch (dst.GetType())
        {
            case ProtoAddress::IPv4:
                req.rt.rtm_family = AF_INET;
                break;
#ifdef HAVE_IPV6
            case ProtoAddress::IPv6:
                req.rt.rtm_family = AF_INET6;
                break;
#endif // HAVE_IPV6
            default:
                PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() invalid destination address!\n");
                return false;
                break;
        }

        req.rt.rtm_dst_len = prefixLen;
        req.rt.rtm_src_len = 0;
        req.rt.rtm_table = RT_TABLE_UNSPEC;
        req.rt.rtm_protocol = RTPROT_BOOT;

        if (gw.IsValid())
            req.rt.rtm_scope = RT_SCOPE_UNIVERSE;   
        else
            req.rt.rtm_scope = RT_SCOPE_LINK;
        if (dst.IsMulticast())
            req.rt.rtm_type = RTN_MULTICAST;
        else
            req.rt.rtm_type = RTN_UNICAST;

        req.rt.rtm_flags = 0;

        // Set destination rtattr
        if (dst.IsValid())
        {       
            unsigned int addrBits = dst.GetLength() << 3;
            if (prefixLen < addrBits)
            {
                // address bits past mask _must_ be ZERO, we test for that here
                unsigned int index = prefixLen >> 3;
                const char* bytes = dst.GetRawHostAddress();
                if (0 != ((0x00ff >> (prefixLen & 0x07)) & bytes[index]))
                {
                    PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() invalid address for given mask\n");
                    return false;   
                }
                while (++index < dst.GetLength())
                {
                    if (0 != bytes[index] )
                    {
                        PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() invalid address for given mask\n");
                        return false;
                    }  
                }            
            }
            else if (prefixLen > addrBits)
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() invalid mask.\n");
                return false;
            }
            if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_DST, dst.GetRawHostAddress(), dst.GetLength()))
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() error setting RTA_DST attr.\n");
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() no valid destination given\n");
            return false;
        }

        if (gw.IsValid())
        {
            if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_GATEWAY, gw.GetRawHostAddress(), gw.GetLength()))
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() error setting RTA_GATEWAY attr.\n");
                return false;
            }
        }

        if (0 != ifIndex)
        {
            UINT32 index = ifIndex;
            if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_OIF, &index, sizeof(UINT32)))
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() error adding RTA_GATEWAY attr.\n");
                return false;
            }  
        }

        // Send request to netlink socket
        int result = send(descriptor, &req, req.msg.nlmsg_len, 0);
        if ((int)req.msg.nlmsg_len != result)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::DeleteRoute() send() error: %s\n",
                    strerror(errno));
            return false;   
        }

        // Check response for error code
        if (!NetlinkCheckResponse(seq))
        {
            PLOG(PL_DEBUG, "LinuxRouteMgr::DeleteRoute() error deleting route\n");
            return false;   
        }
    }
    return true;
}  // end LinuxRouteMgr::DeleteRoute()

            
bool LinuxRouteMgr::GetRoute(const ProtoAddress& dst, 
                              unsigned int       prefixLen,
                              ProtoAddress&      gw,
                              unsigned int&      ifIndex,
                              int&               metric)
{   
    bool routeFound = false;
    bool complete = false;
    int bufferSize = 4096;    
    do
    {
        // Init return values
        gw.Invalidate();
        ifIndex = 0;
        char* buffer = new char[bufferSize];
        if (!buffer)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() new buffer error: %s\n", strerror(errno));
            return false;   
        }
    
        // Construct request for route
        struct
        {
            struct nlmsghdr msg;
            struct rtmsg    rt;
            char            buf[512];   
        } req;
        memset(&req, 0, sizeof(req));

        // netlink message header
        req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
        req.msg.nlmsg_type = RTM_GETROUTE;
        req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH;
        UINT32 seq = sequence++;
        req.msg.nlmsg_seq = seq; 
        req.msg.nlmsg_pid = port_id;

        // route dump request
        switch (dst.GetType())
        {
            case ProtoAddress::IPv4:
                req.rt.rtm_family = AF_INET;
                break;
#ifdef HAVE_IPV6
            case ProtoAddress::IPv6:
               req.rt.rtm_family = AF_INET6;
                break;
#endif // HAVE_IPV6
            default:
                PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() invalid destination address!\n");
                delete[] buffer;
                return false;
                break;
        }
        req.rt.rtm_dst_len = prefixLen;
        req.rt.rtm_src_len = 0;
        req.rt.rtm_table = RT_TABLE_UNSPEC;
        req.rt.rtm_protocol = RTPROT_UNSPEC;
        req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
        req.rt.rtm_type = RTN_UNSPEC;

        req.rt.rtm_flags = 0;

        // Set destination rtattr
        if (dst.IsValid())
        {
            unsigned int addrBits = dst.GetLength() << 3;
            if (prefixLen < addrBits)
            {
                // address bits past mask _must_ be ZERO, we test for that here
                unsigned int index = prefixLen >> 3;
                const char* bytes = dst.GetRawHostAddress();
                if (0 != ((0x00ff >> (prefixLen & 0x07)) & bytes[index]))
                {
                    PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() invalid address for given mask\n");
                    delete[] buffer;
                    return false;   
                }
                while (++index < dst.GetLength())
                {
                    if (0 != bytes[index] )
                    {
                        PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() invalid address for given mask\n");
                        delete[] buffer;
                        return false;
                    }  
                }            
            }
            else if (prefixLen > addrBits)
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() invalid mask.\n");
                delete[] buffer;
                return false;
            }
            if (!NetlinkAddAttr(&req.msg, sizeof(req), RTA_DST, dst.GetRawHostAddress(), dst.GetLength()))
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() error setting RTA_DST attr.\n");
                delete[] buffer;
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() no valid destination given\n");
            delete[] buffer;
            return false;
        }

        // write message to our netlink socket
        int result = send(descriptor, &req, req.msg.nlmsg_len, 0);
        if (result < 0)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() send() error: %s\n",
                    strerror(errno));
            delete[] buffer;
            return false;   
        }

        // read response(s) until NLMSG_DONE
        bool done = false;
        bool truncated = false;
        while(!done)
        {
            int msgLen = recv(descriptor, buffer, bufferSize, 0); 
            if (msgLen < 0)
            {
                PLOG(PL_ERROR, "LinuxRouteMgr::GetRoute() recv() error: %s\n", strerror(errno)); 
                delete[] buffer;
                return false;  
            }
            else if (msgLen == bufferSize)
            {
                bufferSize *= 2;
                truncated = true; 
                // TBD - should use struct msghdr.msg_flags MSG_TRUNC flag to detect this isntead 
            }
            
            struct nlmsghdr* msg = (struct nlmsghdr*)buffer;
            for (; 0 != NLMSG_OK(msg, (unsigned int)msgLen); msg = NLMSG_NEXT(msg, msgLen))
            {
//Justin comment oout pid part below if pid issues occur.  This can happen with some virtualized systems. ie core namespaces
//Justin Below is a semi-hack to get SMF to function correctly in core namespaces.  
//If when the msg->nlmsg_pid gets fixed this ifdef can be removed
#ifndef CORE_NAMESPACES
                if ((msg->nlmsg_pid == port_id) && (msg->nlmsg_seq == seq))
#else
                if (msg->nlmsg_seq == seq)
#endif // if/else !CORE_NAMESPACES
                {       
                    if (!truncated) complete = true;  // matching response within our buffer bounds   
                    switch (msg->nlmsg_type)
                    {
                        case NLMSG_NOOP:
                            TRACE("recvd NLMSG_NOOP ...\n");
                            break;
                        case NLMSG_ERROR:
                        {
                            struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                            PLOG(PL_DEBUG, "LinuxRouteMgr::GetRoute() recvd matching NLMSG_ERROR error seq:%d code:%d...\n", 
                                    msg->nlmsg_seq, errorMsg->error);
                            delete[] buffer;
                            return false;
                            break;
                        }
                        case NLMSG_DONE:
                            //TRACE("recvd NLMSG_DONE ...\n");
                            done = true;
                            break;
                        case NLMSG_OVERRUN:
                            TRACE("recvd NLMSG_OVERRUN ...\n");
                            break;
                        case RTM_GETROUTE:
                        {
                            TRACE("recvd RTM_GETROUTE ... \n");
                            break;
                        }
                        case RTM_NEWROUTE:
                        {
                            if (truncated) break;  // ignore "truncated" responses
                            ProtoAddress destination;
                            destination.Reset(dst.GetType());
                            unsigned int prefixLen = 0;
                            //bool havePrefSource = false;
                            ProtoAddress gateway;
                            gateway.Invalidate();
                            unsigned int interfaceIndex = 0;

                            struct rtmsg* rtm = (struct rtmsg*)NLMSG_DATA(msg);
                            struct rtattr* rta = RTM_RTA(rtm);
                            int rtaLen = msg->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));
                            for (; RTA_OK(rta, rtaLen); 
                                 rta = RTA_NEXT(rta, rtaLen))
                            {
                                switch (rta->rta_type)
                                {
                                    case RTA_DST:
                                    {   
                                        prefixLen = rtm->rtm_dst_len;
                                        destination.SetRawHostAddress(dst.GetType(), (char*)RTA_DATA(rta), 
                                                                      (prefixLen+7)>>3);
                                        //TRACE("got dst: \"%s\"\n", destination.GetHostString());
                                        break;
                                    }
                                    case RTA_GATEWAY:
                                    {   
                                        gateway.SetRawHostAddress(dst.GetType(), (char*)RTA_DATA(rta), dst.GetLength());
                                        //TRACE("got gw: \"%s\"\n", gw.GetHostString());
                                        break;
                                    }

                                    case RTA_PREFSRC:
                                    {   
                                        //ProtoAddress addr;
                                        //addr.SetRawHostAddress(dst.GetType(), (char*)RTA_DATA(rta), dst.GetLength());
                                        //havePrefSource = true;
                                        break;
                                    }
                                    case RTA_OIF:
                                    {
                                        ASSERT(4 == RTA_PAYLOAD(rta));
                                        memcpy(&interfaceIndex, RTA_DATA(rta), 4);
                                        //TRACE("got ifIndex: %d\n", ifIndex);
                                        break;
                                    }
                                    case RTA_PRIORITY:
                                    {
                                        UINT32 priority;
                                        memcpy(&priority, RTA_DATA(rta), 4);
                                        metric = priority;
                                        //TRACE("RTA_PRIORITY value: %d\n", metric);
                                        break;
                                    }
                                    default:
                                        //TRACE("LinuxRouteMgr::GetRoute() unhandled rtattr type:%d len:%d\n", 
                                        //      rta->rta_type, RTA_PAYLOAD(rta));
                                    break;

                                }  // end switch(rta_type)
                            }  // end for(RTA_NEXT())
                            
                            if ((RTN_BROADCAST != rtm->rtm_type) &&
                                (RTN_UNREACHABLE != rtm->rtm_type) &&
                                //destination.IsValid() &&
                                destination.HostIsEqual(dst) &&
                                (RTN_LOCAL != rtm->rtm_type) &&
                                (0 == (RTM_F_CLONED & rtm->rtm_flags)))
                            {
                                if (!routeFound)
                                {
                                    if (gateway.IsValid() &&
                                        !gateway.HostIsEqual(dst) &&
                                        !gateway.IsUnspecified())
                                        gw = gateway;
                                    if (gw.IsValid() || (interfaceIndex > 0))
                                    {
                                        ifIndex = interfaceIndex;
                                        // (TBD) fill in real metric
                                        metric = -1;
                                        routeFound = true;  
                                        //delete[] buffer; return true;
                                    }
                                }
                            }
                            break;
                        }
                        default:
                            TRACE("matching reply type:%d len:%d bytes\n", 
                                  msg->nlmsg_type, msg->nlmsg_len);
                            break;
                    }  // end switch(nlmsg_type)
                }
                else
                {
                    if (NLMSG_ERROR == msg->nlmsg_type)
                    {
                        struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                        PLOG(PL_DEBUG, "LinuxRouteMgr::GetRoute() recvd non-matching NLMSG_ERROR seq:%d code:%d\n", 
                                msg->nlmsg_seq, errorMsg->error);  
                    }
                }  // end if/else (matching pid && seq)
            }  // end for(NLMSG_NEXT())
        }  // end while (!done)
        delete[] buffer;
        buffer = NULL;
    } while (!complete);
    return routeFound;
}  // end LinuxRouteMgr::GetRoute()
            
            
bool LinuxRouteMgr::GetAllRoutes(ProtoAddress::Type addrType,
                                 ProtoRouteTable&   routeTable)
{
    // The "do" loop is needed to make sure we get all of the routes
    // Routing tables can be big (particularly IPv6) and since the
    // Netlink does _not_ do multi-part messages properly (It only
    // gives you part of the routing table if your receive buffer 
    // isn't big enough), we repeat the request, increasing the buffer
    // size until it works

    bool complete = false;
    
    int bufferSize = 4096;
    int failSafeCount = 0;
    int failSafeMax = 10000;
    do
    {
        if(failSafeCount++>failSafeMax)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() didn't find any routes!\n");
            return false;
        }
        char* buffer = new char[bufferSize];
        if (!buffer)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() new buffer error: %s\n", strerror(errno));
            return false;   
        }
        // Construct request for all routes
        struct
        {
            struct nlmsghdr msg;
            struct rtmsg    rt;   
        } req;
        memset(&req, 0, sizeof(req));

        // netlink message header
        req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
        req.msg.nlmsg_type = RTM_GETROUTE;
        req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_MULTI;
        UINT32 seq = sequence++;
        req.msg.nlmsg_seq = seq; 
        req.msg.nlmsg_pid = port_id;

        // route dump request
        switch (addrType)
        {
            case ProtoAddress::IPv4:
                req.rt.rtm_family = AF_INET;
                break;
#ifdef HAVE_IPV6
            case ProtoAddress::IPv6:
                req.rt.rtm_family = AF_INET6;
                break; 
#endif // HAVE_IPV6
            default:
                PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() invalid address type\n");
                delete[] buffer;
                return false;
        }

        req.rt.rtm_dst_len = 0;
        req.rt.rtm_src_len = 0;
        req.rt.rtm_table = RT_TABLE_UNSPEC;
        req.rt.rtm_protocol = RTPROT_UNSPEC;
        req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
        req.rt.rtm_type = RTN_UNSPEC; 
        req.rt.rtm_flags = 0;

        // write message to our netlink socket
        int result = send(descriptor, &req, req.msg.nlmsg_len, 0);
        if (result < 0)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() send() error: %s\n", strerror(errno));
            delete[] buffer;
            return false;   
        }

        // read response(s) until NLMSG_DONE 
        // (if response is truncated, rescale "bufferSize" and try again
        bool done = false;
        bool truncated = false;
        while(!done)
        {
//fprintf(stderr,"tiger time %d is buffer size\n",bufferSize);
            int msgLen = recv(descriptor, buffer, bufferSize, 0); 
//fprintf(stderr,"tiger time sdfsdfi\n");
            if (msgLen < 0)
            {
//fprintf(stderr,"tiger time 2\n");
                PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() recv() error: %s\n", 
                                strerror(errno));
                delete[] buffer;
                return false;  
            } 
            else if (msgLen == bufferSize)
            {
//fprintf(stderr,"tiger time 3\n");
                // assume our receive buffer wasn't big enough
                // mark response as "truncated"
                bufferSize *= 2;
                truncated = true;  
            }

            struct nlmsghdr* msg = (struct nlmsghdr*)buffer;
            for (; 0 != NLMSG_OK(msg, (unsigned int)msgLen); msg = NLMSG_NEXT(msg, msgLen))
            { 
// Justin Below is a semi-hack to get SMF to function correctly in core namespaces.  
// If when the msg->nlmsg_pid gets fixed this #ifdef can be removed
#ifndef CORE_NAMESPACES
                if ((msg->nlmsg_pid == port_id) && (msg->nlmsg_seq == seq))
#else
                if (msg->nlmsg_seq == seq)
#endif // if/else !CORE_NAMESPACES
                {       
                    if (!truncated) complete = true;  // we got a matching response _within_ our buffer bounds
                    // if (0 != (NLM_F_MULTI & msg->nlmsg_flags)) TRACE("multi part message ...\n");
                    switch (msg->nlmsg_type)
                    {
                        case NLMSG_NOOP:
                            TRACE("recvd NLMSG_NOOP ...\n");
                            break;
                        case NLMSG_ERROR:
                        {
                            struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                            PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() recvd NLMSG_ERROR error seq:%d code:%d...\n", 
                                    msg->nlmsg_seq, errorMsg->error);
                            break;
                        }
                        case NLMSG_DONE:
                            done = true;
                            break;
                        case NLMSG_OVERRUN:
                            TRACE("recvd NLMSG_OVERRUN ...\n");
                            break;
                        case RTM_GETROUTE:
                        {
                            TRACE("recvd RTM_GETROUTE ...\n");
                            break;
                        }
                        case RTM_NEWROUTE:
                        {
                            if (truncated) break; // ignore truncated responses
                            //TRACE("matching RTM_NEWROUTE reply. len:%d bytes\n", msg->nlmsg_len);
                            ProtoAddress gateway;
                            gateway.Invalidate();
                            ProtoAddress destination;
                            destination.Invalidate();
                            unsigned int prefixLen = 0;
                            unsigned int ifIndex = 0;
                            int metric = -1;
                            struct rtmsg* rtm = (struct rtmsg*)NLMSG_DATA(msg);
                            struct rtattr* rta = RTM_RTA(rtm);
                            int rtaLen = msg->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));
                            ProtoAddress::Type addrType = ProtoAddress::INVALID;
                            int addrLen = 0;
                            switch (rtm->rtm_family)
                            {
                                case AF_INET:
                                    addrType = ProtoAddress::IPv4;
                                    addrLen = 4;
                                    break;
#ifdef HAVE_IPV6        
                                case AF_INET6:
                                    addrType = ProtoAddress::IPv6;
                                    addrLen = 16;
                                    break;
#endif // HAVE_IPV6
                                default:
                                    PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() recvd invalid addr family\n");
                                    delete[] buffer;
                                    return false;
                                    break;
                            }
                            for (; RTA_OK(rta, rtaLen); 
                                 rta = RTA_NEXT(rta, rtaLen))
                            {
                                switch (rta->rta_type)
                                {
                                    case RTA_DST:
                                    {   
                                        prefixLen = rtm->rtm_dst_len;
                                        destination.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), (prefixLen+7)>>3);
                                        //TRACE("RTA_DST: %s/%d\n", destination.GetHostString(), 
                                        //                           rtm->rtm_dst_len);
                                        break;
                                    }
                                    case RTA_GATEWAY:
                                    {   
                                        gateway.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), addrLen);
                                        //TRACE("RTA_GWY: %s\n", gateway.GetHostString());
                                        break;
                                    }

                                    case RTA_PREFSRC:
                                    {   
                                        //ProtoAddress addr;
                                        //addr.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), addrLen);
                                        //TRACE("RTA_SRC: %s\n", addr.GetHostString());
                                        break;
                                    }
                                    case RTA_OIF:
                                    {
                                        memcpy(&ifIndex, RTA_DATA(rta), RTA_PAYLOAD(rta));
                                        //TRACE("RTA_OIF: %d\n", ifIndex);
                                        break;
                                    }
                                    case RTA_PRIORITY:
                                    {
                                        UINT32 priority;
                                        memcpy(&priority, RTA_DATA(rta), 4);
                                        metric = priority;
                                        //TRACE("LinuxRouteMgr::GetAllRoutes() RTA_PRIORITY value: %d\n", metric);
                                        break;
                                    }
                                    case RTA_METRICS:
                                    {
                                        //TRACE("get all routes got RTA_METRIC ...\n");
                                        break;
                                    }
                                    
                                    default:
                                        //TRACE("LinuxRouteMgr::GetAllRoutes() unhandled rtattr type:%d len:%d\n", 
                                        //      rta->rta_type, RTA_PAYLOAD(rta));
                                    break;

                                }
                            }  // end for(RTA_NEXT())

                            if ((RTN_BROADCAST != rtm->rtm_type) &&
                                (RTN_UNREACHABLE != rtm->rtm_type) &&
                                (RTN_LOCAL != rtm->rtm_type) &&
                                (0 == (RTM_F_CLONED & rtm->rtm_flags)))
                            {
                                if (!destination.IsValid()) 
                                {
                                    destination.Reset(addrType); 
                                    prefixLen = 0;
                                }
                                
                                if (gateway.IsValid() && (gateway.IsUnspecified() || gateway.HostIsEqual(destination))) 
                                {
                                    //TRACE("found UNSPECIFIED gateway\n");
                                    gateway.Invalidate();
                                }
                                
                                if (!routeTable.SetRoute(destination, prefixLen, gateway, ifIndex, metric))
                                {
                                    PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() error creating table entry\n");
                                    delete[] buffer;
                                    return false;   
                                }
                            }
                            break;
                        }
                        default:
                            TRACE("matching reply type:%d len:%d bytes\n", 
                                  msg->nlmsg_type, msg->nlmsg_len);
                            break;
                    }  // end switch()
                }
                else
                {
            //fprintf(stderr,"NLMSG_OK if false\n");
            //fprintf(stderr,"%d %d is msg type\n",msg->nlmsg_type,NLMSG_ERROR);
                    if (NLMSG_ERROR == msg->nlmsg_type)
                    {
                    //fprintf(stderr,"before error message\n");
                        struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                        PLOG(PL_ERROR, "LinuxRouteMgr::GetAllRoutes() recvd NLMSG_ERROR seq:%d code:%d\n", 
                                msg->nlmsg_seq, errorMsg->error);  
                    }
                    //fprintf(stderr," bunny time 1\n");
                }
                    //fprintf(stderr," bunny time 2\n");
            }  // end for(NLMSG_NEXT())
                    //fprintf(stderr," bunny time 3\n");
        }  // end while (!done)
                    //fprintf(stderr," bunny time 4\n");
        delete[] buffer;
        buffer = NULL;
                    //fprintf(stderr," bunny time 5\n");
    } while (!complete);          
    return true;
}  // end LinuxRouteMgr::GetAllRoutes()
          
bool LinuxRouteMgr::GetInterfaceAddressList(unsigned int        ifIndex, 
                                            ProtoAddress::Type  addrType,
                                            ProtoAddressList&   addrList)
{
    ProtoAddressList localAddrList;  // keep link/site local addresses separate and add to "addrList" at end
    // Construct request for interface address
    struct
    {
        struct nlmsghdr     msg;
        struct ifaddrmsg    ifa;   
    } req;
    memset(&req, 0, sizeof(req));
      
//    DMSG(0,"Justin comment in getinterfaceaddress\n");
    // netlink message header
    req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.msg.nlmsg_type = RTM_GETADDR;
    req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH ;
    UINT32 seq = sequence++;
    req.msg.nlmsg_seq = seq; 
    req.msg.nlmsg_pid = port_id;
    
    // route dump request
    unsigned int addrLength = 0;
    switch (addrType)
    {
        case ProtoAddress::IPv4:
//    DMSG(0,"Justin comment in getinterfaceaddress is ipv4\n");
            req.ifa.ifa_family = AF_INET;
            req.ifa.ifa_prefixlen = 32;
            addrLength = 4;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
//    DMSG(0,"Justin comment in getinterfaceaddress is ipv6\n");
            req.ifa.ifa_family = AF_INET6;
            req.ifa.ifa_prefixlen = 128;
            addrLength = 16;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() invalid destination address!\n");
            return false;
            break;
    }
    req.ifa.ifa_flags = 0;//IFA_F_SECONDARY;//0;//IFA_F_PERMANENT;
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
    req.ifa.ifa_index = ifIndex;
    
    // write message to our netlink socket
//    DMSG(0,"Justin before send comment in getinterfaceaddress is ipv4\n");
    int result = send(descriptor, &req, req.msg.nlmsg_len, 0);
//    DMSG(0,"Justin comment after send in getinterfaceaddress is ipv4\n");
    if (result < 0)
    {
        PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() send() error: %s\n",
                strerror(errno));
        return false;   
    }
    
    // read response(s)
    bool done = false;
    while(!done)
    {
//        DMSG(0,"Justin in while comment in getinterfaceaddress is ipv4\n");
        char buffer[4096];
        int msgLen = recv(descriptor, buffer, 4096, 0); 
        if (msgLen < 0)
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() recv() error: %s\n", strerror(errno)); 
            return false;  
        }
        struct nlmsghdr* msg = (struct nlmsghdr*)buffer;
        for (; 0 != NLMSG_OK(msg, (unsigned int)msgLen); msg = NLMSG_NEXT(msg, msgLen))
        {
//Justin Below is a semi-hack to get SMF to function correctly in core namespaces.  
//If when the msg->nlmsg_pid gets fixed this #if can be removed
#ifndef CORE_NAMESPACES
          if ((msg->nlmsg_pid == port_id) && (msg->nlmsg_seq == seq))
#else
            if (msg->nlmsg_seq == seq)
#endif // if/else !CORE_NAMESPACES
            {          
                switch (msg->nlmsg_type)
                {
                    case NLMSG_NOOP:
                        //TRACE("recvd NLMSG_NOOP ...\n");
                        break;
                    case NLMSG_ERROR:
                    {
                        struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                        PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() recvd NLMSG_ERROR error seq:%d code:%d...\n", 
                                msg->nlmsg_seq, errorMsg->error);
                        return false;
                        break;
                    }
                    case NLMSG_DONE:
                        //TRACE("recvd NLMSG_DONE ...\n");
                        done = true;
                        break;
                    case RTM_NEWADDR:
                    {
                        //TRACE("recvd RTM_NEWADDR ... \n");
                        struct ifaddrmsg* ifa = (struct ifaddrmsg*)NLMSG_DATA(msg);
                        struct rtattr* rta = IFA_RTA(ifa);
                        int rtaLen = msg->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifaddrmsg));
                        for (; RTA_OK(rta, rtaLen); 
                             rta = RTA_NEXT(rta, rtaLen))
                        {
                            switch (rta->rta_type)
                            {
                                case IFA_ADDRESS:
                                case IFA_LOCAL:
                                {   
                                    //ProtoAddress addr;
                                    //addr.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), addrLength);
                                    //TRACE("%s: %s (index:%d scope:%d)\n", (IFA_ADDRESS == rta->rta_type) ?
                                    //      "IFA_ADDRESS" : "IFA_LOCAL", addr.GetHostString(),
                                    //      ifa->ifa_index, ifa->ifa_scope);
                                    if (ifa->ifa_index == ifIndex)
                                    {
                                        switch (ifa->ifa_scope)
                                        {
                                            case RT_SCOPE_UNIVERSE: 
                                            {
                                                ProtoAddress theAddress;
                                                theAddress.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), addrLength);
                                                if (theAddress.IsValid())
                                                {
                                                    if (!addrList.Insert(theAddress))
                                                    {
                                                        PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() error: couldn't add to addrList\n");
                                                        done = true;
                                                    }
                                                }
                                                break;
                                            }                             
                                            case RT_SCOPE_SITE:
                                            case RT_SCOPE_LINK:
                                            {
                                                // Keep site-local addresses in separate list and add at end.
                                                ProtoAddress theAddress;
                                                theAddress.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), addrLength);
                                                if (theAddress.IsValid() && !localAddrList.Insert(theAddress))
                                                    PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() error: couldn't add to localAddrList\n");
                                                break;
                                            }
                                            default:
                                                // ignore other address types for now
                                                break;
                                        }
                                    }
                                    break;
                                }
                                case IFA_BROADCAST:
                                {
                                    //ProtoAddress addr;
                                    //addr.SetRawHostAddress(addrType, (char*)RTA_DATA(rta), addrLength);
                                    //TRACE("IFA_BROADCAST: %s\n", addr.GetHostString());
                                    break;
                                }
                                default:
                                    //TRACE("LinuxRouteMgr::GetInterfaceAddressList() unhandled rtattr type:%d len:%d\n", 
                                    //       rta->rta_type, RTA_PAYLOAD(rta));
                                break;
                                
                            }  // end switch(rta_type)
                        }  // end for(RTA_NEXT())
                        break;
                    }
                    default:
                        PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() matching reply type:%d len:%d bytes\n", 
                                msg->nlmsg_type, msg->nlmsg_len);
                        break;
                }  // end switch(nlmsg_type)
            }
            else
            {
                if (NLMSG_ERROR == msg->nlmsg_type)
                {
                    struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                    PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() recvd NLMSG_ERROR seq:%d code:%d\n", 
                            msg->nlmsg_seq, errorMsg->error);  
                }
            }  // end if/else (matching pid && seq)
        }  // end for(NLMSG_NEXT())
    }  // end while (!done)
    
    ProtoAddressList::Iterator iterator(localAddrList);
    ProtoAddress localAddr;
    while (iterator.GetNextAddress(localAddr))
    {
        if (!addrList.Insert(localAddr))
        {
            PLOG(PL_ERROR, "LinuxRouteMgr::GetInterfaceAddressList() error: couldn't add localAddr to addrList\n");
            break;
        }
    }    
    if (addrList.IsEmpty())
        PLOG(PL_WARN, "LinuxRouteMgr::GetInterfaceAddressList() warning: no addresses found\n");
    return true;
}  // end LinuxRouteMgr::GetInterfaceAddressList()
