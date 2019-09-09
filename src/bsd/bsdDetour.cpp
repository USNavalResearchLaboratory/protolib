
#include "protoDetour.h"
#include "protoSocket.h"
#include "protoRouteMgr.h"  // for ifAddr/ifIndex mapping

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>  // for fcntl(), etc

class BsdDetour : public ProtoDetour
{
    public:
        BsdDetour();
        ~BsdDetour();
        
        bool Open(int hookFlags                     = 0, 
                  const ProtoAddress& srcFilterAddr = PROTO_ADDR_NONE, 
                  unsigned int        srcFilterMask = 0,
                  const ProtoAddress& dstFilterAddr = PROTO_ADDR_NONE,
                  unsigned int        dstFilterMask = 0,
                  int                 dscpValue     = -1);
        void Close();
        bool Recv(char*         buffer, 
                  unsigned int& numBytes, 
                  Direction*    direction = NULL,
                  ProtoAddress* srcMac = NULL,
                  unsigned int* ifIndex = NULL);
        
        bool Allow(const char* buffer, unsigned int numBytes);
        bool Drop();
        bool Inject(const char* buffer, unsigned int numBytes);
        
        virtual bool SetMulticastInterface(const char* interfaceName);
        
        enum {DIVERT_PORT = 2998};
                    
    private:
        enum Action
        {
            INSTALL,
            DELETE
        };
        bool SetIPFirewall(Action action,
                           int hookFlags ,
                           const ProtoAddress& srcFilterAddr, 
                           unsigned int        srcFilterMask,
                           const ProtoAddress& dstFilterAddr,
                           unsigned int        dstFilterMask);
        
        ProtoAddress            pkt_addr;  // iface IP addr for inbound, unspecified for outbound
        
        int                     domain;    // AF_INET or AF_INET6
        int                     hook_flags;
        ProtoAddress            src_filter_addr;
        unsigned int            src_filter_mask;
        ProtoAddress            dst_filter_addr;
        unsigned int            dst_filter_mask;
        
        unsigned short          rule_number[3];
        unsigned int            rule_count;
        bool                    inject_only;
        
        
        class AddressListItem : public ProtoTree::Item
        {
            public:
                AddressListItem(const ProtoAddress& addr, unsigned int ifIndex);
            
                unsigned int GetInterfaceIndex() const
                    {return if_index;}
            
                // ProtoTree::Item overrides
                const char* GetKey() const
                    {return if_addr.GetRawHostAddress();}
                unsigned int GetKeysize() const
                    {return (if_addr.GetLength() << 3);}
                    
            private:
                ProtoAddress    if_addr;
                unsigned int    if_index;
        };
        
        ProtoTree               if_addr_list;  // for if_addr -> ifIndex lookup
            
};  // end class BsdDetour
    
ProtoDetour* ProtoDetour::Create()
{
    return static_cast<ProtoDetour*>(new BsdDetour());   
}  // end ProtoDetour::Create(), (void*)nl_head  

BsdDetour::AddressListItem::AddressListItem(const ProtoAddress& ifAddr, unsigned int ifIndex)
 : if_addr(ifAddr), if_index(ifIndex)
{
}

BsdDetour::BsdDetour()
 : hook_flags(0), rule_count(0), inject_only(false)
{
    memset(&pkt_addr, 0, sizeof(struct sockaddr_storage));
}

BsdDetour::~BsdDetour()
{
    Close();
}

