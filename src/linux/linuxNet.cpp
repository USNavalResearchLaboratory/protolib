
#include "protoNet.h"
#include "protoList.h"
#include "protoString.h"  // for ProtoTokenator
#include "protoDebug.h"

#include <stdio.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <unistd.h>  // for getpid()
#include <sys/ioctl.h>

// Note that the remainder of the Linux ProtoNet stuff is
// implemented in the "src/unix/unixNet.cpp" file
// in the Protolib source tree and the common stuff is
// in "src/common/protoNet.cpp"


// This class wraps around a netlink socket to provide methods for 
// sending/receiving netlink messages for different purposes.
class ProtoNetlink
{
    public:
        ProtoNetlink();
        ~ProtoNetlink();
        
        bool Open();
        void Close();
        
        UINT32 GetPortId() const
            {return port_id;}
        
        bool SendRequest(void* req, size_t len);
        bool RecvResponse(UINT32 seq, struct nlmsghdr** bufferHandle, int* msgSize);
        
    private:
        int     descriptor;
        UINT32  port_id;  // aka netlink pid (not process id)
};  // end class ProtoNetlink


ProtoNetlink::ProtoNetlink()
 : descriptor(-1)
{
}

ProtoNetlink::~ProtoNetlink()
{
    Close();
}

bool ProtoNetlink::Open()
{
    Close(); // in case already open
    descriptor = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (descriptor < 0)
    {
        PLOG(PL_ERROR, "ProtoNetlink::Open() socket() error: %s\n", GetErrorString());
        return false;
    }

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
    if (AF_NETLINK != localAddr.nl_family)
    {
        PLOG(PL_ERROR, "ProtoNetlink::Open() error: invalid socket type?!\n");
        Close();
        return false;
    }
    port_id = localAddr.nl_pid;
    return true;
}  // end ProtoNetlink::Open()

void ProtoNetlink::Close()
{
    if (descriptor >= 0)
    {
        close(descriptor);
        descriptor = -1;
    }
}  // end ProtoNetlink::Close()

bool ProtoNetlink::SendRequest(void* req, size_t len)
{
    // init iovec structure for sendmsg()
    struct iovec io;
    io.iov_base = req;
    io.iov_len = len;
    // init netlink sockaddr (addressed to "kernel")
    struct sockaddr_nl kernel;
    memset(&kernel, 0, sizeof(kernel));
    kernel.nl_family = AF_NETLINK;
    // init msghdr struct for sendmsg()
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_name = &kernel;
    msg.msg_namelen = sizeof(kernel);
    // now sendmsg (handling EINTR)
    for (;;)
    {
        if (0 > sendmsg(descriptor, &msg, 0))
        {
            if (EINTR == errno) continue;
            PLOG(PL_ERROR, "ProtoNetlink::SendRequest() sendmsg() error: %s\n", GetErrorString());
            return false;
        }
        else
        {
            return true;
        } 
    }  
}  // end ProtoNetlink::SendRequest()

bool ProtoNetlink::RecvResponse(UINT32 seq, struct nlmsghdr** bufferHandle, int* msgSize)
{
    if (descriptor < 0)
    {
        PLOG(PL_ERROR, "ProtoNetlink::RecvResponse() error: netlink socket not open\n");
        return false;
    }
    if ((NULL == bufferHandle)  || (NULL == msgSize))
    {
        PLOG(PL_ERROR, "ProtoNetlink::RecvResponse() error: invalid parameters\n");
        return false;
    }
    *bufferHandle = NULL;
    *msgSize = 0;
    // TBD - we need to check this behavior regarding determining buffer size ...
    // (The "buffer increase" strategy doesn't work so initial size must be big enough for now.
    //  I need to see what strategy works (e.g. multi-part receives, or re-send request, etc)
    size_t bufsize = 4096/sizeof(struct nlmsghdr);  // initial size (will be increased if needed
    struct nlmsghdr* buffer = NULL;
    for (;;)
    {
        if (NULL != buffer)
            delete[] buffer;
        if (NULL == (buffer = new struct nlmsghdr[bufsize]))
        {
            PLOG(PL_ERROR, "ProtoNetlink::RecvResponse() new nlmsghdr[] error: %s\n", GetErrorString());
            return false;
        }   
        for (;;)
        {
            // init iovec struct
            struct iovec io;
            io.iov_base = buffer;
            io.iov_len = bufsize*sizeof(struct nlmsghdr);
            struct sockaddr_nl addr;
            // init msghdr struct for recvmsg() 
            struct msghdr msg;
            msg.msg_iov = &io;
            msg.msg_iovlen = 1;
            msg.msg_name = &addr;
            msg.msg_namelen = sizeof(addr);
            msg.msg_control = NULL;
            msg.msg_controllen = 0;
            msg.msg_flags = 0;
               
            ssize_t result = recvmsg(descriptor, &msg, 0);
            if (result < 0)
            {
                if (EINTR == errno) continue;
                PLOG(PL_ERROR, "ProtoNetlink::RecvResponse() recvmsg() error: %s\n", GetErrorString());
                delete[] buffer;
                return false;
            }
            else if (0 != (MSG_TRUNC & msg.msg_flags))
            {
                // buffer was too, small
                bufsize *= 2;
                break;
            }
            else
            {
                int saveResult = result;
                // We got a complete response, parse and make sure it's OK
                for(struct nlmsghdr* hdr = buffer; NLMSG_OK(hdr, (unsigned int)result); hdr = NLMSG_NEXT(hdr, result))
                {
                    // Is it the response I'm looking for?
                    if ((hdr->nlmsg_pid != port_id) || (seq != hdr->nlmsg_seq))
                        continue;  // not for me
                    if (NLMSG_ERROR == hdr->nlmsg_type)
                    {
                        PLOG(PL_ERROR, "ProtoNetlink::RecvResponse() error: NLMSG_ERROR\n");
                        delete[] buffer;
                        return false;
                    }
                }
                // A non-error response was received
                *bufferHandle = buffer;
                *msgSize = saveResult;
                return true;
            }
        }
    }
}  // end ProtoNetlink::RecvResponse()

