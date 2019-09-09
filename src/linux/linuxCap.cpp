
#include "protoCap.h"
#include "protoDebug.h"
#include "protoSocket.h"

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
    
    if ((descriptor = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        PLOG(PL_ERROR, "LinuxCap::Open() socket(PF_PACKET) error: %s\n", GetErrorString());
        return false;   
    }
    
    // try to turn on broadcast capability (why?)
    int enable = 1;
    if (setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
        PLOG(PL_ERROR, "LinuxCap::Open() setsockopt(SO_BROADCAST) warning: %s\n", 
                GetErrorString());
    
    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "LinuxCap::Open() error getting interface index\n");
        Close();
        return false;   
    }
    
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
    
    // Init our interface address structure  
    struct sockaddr_ll  ifaceAddr; 
    memset((char*)&ifaceAddr, 0, sizeof(ifaceAddr));
    ifaceAddr.sll_protocol = htons(ETH_P_ALL);
    ifaceAddr.sll_ifindex = ifIndex;
    ifaceAddr.sll_family = AF_PACKET;
    memcpy(ifaceAddr.sll_addr, if_addr.GetRawHostAddress(), 6);
    ifaceAddr.sll_halen = if_addr.GetLength();
    
    // bind() the socket to the specified interface
    if (bind(descriptor, (struct sockaddr*)&ifaceAddr, sizeof(ifaceAddr)) < 0)
    {
        PLOG(PL_ERROR, "LinuxCap::Open() bind error: %s\n", GetErrorString());
        Close();
        return false;      
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
    }  
}  // end LinuxCap::Close()

bool LinuxCap::Send(const char* buffer, unsigned int& numBytes)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
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
                    PLOG(PL_WARN, "LinuxCap::Send() error: %s\n", GetErrorString());
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
