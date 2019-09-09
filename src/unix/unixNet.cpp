#include "protoNet.h"
#include "protoDebug.h"


// TBD - This code could be streamlined a bit and with moving stuff to "linuxNet.cpp" using NETLINK
//       for everything so Android is supported there, too and keep this stuff cleaner.  Perhaps a
//       "bsdNet.cpp" for OSX/BSD and keep the more legacy Unix stuff here???  I can't go on supporting
//       the SOLARIS or IRIX stuff much longer anyway.

#include <unistd.h>
#include <stdlib.h>  // for atoi()
#include <stdio.h>  // for sprintf()
/*
#ifdef HAVE_IPV6

#ifdef MACOSX
#include <arpa/nameser.h>
#endif // MACOSX
#include <resolv.h>
#endif  // HAVE_IPV6
*/
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <fcntl.h>

#ifndef SIOCGIFHWADDR
#if defined(SOLARIS) || defined(IRIX)
#include <sys/sockio.h> // for SIOCGIFADDR ioctl
#include <netdb.h>      // for rest_init()
#include <sys/dlpi.h>
#include <stropts.h>
#else
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#endif  // if/else (SOLARIS || IRIX)
#endif  // !SIOCGIFHWADDR

#ifndef ANDROID
// Android doesn't have getifaddrs(), so we have some netlink stuff
// in "linuxNet.cpp" to implement specific some functions for Android
#include <ifaddrs.h> 
#endif // !ANDROID

// Implementation of ProtoNet functions for Unix systems.  These are the mechanisms that are common to
// most Unix systems.  Some additional ProtoNet mechanisms that have Linux- or MacOS-specific code are
// implemented in "src/linux/linuxNet.cpp" or "src/macos/macosNet.cpp", etc.
#ifndef ANDROID  // Android version implemented in linuxNet.cpp because no Android getifaddrs()
unsigned int ProtoNet::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{
    int family;
    switch (ifAddr.GetType())
    {
        case ProtoAddress::IPv4:
            family = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            family = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "UnixNet::GetInterfaceName() error: invalid address type\n");
            return 0;
    }
    struct ifaddrs* ifap;
    if (0 == getifaddrs(&ifap))
    {
        // Look for addrType address for given "interfaceName"
        struct ifaddrs* ptr = ifap;
        unsigned int namelen = 0;
        while (ptr)
        {
            if ((NULL != ptr->ifa_addr) && (family == ptr->ifa_addr->sa_family))
            {
                ProtoAddress theAddr;
                theAddr.SetSockAddr(*(ptr->ifa_addr));
                if (theAddr.HostIsEqual(ifAddr))
                {
                    namelen = (unsigned int)strlen(ptr->ifa_name);
                    if (namelen > IFNAMSIZ) namelen = IFNAMSIZ;
                    if (NULL == buffer) break;
                    unsigned int maxlen = (buflen > IFNAMSIZ) ? IFNAMSIZ : buflen;
                    strncpy(buffer, ptr->ifa_name, maxlen);
                    break;
                }
            }
            ptr = ptr->ifa_next;
        }
        freeifaddrs(ifap);
        if (0 == namelen)
            PLOG(PL_ERROR, "UnixNet::GetInterfaceName() error: unknown interface address\n");
        return namelen;
    }
    else
    {
        PLOG(PL_ERROR, "UnixNet::GetInterfaceName() getifaddrs() error: %s\n", GetErrorString());
        return 0;
    }
}  // end ProtoNet::GetInterfaceName(by address)