#ifdef ANDROID
// Although these function would also work for Linux, we just have these defined for
// Android where the getifaddrs() function is not available.  The non-Android Linux
// versions of these are implemented in the more general "unixNet.cpp" module using getifaddrs()

unsigned int ProtoNet::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{
    // For now on Android, we'll get our list of interface indices and check each until we find
    // one with a matching address and then get its name.  The reason we have a different approach
    // for this for Linux is to handle interface alias names, but not sure that applies to Android anyway?
    unsigned int ifCount = GetInterfaceCount();
    if (0 == ifCount)
    {
        PLOG(PL_WARN, "ProtoNet::GetInterfaceName() warning: no interfaces?!\n");
        return 0;
    }
    // Then allocate a buffer of the appropriate size
    unsigned int* ifIndices = new unsigned int[ifCount];
    if (NULL == ifIndices)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() new ifIndices[] error: %s\n", GetErrorString());
        return false;
    }
    // Now call with a buffer to get this list of indices
    ifCount = GetInterfaceIndices(ifIndices, ifCount);
    for (unsigned int i = 0; i < ifCount; i++)
    {
        unsigned int ifIndex = ifIndices[i];
        char ifName[IFNAMSIZ+1];
        ifName[IFNAMSIZ] = '\0';
        if (0 == GetInterfaceName(ifIndex, ifName, IFNAMSIZ+1))
        {
            PLOG(PL_WARN, "ProtoNet::GetInterfaceName() no name for interface index %u?\n", ifIndex);
            continue;
        }
        ProtoAddressList addrList;
        if (GetInterfaceAddressList(ifName, ifAddr.GetType(), addrList))
        {
            ProtoAddressList::Iterator iterator(addrList);
            ProtoAddress addr;
            while (iterator.GetNextAddress(addr))
            {
                if (addr.HostIsEqual(ifAddr))
                {
                    delete[]  ifIndices;
                    strncpy(buffer, ifName, buflen);
                    return strlen(ifName);
                }
            }
        }
    }
    delete[] ifIndices;
    return 0;
}  // end ProtoNet::GetInterfaceName()

