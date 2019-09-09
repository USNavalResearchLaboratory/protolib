
#include "protoDebug.h"
#include "protoDetour.h"
#include "protoCap.h"  // used for packet injection
#include "protoNet.h"

#include <stdlib.h>  // for atoi(), getenv()
#include <stdio.h>
#include <unistd.h>  // for close()
#include <linux/netfilter_ipv4.h>  // for NF_IP_LOCAL_OUT, etc
#include <linux/netfilter_ipv6.h>  // for NF_IP6_LOCAL_OUT, etc
#include <linux/netfilter.h>  // for NF_ACCEPT, etc
#include <libnetfilter_queue/libnetfilter_queue.h>

#include <fcntl.h>  // for fcntl(), etc
#include <linux/if_ether.h>  // for ETH_P_IP
#include <net/if_arp.h>   // for ARPHRD_ETHER

#include <linux/version.h>  // for LINUX_VERSION_CODE

/** NOTES: 
 *
 * 1) This newer implementation of LinuxDetour uses netfilter_queue
 *    instead of the ip_queue daemon of that prior implementation.
 *    This allows multiple user space processes to access the firewall.
 *
 * 2) With netfilter_queue, you have to use the "callback" approach so
 *    that the internal NFQ can parse the netlink message with the packet
 *    data.  Unfortunately, this means our LinuxDetour() has to do
 *    an additional copy since we can't read the packet data directly to
 *    the application-provided "buffer" as we could with the prior 
 *    "ip_queue" implementation. (Yuck! - but, oh well)
 *
 */

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
        
        // Simple has used to randomize pid into base nfq_num    
        UINT32 JenkinsHash(UINT32 value);  
        
        bool SetIPTables(UINT16              nfqNum,
                         Action              action,
                         int                 hookFlags ,
                         const ProtoAddress& srcFilterAddr, 
                         unsigned int        srcFilterMask,
                         const ProtoAddress& dstFilterAddr,
                         unsigned int        dstFilterMask,
                         int                 dscpValue);
        
        int                     raw_fd;  // for packet injection
        int                     hook_flags;
        ProtoAddress            src_filter_addr;
        unsigned int            src_filter_mask;
        ProtoAddress            dst_filter_addr;
        unsigned int            dst_filter_mask;
        int                     dscp_value;
        
        enum {NFQ_BUFFER_SIZE = 8192};
        
        static int NfqCallback(nfq_q_handle*       nfqQueue, 
                               struct nfgenmsg*    nfqMsg,
                               nfq_data*           nfqData,
                               void*               userData);
                
        // This is initialized with a value set by the
        // "PROTO_NFQ_NUM_BASE" or uses a hash of the
        // process id otherwise
        static bool             nfq_num_init;
        static UINT16           nfq_num_next;  //   
             
        struct nfq_handle*      nfq_handle;
        struct nfq_q_handle*    nfq_queue;
        UINT16                  nfq_num;  // based on pid
        
        // The NfqCallback() fills these in on a per-packet basis
        UINT32                  nfq_pkt_id;
        char*                   nfq_pkt_data;
        unsigned int            nfq_pkt_len;
        Direction               nfq_direction;
        ProtoAddress            nfq_src_macaddr;
        unsigned int            nfq_ifindex;
            
};  // end class LinuxDetour
    
ProtoDetour* ProtoDetour::Create()
{
    return static_cast<ProtoDetour*>(new LinuxDetour());   
}  // end ProtoDetour::Create(), (void*)nl_head  

bool LinuxDetour::nfq_num_init = true;
UINT16 LinuxDetour::nfq_num_next = 0;

LinuxDetour::LinuxDetour()
 : raw_fd(-1), hook_flags(0), dscp_value(-1), 
   nfq_handle(NULL), nfq_queue(NULL),
   nfq_pkt_id(0), nfq_pkt_data(NULL), nfq_pkt_len(0),
   nfq_direction(UNSPECIFIED), nfq_ifindex(0)
   
{
    if (nfq_num_init)
    {
        char* cp = getenv("PROTO_NFQ_NUM_BASE");
        if (NULL != cp)
            nfq_num_next = (UINT16)atoi(cp);
        else
            nfq_num_next = (UINT16)JenkinsHash(getpid());  // TBD - implement a semaphore for this?
        nfq_num_init = false;
    }
}

LinuxDetour::~LinuxDetour()
{
    Close();
}

