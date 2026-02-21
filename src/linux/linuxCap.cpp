
#include "protoCap.h"
#include "protoDebug.h"
#include "protoSocket.h"
#include "protoNet.h"

#include "protoPktIP.h"
#include "protoPktETH.h"
#include "protoPktGRE.h"

#include <unistd.h>
#include <sys/socket.h>
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
        
};  // end class LinuxCap

ProtoCap* ProtoCap::Create()
{
    return static_cast<ProtoCap*>(new LinuxCap());   
}  // end ProtoCap::Create()

LinuxCap::LinuxCap()
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
    if_type = ProtoNet::GetInterfaceType(ifIndex, &tunnel_local_addr, &tunnel_remote_addr);
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
    
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, if_addr))
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
        // Use sendto() to denote IP/IPv6 properly 
        // (ensures GRE protocol type is correct)
        struct sockaddr_ll addr;
        memset(&addr, 0, sizeof(struct sockaddr_ll));
        addr.sll_family   = AF_PACKET;
        addr.sll_ifindex  = if_index;
        addr.sll_halen = 0;
        
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
