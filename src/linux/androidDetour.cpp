#include "protoDetour.h"
#include "protoCap.h"  // used for packet injection
#include "protoSocket.h"
#include "protoTree.h" // for mapping ifName -> ifIndex

#include <stdio.h>
#include <asm/types.h>
#include <sys/types.h>
#include <unistd.h>          // for getpid()
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter_ipv4/ip_queue.h>
#include <fcntl.h>  // for fcntl(), etc
#include <linux/if_ether.h>  // for ETH_P_IP
#include <net/if_arp.h>   // for ARPHRD_ETHER

/** NOTES: 
 *
 * 1) Must run "modprobe ip_queue" before running
 *    programs using this class
 *
 */

// This is the "old" LinuxDetour code that uses the "ip_queue"
// iptables hook instead of the newer "ipnetfilter_queue" code

class LinuxDetour : public ProtoDetour
{
    public:
        LinuxDetour();
        ~LinuxDetour();
        
        bool Open(int hookFlags                     = 0, 
                  const ProtoAddress& srcFilterAddr = PROTO_ADDR_NONE, 
                  unsigned int        srcFilterMask = 0,
                  const ProtoAddress& dstFilterAddr = PROTO_ADDR_NONE,
                  unsigned int        dstFilterMask = 0,
                  int                 dscpValue     = -1);
        void Close();
        
        virtual bool Recv(char*         buffer, 
                          unsigned int& numBytes, 
                          Direction*    direction = NULL, // optionally learn INBOUND/OUTBOUND direction of pkt
                          ProtoAddress* srcMac = NULL,    // optionally learn previous hop source MAC addr 
                          unsigned int* ifIndex = NULL);  // optionally learn which iface (INBOUND only)
        
        bool Allow(const char* buffer, unsigned int numBytes);
        bool Drop();
        bool Inject(const char* buffer, unsigned int numBytes);
        
        virtual bool SetMulticastInterface(const char* interfaceName);
                    
    private:
        enum Action
        {
            INSTALL,
            DELETE
        };
            
            
        // used for name -> index lookup
        class IfNameItem : public ProtoTree::Item
        {
            public:
                IfNameItem();
                ~IfNameItem();
                bool Init(const char* ifName, unsigned int ifIndex);
                
                const char* GetIfName() const {return if_name;}
                unsigned int GetIfIndex() const {return if_index;}
                
                // Required ProtoTree::Item overrides
                const char* GetKey() const {return if_name;}
                unsigned int GetKeysize() const {return if_name_size;}
            
            private:
                char*           if_name;
                unsigned int    if_name_size;  // in bits
                unsigned int    if_index;
        };  // end class LinuxDetour::IfNameItem
            
        bool SetIPTables(Action action,
                         int hookFlags ,
                         const ProtoAddress& srcFilterAddr, 
                         unsigned int        srcFilterMask,
                         const ProtoAddress& dstFilterAddr,
                         unsigned int        dstFilterMask,
                         int                 dscpValue);
        
        int                     pid;
        int                     seq;
        struct nlmsghdr         nlh;
        struct ipq_packet_msg   pkt_msg;
        int                     raw_fd;  // for packet injection
        int                     hook_flags;
        ProtoAddress            src_filter_addr;
        unsigned int            src_filter_mask;
        ProtoAddress            dst_filter_addr;
        unsigned int            dst_filter_mask;
        int                     dscp_value;
        ProtoTree               if_name_tree;
            
};  // end class LinuxDetour
    
ProtoDetour* ProtoDetour::Create()
{
    return static_cast<ProtoDetour*>(new LinuxDetour());   
}  // end ProtoDetour::Create(), (void*)nl_head  


LinuxDetour::LinuxDetour()
 : seq(0), raw_fd(-1), hook_flags(0), dscp_value(-1)
{
 
}

LinuxDetour::~LinuxDetour()
{
    Close();
}


LinuxDetour::IfNameItem::IfNameItem()
 : if_name(NULL), if_index(0)
{
}