bool ProtoNet::GetInterfaceAddressList(const char*         interfaceName,
                                       ProtoAddress::Type  addressType,
                                       ProtoAddressList&   addrList,
                                       unsigned int*       interfaceIndex)
{
    struct ifreq req;
    memset(&req, 0, sizeof(struct ifreq));
    strncpy(req.ifr_name, interfaceName, IFNAMSIZ);
    int socketFd = -1;
    switch (addressType)
    {
        case ProtoAddress::IPv4:
            req.ifr_addr.sa_family = AF_INET;
            socketFd = socket(PF_INET, SOCK_DGRAM, 0);
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            req.ifr_addr.sa_family = AF_INET6;
            socketFd = socket(PF_INET6, SOCK_DGRAM, 0);
            break;
#endif // HAVE_IPV6
        default:
            req.ifr_addr.sa_family = AF_UNSPEC;
            socketFd = socket(PF_INET, SOCK_DGRAM, 0);
            break;
    }
    
    if (socketFd < 0)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() socket() error: %s\n", GetErrorString()); 
        return false;
    }   
    
    if (ProtoAddress::ETH == addressType)
    {
#ifdef SIOCGIFHWADDR
        // Probably Linux
        // Get hardware (MAC) address instead of IP address
        if (ioctl(socketFd, SIOCGIFHWADDR, &req) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() ioctl(SIOCGIFHWADDR) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;   
        }  
        else
        {
            close(socketFd);
            if (NULL != interfaceIndex) *interfaceIndex = req.ifr_ifindex;
            ProtoAddress ethAddr;
            if (!ethAddr.SetRawHostAddress(ProtoAddress::ETH,
                                           (const char*)&req.ifr_hwaddr.sa_data,
                                           IFHWADDRLEN))
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: invalid ETH addr?\n");
                return false;
            }   
            if (!addrList.Insert(ethAddr))
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
                return false;
            }
            return true;            
        }
#else
#if defined(SOLARIS) || defined(IRIX)
        // Use DLPI instead
        close(socketFd);
        char deviceName[32];
        snprintf(deviceName, 32, "/dev/%s", interfaceName);
        char* ptr = strpbrk(deviceName, "0123456789");
        if (NULL == ptr)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() invalid interface\n");
            return false;
        }
        int ifNumber = atoi(ptr);
        *ptr = '\0';    
        if ((socketFd = open(deviceName, O_RDWR)) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi open() error: %s\n",
                    GetErrorString());
            return false;
        }
        // dlp device opened, now bind to specific interface
        UINT32 buffer[8192];
        union DL_primitives* dlp = (union DL_primitives*)buffer;
        dlp->info_req.dl_primitive = DL_INFO_REQ;
        struct strbuf msg;
        msg.maxlen = 0;
        msg.len = DL_INFO_REQ_SIZE;
        msg.buf = (caddr_t)dlp;
        if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi putmsg(1) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        msg.maxlen = 8192;
        msg.len = 0;
        int flags = 0;
        if (getmsg(socketFd, &msg, NULL, &flags) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg(1) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        if ((DL_INFO_ACK != dlp->dl_primitive) ||
            (msg.len <  (int)DL_INFO_ACK_SIZE))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg(1) error: unexpected response\n");
            close(socketFd);
            return false;
        }
        if (DL_STYLE2 == dlp->info_ack.dl_provider_style)
        {
            dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
            dlp->attach_req.dl_ppa = ifNumber;
            msg.maxlen = 0;
            msg.len = DL_ATTACH_REQ_SIZE;
            msg.buf = (caddr_t)dlp;
            if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi putmsg(DL_ATTACH_REQ) error: %s\n",
                        GetErrorString());
                close(socketFd);
                return false;
            }
            msg.maxlen = 8192;
            msg.len = 0;
            flags = 0;
            if (getmsg(socketFd, &msg, NULL, &flags) < 0)
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg(DL_OK_ACK) error: %s\n",
                        GetErrorString());
                close(socketFd);
                return false;
            }
            if ((DL_OK_ACK != dlp->dl_primitive) ||
                (msg.len <  (int)DL_OK_ACK_SIZE))
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg(DL_OK_ACK) error: unexpected response\n");
                close(socketFd);
                return false;
            }
        }
        memset(&dlp->bind_req, 0, DL_BIND_REQ_SIZE);
        dlp->bind_req.dl_primitive = DL_BIND_REQ;
#ifdef DL_HP_RAWDLS
	    dlp->bind_req.dl_sap = 24;	
	    dlp->bind_req.dl_service_mode = DL_HP_RAWDLS;
#else
	    dlp->bind_req.dl_sap = DL_ETHER;
	    dlp->bind_req.dl_service_mode = DL_CLDLS;