bool BsdDetour::SetIPFirewall(Action              action,
                              int                 hookFlags ,
                              const ProtoAddress& srcFilterAddr, 
                              unsigned int        srcFilterMask,
                              const ProtoAddress& dstFilterAddr,
                              unsigned int        dstFilterMask) 
{
    // 1) IPv4 or IPv6 address family?
    const char* cmd;
    if (srcFilterAddr.GetType() != dstFilterAddr.GetType())
    {
        PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() error: inconsistent src/dst filter addr families\n");
        return false;
    }
    else if (ProtoAddress::IPv4 == srcFilterAddr.GetType())
    {
        cmd = "/sbin/ipfw";  // IPv4 ipfw
    }
    else if (ProtoAddress::IPv6 == srcFilterAddr.GetType())
    {
        cmd = "/sbin/ipfw"; // IPv6 ipfw (ipfw2 does both, in theory)
    }
    else
    {
        PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() error: unspecified filter addr family\n");
        return false;
    }
    // 2) For (DELETE == action) re-interpret hookFlags value as rule number
    if (DELETE == action) hookFlags++;
    // 3) For which firewall hooks?
    while (0 != hookFlags)
    {
        // Make and install "iptables" firewall rules
        const size_t RULE_MAX = 511;
        char rule[RULE_MAX+1];
            
        if (INSTALL == action)
        {
            if (rule_count > 2)
            {
                PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() max ruleCount exceeded!\n");
                return false;    
            }
        
            const char* target;
            if (0 != (hookFlags & OUTPUT))
            {
                target = "out";
                hookFlags &= ~OUTPUT;
            }
            else if (0 != (hookFlags & INPUT))
            {
                target = "in";
                hookFlags &= ~INPUT;
            }
            else if (0 != (hookFlags & FORWARD))
            {
                target = "via any";
                hookFlags &= ~FORWARD;
            }
            else
            {
                break;  // all flags have been processed
            }

            // cmd  = "ipfw" or "ip6fw"
            const char* f = (ProtoAddress::IPv4 == srcFilterAddr.GetType()) ? 
                    "ip" : "ipv6";
            sprintf(rule, "%s add divert %hu %s ", cmd, (UINT16)DIVERT_PORT, f);
            if (0 != srcFilterMask)
            {
                strcat(rule, "from ");
                size_t len = strlen(rule);
                if (!srcFilterAddr.GetHostString(rule+len, RULE_MAX - len))
                {
                    PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() error: bad source addr filter\n");
                    return false;
                } 
                len = strlen(rule);
                sprintf(rule+len, "/%u ", srcFilterMask);
            }
            else
            {
                size_t len = strlen(rule);
                sprintf(rule+len, "from any ");
            }
            if (0 != dstFilterMask)
            {
                strcat(rule, "to ");
                size_t len = strlen(rule);
                if (!dstFilterAddr.GetHostString(rule+len, RULE_MAX - len))
                {
                    PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() error: bad destination addr filter\n");
                    return false;
                } 
                len = strlen(rule);
                sprintf(rule+len, "/%u ", dstFilterMask);
            }
            else
            {
                size_t len = strlen(rule);
                sprintf(rule+len, "to any ");
            }

            // target = "in", "out", or "via any"
            strcat(rule, target);
        }
        else  // (DELETE == action)
        {
            sprintf(rule, "%s delete %d\n", cmd, hookFlags - 1);
            hookFlags = 0;
        }
                   
        // Add redirection so we can get stderr result
        strcat(rule, " 2>&1");
        
        FILE* p = popen(rule, "r");
        if (NULL != p)
        {
            char feedback[256];
            if (0 == fread(feedback, 1, 256, p))
            {
                PLOG(PL_WARN, "BsdDetour::SetIPFirewall() error: fread(%s): %s\n",
                        rule, GetErrorString());
            }
            char* ptr = strchr(feedback, '\n');
            if (NULL != ptr) *ptr = '\0';
            feedback[255] = '\0';
            if (0 == pclose(p))
            {
                if (INSTALL == action)
                {
                    // Add firewall rule number to list for delete on close
                    if (1 != sscanf(feedback, "%05hu\n", rule_number+rule_count))
                    {
                        PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() warning: couldn't record firewall rule number\n");
                        return true;
                    }
                    rule_count++;
                }
            }
            else
            {
                PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() \"%s\" error: %s\n",
                         rule, feedback);
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "BsdDetour::SetIPFirewall() error: popen(%s): %s\n",
                    rule, GetErrorString());
            return false;
        }
    }  // end while (0 != hookFlags)
    return true;
}  // end BsdDetour::SetIPFirewall()