bool ProtoNet::GetInterfaceAddressList(const char*         interfaceName,
                                       ProtoAddress::Type  addressType,
                                       ProtoAddressList&   addrList,
                                       unsigned int*       interfaceIndex)
{
    if (ProtoAddress::ETH == addressType)
    {
        struct ifreq req;
        memset(&req, 0, sizeof(struct ifreq));
        strncpy(req.ifr_name, interfaceName, IFNAMSIZ);
        int socketFd = socket(PF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() socket() error: %s\n", GetErrorString());
            return false;
        }
        // Get hardware (MAC) address instead of IP address
        if (ioctl(socketFd, SIOCGIFHWADDR, &req) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() ioctl(SIOCGIFHWADDR) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;   
        }  
        else
        {
            close(socketFd);
            if (NULL != interfaceIndex) 
                *interfaceIndex = req.ifr_ifindex;
            ProtoAddress ethAddr;
            if (!ethAddr.SetRawHostAddress(ProtoAddress::ETH,
                                           (const char*)&req.ifr_hwaddr.sa_data,
                                           IFHWADDRLEN))
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: invalid ETH addr?\n");
                return false;
            }   
            if (!addrList.Insert(ethAddr))
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
                return false;
            }
            return true;            
        }
    }
    
    unsigned int ifIndex = GetInterfaceIndex(interfaceName);
    if (0 == ifIndex)
    {
        // Perhaps "interfaceName" is an address string?
        ProtoAddress ifAddr;
        if (ifAddr.ConvertFromString(interfaceName))
            ifIndex = GetInterfaceIndex(ifAddr);
    }
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: invalid interface name\n");
        return false;
    }
    if (NULL != interfaceIndex) *interfaceIndex = ifIndex;
    // Instantiate a netlink socket and open it
    ProtoNetlink nlink;
    if (!nlink.Open())
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to open netlink socket\n");
        return false;
    }
    
    // Construct request for interface addresses
    struct
    {
        struct nlmsghdr     msg;
        struct ifaddrmsg    ifa;   
    } req;
    memset(&req, 0, sizeof(req));
    
    // fixed sequence number for single request
    UINT32 seq = 1;
    
    // netlink message header
    req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.msg.nlmsg_type = RTM_GETADDR;
    req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH ;
    req.msg.nlmsg_seq = seq; // fixed sequence number for single request
    req.msg.nlmsg_pid = nlink.GetPortId();
    
    // route dump request for given addressType and interface index
    unsigned int addrLength = 0;
    unsigned char addrFamily = AF_UNSPEC;
    switch (addressType)
    {
        case ProtoAddress::IPv4:
            
            addrFamily = req.ifa.ifa_family = AF_INET;
            req.ifa.ifa_prefixlen = 32;
            addrLength = 4;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            
            addrFamily = req.ifa.ifa_family = AF_INET6;
            req.ifa.ifa_prefixlen = 128;
            addrLength = 16;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() invalid address type!\n");
            return false;
    }
    req.ifa.ifa_flags = 0;//IFA_F_SECONDARY;//0;//IFA_F_PERMANENT;
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
    req.ifa.ifa_index = ifIndex;
    if (!nlink.SendRequest(&req, sizeof(req)))
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to send netlink request\n");
        return false;
    }
    // Request sent, now receive response(s) until done
    bool done = false;
    ProtoAddressList localAddrList;  // keep link/site local addresses separate and add to "addrList" at end
    while (!done)
    {
        struct nlmsghdr* buffer;
        int msgLen;
        if (nlink.RecvResponse(seq, &buffer, &msgLen))
        {
            struct nlmsghdr* msg = buffer;
            // Parse response, adding matching addresses for "addressType" and "ifIndex"  
            for (; 0 != NLMSG_OK(msg, (unsigned int)msgLen); msg = NLMSG_NEXT(msg, msgLen))
            {
                // only pay attention to matching netlink responses received
                if ((msg->nlmsg_pid != nlink.GetPortId()) || (msg->nlmsg_seq != seq))
                    continue;  
                switch (msg->nlmsg_type)
                {
                    case NLMSG_NOOP:
                        //TRACE("recvd NLMSG_NOOP ...\n");
                        break;
                    case NLMSG_ERROR:
                    {
                        struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() recvd NLMSG_ERROR error seq:%d code:%d...\n", 
                                       msg->nlmsg_seq, errorMsg->error);
                        delete[] buffer;
                        return false;
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
                        for (; RTA_OK(rta, rtaLen); rta = RTA_NEXT(rta, rtaLen))
                        {
                            switch (rta->rta_type)
                            {
                                case IFA_ADDRESS:
                                case IFA_LOCAL:
                                {   
                                    if ((ifa->ifa_index == ifIndex) && (ifa->ifa_family == addrFamily))
                                    {
                                        switch (ifa->ifa_scope)
                                        {
                                            case RT_SCOPE_UNIVERSE: 
                                            {
                                                ProtoAddress theAddress;
                                                theAddress.SetRawHostAddress(addressType, (char*)RTA_DATA(rta), addrLength);
                                                if (theAddress.IsValid())
                                                {
                                                    if (!addrList.Insert(theAddress))
                                                    {
                                                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: couldn't add to addrList\n");
                                                        done = true;
                                                    }
                                                }
                                                break;
                                            }                             
                                            case RT_SCOPE_SITE:
                                            case RT_SCOPE_LINK:
                                            case RT_SCOPE_HOST:
                                            {
                                                // Keep site-local addresses in separate list and add at end.
                                                ProtoAddress theAddress;
                                                theAddress.SetRawHostAddress(addressType, (char*)RTA_DATA(rta), addrLength);
                                                if (theAddress.IsValid())
                                                {
                                                    if (!localAddrList.Insert(theAddress))
                                                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: couldn't add to localAddrList\n");
                                                }
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
                                    //TRACE("ProtoNet::GetInterfaceAddressList() unhandled rtattr type:%d len:%d\n", 
                                    //       rta->rta_type, RTA_PAYLOAD(rta));
                                    break;
                                
                            }  // end switch(rta_type)
                        }  // end for(RTA_NEXT())
                        break;
                    }
                    default:
                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() matching reply type:%d len:%d bytes\n", 
                                msg->nlmsg_type, msg->nlmsg_len);
                        break;
                }  // end switch(nlmsg_type)
            }
            ASSERT(NULL != buffer);
            delete[] buffer;
        }  // end if (nlink.RecvResponse())
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error:invalid netlink response\n");
            done = true;
        }
    }  // end while(!done)
    ProtoAddressList::Iterator iterator(localAddrList);
    ProtoAddress localAddr;
    while (iterator.GetNextAddress(localAddr))
    {
        if (!addrList.Insert(localAddr))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: couldn't add localAddr to addrList\n");
            break;
        }
    }    
    nlink.Close();
    return true;
}  // end ProtoNet::GetInterfaceAddressList()