UINT32 LinuxDetour::JenkinsHash(UINT32 a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}  // end LinuxDetour::JenkinsHash()

bool LinuxDetour::SetIPTables(UINT16              nfqNum,
                              Action              action,
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
        sprintf(rule, "%s %s %s -j NFQUEUE --queue-num %hu ", cmd, mode, target, nfqNum);
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
        }
        
        // Add redirection so we can get stderr result
        strcat(rule, " 2>&1");
        FILE* p = popen(rule, "r");
        if (NULL != p)
        {
            char errorMsg[256];
            int result = fread(errorMsg, 1, 256, p);
            if ((0 == result) && (0 != ferror(p)))
            {
                PLOG(PL_ERROR, "LinuxDetour::SetIPTables() fread() error: %s\n",
                               GetErrorString());
                return false;
            }
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
    
    int addrFamily;
    if (srcFilterAddr.GetType() != dstFilterAddr.GetType())
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() error: inconsistent src/dst filter addr families\n");
        Close();
        return false;
    }
    else if (ProtoAddress::IPv4 == srcFilterAddr.GetType())
        
    {
        addrFamily = AF_INET;
    }
    else if (ProtoAddress::IPv6 == srcFilterAddr.GetType())
    {
        addrFamily = AF_INET6;
    }
    else
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() error: unspecified filter addr family\n");
        Close();
        return false;
    }
    
    // Make sure the "nfnetlink_queue" modules are loaded
    // (We make this non-fatal in the case the operating system has
    //  these compiled-in and they are not used as loadable modules).
    // FILE* p = popen("/sbin/modprobe -l nfnetlink_queue 2>&1", "r");
    FILE* p = popen("find /lib/modules/$(uname -r) -iname nfnetlink_queue.ko\\* 2>&1", "r");
    if (NULL != p)
    {
        // Read any error information fed back
        char errorMsg[256];
        int result = fread(errorMsg, 1, 256, p);
        if ((0 == result) && (0 != ferror(p)))
        {
            PLOG(PL_ERROR, "LinuxDetour::Open() fread() error: %s\n",
                           GetErrorString());
            return false;
        }
        errorMsg[255] = '\0';
        if (0 != pclose(p))
            PLOG(PL_ERROR, "LinuxDetour::Open() warning: \"/sbin/modprobe -l nfnetlink_queue\" error: %s",
                           errorMsg);
    }
    else
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() warning: popen(\"/sbin/modprobe ...\") error: %s\n",
                GetErrorString());
    }
    
    nfq_num = nfq_num_next++;    // TBD - is there a better way??
    
    // Save parameters for firewall rule removal
    hook_flags = hookFlags;
    src_filter_addr = srcFilterAddr;
    src_filter_mask = srcFilterMask;
    dst_filter_addr = dstFilterAddr;
    dst_filter_mask = dstFilterMask;
    dscp_value = dscpValue;
    // Set up iptables (or ip6tables) if non-zero "hookFlags" are provided
    if (0 != hookFlags)
    {
        if (!SetIPTables(nfq_num, INSTALL, hookFlags, 
                         srcFilterAddr, srcFilterMask,
                         dstFilterAddr, dstFilterMask,
                         dscpValue))
        {
            PLOG(PL_ERROR, "LinuxDetour::Open() error: couldn't install firewall rules\n");   
            Close();
            return false;
        }   
    }
    
    // The first three nfq calls here set up the nfq library
    // Open netfilter_queue handle
    if (NULL == (nfq_handle = nfq_open()))
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() nfq_open() error: %s\n", GetErrorString());   
        Close();
        return false;
    }
    descriptor = nfq_fd(nfq_handle);
    
    // Not sure this step is necessary, but was in example code
    if (nfq_unbind_pf(nfq_handle, addrFamily) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() warning: nfq_unbind_pf() error: %s\n", GetErrorString());   
        //Close();
        //return false;
    }
    // "bind" our nfq handle for the specified address family
    if (nfq_bind_pf(nfq_handle, addrFamily) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() nfq_bind_pf() error: %s\n", GetErrorString());   
        Close();
        return false;
    }
    
    // Next, set up an NFQ queue 
    if (NULL == (nfq_queue = nfq_create_queue(nfq_handle, nfq_num, NfqCallback, this)))
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() nfq_create_queue() error: %s\n", GetErrorString());   
        Close();
        return false;
    }
    
    // Turn on packet copy mode
    if (nfq_set_mode(nfq_queue, NFQNL_COPY_PACKET, NFQ_BUFFER_SIZE) < 0)
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() nfq_set_mode() error: %s\n", GetErrorString());   
        Close();
        return false;
    }
    
    if (!ProtoDetour::Open())
    {
        PLOG(PL_ERROR, "LinuxDetour::Open() ProtoDetour::Open() error\n");
        Close();
        return false;   
    }
    return true;
}  // end LinuxDetour::Open()  

