/** 
 * @file bpfCap.cpp
 * @brief ProtoCap class using Berkely Packet Filter (bpf) for packet capture.
 */

/**
 *
 * @class BpfCap
 * 
 * @brief This is an implementation of the Protolib class ProtoCap
 * using the Berkeley Packet Filter (bpf) for packet capture
 *
 */
 
#include "protoCap.h"  // for ProtoCap definition
#include "protoSocket.h"
#include "protoDebug.h"


#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdio.h>  // for snprintf()
#include <fcntl.h>   // for open()
#include <unistd.h>  // for close()

// NOTE: On MacOSX (Darwin), we may want to use a PF_NDRV socket instead ???

class BpfCap : public ProtoCap
{
    public:
        BpfCap();
        ~BpfCap();
            
        bool Open(const char* interfaceName = NULL);
        void Close();
        bool Send(const char* buffer, unsigned int& numBytes);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
    
    private:
        char*           bpf_buffer;
        unsigned int    bpf_buflen;
        unsigned int    bpf_captured;
        unsigned int    bpf_index;    
};  // end class BpfCap


ProtoCap* ProtoCap::Create()
{
    return (static_cast<ProtoCap*>(new BpfCap));   
}
/**
 * @brief Enables input notification by default.
 *
 */
BpfCap::BpfCap()
 : bpf_buffer(NULL), bpf_buflen(0), bpf_captured(0), bpf_index(0)
{
    StartInputNotification();  // enable input notification by default
}

BpfCap::~BpfCap()
{
    Close();   
}
/**
 * @brief Associates a bpf filter with an interface.
 *
 * Associates a Berkely Packet Filter with the specified interface or it not provded, the 
 * default local interface will be used.  Enables immediate packet by packet notification.
 *
 * @param interfaceName
 * @return success or failure indicator 
 */