bool LinuxDetour::IfNameItem::Init(const char* ifName, unsigned int ifIndex)
{
    if (NULL != if_name) delete[] if_name;
    size_t nameLen = strlen(ifName);
    if (nameLen > IFNAMSIZ) nameLen = IFNAMSIZ;
    nameLen++;
    if (NULL == (if_name = new char[nameLen]))
    {
        PLOG(PL_ERROR, "LinuxDetour::IfNameItem::Init() new if_name error: %s\n", GetErrorString());
        if_name_size = 0;
        return false;
    }
    strcpy(if_name, ifName);
    if_name_size = (nameLen - 1) << 3;
    if_index = ifIndex;
    return true;
}  // end LinuxDetour::IfNameItem::Init()

LinuxDetour::IfNameItem::~IfNameItem()
{
    if (NULL != if_name)
    {
        delete[] if_name;
        if_name = NULL;
    }
}

bool LinuxDetour::SetIPTables(Action              action,
                              int                 hookFlags ,
                              const ProtoAddress& srcFilterAddr, 
                              unsigned int        srcFilterMask,
                              const ProtoAddress& dstFilterAddr,
                              unsigned int        dstFilterMask,
                              int                 dscpValue)
{
    // 1) IPv4 or IPv6 address family? 
    //    (Note we now use the "mangle" table for our stuf)
    const char* cmd;
    if (srcFilterAddr.GetType() != dstFilterAddr.GetType())
    {
        PLOG(PL_ERROR, "LinuxDetour::SetIPTables() error: inconsistent src/dst filter addr families\n");
        return false;
    }
    else if (ProtoAddress::IPv4 == srcFilterAddr.GetType())
    {
        cmd = "/sbin/iptables -t mangle";  // IPv4 iptables
    }
    else if (ProtoAddress::IPv6 == srcFilterAddr.GetType())
    {
        cmd = "/sbin/ip6tables -t mangle";
    }
    else
    {
        PLOG(PL_ERROR, "LinuxDetour::SetIPTables() error: unspecified filter addr family\n");
        return false;
    }
    // 2) INSTALL ("-A") or DELETE ("-D") the rule
    const char* mode = (INSTALL == action) ? "-A" : "-D";
    // 3) For which firewall hooks?
    while (0 != hookFlags)
    {
        const char* target;
        if (0 != (hookFlags & OUTPUT))
        {
            target = "OUTPUT";
            hookFlags &= ~OUTPUT;
        }
        else if (0 != (hookFlags & INPUT))
        {
            target = "PREROUTING";  // PREROUTING precedes INPUT in mangle table
            hookFlags &= ~INPUT;
        }
        else if (0 != (hookFlags & FORWARD))
        {
            target = "FORWARD";
            hookFlags &= ~FORWARD;
        }
        else
        {
            break;  // all flags have been processed
        }
    
        // Make and install "iptables" firewall rules
        const size_t RULE_MAX = 511;
        char rule[RULE_MAX+1];
        // cmd  = "iptables" or "ip6tables"
        // mode = "-I" or "-D"
        // target = "INPUT", "OUTPUT", or "FORWARD"
        sprintf(rule, "%s %s %s -j QUEUE ", cmd, mode, target);
        if (0 != srcFilterMask)
        {
            strcat(rule, "-s ");
            size_t len = strlen(rule);
            if (!srcFilterAddr.GetHostString(rule+len, RULE_MAX - len))
            {
                PLOG(PL_ERROR, "LinuxDetour::SetIPTables() error: bad source addr filter\n");
                return false;
            } 
            len = strlen(rule);
            sprintf(rule+len, "/%hu ", srcFilterMask);
        }
        if (0 != dstFilterMask)
        {
            strcat(rule, "-d ");
            size_t len = strlen(rule);
            if (!dstFilterAddr.GetHostString(rule+len, RULE_MAX - len))
            {
                PLOG(PL_ERROR, "LinuxDetour::SetIPTables() error: bad destination addr filter\n");
                return false;
            } 
            len = strlen(rule);
            sprintf(rule+len, "/%hu ", dstFilterMask);
        }
         // Add DSCP filter command, if applicable
        if (dscpValue >= 0)
        {
            // TBD - error check dscpValue???
            size_t len = strlen(rule);
            sprintf(rule+len, "-m dscp --dscp %d ", dscpValue);
            dscp_value = dscpValue;
        }
        
        
        // Add redirection so we can get stderr result
        strcat(rule, " 2>&1");
        FILE* p = popen(rule, "r");
        if (NULL != p)
        {
            char errorMsg[256];
            fread(errorMsg, 1, 256, p);
            char* ptr = strchr(errorMsg, '\n');
            if (NULL != ptr) *ptr = '\0';
            errorMsg[255] = '\0';
            if (0 != pclose(p))
            {
                PLOG(PL_ERROR, "LinuxDetour::SetIPTables() \"%s\" error: %s\n",
                         rule, errorMsg);
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "LinuxDetour::SetIPTables() error: popen(%s): %s\n",
                    rule, GetErrorString());
            return false;
        }
    }  // end while (0 != hookFlags)
    return true;
}  // end LinuxDetour::SetIPTables()