#endif
        msg.maxlen = 0;
        msg.len = DL_BIND_REQ_SIZE;
        msg.buf = (caddr_t)dlp;
        if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi putmsg(DL_BIND_REQ) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;     
        }
        msg.maxlen = 8192;
        msg.len = 0;
        flags = 0;
        if (getmsg(socketFd, &msg, NULL, &flags) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg(DL_BIND_ACK) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        if ((DL_BIND_ACK != dlp->dl_primitive) ||
            (msg.len <  (int)DL_BIND_ACK_SIZE))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg(DL_BIND_ACK) error: unexpected response\n");
            close(socketFd);
            return false;
        }
        // We're bound to the interface, now request interface address
        dlp->info_req.dl_primitive = DL_INFO_REQ;
        msg.maxlen = 0;
        msg.len = DL_INFO_REQ_SIZE;
        msg.buf = (caddr_t)dlp;
        if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi putmsg() error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;     
        }
        msg.maxlen = 8192;
        msg.len = 0;
        flags = 0;
        if (getmsg(socketFd, &msg, NULL, &flags) < 0)
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg() error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        if ((DL_INFO_ACK != dlp->dl_primitive) || (msg.len <  (int)DL_INFO_ACK_SIZE))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() dlpi getmsg() error: unexpected response\n");
            close(socketFd);
            return false;
        }
        ProtoAddress macAddr;
        macAddr.SetRawHostAddress(addressType, (char*)(buffer + dlp->physaddr_ack.dl_addr_offset), 6);
        if (NULL != interfaceIndex) *interfaceIndex = ifNumber;
        close(socketFd);
        if (!addrList.Insert(macAddr))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
            return false; 
        }
        return true;
#else
        // For now, assume we're BSD and use getifaddrs()
        close(socketFd);  // don't need the socket
        struct ifaddrs* ifap;
        if (0 == getifaddrs(&ifap))
        {
            // Look for AF_LINK address for given "interfaceName"
            struct ifaddrs* ptr = ifap;
            while (NULL != ptr)
            {
                if ((NULL != ptr->ifa_addr) && (AF_LINK == ptr->ifa_addr->sa_family))
                {
                    if (!strcmp(interfaceName, ptr->ifa_name))
                    {
                        // note the (void*) cast here gets rid of cast-align mis-warning
                        struct sockaddr_dl* sdl = (struct sockaddr_dl*)((void*)ptr->ifa_addr);
                        if (IFT_ETHER != sdl->sdl_type)
                        {
                            freeifaddrs(ifap);
                            PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList() error: non-Ethertype iface: %s\n", interfaceName);
                            return false;
                        }
                        ProtoAddress macAddr;
                        macAddr.SetRawHostAddress(addressType, 
                                                  sdl->sdl_data + sdl->sdl_nlen,
                                                  sdl->sdl_alen);
                        if (NULL != interfaceIndex) 
                            *interfaceIndex = sdl->sdl_index;
                        freeifaddrs(ifap);
                        if (!addrList.Insert(macAddr))
                        {
                            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
                            return false; 
                        }
                        return true;
                    }
                }   
                ptr = ptr->ifa_next;
            }
            freeifaddrs(ifap);
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() unknown interface name\n");
            return false; // change to true when implemented
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() getifaddrs() error: %s\n",
                    GetErrorString());
            return false;  
        }
#endif // if/else (SOLARIS || IRIX)
#endif // if/else SIOCGIFHWADDR
    }  // end if (ETH == addrType)

