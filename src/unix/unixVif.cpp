#include "protoVif.h"
#include "protoNet.h"
#include "protoDebug.h"

#ifdef UNIX
#include <unistd.h>      // for close()
#include <net/if.h>      // for ifreq
#include <fcntl.h>       // for open()
#include <sys/ioctl.h>   // for ioctl()
#include <stdlib.h>      // for system()
#include <stdio.h>       // for sprintf()
#endif

#ifdef LINUX
extern "C" 
{
#include <linux/if_tun.h>
}
#endif // LINUX

class UnixVif : public ProtoVif
{
  public:
    UnixVif();
    ~UnixVif();
            
    bool Open(const char* vifName, const ProtoAddress& ipAddr, unsigned int maskLen);

    void Close();
    
    bool SetHardwareAddress(const ProtoAddress& ethAddr);
    
    bool SetARP(bool status);
    
    bool Write(const char* buffer, unsigned int numBytes);
    
    bool Read(char* buffer, unsigned int& numBytes);
          
}; // end class ProtoVif

ProtoVif* ProtoVif::Create()
{
    return static_cast<ProtoVif*>(new UnixVif);
}  // end ProtoVif::Create()

UnixVif::UnixVif()
{
}

UnixVif::~UnixVif()
{
}

bool UnixVif::Open(const char* vifName, const ProtoAddress& ipAddr, unsigned int maskLen)
{
    Close();  // in case already open
#ifdef LINUX 
#ifdef __ANDROID__
    const char* devName = "/dev//tun";
#else
    // 0) Prefer the flow control enabled TUN/TAP first
    const char* devName = "/dev/net/tun_flowctl";
#endif // if/else __ANDROID__
    descriptor = open(devName, O_RDWR);
    if (descriptor < 0)
    {
        // 1) Open up the TUN/TAP device
        const char* devName = "/dev/net/tun";
        if ((descriptor = open(devName, O_RDWR)) < 0)
        {
            PLOG(PL_ERROR,"UnixVif::Open(%s) error: open(\"%s\") failed: %s\n", vifName, devName, GetErrorString());            
            return false;
        }
    }
    // 2) Set up a TAP virtual interface with given "vifName"
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    // Flags: IFF_TUN   - TUN device (no Ethernet headers) 
    //        IFF_TAP   - TAP device (includes ethernet headers)  
    //        IFF_NO_PI - Do not provide packet information  
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, vifName, IFNAMSIZ);
    if (ioctl(descriptor, TUNSETIFF, &ifr) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::Open(%s) error: ioctl(TUNSETIFF) failed: %s\n", vifName, GetErrorString());            
        Close(); 
        return false;
    }
    /* doesn't do what i want!
    // This enables flow control in the Linux tap device?
    int sndbuf = 500*1500;  // 500 packets worth?
    if (ioctl(descriptor, TUNSETSNDBUF, &sndbuf) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::Open(%s) error: ioctl(TUNSETIFF) failed: %s\n", vifName, GetErrorString());   
    }
    TRACE("set tap sndbuf to %d\n", sndbuf);
    */
    strncpy(vif_name, vifName, VIF_NAME_MAX);
#endif // LINUX
    
#ifdef MACOSX
    // On MacOSX, we have to iteratively try tap0, tap1, etc until we find one we can use
    char devName[PATH_MAX];
    for (int i = 0; i < 256; i++)
    {
        //TRACE("trying to open /dev/tap%d ...\n", i);
        sprintf(devName, "/dev/tap%d", i);
        if ((descriptor = open(devName, O_RDWR)) < 0)
        {
            PLOG(PL_ERROR,"UnixVif::Open() error: open(\"%s\") failed: %s\n", devName, GetErrorString());            
            continue;
        }   
        sprintf(vif_name, "tap%d", i);
        break;
    }    
    if (INVALID_HANDLE == descriptor)
    {
        PLOG(PL_ERROR,"UnixVif::Open() error: no TAP device available!\n");            
        return false;
    }
#endif  // MACOSX  
    
    // 3) Configure the interface via "ifconfig" command
    // (TBD) Do this with an ioctl() call instead??
    char cmd[1024];
#ifdef __ANDROID__
    // Bring interface up and give it an address
    sprintf(cmd, "ip link set %s up", vif_name);
#else
    if (ipAddr.IsValid())  //IP address specified
        sprintf(cmd, "/sbin/ifconfig %s %s/%d up", vif_name, ipAddr.GetHostString(), maskLen);
    else  // IP address NOT specified, use system scripts
        sprintf(cmd, "/sbin/ifconfig %s up", vif_name);