bool LinuxDetour::Open(int                 hookFlags, 
                       const ProtoAddress& srcFilterAddr, 
                       unsigned int        srcFilterMask,
                       const ProtoAddress& dstFilterAddr,
                       unsigned int        dstFilterMask,
                       int                 dscpValue)
{
    if (IsOpen()) Close();
    
    // 0) Open raw socket for optional packet injection use
    //if (0 > (raw_fd = socket(domain, SOCK_RAW, IPPROTO_RAW)))
    if (0 > (raw_fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)))  // or can we IPv6 on an AF_INET/HDRINCL socket?
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() socket(IPPROTO_RAW) error: %s\n", 
                GetErrorString());
        return false;
    }
    //if (AF_INET == domain)
    {
        // Note: no IP_HDRINCL for IPv6 raw sockets ?
        int enable = 1;
        if (setsockopt(raw_fd, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)))
        {
            PLOG(PL_ERROR, "LinuxDetour::Open() setsockopt(IP_HDRINCL) error: %s\n",
                    GetErrorString());
            Close();
            return false;
        }
    }
    // Set to non-blocking for our purposes (TBD) Add a SetBlocking() method    
    if(-1 == fcntl(raw_fd, F_SETFL, fcntl(raw_fd, F_GETFL, 0) | O_NONBLOCK))
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
        Close();
        return false;
    }
    
    // Check for "inject-only" mode and "rewire appropriately
    if (0 != (hookFlags & INJECT))
    {
        descriptor = raw_fd;
        raw_fd = -1;
        ProtoDetour::Open();
        StopInputNotification();
        return true;
    }
    
    int domain;
    if (srcFilterAddr.GetType() != dstFilterAddr.GetType())
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() error: inconsistent src/dst filter addr families\n");
        Close();
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
        PLOG(PL_ERROR, "LinuxDetour::Open() error: unspecified filter addr family\n");
        Close();
        return false;
    }
    
    // Make sure the "ip_queue" (or "ip6_queue") modules are loaded
    // (We make this non-fatal in the case the operating system has
    //  these compiled-in and they are not used as loadable modules).
    FILE* p;
    if (AF_INET == domain)
        p = popen("/sbin/modprobe ip_queue 2>&1", "r");
    else
        p = popen("/sbin/modprobe ip6_queue 2>&1", "r");
    if (NULL != p)
    {
        // Read any error information fed back
        char errorMsg[256];
        fread(errorMsg, 1, 256, p);
        errorMsg[255] = '\0';
        if (0 != pclose(p))
            PLOG(PL_ERROR, "LinuxDetour::Open() warning: \"/sbin/modprobe %s\" error: %s",
                    (AF_INET == domain) ? "ip_queue" : "ip6_queue", errorMsg);
    }
    else
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() warning: popen(/sbin/modprobe) error: %s\n",
                GetErrorString());
    }
    
    // Save parameters for firewall rule removal
    hook_flags = hookFlags;
    src_filter_addr = srcFilterAddr;
    src_filter_mask = srcFilterMask;
    dst_filter_addr = dstFilterAddr;
    dst_filter_mask = dstFilterMask;
    // Set up iptables (or ip6tables) if non-zero "hookFlags" are provided
    if (0 != hookFlags)
    {
        if (!SetIPTables(INSTALL, hookFlags, 
                         srcFilterAddr, srcFilterMask,
                         dstFilterAddr, dstFilterMask, dscpValue))
        {
            PLOG(PL_ERROR, "LinuxDetour::Open() error: couldn't install firewall rules\n");   
            Close();
            return false;
        }   
    }
    
    // 1) Open netlink firewall socket 
    int protocol;
    if (AF_INET == domain)
        protocol = NETLINK_FIREWALL;
    else
        protocol = NETLINK_IP6_FW;
    if ((descriptor = socket(PF_NETLINK, SOCK_RAW, protocol)) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() socket(NETLINK_FIREWALL) error: %s\n",
                GetErrorString());  
        Close();
        return false; 
    }
    // 2) save our process id
    pid = getpid();
    
    // 3) Send a mode message
    struct
    {
        struct nlmsghdr nlh;
        ipq_mode_msg    mode;
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req));
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.nlh.nlmsg_type = IPQM_MODE;
    req.nlh.nlmsg_pid = pid;
    req.mode.value = IPQ_COPY_PACKET;
    req.mode.range = 0;   
    
    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(struct sockaddr_nl));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = 0;
    addr.nl_groups = 0;
    
    if (sendto(descriptor, &req, req.nlh.nlmsg_len, 0,
               (struct sockaddr*)&addr, sizeof(struct sockaddr_nl)) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() sendto() error: %s\n", GetErrorString());
        Close();
        return false;   
    }
    
    if (!ProtoDetour::Open())
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() ProtoChannel::Open() error\n");
        Close();
        return false;   
    }
    
    // Get array of ifIndices to iterate over and create ifName->ifIndex ProtoTree
    unsigned int ifIndexArray[256];
    unsigned int ifCount = ProtoSocket::GetInterfaceIndices(ifIndexArray, 256);
    if (ifCount < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() warning: unable to retrieve list of network interface indices\n");
    }
    else if (0 == ifCount)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() warning: no network interface indices were found.\n");
    }
    else if (ifCount > 256)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() warning: found network interfaces indices exceeding maximum count of 256.\n");
        ifCount = 256;
    }
    for (unsigned int i = 0; i < ifCount; i++)
    {
        char ifName[IFNAMSIZ+1];
        ifName[IFNAMSIZ] = '\0';
        if (ProtoSocket::GetInterfaceName(ifIndexArray[i], ifName, IFNAMSIZ))
        {
            IfNameItem* item = static_cast<IfNameItem*>(if_name_tree.Find(ifName, strlen(ifName) << 3));
            if (NULL != item)
            {
                PLOG(PL_ERROR, "LinuxDetour::Open() warning: duplicate ifName?!\n");
            }
            else
            {
                if (NULL == (item = new IfNameItem()))
                {
                    PLOG(PL_ERROR, "LinuxDetour::Open() new IfNameItem error: %s\n", GetErrorString());
                    Close();
                    return false;
                }
                if (!item->Init(ifName, ifIndexArray[i]))
                {
                    PLOG(PL_ERROR, "LinuxDetour::Open() new IfNameItem error: %s\n", GetErrorString());
                    delete item;
                    Close();
                    return false;
                }
                if_name_tree.Insert(*item);
            }
        }
        else
        {
            PLOG(PL_ERROR, "LinuxDetour::Open() warning: failed to get ifName for ifIndex:%d\n", ifIndexArray[i]);
        }
    }
    return true;
}  // end LinuxDetour::Open()  