#ifdef HAVE_IPV6    
    // Try using getifaddrs() to get all address else resort to ioctl(SIOCGIFADDR) below 
    struct ifaddrs* ifap;
    if (0 == getifaddrs(&ifap))
    {
        close(socketFd);
        // Look for addrType address for given "interfaceName"
        struct ifaddrs* ptr = ifap;
        bool foundIface = false;
        while (NULL != ptr)
        {
            char ifname[IFNAMSIZ+1];
            ifname[IFNAMSIZ] = '\0';
            strncpy(ifname, ptr->ifa_name, IFNAMSIZ);
#ifdef LINUX
            // This lets us find alias addrs on Linux
            if (NULL == strchr(interfaceName, ':'))
            {
                // "interfaceName" is a primary (non-alias) interface,
                // so match any aliases, too
                char* colon = strchr(ifname, ':');
                if (NULL != colon) *colon = '\0';
            }
#endif // LINUX
            if (0 == strcmp(interfaceName, ifname))
            {
                if ((NULL != ptr->ifa_addr) && (ptr->ifa_addr->sa_family == req.ifr_addr.sa_family))
                {
                    ProtoAddress ifAddr;
                    if (!ifAddr.SetSockAddr(*(ptr->ifa_addr)))
                    {
                        PLOG(PL_WARN, "ProtoNet::GetInterfaceAddressList() error: invalid address family\n");
                        ptr = ptr->ifa_next;
                        continue;
                    }
                    if (!addrList.Insert(ifAddr))
                    {
                        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add address to list!\n");
                        freeifaddrs(ifap);
                        close(socketFd);
                        return false;
                    }
                }
                foundIface = true;
            }
            ptr = ptr->ifa_next;
        }
        freeifaddrs(ifap);
        
        if (foundIface)
        {
            if (NULL != interfaceIndex) 
                *interfaceIndex = GetInterfaceIndex(interfaceName);
        }
        else
        {
            // Perhaps "interfaceName" is an address string?
            ProtoAddress ifAddr;
            if (ifAddr.ConvertFromString(interfaceName))
            {
                char nameBuffer[IFNAMSIZ+1];
                if (GetInterfaceName(ifAddr, nameBuffer, IFNAMSIZ+1))
                {
                    return GetInterfaceAddressList(nameBuffer, addressType, addrList, interfaceIndex);
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unknown interface address\n");
                }
            }
            return false; 
            
        }
    }
    else 
#endif // HAVE_IPV6
    if (ioctl(socketFd, SIOCGIFADDR, &req) < 0)
    {
        // (TBD - more sophisticated warning logic here
        PLOG(PL_DEBUG, "ProtoNet::GetInterfaceAddressList() ioctl(SIOCGIFADDR) error for iface>%s: %s\n",
                      interfaceName, GetErrorString()); 
        close(socketFd);
        // Perhaps "interfaceName" is an address string?
        ProtoAddress ifAddr;
        if (ifAddr.ConvertFromString(interfaceName))
        {
            char nameBuffer[IFNAMSIZ+1];
            if (GetInterfaceName(ifAddr, nameBuffer, IFNAMSIZ+1))
            {
                return GetInterfaceAddressList(nameBuffer, addressType, addrList, interfaceIndex);
            }
            else
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unknown interface address\n");
            }
        }
        return false; 
    }
    else
    {
        close(socketFd);
        ProtoAddress ifAddr;
#ifdef MACOSX
        // (TBD) make this more general somehow???
        if (0 == req.ifr_addr.sa_len)
        {
            PLOG(PL_DEBUG, "ProtoNet::GetInterfaceAddressList() warning: no addresses for given family?\n");
            return false;
        }
        else 
#endif // MACOSX
        if (ifAddr.SetSockAddr(req.ifr_addr))
        {
            if (addrList.Insert(ifAddr))
            {
                return true;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: unable to add ifAddr to list\n");
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList() error: invalid address family\n");
            return false;
        }
        if (NULL != interfaceIndex) 
            *interfaceIndex = GetInterfaceIndex(req.ifr_name);
    }
    return true;
}  // end ProtoNet::GetInterfaceAddressList()