unsigned int ProtoNet::GetInterfaceAddressMask(const char* ifaceName, const ProtoAddress& theAddr)
{
    unsigned int ifIndex = GetInterfaceIndex(ifaceName);
    if (0 == ifIndex)
    {
        // Perhaps "interfaceName" is an address string?
        ProtoAddress ifAddr;
        if (ifAddr.ConvertFromString(ifaceName))
            ifIndex = GetInterfaceIndex(ifAddr);
    }
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: invalid interface name\n");
        return false;
    }
    // Instantiate a netlink socket and open it
    ProtoNetlink nlink;
    if (!nlink.Open())
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to open netlink socket\n");
        return false;
    }
    // Construct request for interface addresses
    struct
    {
        struct nlmsghdr     msg;
        struct ifaddrmsg    ifa;   
    } req;
    memset(&req, 0, sizeof(req));
    
    // fixed sequence number for single request
    UINT32 seq = 1;
    
    // netlink message header
    req.msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.msg.nlmsg_type = RTM_GETADDR;
    req.msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH ;
    req.msg.nlmsg_seq = seq; // fixed sequence number for single request
    req.msg.nlmsg_pid = nlink.GetPortId();
    
    // route dump request for given addressType and interface index
    unsigned char addrFamily = AF_UNSPEC;
    switch (theAddr.GetType())
    {
        case ProtoAddress::IPv4:
            
            addrFamily = req.ifa.ifa_family = AF_INET;
            req.ifa.ifa_prefixlen = 32;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            
            addrFamily = req.ifa.ifa_family = AF_INET6;
            req.ifa.ifa_prefixlen = 128;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() invalid address type!\n");
            return false;
    }
    req.ifa.ifa_flags = 0;//IFA_F_SECONDARY;//0;//IFA_F_PERMANENT;
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
    req.ifa.ifa_index = ifIndex;
    if (!nlink.SendRequest(&req, sizeof(req)))
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to send netlink request\n");
        return false;
    }
    // Request sent, now receive response(s) until done
    bool done = false;
    ProtoAddressList localAddrList;  // keep link/site local addresses separate and add to "addrList" at end
    while (!done)
    {
        struct nlmsghdr* buffer = NULL;
        int msgLen;
        if (nlink.RecvResponse(seq, &buffer, &msgLen))
        {
            struct nlmsghdr* msg = buffer;
            // Parse response, adding matching addresses for "addressType" and "ifIndex"  
            for (; 0 != NLMSG_OK(msg, (unsigned int)msgLen); msg = NLMSG_NEXT(msg, msgLen))
            {
                // only pay attention to matching netlink responses received
                if ((msg->nlmsg_pid != nlink.GetPortId()) || (msg->nlmsg_seq != seq))
                    continue;  
                switch (msg->nlmsg_type)
                {
                    case NLMSG_NOOP:
                        //TRACE("recvd NLMSG_NOOP ...\n");
                        break;
                    case NLMSG_ERROR:
                    {
                        struct nlmsgerr* errorMsg = (struct nlmsgerr*)NLMSG_DATA(msg);
                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() recvd NLMSG_ERROR error seq:%d code:%d...\n", 
                                       msg->nlmsg_seq, errorMsg->error);
                        delete[] buffer;
                        return false;
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
                        for (; RTA_OK(rta, rtaLen); rta = RTA_NEXT(rta, rtaLen))
                        {
                            switch (rta->rta_type)
                            {
                                case IFA_ADDRESS:
                                case IFA_LOCAL:
                                case IFA_BROADCAST:
                                {   
                                    if ((ifa->ifa_index == ifIndex) && (ifa->ifa_family == addrFamily))
                                    {
                                        ProtoAddress addr;
                                        addr.SetRawHostAddress(theAddr.GetType(), (char*)RTA_DATA(rta), theAddr.GetLength());
                                        if (addr.HostIsEqual(theAddr))
                                        {
                                            // We have a match, return its prefix length
                                            unsigned int prefixLen = ifa->ifa_prefixlen;
                                            nlink.Close();
                                            delete[] buffer;
                                            return prefixLen;
                                        }
                                    }
                                    break;
                                }
                                default:
                                    //TRACE("ProtoNet::GetInterfaceAddressList() unhandled rtattr type:%d len:%d\n", 
                                    //       rta->rta_type, RTA_PAYLOAD(rta));
                                    break;
                                
                            }  // end switch(rta_type)
                        }  // end for(RTA_NEXT())
                        break;
                    }
                    default:
                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() matching reply type:%d len:%d bytes\n", 
                                msg->nlmsg_type, msg->nlmsg_len);
                        break;
                }  // end switch(nlmsg_type)
            }
            ASSERT(NULL != buffer);
            delete[] buffer;
        }  // end if (nlink.RecvResponse()
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error:invalid netlink response\n");
            done = true;
        }
    }  // end while(!done)
    nlink.Close();
    return 0;
}  // end ProtoNet::GetInterfaceAddressMask()