bool BsdDetour::Open(int                 hookFlags, 
                     const ProtoAddress& srcFilterAddr, 
                     unsigned int        srcFilterMask,
                     const ProtoAddress& dstFilterAddr,
                     unsigned int        dstFilterMask,
                     int                 /*dscpValue*/)  // TBD - support DSCP
{
    if (IsOpen()) Close();
    
    // Check for "inject-only" mode and "rewire appropriately
    inject_only = false;
    if (0 != (hookFlags & INJECT))
    {
        // 0) Open raw socket for optional packet injection use
        //if (0 > (raw_fd = socket(domain, SOCK_RAW, IPPROTO_RAW)))
        if (0 > (descriptor = socket(PF_INET, SOCK_RAW, IPPROTO_RAW)))  // or can we IPv6 on an AF_INET/HDRINCL socket?
        {
            PLOG(PL_ERROR, "BsdDetour::Open() socket(IPPROTO_RAW) error: %s\n", 
                    GetErrorString());
            return false;
        }
        domain = AF_INET;
        if (AF_INET == domain)
        {
            // Note: no IP_HDRINCL for IPv6 raw sockets ?
            int enable = 1;
            if (setsockopt(descriptor, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)))
            {
                PLOG(PL_ERROR, "BsdDetour::Open() setsockopt(IP_HDRINCL) error: %s\n",
                        GetErrorString());
                Close();
                return false;
            }
        }
        ProtoDetour::Open();
        StopInputNotification();
        
        // Set to non-blocking for our purposes (TBD) Add a SetBlocking() method    
        if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) | O_NONBLOCK))
        {
            PLOG(PL_ERROR, "BsdDetour::Open() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
            Close();
            return false;
        }
        inject_only = true;
        return true;
    }
    
    
    if (srcFilterAddr.GetType() != dstFilterAddr.GetType())
    {
        PLOG(PL_ERROR, "BsdDetour::Open() error: inconsistent src/dst filter addr families\n");
        return false;
    }
    else if (ProtoAddress::IPv4 == srcFilterAddr.GetType())
        
    {
        domain = AF_INET;
    }
    else if (ProtoAddress::IPv6 == srcFilterAddr.GetType())
    {
        domain = AF_INET6;
    }
    else
    {
        PLOG(PL_ERROR, "BsdDetour::Open() error: unspecified filter addr family\n");
        return false;
    }
    
    // Setup ipfw rule(s) ...
    // Save parameters for firewall rule removal
    hook_flags = hookFlags;
    src_filter_addr = srcFilterAddr;
    src_filter_mask = srcFilterMask;
    dst_filter_addr = dstFilterAddr;
    dst_filter_mask = dstFilterMask;
    if (0 != hookFlags)
    {
         if (!SetIPFirewall(INSTALL, hookFlags, 
                            srcFilterAddr, srcFilterMask,
                            dstFilterAddr, dstFilterMask))
        {
            PLOG(PL_ERROR, "BsdDetour::Open() error: couldn't install firewall rules\n");   
            Close();
            return false;
        }     
    }
    
    // Open a divert socket ...
    if ((descriptor = socket(domain, SOCK_RAW, IPPROTO_DIVERT)) < 0)
    {
        PLOG(PL_ERROR, "BsdDetour::Open() socket() error: %s\n", GetErrorString());
        Close();
        return false;   
    }
    
    // Bind to our ipfw divert "port"
    UINT16 ipfwPort = DIVERT_PORT;  // (TBD) manage port numbers dynamically?
    struct sockaddr_storage socketAddr;
    socklen_t addrSize;
    if (AF_INET == domain)
    {
        addrSize = sizeof(struct sockaddr_in);
        memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in));    
        ((struct sockaddr_in*)&socketAddr)->sin_family = AF_INET;
        ((struct sockaddr_in*)&socketAddr)->sin_port = htons(ipfwPort);
    }
    else // if (AF_INET6 == domain)
    {
        addrSize = sizeof(struct sockaddr_in6);
        memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in6));    
        ((struct sockaddr_in6*)&socketAddr)->sin6_family = AF_INET6;
        ((struct sockaddr_in6*)&socketAddr)->sin6_port = htons(ipfwPort);
        ((struct sockaddr_in6*)&socketAddr)->sin6_flowinfo = 0;
    }
    if (bind(descriptor, (struct sockaddr*)&socketAddr, addrSize) < 0)
    {
        PLOG(PL_ERROR, "BsdDetour::Open() bind() error: %s\n", GetErrorString());
        Close();
        return false;  
    }
    
    if (!ProtoDetour::Open())
    {
        PLOG(PL_ERROR, "BsdDetour::Open() ProtoChannel::Open() error\n");
        Close();
        return false;   
    }
    
    // Set up ProtoAddressList for lookup of ifAddr -> ifIndex
    ProtoRouteMgr* rtMgr = ProtoRouteMgr::Create();
    if (NULL == rtMgr)
    {
        PLOG(PL_ERROR, "BsdDetour::Open(): ProtoRouteMgr::Create() error: %s\n", GetErrorString());
        Close();
        return false;        
    }
    if (!rtMgr->Open())
    {
        PLOG(PL_ERROR, "BsdDetour::Open(): error: unable to open ProtoRouteMgr\n");
        delete rtMgr;
        Close();
        return false;
    }
    unsigned int ifIndexArray[256];
    unsigned int ifCount = ProtoSocket::GetInterfaceIndices(ifIndexArray, 256);
    if (0 == ifCount)
    {
        PLOG(PL_ERROR, "BsdDetour::Open(): warning: no network interface indices were found.\n");
    }
    else if (ifCount > 256)
    {
        PLOG(PL_ERROR, "BsdDetour::Open(): warning: found network interfaces indices exceeding maximum count.\n");
        ifCount = 256;
    }
    // Add any IP addrs assigned to this iface to our if_addr_list
    for (unsigned int i = 0; i < ifCount; i++)
    {
        unsigned int ifIndex = ifIndexArray[i];
        ProtoAddressList tempList;
        if (!rtMgr->GetInterfaceAddressList(ifIndex, ProtoAddress::IPv4, tempList))
        {
            PLOG(PL_ERROR, "BsdDetour::Open() warning: couldn't retrieve IPv4 address for iface index: %d\n", ifIndex);
        }
        if (!rtMgr->GetInterfaceAddressList(ifIndex, ProtoAddress::IPv6, tempList))
        {
            PLOG(PL_ERROR, "BsdDetour::Open() warning: couldn't retrieve IPv6 address for iface index: %d\n", ifIndex);
        }
        ProtoAddressList::Iterator it(tempList);
        ProtoAddress addr;
        while (it.GetNextAddress(addr))
        {
            AddressListItem* ifItem = new AddressListItem(addr, ifIndex);
            if (NULL == ifItem)
            {
                PLOG(PL_ERROR, "BsdDetour::Open() new AddressListItem error: %s\n", GetErrorString());
                delete rtMgr;
                Close();
                return false;
            }
            if_addr_list.Insert(*ifItem);
        }
    }
    delete rtMgr;
    return true;
}  // end BsdDetour::Open()  