unsigned int ProtoNet::GetInterfaceAddressMask(const char* ifaceName, const ProtoAddress& theAddr)
{
    int family;
    switch (theAddr.GetType())
    {
        case ProtoAddress::IPv4:
            family = AF_INET;
            break;
        case ProtoAddress::IPv6:
            family = AF_INET6;
            break;
        default:
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask() error: invalid address type\n");
            return 0;
    }
    struct ifaddrs* ifap;
    if (0 == getifaddrs(&ifap))
    {
        // Look for addrType address for given "interfaceName"
        struct ifaddrs* ptr = ifap;
        bool foundIface = false;
        while (NULL != ptr)
        {
            if ((NULL == ptr->ifa_addr) || (ptr->ifa_addr->sa_family != family))
            {
                ptr = ptr->ifa_next;
                continue;
            }
            char ifname[IFNAMSIZ+1];
            ifname[IFNAMSIZ] = '\0';
            strncpy(ifname, ptr->ifa_name, IFNAMSIZ);
#ifdef LINUX
            // This lets us find alias addrs on Linux
            char* colon = strchr(ifname, ':');
            if (NULL != colon) *colon = '\0';
#endif // LINUX
            if (0 == strcmp(ifaceName, ifname))
            {
                ProtoAddress ifAddr;
                if (!ifAddr.SetSockAddr(*(ptr->ifa_addr)))
                {
                    ptr = ptr->ifa_next;
                    continue;
                }
                if (theAddr.HostIsEqual(ifAddr))
                {
                    if (NULL == ptr->ifa_netmask)
                    {
                        // No netmask, so assume full address length?
                        freeifaddrs(ifap);
                        return (ifAddr.GetLength() << 3);
                    }
                    ProtoAddress maskAddr;
                    if (0 != ptr->ifa_netmask->sa_family)
                    {
                        maskAddr.SetSockAddr(*(ptr->ifa_netmask));
                    }
                    else
                    {
                        // For some reason (at least on MacOSX), we sometimes get a null sa_family on the mask
                        // So we assume netmask _is_ same family as the ifAddr
                        // This seems to happen on "alias" addresses. (TBD - maybe this helps us know which are aliases
                        // and which are not?)
                        struct sockaddr maddr;
                        memcpy(&maddr, ptr->ifa_netmask, sizeof(sockaddr));
                        maddr.sa_family = ptr->ifa_addr->sa_family;
                        maskAddr.SetSockAddr(maddr);
                    }
                    freeifaddrs(ifap);
                    return maskAddr.GetPrefixLength();
                }
                foundIface = true;
            }
            ptr = ptr->ifa_next;
        }
        freeifaddrs(ifap);
        if (!foundIface)
        {
            // Perhaps "interfaceName" is an address string instead?
            ProtoAddress ifAddr;
            if (ifAddr.ConvertFromString(ifaceName))
            {
                char nameBuffer[IFNAMSIZ+1];
                if (GetInterfaceName(ifAddr, nameBuffer, IFNAMSIZ+1))
                {
                    return GetInterfaceAddressMask(nameBuffer, theAddr);
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask() error: unknown interface name\n");
                }
            }
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressMask() getifaddrs() error: %s\n");  
    }
    return 0;
} // end ProtoNet::GetInterfaceAddressMask()
#endif // !ANDROID

#if defined(SIOCGIFINDEX) && (defined(ANDROID) || !defined(HAVE_IPV6)) 
// Internal helper function for systems without getifaddrs() (e.g. Android or older stuff)
static int GetInterfaceList(struct ifconf& conf)
{
    int sockFd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0) 
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceList() socket() error: %s\n", GetErrorString());
        return 0;
    }

    int ifNum = 32;  // first guess for max # of interfaces
#ifdef SIOCGIFNUM  // Solaris has this, others might
	if (ioctl(sock, SIOCGIFNUM, &ifNum) >= 0) ifNum++;
#endif // SIOCGIFNUM
    // We loop here until we get a fully successful SIOGIFCONF
    // This returns us a list of all interfaces
    int bufLen;
    conf.ifc_buf = NULL;
    do
    {
        if (NULL != conf.ifc_buf) delete[] conf.ifc_buf;  // remove previous buffer
        bufLen = ifNum * sizeof(struct ifreq);
        conf.ifc_len = bufLen;
        conf.ifc_buf = new char[bufLen];
        if ((NULL == conf.ifc_buf))
        {
            PLOG(PL_ERROR, "ProtoNet::GetInterfaceList() new conf.ifc_buf error: %s\n", GetErrorString());
            conf.ifc_len = 0;
            break;
        }
        if ((ioctl(sockFd, SIOCGIFCONF, &conf) < 0))
        {
            PLOG(PL_WARN, "ProtoNet::GetInterfaceList() ioctl(SIOCGIFCONF) warning: %s\n", GetErrorString());
			conf.ifc_len = 0;  // reset for fall-through below
        }
        ifNum *= 2;  // last guess not big enough, so make bigger & try again
    } while (conf.ifc_len >= bufLen);
    close(sockFd);  // done with socket (whether error or not)
    return (conf.ifc_len / sizeof(struct ifreq));  // number of interfaces (or 0)
}  // end ProtoNet::GetInterfaceList()
#endif // SIOCGIFINDEX  && (ANDROID || !HAVE_IPV6)