#endif // ANDROID

static unsigned int Readline(FILE* filePtr, char* buffer, unsigned int buflen)
{
    unsigned int index = 0;
    while (index < buflen)
    {
        if (0 == fread(buffer+index, 1 , 1, filePtr))
            return index;
        else if ('\n' == buffer[index])
            return (index + 1);
        else
            index++;
    }
    return (index+1);  // return value > buflen indicates line exceeds buflen
}  // end Readline()


bool ProtoNet::GetGroupMemberships(const char* ifaceName, ProtoAddress::Type addrType, ProtoAddressList& addrList)
{
    if (ProtoAddress::IPv4 == addrType)
    {
        // TBD - is there a netlink interface for this instead?
        FILE* filePtr = fopen("/proc/net/igmp", "r");
        if (NULL == filePtr)
        {
            PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() fopen(/proc/net/igmp) error: %s\n", GetErrorString());
            return false;
        }
        bool seekingIface = true;
        unsigned int numGroups = 0;
        unsigned int skipCount = 1;  // skip the first line (header line)
        unsigned len;
        char buffer[256];
        while (0 != (len = Readline(filePtr, buffer, 255)))
        {
            if (len > 255)
            {
                PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: /proc/net/igmp line length exceeds max expected!\n");
                fclose(filePtr);
                return false;
            }     
            if (0 != skipCount)
            {
                skipCount--;
                continue;
            }
            buffer[len+1] = '\0';  // apply NULL terminator    
            if (seekingIface)
            {
                ProtoTokenator tk(buffer);
                unsigned int index = 0;
                const char* next;
                while (NULL != (next = tk.GetNextItem()))
                {
                    if (1 == index)
                    {
                        // Interface name is second item on line
                        if (0 == strcmp(ifaceName, next))
                            seekingIface = false;  // we found it
                    }
                    else if (3 == index)
                    {
                        unsigned int count;
                        int result;
                        if (1 != (result = sscanf(next, "%u", &count)))
                        {
                            if (seekingIface)
                            {
                                skipCount = 0;
                            }
                            else
                            {
                                PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: invalid 'Count' %s for iface %s?\n",
                                                next, ifaceName);
                                fclose(filePtr);
                                return false;
                            }
                        }
                        else if (seekingIface)
                        {
                            skipCount = count;
                        }
                        else
                        {
                            if (0 == count)
                            {
                                fclose(filePtr);
                                return true;
                            }
                            numGroups = count;
                        }
                    }
                    index++;
                }      
                if (index < 3)
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: no 'Count' for iface %s?\n", ifaceName);
                    fclose(filePtr);
                    return false;
                }
            }
            else 
            {
                ASSERT(0 != numGroups);
                ProtoTokenator tk(buffer);
                const char* groupText = tk.GetNextItem();  // first token is group addr in hex
                UINT32 groupInt;  // note it's already in Network Byte Order (Big Endian)
                if (1 != sscanf(groupText, "%x", &groupInt))
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: invalid group addr for iface %s?\n", ifaceName);
                    fclose(filePtr);
                    return false;
                }
                ProtoAddress groupAddr;
                groupAddr.SetRawHostAddress(ProtoAddress::IPv4, (const char*)&groupInt, 4);
                if (!addrList.Insert(groupAddr))
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: unable to insert IPv4 group addr to list!\n");
                    fclose(filePtr);
                    return false;
                }
                numGroups--;
                if (0 == numGroups)
                {
                    fclose(filePtr);
                    return true;
                }
            } 
        }
        fclose(filePtr);
        if (0 != numGroups)
        {
            PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: missing group listings for interface %s\n", ifaceName);
            return false;
        }
        else if (seekingIface)
        {
            PLOG(PL_WARN, "ProtoNet::GetGroupMemberships() warning: requested interface %s not listed?!\n", ifaceName);
        }       
        else
        {
            ASSERT(0);  // shouldn't get here
        }
        return true;
    }
    else if (ProtoAddress::IPv6 == addrType)
    {
        // TBD - is there a netlink interface for this instead?
        FILE* filePtr = fopen("/proc/net/igmp6", "r");
        if (NULL == filePtr)
        {
            PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() fopen(/proc/net/igmp) error: %s\n", GetErrorString());
            return false;
        }
        unsigned len;
        char buffer[256];
        while (0 != (len = Readline(filePtr, buffer, 255)))
        {
            // Format of lines are <index> <ifaceName> <address> ...
            ProtoTokenator tk(buffer);
            const char* next;
            bool match = false;
            unsigned int index = 0;
            while (NULL != (next = tk.GetNextItem()))
            {
                if (1 == index)
                {
                    if (0 == strcmp(next, ifaceName))
                        match = true;
                    else
                        break;
                }             
                else if (2 == index)
                {
                    ASSERT(match);
                    break;  // 'next' points to address string
                }    
                index++;      
            }  // end while (tk.GetNextItem())
            if (match)
            {
                if (NULL == next)
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: missing IPv6 group addr %s for interface %s\n",
                                    next, ifaceName);
                    fclose(filePtr);
                    return false;
                }
                if (strlen(next) < 32)
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: incomplete IPv6 group addr %s for interface %s\n",
                                    next, ifaceName);
                    fclose(filePtr);
                    return false;
                }
                char addr[16];  // will contain IPv6 group addr
                for (int i = 0; i < 32; i += 8)
                {
                    //char wordText[9];
                    //wordText[8] = '\0';
                    //strncpy(wordText, next + i, 8);
                    UINT32 word;
                    if (1 != sscanf(next + i, "%8x", &word))
                    {
                        PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: invalid IPv6 group addr %s for interface %s\n",
                                       next, ifaceName);
                        fclose(filePtr);
                        return false;
                    }
                    word = htonl(word);
                    memcpy(addr + i/2, &word, 4);
                }
                ProtoAddress groupAddr;
                groupAddr.SetRawHostAddress(ProtoAddress::IPv6, addr, 16);
                if (!addrList.Insert(groupAddr))
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: unable to insert IPv6 group addr to list!\n");
                    fclose(filePtr);
                    return false;
                }
            }
        }  // end while (Readline())
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: invalid address typs\n");
        return false;
    }
}  // end ProtoNet::GetGroupMemberships()