bool BpfCap::Open(const char* interfaceName)
{
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            PLOG(PL_ERROR, "BpfCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             PLOG(PL_ERROR, "BpfCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
    }
    
    ProtoAddress macAddr;
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, if_addr))
        PLOG(PL_ERROR, "BpfCap::Open() warning: unable to get MAC address for interface \"%s\"\n", interfaceName);
    
    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    
    int i = 0;
    int fd = -1;
    do
    {
        char bpfName[256];
        bpfName[255] = '\0';
        snprintf(bpfName, 255, "/dev/bpf%d", i++);
        fd = open(bpfName, O_RDWR);
    } while ((fd < 0) && (EBUSY == errno));
    
    if (fd < 0)
    {
        PLOG(PL_ERROR, "BpfCap::Open() all bpf devices busy\n");
        return false;   
    }    
    
    // Check the BPF version
    struct bpf_version bpfVersion;
    if (ioctl(fd, BIOCVERSION, (caddr_t)&bpfVersion) < 0) 
    {
		PLOG(PL_ERROR, "BpfCap::Open() ioctl(BIOCVERSION) error: %s\n",
                GetErrorString());
		close(fd);
		return false;
	}
	if (bpfVersion.bv_major != BPF_MAJOR_VERSION ||
	    bpfVersion.bv_minor < BPF_MINOR_VERSION) 
    {
		PLOG(PL_ERROR, "BpfCap::Open() kernel bpf version out of date\n");
        close(fd);
		return false;
	}
    // Set which interface we interested in, and buffer size
    unsigned int buflen;
    if ((ioctl(fd, BIOCGBLEN, (caddr_t)&buflen) < 0) || buflen < 32768)
		buflen = 32768;	
    
    for ( ; buflen != 0; buflen >>= 1) 
    {
        ioctl(fd, BIOCSBLEN, (caddr_t)&buflen);
        struct ifreq ifr;
        strncpy(ifr.ifr_name, interfaceName, sizeof(ifr.ifr_name));
		if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) >= 0)
			break;	
		if (errno != ENOBUFS) 
        {
            PLOG(PL_ERROR, "BpfCap::Open() ioctl(BIOCSETIF) error: %s\n",
                   GetErrorString());
            close(fd);
            return false;
		}
    }
    
    if (0 == buflen)
    {
        PLOG(PL_ERROR, "BpfCap::Open() unable to set bpf buffer\n");
        close(fd);
        return false;
    }    
    // Set interface to promiscuous mode
    // (TBD) Allow it to open in non-promiscuous mode???
    if (ioctl(fd, BIOCPROMISC, NULL) < 0) 
    {
        PLOG(PL_ERROR, "BpfCap::Open() ioctl(BIOCPROMISC) error: %s\n",
                GetErrorString());
        close(fd);
        return false;
    }
    
    // For protolib purposes we generally want immediate
    // packet-by-packet notification
    unsigned int enable = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &enable) < 0) 
    {
		PLOG(PL_ERROR, "BpfCap::Open() ioctl(BIOCIMMEDIATE) error: %s\n",
                GetErrorString());
        close(fd);
        return false;
	}    
    
    // Set it to non-blocking, too
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags)
    {
        PLOG(PL_ERROR, "BpfCap::Open() fcnt(F_GETFL) error: %s\n",
                GetErrorString());
        close(fd);
        return false;   
    }
    flags |= O_NONBLOCK;
    if (-1 == fcntl(fd, F_SETFL, flags))
    {
        PLOG(PL_ERROR, "BpfCap::Open() fcnt(F_SETFL O_NONBLOCK) error: %s\n",
                GetErrorString());
        close(fd);
        return false;  
    }
    
    if (ioctl(fd, BIOCGBLEN, (caddr_t)&buflen) < 0) 
    {
		PLOG(PL_ERROR, "BpfCap::Open() ioctl(BIOCGBLEN) error: %s\n",
                GetErrorString());
        close(fd);
        return false;
	}
    
    Close(); // just in case
    
    // Allocate buffer for reading available packets
    if (NULL == (bpf_buffer = new char[buflen]))
    {
        PLOG(PL_ERROR, "BpfCap::Open() new bp_buffer error: %s\n",
                GetErrorString());
        Close();
        return false;   
    }
    bpf_buflen = buflen;
    descriptor = fd;
    
    if (!ProtoCap::Open(interfaceName))
    {
        PLOG(PL_ERROR, "BpfCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    if_index = ifIndex;
    return true;
}  // end BpfCap::Open()

void BpfCap::Close()
{
    if (NULL != bpf_buffer)
    {
        delete[] bpf_buffer;
        bpf_buffer = NULL;   
        bpf_buflen = 0; 
    }    
    ProtoCap::Close();
    if (INVALID_HANDLE != descriptor)
    {
        close(descriptor);
        descriptor = INVALID_HANDLE; 
    }  
}  // end BpfCap::Close()


/**
 * @brief Recv's the packet into buffer.  
 *
 * @param buffer
 * @param numBytes
 * @param direction
 *
 * @return success or failure indicator 
 */

bool BpfCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    if (NULL != direction) *direction = INBOUND;
    if (bpf_index >= bpf_captured)
    {
        for (;;)
        {
            ssize_t result = read(descriptor, bpf_buffer, bpf_buflen);
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
                        PLOG(PL_ERROR, "BpfCap::Recv() read() error: %s\n",
                            GetErrorString());
                        numBytes = 0;
                        return false;   
                }
            }
            else
            {
                bpf_captured = result;
                bpf_index = 0;
                break;
            }
        }
    }
    
    // Determine next packet (if applicable)
    if (bpf_captured > bpf_index)
    {
        // The (void*) cast here avoids alignment warnings from the compiler
        struct bpf_hdr* bpfHdr = (struct bpf_hdr*)((void*)(bpf_buffer + bpf_index));
        if (numBytes >= bpfHdr->bh_caplen)
        {
            memcpy(buffer, bpf_buffer+bpf_index+bpfHdr->bh_hdrlen, bpfHdr->bh_caplen);
            numBytes = bpfHdr->bh_caplen;
            bpf_index += BPF_WORDALIGN(bpfHdr->bh_caplen + bpfHdr->bh_hdrlen);
        }
        else
        {
            PLOG(PL_ERROR, "BpfCap::Recv() error packet too big for buffer\n");
            return false;
        }
    }
    else
    {
        numBytes = 0;
    }
    // TBD - make sure this doesn't screw up inbound multicast/broadcast traffic
    if ((NULL != direction) && (0 == memcmp(if_addr.GetRawHostAddress(), buffer+6, 6)))
        *direction = OUTBOUND;
    return true;
}  // end BpfCap::Recv()

/**
 * @brief Write packet to the bpf descriptor.
 *
 * 802.3 frames are not supported
 *
 * @param buffer
 * @param buflen
 *
 * @return success or failure indicator 
 */

bool BpfCap::Send(const char* buffer, unsigned int& numBytes)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
        PLOG(PL_ERROR, "BpfCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
        return false;
    }
    for (;;)
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
                    PLOG(PL_ERROR, "BpfCap::Send() error: %s", GetErrorString());
                    break;
            }   
            return false;              
        }
        else
        {
            ASSERT(result == (ssize_t)numBytes);
            break;
        }
    }
    return true;
}  // end BpfCap::Send()