unsigned int ProtoNet::GetInterfaceIndices(unsigned int* indexArray, unsigned int indexArraySize)
{
    unsigned int indexCount = 0;
#if defined(HAVE_IPV6) && !defined(ANDROID)
    struct if_nameindex* ifdx = if_nameindex();
    if (NULL == ifdx) return 0;  // no interfaces found
    struct if_nameindex* ifPtr = ifdx;
	while ((0 != ifPtr->if_index))
	{
		// need to take into account (NULL, 0) input (see GetInterfaceName)
		if ((NULL != indexArray) && (indexCount < indexArraySize))
			indexArray[indexCount] = ifPtr->if_index;
		indexCount++;         
		ifPtr++;
	} 
	if_freenameindex(ifdx);
#else  // !HAVE_IPV6  || ANDROID
#ifdef SIOCGIFINDEX
    struct ifconf conf;
    conf.ifc_buf = NULL;  // initialize
	indexCount = GetInterfaceList(conf);
    if (NULL != indexArray)
    {
        if (indexCount < indexArraySize) indexArraySize = indexCount;
        for (unsigned int i = 0; i < indexArraySize; i++)
            indexArray[i] = GetInterfaceIndex(conf.ifc_req[i].ifr_name);
    }
    if (NULL != conf.ifc_buf) delete[] conf.ifc_buf;

#else  // !SIOCGIFINDEX
	PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndices() error: interface indices not supported\n");
#endif  // if/else SIOCGIFINDEX
#endif  // if/else HAVE_IPV6  && !ANDROID
    return indexCount;
}  // end ProtoNet::GetInterfaceIndices()

unsigned int ProtoNet::GetInterfaceIndex(const char* interfaceName)
{
    unsigned int index = 0;    
#ifdef HAVE_IPV6
    index = if_nametoindex(interfaceName);
#else
#ifdef SIOCGIFINDEX
    int sockFd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0) 
    {
        PLOG(PL_WARN, "ProtoNet::GetInterfaceIndex() socket() error: %s\n",
                       GetErrorString());
        return 0;
    }
    struct ifreq req;
    strncpy(req.ifr_name, interfaceName, IFNAMSIZ);
    if (ioctl(sockFd, SIOCGIFINDEX, &req) < 0)
        PLOG(PL_WARN, "ProtoNet::GetInterfaceIndex() ioctl(SIOCGIFINDEX) error: %s\n",
                       GetErrorString());
    else
        index =  req.ifr_ifindex;
    close(sockFd);
#else
    PLOG(PL_ERROR, "ProtoNet::GetInterfaceIndex() error: interface indices not supported\n");
    return 0;
#endif  // if/else SIOCGIFINDEX
#endif  // if/else HAVE_IPV6
    if (0 == index)
    {
        // Perhaps "interfaceName" was an address string?
        ProtoAddress ifAddr;
        if (ifAddr.ResolveFromString(interfaceName))
        {
            char nameBuffer[IFNAMSIZ+1];
            if (GetInterfaceName(ifAddr, nameBuffer, IFNAMSIZ+1))
                return GetInterfaceIndex(nameBuffer);
        }
    }
    return index;
}  // end ProtoNet::GetInterfaceIndex()

unsigned int ProtoNet::GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen)
{
#ifdef HAVE_IPV6
    char ifName[IFNAMSIZ+1];
    if (NULL != if_indextoname(index, ifName))
    {
        strncpy(buffer, ifName, buflen);
        return (unsigned int)strlen(ifName);
    }
    else
    {
        return 0;
    }
#else
#ifdef SIOCGIFNAME
    int sockFd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0) 
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() socket() error: %s\n",
                       GetErrorString());
        return 0;
    }
    struct ifreq req;
    req.ifr_ifindex = index;
    if (ioctl(sockFd, SIOCGIFNAME, &req) < 0)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() ioctl(SIOCGIFNAME) error: %s\n",
                       GetErrorString());
        close(sockFd);
        return 0;
    }
    close(sockFd);
    if (NULL != buffer)
    {
        if (buflen > IFNAMSIZ)
        {
            buffer[IFNAMSIZ] = '\0';
            buflen = IFNAMSIZ;
        }
        strncpy(buffer, req.ifr_name, buflen);
    }
    return strnlen(req.ifr_name, IFNAMSIZ);
#else
    PLOG(PL_ERROR, "ProtoNet::GetInterfaceName() error: getting name by index not supported\n");
    return 0;