class LinuxNetMonitor : public ProtoNet::Monitor
{
    public:
        LinuxNetMonitor();
        ~LinuxNetMonitor();
        
        bool Open();
        void Close();
        bool GetNextEvent(Event& theEvent);
        
    private:
        // Since a Linux netlink message may have multiple 
        // network interface events, we cache them in a linked
        // list for retrieval by the GetNextEvent() method    
        class EventItem : public Event, public ProtoList::Item
        {
            public:
                EventItem();
                ~EventItem();
        };  // end class LinuxNetMonitor::EventItem         
        class EventList : public ProtoListTemplate<EventItem> {};  
        
        class Interface : public ProtoTree::Item
        {
            public:
                Interface(unsigned int index, const char* name);
                ~Interface();
                
                void SetName(const char* ifName)
                    {strncpy(iface_name, ifName, IFNAMSIZ);} 
                const char* GetName() const
                    {return iface_name;}
                unsigned int GetIndex() const
                    {return iface_index;}
                
            private:
                const char* GetKey() const
                    {return ((const char*)&iface_index);}   
                unsigned int GetKeysize() const
                    {return (sizeof(unsigned int) << 3);} 
                    
                char            iface_name[IFNAMSIZ+1];
                unsigned int    iface_name_bits;
                unsigned int    iface_index;
        };  // end class LinuxNetMonitor::Interface
        class InterfaceList : public ProtoTreeTemplate<Interface>
        {
            public:
                Interface* FindInterface(unsigned int ifIndex)
                    {return Find((char*)&ifIndex, sizeof(unsigned int) << 3);}
        };
        