void LinuxDetour::Close()
{
    if (raw_fd >= 0)
    {
        close(raw_fd);
        raw_fd = -1;
    }
    if (0 != hook_flags)
    {
        SetIPTables(nfq_num, DELETE, hook_flags,
                    src_filter_addr, src_filter_mask,
                    dst_filter_addr, dst_filter_mask, dscp_value);
        hook_flags = 0;   
    }
    if (descriptor >= 0)
    {
        ProtoDetour::Close();
        descriptor = INVALID_HANDLE;  
    } 
    
    if (NULL != nfq_queue)
    {
        nfq_destroy_queue(nfq_queue);
        nfq_queue = NULL;
    }
    
    if (NULL != nfq_handle)
    {
        nfq_close(nfq_handle);
        nfq_handle = NULL;
    }
    
}  // end LinuxDetour::Close()

bool LinuxDetour::Recv(char*            buffer, 
                       unsigned int&    numBytes, 
                       Direction*       direction, 
                       ProtoAddress*    srcMac,
                       unsigned int*    ifIndex)
{
    if (NULL != nfq_pkt_data)
    {
        PLOG(PL_ERROR, "LinuxDetour::Recv() error: existing packet pending allow/drop!\n");
        return false;
    }
    char nfqBuffer[NFQ_BUFFER_SIZE];
    int result = recv(descriptor, nfqBuffer, NFQ_BUFFER_SIZE, 0);
    if (result < 0)
    {
        numBytes = 0;
        if (NULL != direction) *direction = UNSPECIFIED;
        if (NULL != srcMac) srcMac->Invalidate();
        if (NULL != ifIndex) *ifIndex = 0;
        if ((EAGAIN != errno) && (EINTR != errno))
        {   
            PLOG(PL_ERROR, "LinuxDetour::Recv() recv() error: %s\n", GetErrorString());
            return false;   
        }
    } 
    else
    {
        // This will invoke our "nfq_callback" which sets a pointer to
        // the packet data "nfq_pkt_data" and the "nfq_pkt_len" value
        nfq_handle_packet(nfq_handle, nfqBuffer, result);
        if (NULL != nfq_pkt_data)
        {
            if (NULL != direction) *direction = nfq_direction;
            if (NULL != srcMac) *srcMac = nfq_src_macaddr;
            if (NULL != ifIndex) *ifIndex = nfq_ifindex;
            if (numBytes < nfq_pkt_len)
                memcpy(buffer, nfq_pkt_data, numBytes);
            else
                memcpy(buffer, nfq_pkt_data, nfq_pkt_len);
            numBytes = nfq_pkt_len;
        }
        else
        {
            // It was a "false alarm" (i.e. not a relevant nfq event)
            numBytes = 0;
            if (NULL != direction) *direction = UNSPECIFIED;
            if (NULL != srcMac) srcMac->Invalidate();
            if (NULL != ifIndex) *ifIndex = 0;
        }
    }
    return true;
}  // end LinuxDetour::Recv()