#endif // if/else SIOCGIFNAME
#endif // if/else HAVE_IPV6
}  // end ProtoNet::GetInterfaceName(by index)

ProtoNet::InterfaceStatus ProtoNet::GetInterfaceStatus(const char* ifaceName)
{
    int fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceStatus() socket() error: %s\n", GetErrorString());
        return IFACE_UNKNOWN;
    }
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, ifaceName, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFFLAGS, &req) < 0)
    {
        PLOG(PL_ERROR, "ProtoNet::GetInterfaceStatus() ioctl(SIOCGIFFLAGS) error: %s\n", GetErrorString());
        close(fd);
        return IFACE_UNKNOWN;
    }
    close(fd);
    
    if (0 != (req.ifr_flags & IFF_UP))
        return IFACE_UP;
    else 
        return IFACE_DOWN;
    
}  // end ProtoNet::GetInterfaceStatus(by name)

ProtoNet::InterfaceStatus ProtoNet::GetInterfaceStatus(unsigned int ifaceIndex)
{
    char ifaceName[IFNAMSIZ+1];
    ifaceName[IFNAMSIZ] = '\0';
    if (!GetInterfaceName(ifaceIndex, ifaceName, IFNAMSIZ))
    {
        PLOG(PL_ERROR, "ProtoNet::InterfaceIsUp() socket() error: %s\n", GetErrorString());
        return IFACE_UNKNOWN;
    }
    return GetInterfaceStatus(ifaceName);
}  // end ProtoNet::GetInterfaceStatus(by index)

// TBD - implement with proper system APIs instead of "ifconfig" command
bool ProtoNet::AddInterfaceAddress(const char* ifaceName, const ProtoAddress& ifaceAddr, unsigned int maskLen)
{
    char cmd[1024];
#ifdef LINUX
#ifdef __ANDROID__
    sprintf(cmd, "ip addr add %s/%u dev %s", ifaceAddr.GetHostString(), maskLen, ifaceName); 
#else
    switch (ifaceAddr.GetType())
    {
        case ProtoAddress::IPv4:
        {
            // Does the interface have any IPv4 addresses already assigned?
            ProtoAddressList addrList;
            GetInterfaceAddressList(ifaceName, ProtoAddress::IPv4, addrList);
            // See if we need to make an alias addr (or set this as primary)
            unsigned int addrCount = 0;
            bool hasPrimary = false;
            ProtoAddress addr;
            ProtoAddressList::Iterator iterator(addrList);
            while (iterator.GetNextAddress(addr))
            {
                addrCount++;
                if (!hasPrimary)
                {
                    char ifname[IFNAMSIZ+1];
                    ifname[IFNAMSIZ] = '\0';
                    if (!GetInterfaceName(addr, ifname, IFNAMSIZ))
                    {
                        PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: unable to get interface name for addr %s\n",
                                addr.GetHostString());
                        continue;
                    }
                    if (0 == strcmp(ifname, ifaceName)) hasPrimary = true;
                }
            }
            if (hasPrimary)
            {
                char aliasName[IFNAMSIZ+1];
                aliasName[IFNAMSIZ] = '\0';
                strncpy(aliasName, ifaceName, IFNAMSIZ);
                unsigned int namelen = strlen(aliasName);
                if (namelen < IFNAMSIZ)
                {
                    strcat(aliasName, ":");
                    namelen++;
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: interface name too long to alias\n");
                    return false;
                }
                char* iptr = aliasName + namelen;
                int space = IFNAMSIZ - namelen;
                int index = addrCount - 1;
                while (index < 10)
                {
                    int result = snprintf(iptr, space, "%d", index);
                    if (result < 0)
                    {
                        PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() snprintf() error: %s\n", GetErrorString());
                        return false;
                    }
                    else if (result > space)
                    {
                        PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: alias exceeds max interface name length\n");
                        return false;
                    }
                    ProtoAddress ifAddr;
                    if (!GetInterfaceAddress(aliasName, ProtoAddress::IPv4, ifAddr))
                    {
                        // This alias is available, so set address
                        if (32 == maskLen)
                            sprintf(cmd, "/sbin/ifconfig %s %s broadcast 0.0.0.0 netmask 255.255.255.255", aliasName, ifaceAddr.GetHostString());
                        else
                            sprintf(cmd, "/sbin/ifconfig %s %s/%u", aliasName, ifaceAddr.GetHostString(), maskLen);
                        break;
                    }
                    index++;
                }
                if (10 == index) return false;
                if (index < 0)
                {
                    PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: no available alias found\n");
                    return false;
                }
            }
            else
            {
                // Set primary address for interface ifaceName
                if (32 == maskLen)
                    sprintf(cmd, "/sbin/ifconfig %s %s broadcast 0.0.0.0 netmask 255.255.255.255", ifaceName, ifaceAddr.GetHostString());
                else                            
                    sprintf(cmd, "/sbin/ifconfig %s %s/%u", ifaceName, ifaceAddr.GetHostString(), maskLen);
            }
            break;
        }
        case ProtoAddress::IPv6:
        {
            sprintf(cmd, "/sbin/ifconfig %s add %s/%u", ifaceName, ifaceAddr.GetHostString(), maskLen);
            break;
        }
        default:
        {
            PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: invalid address type\n");
            return false;
        }
    }