        InterfaceList   iface_list;
        EventList       event_list;
        EventList       event_pool;
            
};  // end class LinuxNetMonitor


LinuxNetMonitor::Interface::Interface(unsigned int index, const char* name)
 : iface_index(index)
{
    iface_name[IFNAMSIZ] = '\0';
    SetName(name);
}

LinuxNetMonitor::Interface::~Interface()
{
}

// This is the implementation of the ProtoNet::Monitor::Create()
// static method (our Linux-specific factory)
ProtoNet::Monitor* ProtoNet::Monitor::Create()
{
    return static_cast<ProtoNet::Monitor*>(new LinuxNetMonitor);
}  // end ProtoNet::Monitor::Create()
        
LinuxNetMonitor::LinuxNetMonitor()
{
}

LinuxNetMonitor::~LinuxNetMonitor()
{
}

bool LinuxNetMonitor::Open()
{
    if (IsOpen()) Close();
    if (0 > (descriptor = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)))
    {
        PLOG(PL_ERROR, "LinuxNetMonitor::Open() socket() error: %s\n", 
                GetErrorString());
        return false;
    }    
    
    // Send a netlink request message to subscribe to the
    // RTMGRP_IPV4_IFADDR and RTMGRP_IPV6_IFADDR groups for 
    // network interface status update messages
    struct sockaddr_nl localAddr;
    localAddr.nl_family = AF_NETLINK;
	localAddr.nl_pid = 0;  // system will assign us a unique netlink port id
	localAddr.nl_groups |= RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;// | RTMGRP_IPV6_IFINFO;
	if (0 > bind(descriptor, (struct sockaddr*) &localAddr, sizeof(localAddr)))
    {
        PLOG(PL_ERROR, "LinuxNetMonitor::Open() bind() error: %s\n", 
                GetErrorString());
        Close();
        return false;
    }
    if (!ProtoNet::Monitor::Open())
    {
        Close();
        return false;
    }
    return true;
}  // end LinuxNetMonitor::Open()

void LinuxNetMonitor::Close()
{
    if (IsOpen())
    {
        ProtoNet::Monitor::Close();
        close(descriptor);
        descriptor = INVALID_HANDLE;
    }
    event_list.Destroy();
    event_pool.Destroy();
}  // end LinuxNetMonitor::Close()