int LinuxDetour::NfqCallback(nfq_q_handle*       nfqQueue, 
                             struct nfgenmsg*    nfqMsg,
                             nfq_data*           nfqData,
                             void*               userData)
{
    ASSERT(NULL != userData);
    LinuxDetour* linuxDetour = reinterpret_cast<LinuxDetour*>(userData);
    // We use the "hook" point to determine inbound | outbound packet "direction"
    linuxDetour->nfq_direction = UNSPECIFIED;
    struct nfqnl_msg_packet_hdr* header = nfq_get_msg_packet_hdr(nfqData);
    if(NULL != header)
    {
        linuxDetour->nfq_pkt_id = ntohl(header->packet_id);
        if (ProtoAddress::IPv4 == linuxDetour->src_filter_addr.GetType())
        {
            if (NF_IP_LOCAL_OUT == header->hook)
                linuxDetour->nfq_direction = OUTBOUND;
            else
                linuxDetour->nfq_direction = INBOUND;
        }
        else  // (assume IPv6)
        {
            if (NF_IP_LOCAL_OUT == header->hook)
                linuxDetour->nfq_direction = OUTBOUND;
            else
                linuxDetour->nfq_direction = INBOUND;
        }
    }
    
    // Get the src mac addr for the packet if available
    struct nfqnl_msg_packet_hw* hw = nfq_get_packet_hw(nfqData);
    if (NULL != hw)
        linuxDetour->nfq_src_macaddr.SetRawHostAddress(ProtoAddress::ETH, (char*)hw->hw_addr, (UINT8)ntohs(hw->hw_addrlen));
    else
        linuxDetour->nfq_src_macaddr.Invalidate();
    // Here, based on the "direction" we cache the associated
    // input or output ifindex.  If direction is "UNSPECIFIED",
    // we look for a valid input or output ifindex to guess direction
    // TBD - test and refine this.
    if (OUTBOUND == linuxDetour->nfq_direction)
        linuxDetour->nfq_ifindex = nfq_get_outdev(nfqData);
    else if (INBOUND == linuxDetour->nfq_direction)
        linuxDetour->nfq_ifindex = nfq_get_indev(nfqData);
    else if (0 != (linuxDetour->nfq_ifindex = nfq_get_indev(nfqData)))
        linuxDetour->nfq_direction = INBOUND;
    else if (0 != (linuxDetour->nfq_ifindex = nfq_get_outdev(nfqData)))
        linuxDetour->nfq_direction = INBOUND;
    
    // Finally record packet length and cache pointer to IP packet data
    
    // A change to the nfq_get_payload() prototype seemed to kick in around Linux header files
    // version 3.6?  (This will probably need to be fine tuned for the right version threshold.)

#define LINUX_VERSION_MAJOR (LINUX_VERSION_CODE/65536)
#define LINUX_VERSION_MINOR ((LINUX_VERSION_CODE - (LINUX_VERSION_MAJOR*65536)) / 256)

#if ((LINUX_VERSION_MAJOR > 3) || ((LINUX_VERSION_MAJOR == 3) && (LINUX_VERSION_MINOR > 5)))
    linuxDetour->nfq_pkt_len = nfq_get_payload(nfqData, (unsigned char**)(&linuxDetour->nfq_pkt_data));
#else
    linuxDetour->nfq_pkt_len = nfq_get_payload(nfqData, &linuxDetour->nfq_pkt_data);
#endif //
    return 0;
}  // end LinuxDetour::NfqCallback()

bool LinuxDetour::Allow(const char* buffer, unsigned int numBytes)
{
    if (NULL == nfq_queue)
    {
        PLOG(PL_ERROR, "LinuxDetour::Allow() error: not opened\n");
        return false;
    }
    if (NULL == nfq_pkt_data)
    {
        PLOG(PL_ERROR, "LinuxDetour::Allow() error: no pending packet\n");
        return false;
    }
    if (0 > nfq_set_verdict(nfq_queue, nfq_pkt_id, NF_ACCEPT, numBytes, (unsigned char*)buffer))
    {
        PLOG(PL_ERROR, "LinuxDetour::Allow() nfq_set_verdict() error: %s\n",
                        GetErrorString());
        return false;
    }
    nfq_pkt_data = NULL; // indicates verdict has been set
    return true;
}  // end LinuxDetour::Allow()

bool LinuxDetour::Drop()
{
    if (NULL == nfq_queue)
    {
        PLOG(PL_ERROR, "LinuxDetour::Drop() error: not opened\n");
        return false;
    }
    if (NULL == nfq_pkt_data)
    {
        PLOG(PL_ERROR, "LinuxDetour::Drop() error: no pending packet\n");
        return false;
    }
    if (0 > nfq_set_verdict(nfq_queue, nfq_pkt_id, NF_DROP, 0, NULL))
    {
        PLOG(PL_ERROR, "LinuxDetour::Drop() nfq_set_verdict() error: %s\n",
                        GetErrorString());
        return false;
    }
    nfq_pkt_data = NULL; // indicates verdict has been set
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
            unsigned int interfaceIndex = ProtoNet::GetInterfaceIndex(interfaceName);
            result = setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                (char*)&interfaceIndex, sizeof(interfaceIndex));
        }
        else 
#endif // HAVE_IPV6 
        {  
            struct in_addr localAddr;
            ProtoAddress interfaceAddress;
            if (ProtoNet::GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
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