#endif // if/else __ANDROID__
#else
    switch (ifaceAddr.GetType())
    {
        case ProtoAddress::IPv4:
            sprintf(cmd, "/sbin/ifconfig %s %s/%u alias", ifaceName, ifaceAddr.GetHostString(), maskLen);
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            sprintf(cmd, "/sbin/ifconfig %s inet6 %s/%u alias", ifaceName, ifaceAddr.GetHostString(), maskLen);
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() error: invalid address type\n");
            return false;
    }
#endif // if/else LINUX / OTHER
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "ProtoNet::AddInterfaceAddress() /sbin/ifconfig error: %s\n", GetErrorString());
        return false;
    }
    return true;
}   // end ProtoNet::AddInterfaceAddress()

bool ProtoNet::RemoveInterfaceAddress(const char* ifaceName, const ProtoAddress& ifaceAddr, unsigned int maskLen)
{
    char cmd[1024];
#ifdef LINUX
#ifdef __ANDROID__
    sprintf(cmd, "ip addr del %s/%u dev %s", ifaceAddr.GetHostString(), maskLen, ifaceName);
#else
    switch (ifaceAddr.GetType())
    {
        case ProtoAddress::IPv4:
        {
            // On linux we need to find the right interface alias
            char ifname[IFNAMSIZ+1];
            ifname[IFNAMSIZ] = '\0';
            if (!GetInterfaceName(ifaceAddr, ifname, IFNAMSIZ))
            {
                PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() error: unknown interface address\n");
                return false;
            }
            if (NULL == strchr(ifname, ':'))
            {
                // Assign INADDR_NONE to remove address
                sprintf(cmd, "/sbin/ifconfig %s 0.0.0.0", ifname);
            }
            else
            {
                // Bring down alias interface
                sprintf(cmd, "/sbin/ifconfig %s down", ifname);
            }
            break;
        }
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
        {
            // delete IPv6 address
            if (0 != maskLen)
                sprintf(cmd, "/sbin/ifconfig %s del %s/%d", ifaceName, ifaceAddr.GetHostString(), maskLen);
            else
                sprintf(cmd, "/sbin/ifconfig %s del %s", ifaceName, ifaceAddr.GetHostString());
            break;
        }
#endif // HAVE_IPV8
        default:
        {
            PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() error: invalid address type\n");
            return false;
        }
    }    
#endif // if/else __ANDROID__
#else  // BSD, MacOSX, etc       
    switch (ifaceAddr.GetType())
    {
        case ProtoAddress::IPv4:
            sprintf(cmd, "/sbin/ifconfig %s %s -alias", ifaceName, ifaceAddr.GetHostString());
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            sprintf(cmd, "/sbin/ifconfig %s inet6 %s -alias", ifaceName, ifaceAddr.GetHostString());
            break;
#endif // HAVE_IPV6
        default:
            PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() error: invalid address type\n");
            return false;
    }
#endif  // if/else LINUX/OTHER 
    if (system(cmd) < 0)
    {
        PLOG(PL_ERROR, "ProtoNet::RemoveInterfaceAddress() /sbin/ifconfig error: %s\n", GetErrorString());
        return false;
    }
    return true;
}   // end ProtoNet::RemoveInterfaceAddress()