bool LinuxNetMonitor::GetNextEvent(Event& theEvent)
{
    // 0) Initialize event instance
    theEvent.SetType(Event::UNKNOWN_EVENT);
    theEvent.SetInterfaceIndex(0);
    theEvent.AccessAddress().Invalidate();
    
    // 1) Get next event from list or recv() from netlink
    EventItem* eventItem = event_list.RemoveHead();
    if (NULL == eventItem)
    {
        // There were not any existing events in our list, so
        // get more from netlink
        char buffer[4096];
        struct nlmsghdr* nlh = (struct nlmsghdr*)buffer;
        ssize_t result = recv(descriptor, buffer, 4096, 0);
        if (result < 0)
        {
            switch(errno)
            {
                case EINTR:
                case EAGAIN:
                    theEvent.SetType(Event::NULL_EVENT);
                    return true;
                default:
                    PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() recv(PEEK) error: %s", GetErrorString());
                    return false;
            }
        }
        else if (0 == result)
        {
            theEvent.SetType(Event::NULL_EVENT);
            return true;
        }
        unsigned int msgLen = (unsigned int)result;
        for (;(NLMSG_OK(nlh, msgLen)) && (nlh->nlmsg_type != NLMSG_DONE); nlh = NLMSG_NEXT(nlh, msgLen))
        {
            eventItem = NULL;
            switch (nlh->nlmsg_type)
            {
                case RTM_NEWLINK:
                case RTM_DELLINK:
                {
                    eventItem = event_pool.RemoveHead();
                    if (NULL == eventItem) eventItem = new EventItem();
                    if (NULL == eventItem)
                    {
                        PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new EventItem error: %s\n", GetErrorString());
                        theEvent.SetType(Event::NULL_EVENT);
                        return false;
                    }
                    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(nlh);
                    if (RTM_NEWLINK == nlh->nlmsg_type)
                    {
                        eventItem->SetType(Event::IFACE_UP);
                    }
                    else
                    {
                        eventItem->SetType(Event::IFACE_DOWN);
                    }
                    eventItem->SetInterfaceIndex(ifi->ifi_index);
                    // TBD - look through the message RTA's (if any) for the iface name?
                    event_list.Append(*eventItem);
                    break;
                }
                case RTM_NEWADDR:
                case RTM_DELADDR:
                {
                    struct ifaddrmsg* ifa = (struct ifaddrmsg*) NLMSG_DATA(nlh);
                    // For now, we only look for IPv4 or Ipv6 addresses (TBD - other addr families?)
                    if ((AF_INET != ifa->ifa_family) && (AF_INET6 != ifa->ifa_family))  continue;
                    struct rtattr* rth = IFA_RTA(ifa);
                    int rtl = IFA_PAYLOAD(nlh);
                    for (;rtl && RTA_OK(rth, rtl); rth = RTA_NEXT(rth,rtl))
                    {
                        if (IFA_LOCAL == rth->rta_type)
                        {
                            eventItem = event_pool.RemoveHead();
                            if (NULL == eventItem) eventItem = new EventItem();
                            if (NULL == eventItem)
                            {
                                PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new EventItem error: %s\n", GetErrorString());
                                theEvent.SetType(Event::NULL_EVENT);
                                return false;
                            }
                            if (RTM_NEWADDR == nlh->nlmsg_type)
                                eventItem->SetType(Event::IFACE_ADDR_NEW);
                            else
                                eventItem->SetType(Event::IFACE_ADDR_DELETE);
                            eventItem->SetInterfaceIndex(ifa->ifa_index);
                            if (AF_INET == ifa->ifa_family)
                                eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv4, (char*)RTA_DATA(rth), 4);
                            else //if (AF_INET6 == ifa->ifa_family)
                                eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv6, (char*)RTA_DATA(rth), 16);
                            event_list.Append(*eventItem);
                        }
                        else if (IFA_ADDRESS == rth->rta_type)
                        {
                            // Note that Linux doesn't seem to reliably (or at all) issue RTM_DELLINK messages
                            // So - as a cheat hack, we're going to issue IFACE_DOWN event when the link local
                            // address is deleted (fingers crossed!)
                            if ((RTM_DELADDR == nlh->nlmsg_type) &&
                                (RT_SCOPE_LINK == ifa->ifa_scope))
                            {
                                eventItem = event_pool.RemoveHead();
                                if (NULL == eventItem) eventItem = new EventItem();
                                if (NULL == eventItem)
                                {
                                    PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new EventItem error: %s\n", GetErrorString());
                                    theEvent.SetType(Event::NULL_EVENT);
                                    return false;
                                }
                                eventItem->SetType(Event::IFACE_DOWN);
                                eventItem->SetInterfaceIndex(ifa->ifa_index);
                                event_list.Append(*eventItem);
                                break; // we break because multiple link down indications aren't need
                                       // BUT if we want to link local addr changes here ...
                            }
                        }
                    }
                    break;
                }
                default:
                    //TRACE("OTHER message type %d\n", nlh->nlmsg_type);
                    break;
            }  // end switch(nlh->nlmsg_type) 
            
            if (NULL != eventItem)
            {
                // We enqueued and event, we need to set its interface name if possible
                // 1) Do we already know this interface?
                unsigned int ifIndex = eventItem->GetInterfaceIndex();
                Interface* iface = iface_list.FindInterface(ifIndex);
                if ((NULL == iface) || (Event::IFACE_DOWN != eventItem->GetType()))
                {
                    // Get the interface name (if it's new or possibly changed
                    char ifName[IFNAMSIZ+1];
                    ifName[IFNAMSIZ] = '\0';
                    if (ProtoNet::GetInterfaceName(ifIndex, ifName, IFNAMSIZ))
                    {
                        eventItem->SetInterfaceName(ifName);
                        if ((NULL == iface) && (Event::IFACE_DOWN != eventItem->GetType()))
                        {
                            if (NULL != (iface = new Interface(ifIndex, ifName)))
                                iface_list.Insert(*iface);
                            else
                                PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new Interface error: %s\n", GetErrorString());
                        }
                    }
                    else
                    {
                        if (NULL != iface) eventItem->SetInterfaceName(iface->GetName());
                        PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() warning: unable to get interface name for index %d\n", ifIndex);
                    }                    
                }
                if ((Event::IFACE_DOWN == eventItem->GetType()) && (NULL != iface))
                {
                    eventItem->SetInterfaceName(iface->GetName());
                    iface_list.Remove(*iface);
                    delete iface;
                }
            }
            
            
        }  // end for NLMSG_OK ...
        eventItem = event_list.RemoveHead();
    }  // end if (NULL == eventItem)
    if (NULL == eventItem)
    {
        theEvent.SetType(Event::NULL_EVENT);
    }
    else
    {    
        theEvent = static_cast<Event&>(*eventItem);
        event_pool.Append(*eventItem);
    }
    return true;
}  // end LinuxNetMonitor::GetNextEvent()

LinuxNetMonitor::EventItem::EventItem()
{
}

LinuxNetMonitor::EventItem::~EventItem()
{
}
