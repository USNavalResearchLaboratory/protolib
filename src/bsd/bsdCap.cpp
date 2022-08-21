/** 
 * @file bsdCap.cpp
 * 
 * @brief This is an implementation of the Protolib class ProtoCap
 *  using the BSD PF_NDRV socket for packet capture
 *
 */
/** 
 * @class BsdCap
 * 
 * @brief This is an implementation of the Protolib class ProtoCap
 *  using the BSD PF_NDRV socket for packet capture
 *
 *  (THIS WILL _NOT_ WORK FOR AN INTERFACE THAT HAS IP ENABLED ALREADY !!!)
 *
 *  SO, ONE SHOULD PROBABLY USE "bpfCap.cpp" on BSD for most purposes
 */
 
#include "protoCap.h"  // for ProtoCap definition
#include "protoSocket.h"
#include "protoDebug.h"
#include "protoPktETH.h"  // for Ether Type enumeration

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/dlil.h>
#include <net/ndrv.h>
#include <stdio.h>  // for snprintf()
#include <fcntl.h>   // for open()
#include <unistd.h>  // for close()

class BsdCap : public ProtoCap
{
    public:
        BsdCap();
        ~BsdCap();
            
        bool Open(const char* interfaceName = NULL);
        void Close();
        bool Send(const char* buffer, unsigned int& numBytes);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
};  // end class BsdCap

/**
 * @brief Returns a new instance of BsdCap cast to a pointer to a ProtoCap.
 *
 */

ProtoCap* ProtoCap::Create()
{
    return (static_cast<ProtoCap*>(new BsdCap));   
}

/**
 * @brief Enables input notification by default.
 *
 */

BsdCap::BsdCap()
{
    StartInputNotification();  // enable input notification by default
}

BsdCap::~BsdCap()
{
    Close();   
}
/**
 * @brief Associates a BSD PF-NDRV socket with an interface.
 *
 * Associates a BSD PF_NDRV socket with the specified interface or it not provded, the 
 * default local interface will be used. 
 *
 * @param interfaceName
 * @return success or failure indicator 
 */


bool BsdCap::Open(const char* interfaceName)
{
    Close();  // just in case
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            PLOG(PL_ERROR, "BsdCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             PLOG(PL_ERROR, "BsdCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
    }
    
    ProtoAddress macAddr;
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, if_addr))
        PLOG(PL_ERROR, "BsdCap::Open() warning: unable to get MAC address for interface \"%s\"\n", interfaceName);
    
    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    
    // 1) Open a PF_NDRV socket
    descriptor = socket(PF_NDRV, SOCK_RAW, 0);
    if (descriptor < 0)
    {
        PLOG(PL_ERROR, "BsdCap::Open() error: socket(PF_NDRV) failure: %s\n", GetErrorString());
        return false;
    }
    
    // 2) Bind to our interface name
    struct sockaddr_ndrv sa;
    sa.snd_len = sizeof(sa);
    sa.snd_family = PF_NDRV;
    strncpy((char*)sa.snd_name, interfaceName, IFNAMSIZ);
    if (bind(descriptor, (struct sockaddr*)&sa, sa.snd_len) < 0)
    {
        PLOG(PL_ERROR, "BsdCap::Open() error: bind(%s) failure: %s\n", interfaceName, GetErrorString());
        Close();
        return false;
    }
    
    // 3) Register for IP, IPv6, and ARP packets
    
    struct ndrv_demux_desc desc[3];
    memset(desc, 0, 3*sizeof(desc));
    
    desc[0].type = NDRV_DEMUXTYPE_ETHERTYPE;
    desc[0].length = sizeof(unsigned short);
    desc[0].data.ether_type = htons(ProtoPktETH::IP);
    desc[1].type = NDRV_DEMUXTYPE_ETHERTYPE;
    desc[1].length = sizeof(unsigned short);
    desc[1].data.ether_type = htons(ProtoPktETH::IPv6);
    desc[2].type = NDRV_DEMUXTYPE_ETHERTYPE;
    desc[2].length = sizeof(unsigned short);
    desc[2].data.ether_type = htons(ProtoPktETH::ARP);
    
    
    struct ndrv_protocol_desc ndrvDesc;
    ndrvDesc.version = NDRV_PROTOCOL_DESC_VERS;
    ndrvDesc.protocol_family = PF_INET; 
    ndrvDesc.demux_count = 3;
    ndrvDesc.demux_list = desc;
    
    if (setsockopt(descriptor, SOL_NDRVPROTO, NDRV_SETDMXSPEC,
                   &ndrvDesc, sizeof(ndrvDesc)) < 0)
    {
        PLOG(PL_ERROR, "BsdCap::Open() error: setsockopt(NDRV_SETDMXSPEC) failure: %s\n",
                GetErrorString());
        Close();
        return false;   
    }
    
    if (!ProtoCap::Open(interfaceName))
    {
        PLOG(PL_ERROR, "BsdCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    if_index = ifIndex;
    return true;
}  // end BsdCap::Open()

void BsdCap::Close()
{
    if (descriptor >= 0)
    {
        ProtoCap::Close();
        close(descriptor);
        descriptor = -1;   
    }
}  // end BsdCap::Close()

/**
 * @brief Recv's the packet into buffer.  
 *
 * @param buffer
 * @param numBytes
 * @param direction
 *
 * @return success or failure indicator 
 */

bool BsdCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    if (direction) *direction = UNSPECIFIED;
    for (;;)
    {
        ssize_t result = read(descriptor, buffer, numBytes);
        if (result < 0)
        {
            switch (errno)
            {
                case EINTR:
                    continue;  // try again
                case EWOULDBLOCK:
                    numBytes = 0;
                    return true;
                default:
                    PLOG(PL_ERROR, "BsdCap::Recv() read() error: %s\n",
                        GetErrorString());
                    numBytes = 0;
                    return false;   
            }
        }
        else
        {
            numBytes = result;
            break;
        }
    }
    return true;
}  // end BsdCap::Recv()
/**
 * @brief Write packet to the bsd socket descriptor.
 *
 * 802.3 frames are not supported
 *
 * @param buffer
 * @param buflen
 *
 * @return success or failure indicator 
 */


bool BsdCap::Send(const char* buffer, unsigned int& numBytes)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
        PLOG(PL_DEBUG, "BsdCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
        return false;
    }
    for (;;)
    {
        struct sockaddr sockAddr;  // note the raw frame device here does not use the sockAddr
        ssize_t result = sendto(descriptor, buffer+put, buflen-put, 0, &sockAddr, sizeof(sockAddr));
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
                    PLOG(PL_ERROR, "BsdCap::Send() error: %s", GetErrorString());
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
}  // end BsdCap::Send()