void LinuxDetour::Close()
{
    // Empty our ifName->ifIndex tree
    ProtoTree::Item* rootItem;
    while (NULL != (rootItem = if_name_tree.GetRoot()))
    {
        if_name_tree.Remove(*rootItem);
        delete rootItem;   
    }
    
    if (raw_fd >= 0)
    {
        close(raw_fd);
        raw_fd = -1;
    }
    if (0 != hook_flags)
    {
        SetIPTables(DELETE, hook_flags,
                    src_filter_addr, src_filter_mask,
                    dst_filter_addr, dst_filter_mask, dscp_value);
        hook_flags = 0;   
    }
    if (descriptor >= 0)
    {
        ProtoDetour::Close();
        close(descriptor);
        descriptor = INVALID_HANDLE;  
    } 
}  // end LinuxDetour::Close()

bool LinuxDetour::Recv(char*            buffer, 
                       unsigned int&    numBytes, 
                       Direction*       direction, 
                       ProtoAddress*    srcMac,
                       unsigned int*    ifIndex)
{
    if (NULL != srcMac) srcMac->Invalidate();
    struct iovec iov[3];
    iov[0].iov_base = &nlh;
    iov[0].iov_len = sizeof(nlh);
    iov[1].iov_base = &pkt_msg;
    iov[1].iov_len = sizeof(pkt_msg);
    iov[2].iov_base = buffer;
    iov[2].iov_len = numBytes;
    struct sockaddr_nl addr;   
    struct msghdr msg;
    msg.msg_name = (void*)&addr;
    msg.msg_namelen = sizeof(struct sockaddr_nl);
    msg.msg_iov = iov;
    msg.msg_iovlen = 3;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    size_t result = recvmsg(descriptor, &msg, 0);
    if (result < 0)
    {
        numBytes = 0;
        if ((EAGAIN == errno) || (EINTR == errno))
        {
            return true;
        }
        else
        {   
            PLOG(PL_ERROR, "LinuxDetour::Recv() recvfrom() error: %s\n", GetErrorString());
            return false;   
        }
    } 
    else if (IPQM_PACKET != nlh.nlmsg_type)
    {
        numBytes = 0;
        return true;   
    }
    else
    {
        if (NULL != direction)
        {
            switch (pkt_msg.hook)
            {
                case NF_IP_LOCAL_OUT:
                //case NF_IP6_LOCAL_OUT:
                    *direction = OUTBOUND;  // locally-generated packet
                    break;
                default:   
                    *direction = INBOUND;   // packet from somewhere else
                    break;                  // (assume it is INBOUND)
            }   
        }
        if ((NULL != srcMac) &&
            (ARPHRD_ETHER == pkt_msg.hw_type) &&
            (ETH_ALEN == pkt_msg.hw_addrlen))
        {
            srcMac->SetRawHostAddress(ProtoAddress::ETH, (char*)pkt_msg.hw_addr, ETH_ALEN);
        }
        numBytes = pkt_msg.data_len;
        if (NULL != ifIndex) 
        {
            // Attempt to fill in "ifIndex"
            if (NF_IP_LOCAL_OUT == pkt_msg.hook)
            {
                // OUTBOUND packet so set "ifIndex" to pkt_msg.outdev_name
                size_t nameLen = strlen(pkt_msg.outdev_name);
                if (nameLen > IFNAMSIZ) nameLen = IFNAMSIZ;
                IfNameItem* item = static_cast<IfNameItem*>(if_name_tree.Find(pkt_msg.outdev_name, nameLen << 3));
                if (NULL != item)
                {
                    *ifIndex = item->GetIfIndex();
                }
                else
                {
                    PLOG(PL_ERROR, "LinuxDetour::Recv() warning: OUTBOUND packet to unknown ifName: %s\n", pkt_msg.outdev_name);
                    *ifIndex = 0;
                }
            }
            else
            {
                // Assume INBOUND packet so set "ifIndex" to pkt_msg.indev_name
                size_t nameLen = strlen(pkt_msg.indev_name);
                if (nameLen > IFNAMSIZ) nameLen = IFNAMSIZ;
                IfNameItem* item = static_cast<IfNameItem*>(if_name_tree.FindClosestMatch(pkt_msg.indev_name, nameLen << 3));
                if (NULL != item)
                {
                    *ifIndex = item->GetIfIndex();
                }
                else
                {
                    PLOG(PL_ERROR, "LinuxDetour::Recv() warning: INBOUND packet from unknown ifName: %s\n", pkt_msg.indev_name);
                    *ifIndex = 0;
                }
            }
        }
        return true;
    }
}  // end LinuxDetour::Recv()

