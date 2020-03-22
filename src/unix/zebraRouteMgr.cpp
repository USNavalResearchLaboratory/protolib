#include "unix/zebraRouteMgr.h"

void ZebraRouteMgr::OnClientSocketEvent(ProtoSocket&       theSocket,
                                       ProtoSocket::Event theEvent)
{
     //printf("Socket Event\n");
}

ZebraRouteMgr::ZebraRouteMgr()
	:zPipe(ProtoPipe::STREAM)
{
}

ZebraRouteMgr::~ZebraRouteMgr()
{
    Close();
}

bool ZebraRouteMgr::SetForwarding(bool state)
{
    return true;
}  // end ZebraRouteMgr::SetForwarding()


bool ZebraRouteMgr::Open(const void* /*userData*/)
{
    //printf("zebra mgr open\n");
    if ((descriptor = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0)
    {
        printf("ZebraRouteMgr::Open() socket(NETLINK_ROUTE) error: %s\n",
                strerror(errno));
        return false;
    }
    else
    {
        pid = (UINT32)getpid();
    }

    //printf("descriptor open\n");

    if (!zPipe.Connect("/var/run/zserv.api")) {
        if(!zPipe.Connect("/var/run/quagga/zserv.api")) {
	    printf("ZebraRouteMgr::Open fail to connect %s\n",zPipe.GetErrorString());
            printf("ZebraRouteMgr::Open tries to connect to /var/run/zserv.api and /var/run/quagga/zserv.api.  Is Zebra running?  Is its runs directory one of the above (set durring zebra configure)?\n");
            return false;
        }
    }
    return true;

}  // end ZebraRouteMgr::Open()
bool ZebraRouteMgr::IsOpen() const
{
    //return zSock.IsOpen();
    return zPipe.IsOpen();
}

void ZebraRouteMgr::Close()
{
    if (IsOpen())
    {
	//zSock.Close();
	zPipe.Close();
    }   
}  // end ZebraRouteMgr::Close()

/*
  * "xdr_encode"-like interface that allows daemon (client) to send
  * a message to zebra server for a route that needs to be
  * added/deleted to the kernel. Info about the route is specified
  * by the caller in a struct zapi_ipv4. zapi_ipv4_read() then writes
  * the info down the zclient socket using the stream_* functions.
  *
  * The corresponding read ("xdr_decode") function on the server
  * side is zread_ipv4_add()/zread_ipv4_delete().
  *
  *  0 1 2 3 4 5 6 7 8 9 A B C D E F 0 1 2 3 4 5 6 7 8 9 A B C D E F
  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  * |            Length (2)         |    Command    | Route Type    |
  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  * | ZEBRA Flags   | Message Flags | Prefix length |
  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  * | Destination IPv4 Prefix for route                             |
  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  * | Nexthop count |
  * +-+-+-+-+-+-+-+-+
  *
  *
  * A number of IPv4 nexthop(s) or nexthop interface index(es) are then
  * described, as per the Nexthop count. Each nexthop described as:
  *
  * +-+-+-+-+-+-+-+-+
  * | Nexthop Type  |  Set to one of ZEBRA_NEXTHOP_*
  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  * |       IPv4 Nexthop address or Interface Index number          |
  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *
  * Alternatively, if the flags field has ZEBRA_FLAG_BLACKHOLE or
  * ZEBRA_FLAG_REJECT is set then Nexthop count is set to 1, then _no_
  * nexthop information is provided, and the message describes a prefix
  * to blackhole or reject route.
  *
  * If ZAPI_MESSAGE_DISTANCE is set, the distance value is written as a 1
  * byte value.
  *
  * If ZAPI_MESSAGE_METRIC is set, the metric value is written as an 8
  * byte value.
  *
  * XXX: No attention paid to alignment.
  */
bool ZebraRouteMgr::SetRoute(const ProtoAddress&   dst,
                             unsigned int          prefixLen,
                             const ProtoAddress&   gw,
                             unsigned int          ifIndex,
                             int                   metric)
{
    //printf("enter setRoute\n");
    unsigned char *s = obuf;
    unsigned short *p ; 
    unsigned int len;

    //header
    p = (unsigned short *)s;
    *p = htons(ZEBRA_HEADER_SIZE);
    s +=2 ; // length
    *s++ = ZEBRA_HEADER_MARKER; // marker
    *s++ = ZEBRA_VERSION; //version
    p = (unsigned short *)s;
    *p = htons(ZEBRA_IPV4_ROUTE_ADD); // cmd
    s += 2;
    *s++ = 0 ; //type
    *s++ = 0 ; //flags
    *s++ = ZAPI_MESSAGE_NEXTHOP | ZAPI_MESSAGE_DISTANCE; //message

    // destionation
    *s++ = prefixLen; //prefixlen
    unsigned long ipLong;
    ipLong = dst.IPv4GetAddress(); //tbd for ipv6 and others
    ipLong = htonl(ipLong);
    int prefixInBytes = (prefixLen +7 )/8;
    memcpy(s,&ipLong,prefixInBytes); // copy the prefix addr
    s += prefixInBytes;
    
    // nexthop num
    *s++ = 2; // two nexthop entries
    // nexthop
    *s++ = ZEBRA_NEXTHOP_IPV4; //type: ipv4
    ipLong = gw.IPv4GetAddress(); //tbd for ipv6 and others
    ipLong = htonl(ipLong);
    memcpy(s,&ipLong,4); // copy the prefix addr
    s += 4;

    // ifindex
    *s++ = ZEBRA_NEXTHOP_IFINDEX; // type: ifindex
    ipLong = htonl(ifIndex); 
    memcpy(s,&ipLong,4); // copy the ifindex 
    s += 4;

    // distance
    *s++ = metric; // metric 

    len = s - obuf;
    ipLong = htons(len); // length
    p = (unsigned short *)obuf;
    *p = htons(len);
    //PrintBuffer((char *)obuf,len,0);
    //printf("zPipe Send len=%d\n",len);

    zPipe.Send((char *)obuf,len);
    //printf("Send len=%d %s\n",len,zPipe.GetErrorString());
    return true;
}  // end ZebraRouteMgr::SetRoute()


bool ZebraRouteMgr::DeleteRoute(const ProtoAddress& dst,
                                unsigned int        prefixLen,
                                const ProtoAddress& gw,
                                unsigned int        ifIndex)
{
    //printf("enter delroute\n");
    unsigned char *s = obuf;
    unsigned short *p ; 
    unsigned int len;

    //header
    p = (unsigned short *)s;
    *p = htons(ZEBRA_HEADER_SIZE);
    s +=2 ; // length
    *s++ = ZEBRA_HEADER_MARKER; // marker
    *s++ = ZEBRA_VERSION; //version
    p = (unsigned short *)s;
    *p = htons(ZEBRA_IPV4_ROUTE_DELETE); // cmd
    s += 2;
    *s++ = 0 ; //type
    *s++ = 0 ; //flags
    *s++ = ZAPI_MESSAGE_NEXTHOP ; //message

    // destionation
    *s++ = prefixLen; //prefixlen
    unsigned long ipLong;
    ipLong = dst.IPv4GetAddress(); //tbd for ipv6 and others
    ipLong = htonl(ipLong);
    int prefixInBytes = (prefixLen +7 )/8;
    memcpy(s,&ipLong,prefixInBytes); // copy the prefix addr
    s += prefixInBytes;
    
    // nexthop num
    *s++ = 2; // two nexthop entries
    // nexthop
    *s++ = ZEBRA_NEXTHOP_IPV4; //type: ipv4
    ipLong = gw.IPv4GetAddress(); //tbd for ipv6 and others
    ipLong = htonl(ipLong);
    memcpy(s,&ipLong,4); // copy the prefix addr
    s += 4;

    // ifindex
    *s++ = ZEBRA_NEXTHOP_IFINDEX; // type: ifindex
    ipLong = htonl(ifIndex); 
    memcpy(s,&ipLong,4); // copy the ifindex 
    s += 4;

    len = s - obuf;
    ipLong = htons(len); // length
    p = (unsigned short *)obuf;
    *p = htons(len);
    //PrintBuffer((char *)obuf,len,0);
    //printf("zPipe Send len=%d\n",len);

    zPipe.Send((char *)obuf,len);
    //printf("Send len=%d %s\n",len,zPipe.GetErrorString());
    return true;
}  // end ZebraRouteMgr::DeleteRoute()

            
bool ZebraRouteMgr::GetRoute(const ProtoAddress& dst,
                              unsigned int       prefixLen,
                              ProtoAddress&      gw,
                              unsigned int&      ifIndex,
                              int&               metric)
{   
    return true;
}  // end ZebraRouteMgr::GetRoute()
            
            
bool ZebraRouteMgr::GetAllRoutes(ProtoAddress::Type addrType,
                                 ProtoRouteTable&   routeTable)
{
    // The "do" loop is needed to make sure we get all of the routes
    // Routing tables can be big (particularly IPv6) and since the
    // Netlink does _not_ do multi-part messages properly (It only
    // gives you part of the routing table if your receive buffer 
    // isn't big enough), we repeat the request, increasing the buffer
    // size until it works

    return true;
}  // end ZebraRouteMgr::GetAllRoutes()
          
void
ZebraRouteMgr::PrintBuffer(char* buffer,int len,int dlevel)
{
    /*
     * @brief Will print out the buffer in hex format
     *
     * @param buffer the buffer to be printed
     * @param len the length of the buffer contents to be printed in bytes
     *
     */
    for(int i=0;i<(int)len;i++)
    {
        printf("%02X ",(unsigned char)buffer[i]);
        if(i % 4 == 3)
            printf("\n");
    }
}
bool ZebraRouteMgr::GetInterfaceAddressList(unsigned int        ifIndex, 
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
    req.msg.nlmsg_pid = pid;
    
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
   // DMSG(0,"Justin before send comment in getinterfaceaddress is ipv4\n");
    int result = send(descriptor, &req, req.msg.nlmsg_len, 0);
    //DMSG(0,"Justin comment after send in getinterfaceaddress is ipv4\n");
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
        char buffer[1024];
        int msgLen = recv(descriptor, buffer, 1024, 0); 
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
          if ((msg->nlmsg_pid == (UINT32)pid) && (msg->nlmsg_seq == seq))
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