bool BsdDetour::SetMulticastInterface(const char* interfaceName)
{
    ASSERT(IsOpen());
    if (NULL != interfaceName)
    {
        int result;
#ifdef HAVE_IPV6
        if (AF_INET6 == domain)  
        {
            unsigned int interfaceIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
            result = setsockopt(descriptor, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                (char*)&interfaceIndex, sizeof(interfaceIndex));
        }
        else 
#endif // HAVE_IPV6 
        {  
            struct in_addr localAddr;
            ProtoAddress interfaceAddress;
            if (ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
			{
                localAddr.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else
            {
                PLOG(PL_ERROR, "BsdDetour::SetMulticastInterface() invalid interface name\n");
                return false;
            }
            result = setsockopt(descriptor, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localAddr, 
                                sizeof(localAddr));
        }
        if (result < 0)
        { 
            PLOG(PL_ERROR, "BsdDetour: setsockopt(IP_MULTICAST_IF) error: %s\n", 
                     GetErrorString());
            return false;
        }         
    }     
    return true;
}  // end BsdDetour::SetMulticastInterface()

void BsdDetour::Close()
{
    if (0 != hook_flags)
    {
        for (unsigned int i =0; i < rule_count; i++)
            SetIPFirewall(DELETE, (int)rule_number[i],
                          src_filter_addr, src_filter_mask,
                          dst_filter_addr, dst_filter_mask);
        rule_count = 0;
        hook_flags = 0;   
    }
    if (descriptor >= 0)
    {
        ProtoDetour::Close();
        close(descriptor);
        descriptor = INVALID_HANDLE;  
    }
}  // end BsdDetour::Close()

bool BsdDetour::Recv(char*          buffer, 
                     unsigned int&  numBytes, 
                     Direction*     direction, 
                     ProtoAddress*  srcMac,
                     unsigned int*  ifIndex)
{
    // (TBD) can BSD divert socket get src MAC addr?
    // (It looks like we _could_ get a full MAC header if we
    //  use a "layer 2" hook to capture packets in our 
    //  ipfw rule)
    if (NULL != srcMac) srcMac->Invalidate();  
    
    struct sockaddr_storage sockAddr;
    socklen_t addrLen = sizeof(sockAddr);
    ssize_t result = recvfrom(descriptor, buffer, numBytes, 0, 
                             (struct sockaddr*)&sockAddr, &addrLen);
    if (result < 0)
    {
        pkt_addr.Invalidate();
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                numBytes = 0;
                if (NULL != direction) 
                    *direction = UNSPECIFIED;
                return true;
            default:
                numBytes = 0;
                PLOG(PL_ERROR, "BsdDetour::Recv() recvfrom() error: %s\n", GetErrorString());
                return false;
        }   
    }
    else
    {
        pkt_addr.SetSockAddr(*((struct sockaddr*)&sockAddr));
        numBytes = result;  
        if (NULL != direction) 
        {
            if (pkt_addr.IsUnspecified())
                *direction = OUTBOUND;
            else
                *direction = INBOUND;
        }    
        if (NULL != ifIndex)
        {
            if (pkt_addr.IsUnspecified())
            {
                *ifIndex = 0;
            }
            else
            {
                // Lookup "ifIndex" for given interface IP address
                AddressListItem* ifItem = 
                    static_cast<AddressListItem*>(if_addr_list.Find(pkt_addr.GetRawHostAddress(), pkt_addr.GetLength() << 3));
                if (NULL != ifItem)
                    *ifIndex = ifItem->GetInterfaceIndex();
                else 
                    *ifIndex = 0;
            }
        }
        return true;
    }
}  // end BsdDetour::Recv()

bool BsdDetour::Allow(const char* buffer, unsigned int numBytes)
{
    if (pkt_addr.IsValid())
    {
        socklen_t addrSize = (ProtoAddress::IPv6 == pkt_addr.GetType()) ?
                                sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
        ssize_t result = sendto(descriptor, buffer, (size_t)numBytes, 0, 
                               &pkt_addr.GetSockAddr(), addrSize);
        if (result < 0)
        {
            PLOG(PL_ERROR, "BsdDetour::Allow() sendto() error: %s\n", GetErrorString());
            return false;
        }
        pkt_addr.Invalidate();
        return true;
    }
    else
    {   
        PLOG(PL_ERROR, "BsdDetour::Allow() error: no packet pending\n");
        return false;
    }
}  // end BsdDetour::Allow()

bool BsdDetour::Drop()
{   
    if (pkt_addr.IsValid())
    {
        pkt_addr.Invalidate();
        return true;
    }
    else
    {   
        PLOG(PL_ERROR, "BsdDetour::Drop() error: no packet pending\n");
        return false;
    }
}  // end BsdDetour::Drop()

bool BsdDetour::Inject(const char* buffer, unsigned int numBytes)
{
    if (inject_only)
    {
        unsigned char version = buffer[0];
        version >>= 4;
        ProtoAddress dstAddr;
        socklen_t addrLen;
        if (4 == version)
        {
            dstAddr.SetRawHostAddress(ProtoAddress::IPv4, buffer+16, 4);
            addrLen = sizeof(struct sockaddr);
        }
        else if (6 == version)
        {
            //PLOG(PL_ERROR, "BsdDetour::Inject() IPv6 injection not yet supported!\n");
            dstAddr.SetRawHostAddress(ProtoAddress::IPv6, buffer+24, 16);
            addrLen = sizeof(struct sockaddr_in6);
            //return false;   
        }
        else
        {
            PLOG(PL_ERROR, "BsdDetour::Inject() unknown IP version!\n");
            return false;   
        }
        size_t result = sendto(descriptor, buffer, numBytes, 0, 
                               &dstAddr.GetSockAddr(), addrLen);
        if (result != numBytes)
        {
            PLOG(PL_ERROR, "BsdDetour::Inject() sendto() error: %s\n", GetErrorString());
            return false;
        }
        return true;
    }
    else
    {
        size_t result = write(descriptor, buffer, (size_t)numBytes);
        if (result != numBytes)
        {
            PLOG(PL_ERROR, "BsdDetour::Inject() write() error: %s\n", GetErrorString());
            return false;     
        }
    }
    return true;
}  // end BsdDetour::Inject()

