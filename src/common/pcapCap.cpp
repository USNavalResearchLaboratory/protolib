/** 
 * @file pcapCap.cpp
 * @brief Implementation of ProtoCap class using "libpcap" for packet capture.
 */
 
#include "protoCap.h"  // for ProtoCap definition
#include "protoSocket.h"
#include "protoDebug.h"

#ifdef WIN32
//#include <winpcap.h>
#include <pcap.h>
#else
#include <pcap.h>    // for libpcap routines
#include <unistd.h>  // for write()
#endif  // end if/else WIN32/UNIX
/** 
 * @class PcapCap
 *
 * @brief This is an implementation of the Protolib class ProtoCap
 *  using "libpcap" for packet capture
 *
 */
class PcapCap : public ProtoCap
{
    public:
        PcapCap();
        ~PcapCap();
            
        bool Open(const char* interfaceName = NULL);
        bool IsOpen(){return (NULL != pcap_device);}
        void Close();
        bool Send(const char* buffer, unsigned int buflen);
        bool Forward(char* buffer, unsigned int buflen);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
    
    private:
        pcap_t*         pcap_device; 
        char            eth_addr[6];    
};  // end class PcapCap


ProtoCap* ProtoCap::Create()
{
    return (static_cast<ProtoCap*>(new PcapCap));   
}

PcapCap::PcapCap()
 : pcap_device(NULL)
{
    StartInputNotification();  // enable input notification by default
}

PcapCap::~PcapCap()
{
    Close();   
}

bool PcapCap::Open(const char* interfaceName)
{
    int ifIndex;
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            PLOG(PL_ERROR, "PcapCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             PLOG(PL_ERROR, "PcapCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
        ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    } 
	else 
	{
		ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
		if (ifIndex != 0) 
		{
			if(!ProtoSocket::GetInterfaceName(ifIndex ,buffer,256))
			{
				PLOG(PL_ERROR,"PcapCap::Open() error: couldn't get interface name of index %d\n",ifIndex);
				return false;
			}
			interfaceName = buffer;
		}
		else 
		{
			PLOG(PL_ERROR,"PcapCap::Open() error: coun't get interface index from interface name %s\n",interfaceName);
			return false;
		}
	}
    ProtoAddress macAddr;
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, macAddr))
    {
        PLOG(PL_ERROR, "PcapCap::Open() error getting interface MAC address\n");
        return false;   
    }
    memcpy(eth_addr, macAddr.GetRawHostAddress(), 6);
    Close(); // just in case
    char errbuf[PCAP_ERRBUF_SIZE+1];
    errbuf[0] = '\0';

#ifdef WIN32
    pcap_if_t *alldevs;
    pcap_if_t *d;

    /* Retrieve the device list to get the device prefix */
    /* (pcap index is different than windows adapater index! */

    if (pcap_findalldevs(&alldevs,errbuf) == -1)
    {
        PLOG(PL_ERROR,"PcapCap::Open pcap_findalldevs failed.\n");

        return false;
    }
    /* Find the selected adapter to get the full device name*/
    int i;
    for (d=alldevs,i=0;d != NULL;d=d->next,i++)
    {
        if (char* namePtr = strchr(d->name,'{'))
            if (!strcmp(interfaceName,namePtr))
                break;
    }

    if (d == NULL)
    {
        PLOG(PL_ERROR,"PcapCap::Open() Device (%s) not found in pcap list!\n",interfaceName);
        pcap_freealldevs(alldevs);
        return false;
    }

	TRACE("interfacename before pcap open live %s %s\n",d->name,d->description);

    pcap_device = pcap_open_live(d->name, 65535, 1, 0, errbuf);
#else
	TRACE("interfacename before pcap open live %s\n",interfaceName);

    pcap_device = pcap_open_live((char *)interfaceName, 65535, 1, 0, errbuf);
#endif
    if (NULL == pcap_device)
    {
        PLOG(PL_ERROR, "pcapExample: pcap_open_live() error: %s\n", errbuf);
#ifdef WIN32
        pcap_freealldevs(alldevs);
#endif
        return false;   
    }

