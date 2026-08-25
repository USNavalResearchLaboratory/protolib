
#include "protoCap.h"
#include "protoDebug.h"
#include "protoSocket.h"
#include "protoNet.h"
#include "protoPktIP.h"
#include "protoPktETH.h"
#include "protoPktGRE.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <features.h>    /* for the glibc version number */
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif
#include <netinet/in.h>

/** This implementation of ProtoCap uses the
 *  PF_PACKET socket type available on Linux systems
 */

class LinuxCap : public ProtoCap
{
    public:
        LinuxCap();
        ~LinuxCap();
            
        bool Open(const char* interfaceName = NULL);
        void Close();
        bool Send(const char* buffer, unsigned int& numBytes);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);

    private:
        bool SendCollectMdGre(const char* buffer, unsigned int& numBytes);

        int  gre_raw_fd;
        bool tunnel_collect_md;
};  // end class LinuxCap

ProtoCap* ProtoCap::Create()
{
    return static_cast<ProtoCap*>(new LinuxCap());   
}  // end ProtoCap::Create()

LinuxCap::LinuxCap()
 : gre_raw_fd(INVALID_HANDLE), tunnel_collect_md(false)
{
}

LinuxCap::~LinuxCap()
{   
    Close();
}

bool LinuxCap::Open(const char* interfaceName)
{
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            PLOG(PL_ERROR, "LinuxCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             PLOG(PL_ERROR, "LinuxCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
    }
    
    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "LinuxCap::Open() error getting interface index\n");
        return false;   
    }
    if_type = ProtoNet::GetInterfaceType(ifIndex, &tunnel_local_addr, &tunnel_remote_addr, &tunnel_collect_md);
    if (ProtoNet::IFACE_INVALID_TYPE == if_type)
    {
        PLOG(PL_WARN, "LinuxCap::Open() GetInterfaceType() error: unknown interface type! (assuming ETH type)\n");
        if_type = ProtoNet::IFACE_ETH; 
    }
    
    int sockType = (ProtoNet::IFACE_GRE == if_type) ? SOCK_DGRAM : SOCK_RAW;
    //sockType = SOCK_RAW;
    UINT16 protoType = (ProtoNet::IFACE_GRE == if_type) ? ETH_P_IP : ETH_P_ALL;
    //protoType = ETH_P_ALL;
    
    //if ((descriptor = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    if ((descriptor = socket(PF_PACKET, sockType, htons(protoType))) < 0)
    {
        PLOG(PL_ERROR, "LinuxCap::Open() socket(PF_PACKET) error: %s\n", GetErrorString());
        return false;   
    }
    
    TRACE("LinuxCap::Open(%s) ifIndex:%d ifType:%d descriptor:%d\n", interfaceName, ifIndex, if_type, descriptor);
    
    // try to turn on broadcast capability (why?)
    int enable = 1;
    if (setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
        PLOG(PL_ERROR, "LinuxCap::Open() setsockopt(SO_BROADCAST) warning: %s\n", 
                GetErrorString());
    
    // Set interface to promiscuous mode
    // (TBD) add ProtoCap method to control interface promiscuity
    struct packet_mreq mreq;
    memset(&mreq, 0, sizeof(struct packet_mreq));
    mreq.mr_ifindex = ifIndex;
    mreq.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(descriptor, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        PLOG(PL_ERROR, "LinuxCap::Open() setsockopt(PACKET_MR_PROMISC) warning: %s\n", 
                GetErrorString());
    
    if (ProtoNet::IFACE_GRE == if_type)
    {
        if_addr = tunnel_local_addr;
    }
    else if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, if_addr))
    {
        PLOG(PL_ERROR, "LinuxCap::Open() error getting interface MAC address\n");
        Close();
        return false;
    }
    
    //if (ProtoNet::IFACE_GRE != if_type)
    if (true)
    {
        // Init our interface address structure  
        struct sockaddr_ll  ifaceAddr; 
        memset((char*)&ifaceAddr, 0, sizeof(ifaceAddr));
        ifaceAddr.sll_protocol = htons(protoType);
        ifaceAddr.sll_ifindex = ifIndex;
        //if (ProtoNet::IFACE_GRE != if_type)
        {
            ifaceAddr.sll_family = AF_PACKET;
            memcpy(ifaceAddr.sll_addr, if_addr.GetRawHostAddress(), 6);
            ifaceAddr.sll_halen = if_addr.GetLength();
        }
        // bind() the socket to the specified interface
        if (bind(descriptor, (struct sockaddr*)&ifaceAddr, sizeof(ifaceAddr)) < 0)
        {
            PLOG(PL_ERROR, "LinuxCap::Open() bind error: %s\n", GetErrorString());
            Close();
            return false;      
        }
        setsockopt(descriptor, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, strlen(interfaceName)+1);
    }
    else
    {
        setsockopt(descriptor, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, strlen(interfaceName)+1);
    }

    if (tunnel_collect_md)
    {
        // collect-md GRE has no header_ops; PF_PACKET dest never becomes
        // tunnel metadata. Encapsulate GRE here and sendto() the mapped remote.
        gre_raw_fd = socket(AF_INET, SOCK_RAW, IPPROTO_GRE);
        if (gre_raw_fd < 0)
        {
            PLOG(PL_ERROR, "LinuxCap::Open() socket(IPPROTO_GRE) error: %s\n", GetErrorString());
            Close();
            return false;
        }
        shutdown(gre_raw_fd, SHUT_RD);
        int ttl = 64;
        setsockopt(gre_raw_fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    }
    
    // Explicitly call ProtoCap::Open so that ProtoChannel stuff is properly set up
    if (!ProtoCap::Open(interfaceName))
    {
        PLOG(PL_ERROR, "LinuxCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    
    if_index = ifIndex;
    return true;
}  // end LinuxCap::Open()

void LinuxCap::Close()
{
    ProtoCap::Close();
    if (INVALID_HANDLE != gre_raw_fd)
    {
        close(gre_raw_fd);
        gre_raw_fd = INVALID_HANDLE;
    }
    tunnel_collect_md = false;
    if (INVALID_HANDLE != descriptor)
    {
        close(descriptor);
        descriptor = INVALID_HANDLE; 
        if_index = 0;
        if_type = ProtoNet::IFACE_INVALID_TYPE;
        tunnel_local_addr.Invalidate();
        tunnel_remote_addr.Invalidate();
    }  
}  // end LinuxCap::Close()

bool LinuxCap::Send(const char* buffer, unsigned int& numBytes)
{
    if (0 == numBytes)
    {
        PLOG(PL_WARN, "LinuxCap::Send() warning: can't send zero length frame!\n");
        return false;
    }
    if (ProtoNet::IFACE_GRE != if_type)
    {
        UINT16 type;
        memcpy(&type, buffer+12, 2);
        type = ntohs(type);
        if (type <= 0x05dc)
        {
            // Make sure packet is a type that is OK for us to send
            // (Some packets seem to cause a PF_PACKET socket trouble)
            PLOG(PL_DEBUG, "LinuxCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
        }  
        for(;;)
        {
            ssize_t result = write(descriptor, buffer, numBytes);
            if (result < 0)
            {
                switch (errno)
                {
                    case EINTR:
                        continue;  // try again
                    case EWOULDBLOCK:
                        numBytes = 0;
                    case ENOBUFS:
                        // because this doesn't block write()
                    default:
                        PLOG(PL_WARN, "LinuxCap::Send() write() error: %s\n", GetErrorString());
                        break;
                }   
                return false; 
            }
            else
            {
                ASSERT(result == numBytes);
                break;
            }
        }
    }
    else
    {
        if (tunnel_collect_md)
            return SendCollectMdGre(buffer, numBytes);
        // Use sendto() to denote IP/IPv6 properly 
        // (ensures GRE protocol type is correct)
        struct sockaddr_ll addr;
        memset(&addr, 0, sizeof(struct sockaddr_ll));
        addr.sll_family   = AF_PACKET;
        addr.sll_ifindex  = if_index;
        addr.sll_halen = 0;
        // SOCK_DGRAM sendto() always passes sll_addr to GRE header_ops, even
        // when sll_halen is 0. A zero dest overwrites a configured remote
        // (including a multicast one) with 0.0.0.0 and the packet is dropped.
        // Use the remote already learned from netlink at Open().
        if (tunnel_remote_addr.IsValid() &&
            !tunnel_remote_addr.IsUnspecified() &&
            (ProtoAddress::IPv4 == tunnel_remote_addr.GetType()))
        {
            memcpy(addr.sll_addr, tunnel_remote_addr.GetRawHostAddress(), 4);
            addr.sll_halen = 4;
        }

       // Check IP header version (first nybble)
        switch ((buffer[0] & 0xf0) >> 4)
        {
            case 4:
                addr.sll_protocol = htons(ETH_P_IP);
                break;
            case 6:
                addr.sll_protocol = htons(ETH_P_IPV6);
                break;
            default:
                PLOG(PL_WARN, "LinuxCap::Send(GRE) error: invalid IP protocol version!\n");
                return false;
        }
        /*
        TRACE("   sll_family:%d sll_ifindex:%d sll_protocol:%d sll_halen:%d\n",
               addr.sll_family, addr.sll_ifindex, addr.sll_protocol, addr.sll_halen);
        
        TRACE("   v=%u ihl=%u totlen=%u len=%zu proto=%u\n",
               buffer[0]>>4, buffer[0]&0x0f,
               (buffer[2]<<8)|buffer[3], numBytes,
               buffer[9]);
        */
        
        for (;;)
        {
            /*TRACE("sendto() buffer bytes:\n");
            const char* ptr = buffer;
            for (int i = 0; i < 4; i++)
            {
                TRACE("    ");
                for (int j = 0; j < 16; j++)
                {
                    TRACE("%02x%02x ", *ptr, *(ptr+1));
                    ptr += 2;
                }
                TRACE("\n");
            }
            TRACE("\n");
            */
            
            ssize_t result = sendto(descriptor, buffer, numBytes, 0,
                                    (struct sockaddr*)&addr, sizeof(struct sockaddr_ll));
            if (result < 0)
            {
                switch (errno)
                {
                    case EINTR:
                        continue;  // try again
                    case EWOULDBLOCK:
                        numBytes = 0;
                    case ENOBUFS:
                        // because this doesn't block write()
                    default:
                        PLOG(PL_WARN, "LinuxCap::Send() sendto() error: %s\n", GetErrorString());
                        break;
                }   
                return false; 
            }
            else
            {
                ASSERT(result == numBytes);
                break;
            }
        }
    }
    return true;
}  // end LinuxCap::Send()

bool LinuxCap::SendCollectMdGre(const char* buffer, unsigned int& numBytes)
{
    if (INVALID_HANDLE == gre_raw_fd)
    {
        PLOG(PL_ERROR, "LinuxCap::SendCollectMdGre() error: no IPPROTO_GRE socket\n");
        return false;
    }
    if (!tunnel_remote_addr.IsValid() ||
        tunnel_remote_addr.IsUnspecified() ||
        (ProtoAddress::IPv4 != tunnel_remote_addr.GetType()))
    {
        PLOG(PL_WARN, "LinuxCap::SendCollectMdGre() error: unicast IPv4 remote required\n");
        return false;
    }

    UINT16 greProto;
    switch ((buffer[0] & 0xf0) >> 4)
    {
        case 4:
            greProto = htons(ETH_P_IP);
            break;
        case 6:
            greProto = htons(ETH_P_IPV6);
            break;
        default:
            PLOG(PL_WARN, "LinuxCap::SendCollectMdGre() error: invalid IP protocol version!\n");
            return false;
    }

    // Basic GRE header (no key/seq): flags+version (0) and payload protocol.
    UINT8 greHdr[4];
    memset(greHdr, 0, sizeof(greHdr));
    memcpy(greHdr + 2, &greProto, 2);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    memcpy(&dst.sin_addr, tunnel_remote_addr.GetRawHostAddress(), 4);

    struct iovec iov[2];
    iov[0].iov_base = greHdr;
    iov[0].iov_len = sizeof(greHdr);
    iov[1].iov_base = const_cast<char*>(buffer);
    iov[1].iov_len = numBytes;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &dst;
    msg.msg_namelen = sizeof(dst);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;

    for (;;)
    {
        ssize_t result = sendmsg(gre_raw_fd, &msg, 0);
        if (result < 0)
        {
            switch (errno)
            {
                case EINTR:
                    continue;
                case EWOULDBLOCK:
                    numBytes = 0;
                default:
                    PLOG(PL_WARN, "LinuxCap::SendCollectMdGre() sendmsg() error: %s\n", GetErrorString());
                    break;
            }
            return false;
        }
        break;
    }
    return true;
}  // end LinuxCap::SendCollectMdGre()

bool LinuxCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    struct sockaddr_ll pktAddr;
    socklen_t addrLen = sizeof(pktAddr);
    int result = recvfrom(descriptor, buffer, (size_t)numBytes, 0, 
                          (struct sockaddr*)&pktAddr, &addrLen); 
    if (result < 0)
    {
        numBytes = 0;
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                return true;
            default:
                PLOG(PL_ERROR, "LinuxCap::Recv() error: %s\n", GetErrorString());
                break;
        }
        return false;
    }
    else
    {
        /*
        void* ipBuffer = (UINT32*)buffer;
        unsigned int ipLen = result;
        if (ProtoNet::IFACE_GRE != if_type)
        {
            ProtoPktETH ethPkt(buffer, result);
            ipBuffer = ethPkt.AccessPayload();
            ipLen = ethPkt.GetPayloadLength();
        }
        ProtoPktIP ipPkt;
        if (ipPkt.InitFromBuffer(ipLen, ipBuffer, ipLen))
        {
            ProtoAddress srcAddr, dstAddr;
            ipPkt.GetSrcAddr(srcAddr);
            ipPkt.GetDstAddr(dstAddr);
            ProtoPktIP::Protocol protocol = ProtoPktIP::RESERVED;
            switch (ipPkt.GetVersion())
            {
                case 4:
                {
                    TRACE("IPv4 ");
                    ProtoPktIPv4 ip4(ipPkt);
                    protocol = ip4.GetProtocol();
                    break;
                }
                case 6:
                {
                    TRACE("IPv6 ");
                    ProtoPktIPv6 ip6(ipPkt);
                    protocol = ip6.GetNextHeader();
                    break;
                }
                default:
                    TRACE("IPv%d ??? ", ipPkt.GetVersion());
                    break;
            }
            TRACE("src:%s ", srcAddr.GetHostString());
            TRACE("dst:%s protocol:%d\n", dstAddr.GetHostString(), protocol);
        }
         
        char* ptr = buffer;
        for (int i = 0; i < 4; i++)
        {
            TRACE("    ");
            for (int j = 0; j < 16; j++)
            {
                TRACE("%02x%02x ", *ptr, *(ptr+1));
                ptr += 2;
            }
            TRACE("\n");
        }
        TRACE("\n");
        */
        if (NULL != direction)
        {
            if (pktAddr.sll_pkttype == PACKET_OUTGOING)
                *direction = OUTBOUND;
            else 
                *direction = INBOUND; 
        }
        numBytes = result; 
        return true;   
    }
}  // end LinuxCap::Recv()