bool LinuxDetour::Allow(const char* buffer, unsigned int numBytes)
{
    struct
    {
        struct nlmsghdr        nlh;
        struct ipq_verdict_msg verdict;
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.nlh.nlmsg_type = IPQM_VERDICT;
    req.nlh.nlmsg_pid = pid;
    req.verdict.value = NF_ACCEPT;
    req.verdict.id = pkt_msg.packet_id; // filled in from last Recv()
    req.verdict.data_len = numBytes;
    
    struct iovec iov[3];
    iov[0].iov_base = &req.nlh;
    iov[0].iov_len = sizeof(struct nlmsghdr);
    iov[1].iov_base = &req.verdict;
    iov[1].iov_len = sizeof(struct ipq_verdict_msg);
    iov[2].iov_base = (void*)buffer;
    iov[2].iov_len = numBytes;
    req.nlh.nlmsg_len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
    
    struct sockaddr_nl addr;  
    memset(&addr, 0, sizeof(struct sockaddr_nl)); 
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = 0;
    addr.nl_groups = 0;
    
    struct msghdr msg;
    msg.msg_name = (void*)&addr;
    msg.msg_namelen = sizeof(struct sockaddr_nl);
    msg.msg_iov = iov;
    msg.msg_iovlen = 3;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    
    if (sendmsg(descriptor, &msg, 0) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Allow() sendmsg() error: %s\n", GetErrorString());
        return false;    
    }  
    return true;
}  // end LinuxDetour::Allow()

bool LinuxDetour::Drop()
{
    struct
    {
        struct nlmsghdr        nlh;
        struct ipq_verdict_msg verdict;
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.nlh.nlmsg_type = IPQM_VERDICT;
    req.nlh.nlmsg_pid = pid;
    req.verdict.value = NF_DROP;
    req.verdict.id = pkt_msg.packet_id; // filled in from last Recv()
    req.verdict.data_len = 0;
    
    struct iovec iov[2];
    iov[0].iov_base = &req.nlh;
    iov[0].iov_len = sizeof(struct nlmsghdr);
    iov[1].iov_base = &req.verdict;
    iov[1].iov_len = sizeof(struct ipq_verdict_msg);
    req.nlh.nlmsg_len = iov[0].iov_len + iov[1].iov_len;
    
    struct sockaddr_nl addr;  
    memset(&addr, 0, sizeof(struct sockaddr_nl)); 
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = 0;
    addr.nl_groups = 0;
    
    struct msghdr msg;
    msg.msg_name = (void*)&addr;
    msg.msg_namelen = sizeof(struct sockaddr_nl);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    
    if (sendmsg(descriptor, &msg, 0) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Drop() sendmsg() error: %s\n", GetErrorString());
        return false;    
    }  
    return true;
}  // end LinuxDetour::Drop()

bool LinuxDetour::Inject(const char* buffer, unsigned int numBytes)
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
        PLOG(PL_ERROR, "LinuxDetour::Inject() IPv6 injection not yet supported!\n");
        return false;   
    }
    else
    {
        PLOG(PL_ERROR, "LinuxDetour::Inject() unknown IP version!\n");
        return false;   
    }
    int fd = (raw_fd < 0) ? descriptor : raw_fd;
    size_t result = sendto(fd, buffer, numBytes, 0, 
                           &dstAddr.GetSockAddr(), addrLen);
    if (result != numBytes)
    {
        PLOG(PL_ERROR, "LinuxDetour::Inject() sendto() error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end LinuxDetour::Inject()

bool LinuxDetour::SetMulticastInterface(const char* interfaceName) 
{
    int fd = (raw_fd < 0) ? descriptor : raw_fd;
    if (fd < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::SetMulticastInterface() error: detour not open\n");
        return false;   
    }   
    
    if (interfaceName)
    {
        int result;
#ifdef HAVE_IPV6
        if (ProtoAddress::IPv6 == src_filter_addr.GetType())  
        {
            unsigned int interfaceIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
            result = setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
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
                PLOG(PL_ERROR, "LinuxDetour::SetMulticastInterface() invalid interface name\n");
                return false;
            }
            result = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localAddr, 
                                sizeof(localAddr));
        }
        if (result < 0)
        { 
            PLOG(PL_ERROR, "LinuxDetour: setsockopt(IP_MULTICAST_IF) error: %s\n", 
                     GetErrorString());
            return false;
        }         
    }     
    return true;
}  // end LinuxDetour::SetMulticastInterface()