#ifdef WIN32    // set a minimum pkt num to copy
    pcap_setmintocopy(pcap_device,1);
#endif

    // set non-blocking for async I/O
    if (-1 == pcap_setnonblock(pcap_device, 1, errbuf))
        PLOG(PL_ERROR, "pcapExample: pcap_setnonblock() warning: %s\n", errbuf);
#ifdef WIN32
    input_handle = pcap_getevent(pcap_device);
#else
    descriptor = pcap_get_selectable_fd(pcap_device);
#endif // if/else WIN32/UNIX
    
    if (!ProtoCap::Open(interfaceName))
    {
        PLOG(PL_ERROR, "PcapCap::Open() ProtoCap::Open() error\n");
        Close();
#ifdef WIN32
        pcap_freealldevs(alldevs);
#endif
        return false;   
    }
    if_index = ifIndex;
#ifdef WIN32
    pcap_freealldevs(alldevs);
#endif
    return true;
}  // end PcapCap::Open()

void PcapCap::Close()
{
    if (NULL != pcap_device)  
    {
        ProtoCap::Close();
        pcap_close(pcap_device);
        pcap_device = NULL;
#ifdef WIN32
        input_handle = INVALID_HANDLE_VALUE;
#else
        descriptor = -1;
#endif // if/else WIN32/UNIX
    } 
}  // end PcapCap::Close()

bool PcapCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    struct pcap_pkthdr* hdr;
    const u_char* data; 
    if (NULL != direction) *direction = UNSPECIFIED;  // (TBD) try to get a direction
    switch (pcap_next_ex(pcap_device, &hdr, &data))
    {
        case 1:     // pkt read
        {
            unsigned int copyLen = (numBytes > hdr->caplen) ? hdr->caplen : numBytes;
            memcpy(buffer, data, copyLen);
            numBytes = copyLen;
            if ((NULL != direction) && (0 == memcmp(eth_addr, buffer+6, 6)))
                *direction = OUTBOUND;
            return true;
        }
        case 0:     // no pkt ready?
        {
            numBytes = 0;
            return true;
        }
        default:    // error (or EOF for offline)
        {
            PLOG(PL_ERROR, "PcapCap::Recv() pcap_next_ex() error\n");
            numBytes = 0;
            return false;
        }
    } 
}  // end PcapCap::Recv()

bool PcapCap::Send(const char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
	if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            PLOG(PL_DEBUG, "PcapCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }

#ifdef WIN32
    int pcapreturn = pcap_sendpacket(pcap_device,(unsigned char*)buffer, buflen);
    TRACE("pcapreturn value in send %d\n",pcapreturn);
#else
    unsigned int put = 0;
    while (put < buflen)
    {
        ssize_t result = write(descriptor, buffer, buflen);
        if (result > 0)
        {
            put += result;
        }
        else
        {
            if (EINTR == errno)
            {
                continue;  // try again
            }
            else
            {
                PLOG(PL_ERROR, "PcapCap::Send() error: %s", GetErrorString());
                return false;
            }           
        }
    }
#endif
    return true;
}  // end PcapCap::Send()

bool PcapCap::Forward(char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            PLOG(PL_DEBUG, "PcapCap::Forward() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }
    // Change the src MAC addr to our own
    // (TBD) allow caller to specify dst MAC addr ???
    memcpy(buffer+6, eth_addr, 6);
#ifdef WIN32
    int pcapreturn = pcap_sendpacket(pcap_device, (unsigned char*)buffer, buflen);
	TRACE("this is the pcapreturn value in Forward %d\n",pcapreturn);
#else
    unsigned int put = 0;
    while (put < buflen)
    {
        ssize_t result = write(descriptor, buffer, buflen);
        if (result > 0)
        {
            put += result;
        }
        else
        {
            if (EINTR == errno)
            {
                continue;  // try again
            }
            else
            {
                PLOG(PL_ERROR, "PcapCap::Forward() error: %s", GetErrorString());
                return false;
            }           
        }
    }
#endif
    return true;
}  // end PcapCap::Forward()
