
#include "protoPacketeer.h"
#include "protoDebug.h"
#include "protoSocket.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <features.h>
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif /* __GLIBC__ */
#include <netinet/in.h>
#include <unistd.h>

class LinuxPacketeer : public ProtoPacketeer
{
    public:
        LinuxPacketeer();
        ~LinuxPacketeer();
        
        virtual bool Open(const char* interfaceName);
        virtual bool IsOpen() {return (-1 != socket_fd);}
        virtual void Close();
        virtual bool Send(const char* buffer, unsigned int buflen);
        
    private:
        int                 socket_fd; 
        struct sockaddr_ll  iface_addr;   
            
};  // class LinuxPacketeer

ProtoPacketeer* ProtoPacketeer::Create()
{
    return static_cast<ProtoPacketeer*>(new LinuxPacketeer());   
}  // end ProtoPacketeer::Create()

LinuxPacketeer::LinuxPacketeer()
 : socket_fd(-1)
{
}

LinuxPacketeer::~LinuxPacketeer()
{
    Close();
}

bool LinuxPacketeer::Open(const char* interfaceName)
{
    if ((socket_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        PLOG(PL_ERROR, "LinuxPacketeer::Open() socket() error: %s\n", GetErrorString());
        return false;  
    }
    // try to turn on broadcast capability
    int enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
        PLOG(PL_ERROR, "LinuxPacketeer::Open() setsockopt(SO_BROADCAST) warning: %s\n", 
                GetErrorString());

    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    if (0 == ifIndex)
    {
        PLOG(PL_ERROR, "LinuxPacketeer::Open() error getting interface index\n");
        Close();
        return false;   
    }
    // Init our interface address structure for sends   
    memset((char*)&iface_addr, 0, sizeof(iface_addr));
    iface_addr.sll_family = AF_PACKET;
    iface_addr.sll_ifindex = ifIndex;
    return true;
}  // end LinuxPacketeer::Open()

void LinuxPacketeer::Close()
{
    if (IsOpen())
    {
        close(socket_fd);
        socket_fd = -1;
    }       
}  // end LinuxPacketeer::Close()

bool LinuxPacketeer::Send(const char* buffer, unsigned int buflen)
{
    UINT16 frameType;
    memcpy(&frameType, buffer+12, 2);
    iface_addr.sll_protocol = frameType;  // ntohs() not needed???
    size_t result = sendto(socket_fd, buffer, buflen, 0,
                           (struct sockaddr*)&iface_addr,
                           sizeof(iface_addr));
    if (result != buflen)
    {
        PLOG(PL_ERROR, "LinuxPacketeer::Send() sendto() error: %s\n", GetErrorString());
        return false;  
    }    
    return true;
}  // end LinuxPacketeer::Send()