#endif // if/else __ANDROID__
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::Open(%s) error: \"%s\n\" failed: %s\n", vifName, cmd, GetErrorString());
        Close();
        return false;       
    }
#ifdef __ANDROID__
    // On Android, addr is assigned as a separate step
    if (ipAddr.IsValid())
    {
        if (!ProtoNet::AddInterfaceAddress(vif_name, ipAddr, maskLen))
        {
            PLOG(PL_ERROR, "UnixVif::Open(%s) error: unable to assign IP address!\n", vifName);
            Close();
            return false;
        } 
    }
#endif // __ANDROID__    
    // 4) Snag the virtual interface hardware address
    if (!ProtoNet::GetInterfaceAddress(vif_name, ProtoAddress::ETH, hw_addr))
        PLOG(PL_ERROR, "UnixVif::Open(%s) error: unable to get ETH address!\n", vif_name);

    // 5) Make a call to "ProtoChannel::Open()" to install event dispatching if applicable
    if (!ProtoChannel::Open())
    {
        PLOG(PL_ERROR, "UnixVif::Open(%s) error: couldn't install ProtoChannel\n", vif_name);
        Close();
        return false;    
    }
    else
    {
        return true;
    }
}  // end UnixVif::Open()

void UnixVif::Close()
{
    ProtoChannel::Close();
    close(descriptor);
    descriptor = INVALID_HANDLE;   
}  // end UnixVif::Close()

bool UnixVif::SetARP(bool status)
{
    char cmd[1024];
    sprintf(cmd, "/sbin/ifconfig %s %s", vif_name, status ? "arp" : "-arp");
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::SetARP() error: \"%s\n\" failed: %s\n", cmd, GetErrorString());
        return false;       
    }
    return true;
}  // end UnixVif::SetARP()

bool UnixVif::SetHardwareAddress(const ProtoAddress& ethAddr)
{
    if (ProtoAddress::ETH != ethAddr.GetType())
    {
        PLOG(PL_ERROR, "UnixVif::SetHardwareAddress() error: invalid address type!\n");
        return false;
    }
    const UINT8* addr = (const UINT8*)ethAddr.GetRawHostAddress();
    char cmd[1024];
    
#ifdef LINUX
    // On Linux, we need to bring the iface down before hw addr change    
    sprintf(cmd, "/sbin/ifconfig %s down", vif_name);
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::SetHardwareAddress(%s) error: \"%s\n\" failed: %s\n", cmd, GetErrorString());
        return false;       
    }
    
    sprintf(cmd, "/sbin/ifconfig %s hw ether %02x:%02x:%02x:%02x:%02x:%02x", 
                 vif_name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::SetHardwareAddress(%s) error: \"%s\n\" failed: %s\n", cmd, GetErrorString());
        return false;       
    }
    // On Linux, we need to bring the iface back up after hw addr change     
    sprintf(cmd, "/sbin/ifconfig %s up", vif_name);
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::SetHardwareAddress(%s) error: \"%s\n\" failed: %s\n", cmd, GetErrorString());
        return false;       
    }
#endif // LINUX
    
#ifdef MACOSX    
    // On Mac OSX, the interface is automatically brought down and back up
    sprintf(cmd, "/sbin/ifconfig %s lladdr %02x:%02x:%02x:%02x:%02x:%02x", 
                 vif_name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "UnixVif::SetHardwareAddress(%s) error: \"%s\n\" failed: %s\n", cmd, GetErrorString());
        return false;       
    }
#endif // MACOSX
    
    
    // 4) Snag the virtual interface hardware address
    if (!ProtoNet::GetInterfaceAddress(vif_name, ProtoAddress::ETH, hw_addr))
        PLOG(PL_ERROR, "UnixVif::SetHardwareAddress() error: unable to get ETH address for virtual interface \"%s\"!\n", vif_name);
    
    return true;
}  // end UnixVif::SetHardwareAddress()

bool UnixVif::Write(const char* buffer, unsigned int numBytes) 
{
    int nWritten = write(descriptor, buffer, numBytes);
    if (nWritten != (int)numBytes)
    {
        PLOG(PL_ERROR,"UnixVif::Write() error: write() failure:%s\n", GetErrorString());
        return false;
    }    
    return true;
}  // end UnixVif::Write()

bool UnixVif::Read(char* buffer, unsigned int& numBytes)  
{
    int result = read(descriptor, buffer, numBytes);
    if (result < 0)
    {
        // (TBD) Automatically try again on errno == EINTR ???
        if (EAGAIN != errno)
            PLOG(PL_ERROR, "UnixVif::Read() error read() failure: %s\n", GetErrorString());
        numBytes = 0;
        return false;
    }
    numBytes = result;
    return true;
}  // end UnixVif::Read()
