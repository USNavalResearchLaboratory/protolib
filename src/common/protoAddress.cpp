
#include "protoAddress.h"
#include "protoSocket.h"  // for ProtoSocket::GetInterfaceAddress() routines
#include "protoDebug.h"   // for print out of warnings, etc
/** 
 * @file protoAddress.cpp
 * @brief Network address container class.
 */


#ifdef UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>     // for gethostname()

#if !defined(SIOCGIFHWADDR) && !(defined(SOLARIS) || defined(IRIX))
#define HAVE_IFDL
#include <net/if_types.h>  // for IFT_ETHER
#include <net/if_dl.h>     // for sockaddr_dl
#endif


#ifdef SOLARIS
#include <sys/sockio.h> // for SIOCGIFADDR ioctl
#endif  // SOLARIS
#endif // UNIX

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for sprintf()
#include <string.h>  // for memset()

const ProtoAddress PROTO_ADDR_NONE;

const ProtoAddress PROTO_ADDR_BROADCAST("ff:ff:ff:ff:ff:ff");

ProtoAddress::ProtoAddress()
 : type(INVALID), length(0)
{
    memset(&addr, 0, sizeof(addr));
}

ProtoAddress::ProtoAddress(const ProtoAddress& theAddr)
{
    *this = theAddr;
}

ProtoAddress::ProtoAddress(const char* theAddr)
{
    ConvertFromString(theAddr);
}

ProtoAddress::~ProtoAddress()
{
}

// This infers the ProtoAddress::Type from length (in bytes)
ProtoAddress::Type ProtoAddress::GetType(UINT8 addrLength)
{
#ifdef SIMULATE
        // Note 'SIM' case takes precedence
        if (sizeof(SIMADDR) == addrLength)
            return SIM;
#endif // SIMULATE
    switch (addrLength)
    {
        case 4:
            return IPv4;
        case 16:
            return IPv6;
        case 6:
            return ETH;
        default:
            return INVALID;
    }
}  // end ProtoAddress::GetType()

bool ProtoAddress::IsMulticast() const
{
    switch(type)
    {
        case IPv4:
        {
            UINT32 addrVal = (UINT32)(((struct sockaddr_in*)&addr)->sin_addr.s_addr);
            return (((UINT32)(htonl(0xf0000000) & addrVal)) == htonl(0xe0000000));
        }

#ifdef HAVE_IPV6
        case IPv6:
        {
            if (IN6_IS_ADDR_V4MAPPED(&(((struct sockaddr_in6*)&addr)->sin6_addr)))
            {
                return (htonl(0xe0000000) == 
                        ((UINT32)(htonl(0xf0000000) &
                         IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&addr)->sin6_addr)))));
            }
            else
            {
                return (0 != IN6_IS_ADDR_MULTICAST(&(((struct sockaddr_in6*)&addr)->sin6_addr)) ? true : false);
            }
        }
#endif // HAVE_IPV6
        case ETH:
            // ethernet broadcast also considered mcast here
            return (0 != (0x01 & ((char*)&addr)[0]));

#ifdef SIMULATE
            case SIM:
#ifdef NS2
                return (0 != (addr.addr & 0x80000000));
			// && (addr.addr != 0xffffffff);
#endif // NS2
#ifdef OPNET  
                return (0xe0000000 == (0xf0000000 & addr.addr));
#endif // OPNET
#endif // SIMULATE
            default:
                return false;
    }
}  // end IsMulticast()

bool ProtoAddress::IsBroadcast() const
{
    switch(type)
    {
        case IPv4:
        {
            UINT32 addrVal = (UINT32)(((struct sockaddr_in*)&addr)->sin_addr.s_addr);
            return (addrVal == htonl(0xffffffff));
        }

#ifdef HAVE_IPV6
        case IPv6:
        {
            return false;  // no IPv6 broadcast address
        }
#endif // HAVE_IPV6
        
        case ETH:
        {
            const unsigned char temp[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
            return (0 == memcmp((const char*)&addr, temp, 6));
        }

#ifdef SIMULATE
            case SIM:
#ifdef NS2
                return ((int)0xffffffff == addr.addr);
#endif // NS2
#ifdef OPNET  
		        return (0xffffffff == addr.addr);
#endif // OPNET
#endif // SIMULATE
            default:
                return false;
    }
}  // end IsBroadcast()

bool ProtoAddress::IsLoopback() const
{
    switch(type)
    {
        case IPv4:
        {
            // This was changed since any 127.X.X.X is a loopback address
            // and many Linux configs have started using this fact for some purpose 
            UINT32 addrVal = (UINT32)((struct sockaddr_in*)&addr)->sin_addr.s_addr;
            return (0x7f == (ntohl(addrVal) >> 24));
        }
#ifdef HAVE_IPV6
        case IPv6:
        {
            if (IN6_IS_ADDR_V4MAPPED(&(((struct sockaddr_in6*)&addr)->sin6_addr)))
                return (htonl(0x7f000001) ==
                        IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&addr)->sin6_addr)));
            else
                return (0 != IN6_IS_ADDR_LOOPBACK(&(((struct sockaddr_in6*)&addr)->sin6_addr)) ? true : false);
        }
#endif // HAVE_IPV6
        case ETH:
            return false;
#ifdef SIMULATE
        case SIM:
            return false;
#endif // SIMULATE
        default:
            return false;
    }
}  // end ProtoAddress::IsLoopback()

bool ProtoAddress::IsUnspecified() const
{
    switch(type)
    {
            case IPv4:
            {
                UINT32 addrVal = (UINT32)(((struct sockaddr_in*)&addr)->sin_addr.s_addr);
                return (0x00000000 == addrVal);
            }
#ifdef HAVE_IPV6
        case IPv6:
        {
            if (IN6_IS_ADDR_V4MAPPED(&(((struct sockaddr_in6*)&addr)->sin6_addr)))
            {
                return (0x0000000 == 
                        IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&addr)->sin6_addr)));
            }
            else
            {
                return (0 != IN6_IS_ADDR_UNSPECIFIED(&(((struct sockaddr_in6*)&addr)->sin6_addr)) ? true : false);
            }
        }
#endif // HAVE_IPV6
        case ETH:
            return false;
#ifdef SIMULATE
        case SIM:
            return false;
#endif // SIMULATE
        default:
            return false;
    }
}  // end IsUnspecified()

bool ProtoAddress::IsLinkLocal() const
{
    switch(type)
    {
#ifdef HAVE_IPV6
        case IPv6:
        {
            struct in6_addr* a = &(((struct sockaddr_in6*)&addr)->sin6_addr);
            return (0 != IN6_IS_ADDR_MULTICAST(a) ? 
                    (0 != IN6_IS_ADDR_MC_LINKLOCAL(a) ? true : false) :
                    (0 != IN6_IS_ADDR_LINKLOCAL(a) ? true : false));
        }
#endif // HAVE_IPV6
        case IPv4:
        {
            UINT32 addrVal = (UINT32)(((struct sockaddr_in*)&addr)->sin_addr.s_addr);
            if (((UINT32)(htonl(0xffffff00) & addrVal))
                == (UINT32)htonl(0xe0000000))
            {
                // Address is 224.0.0/24 multicast.
                return true;
            }
            else if (((UINT32)(htonl(0xffff0000) & addrVal))
                     == (UINT32)htonl(0xa9fe0000))
            {
                // Address is 169.254/16 unicast.
                return true;
            }
            return false;
        }
        case ETH:
            return false;  // or return true???
        default:
           return false;
    }
}  // end ProtoAddress::IsLinkLocal()

bool ProtoAddress::IsSiteLocal() const
{
    switch(type)
    {
#ifdef HAVE_IPV6
        case IPv6:
        {
            struct in6_addr* a = &(((struct sockaddr_in6*)&addr)->sin6_addr);
            return (IN6_IS_ADDR_MULTICAST(a) ? 
                    (0 != IN6_IS_ADDR_MC_SITELOCAL(a) ? true : false) :
                    (0 != IN6_IS_ADDR_SITELOCAL(a) ? true : false));
        }
#endif // HAVE_IPV6
        case IPv4:
        case ETH:
            return false;
        default:
           return false;
    }
}  // end ProtoAddress::IsSiteLocal()

/**
*
* @brief Translates the address to a host string
*
* @param buffer
* @param buflen
*
* @retval Returns the address as a host string
*/
const char* ProtoAddress::GetHostString(char* buffer, unsigned int buflen) const
{
    static char altBuffer[256];
    altBuffer[255] = '\0';
    buflen = (NULL != buffer) ? buflen : 255;
#ifdef _UNICODE
    buflen /= 2;
#endif // _UNICODE
    buffer = (NULL != buffer) ? buffer : altBuffer;
    switch (type)
    {
#ifdef WIN32
        case IPv4:
#ifdef HAVE_IPV6
        case IPv6:
#endif // HAVE_IPV6
        {
            // Startup WinSock for name lookup
            if (!Win32Startup())
            {
                PLOG(PL_ERROR, "ProtoAddress: GetHostString(): Error initializing WinSock!\n");
                return NULL;
            }
            unsigned long len = buflen;
            if (0 != WSAAddressToString((SOCKADDR*)&addr, sizeof(addr), NULL, (LPTSTR)buffer, &len))
                PLOG(PL_ERROR, "ProtoAddress::GetHostString() WSAAddressToString() error\n");
			Win32Cleanup();
#ifdef _UNICODE
            // Convert from unicode
            wcstombs(buffer, (wchar_t*)buffer, len);
#endif // _UNICODE
            // Get rid of trailing port number
            if (IPv4 == type)
            {
                char* ptr = strrchr(buffer, ':');
                if (ptr) *ptr = '\0';
            }
#ifdef HAVE_IPV6
            else if (IPv6 == type)
            {
                char* ptr = strchr(buffer, '[');  // nuke start bracket
                if (ptr)
                {
					char * pch;
                    size_t len = strlen(buffer);
                    if (len > buflen) len = buflen;
					ptr++;
					memmove(buffer, ptr, len - (ptr-buffer));   
                }  
                ptr = strrchr(buffer, '%');  // nuke if index, if applicable
                if (!ptr) ptr = strrchr(buffer, ']'); // nuke end bracket
				ptr = (char *)memchr(buffer, ']', strlen(buffer));
				if (ptr)
					buffer[ptr - buffer] = '\0';

            }
#endif // HAVE_IPV6
            return buffer ? buffer : "(null)";
        }
#else 
#ifdef HAVE_IPV6
        case IPv4:
        {
            const char* result = inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr, buffer, buflen);
            return result ? result : "(bad address)";
        }
        case IPv6:
        { 
            const char* result = inet_ntop(AF_INET6, &((struct sockaddr_in6*)&addr)->sin6_addr, buffer, buflen);
            return result ? result : "(bad address)";
        }
#else
        case IPv4:
            strncpy(buffer, inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr), buflen);
            return buffer;
#endif // HAVE_IPV6
#endif // if/else WIN32/UNIX
        case ETH:
        {
            // Print as a hexadecimal number
            unsigned int len = 0;
            for (int i = 0; i < 6; i++)
            {
                if (len < buflen)
                {
                    if (i < 1)
                        len += sprintf(buffer+len, "%02x", ((unsigned char*)&addr)[i]);
                    else
                        len += sprintf(buffer+len, ":%02x", ((unsigned char*)&addr)[i]);
                }
                else
                {
                    break;
                }
            }
            return buffer;
        }
#ifdef SIMULATE
        case SIM:
        {
            char text[32];
#ifndef OPNET // JPH 5/22/06
            sprintf(text, "%u", ((struct sockaddr_sim*)&addr)->addr);
#else
			ip_address_print(text,(IpT_Address)((struct sockaddr_sim*)&addr)->addr);
#endif // OPNET
            strncpy(buffer, text, buflen);
            return buffer ? buffer : "(null)";
        }
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress: GetHostString(): Invalid address type!\n");
            return "(invalid address)";
    }  // end switch(type)
}  // end ProtoAddress::GetHostString()

void ProtoAddress::PortSet(UINT16 thePort) {SetPort(thePort);}

void ProtoAddress::SetPort(UINT16 thePort)
{
    switch(type)
    {
            case IPv4:
                ((struct sockaddr_in*)&addr)->sin_port = htons(thePort);
                break;
#ifdef HAVE_IPV6
            case IPv6:
                ((struct sockaddr_in6*)&addr)->sin6_port = htons(thePort);
                break;
#endif // HAVE_IPV6
#ifdef SIMULATE
                case SIM:
                ((struct sockaddr_sim*)&addr)->port = thePort;
                        break;
#endif // SIMULATE
            case ETH:
                break;
            default:
                Reset(IPv4);
                SetPort(thePort);
                break;
    }
}  // end ProtoAddress::SetPort()

UINT16 ProtoAddress::GetPort() const
{
    switch(type)
    {
        case IPv4:
            return ntohs(((struct sockaddr_in *)&addr)->sin_port);
#ifdef HAVE_IPV6
        case IPv6:
            return ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
#endif // HAVE_IPV6
#ifdef SIMULATE
        case SIM:
            return (((struct sockaddr_sim*)&addr)->port);
#endif // SIMULATE
        case ETH:
        default:
            return 0;  // port 0 is an invalid port
    }
}  // end ProtoAddress::Port()
/**
*
* @brief Resets the address to zero or high values.
*
* @param theType
* @param zero
*
*/
void ProtoAddress::Reset(ProtoAddress::Type theType, bool zero)
{
    char value = zero ? 0x00 : 0xff;
    char fill[16];   
    switch (theType)
    {
        case IPv4:
            memset(fill, value, 4);
            SetRawHostAddress(IPv4, fill, 4);
            break;
#ifdef HAVE_IPV6
        case IPv6:
            memset(fill, value, 16);
            SetRawHostAddress(IPv6, fill, 16);
            break;
#endif // HAVE_IPV6
        case ETH:
            memset(fill, value, 6);
            SetRawHostAddress(ETH, fill, 6);
            break;
#ifdef SIMULATE
        case SIM:
            type = SIM;
            length = sizeof(SIMADDR);
            memset(&((struct sockaddr_sim*)&addr)->addr, value, sizeof(SIMADDR));
            break;
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::Reset() Invalid address type!\n");
            break;
    }
    SetPort(0);
}  // end ProtoAddress::Reset();

/**
*
* @brief Initializes the address (incl. port) from the provided sockaddr struct.
*
* @param theAddr
*
* @retval Returns success/failure
*/
bool ProtoAddress::SetSockAddr(const struct sockaddr& theAddr)
{
    switch (theAddr.sa_family)
    {
        case AF_INET:
            //((struct sockaddr_in&)addr) = ((struct sockaddr_in&)theAddr);
            // memcpy() safer here due to memory alignment issues on 
            // some compilers / platforms (e.g. Android ARM)??
            memcpy(&addr, &theAddr, sizeof(struct sockaddr_in));
            type = IPv4;
            length = 4;
            return true;
#ifdef HAVE_IPV6
        case AF_INET6:
            //((struct sockaddr_in6&)addr) = ((struct sockaddr_in6&)theAddr);
            // memcpy() safer here due to memory alignment issues on 
            // some compilers / platforms (e.g. Android ARM)??
            memcpy(&addr, &theAddr, sizeof(struct sockaddr_in6));
            type = IPv6;
            length = 16;
            return true;
#endif // HAVE_IPV6
#ifdef HAVE_IFDL
        case AF_LINK:
        {
            // The (void*) cast here avoid cast-align warning. TBD - may need to revisit this.
            struct sockaddr_dl* sdl = (struct sockaddr_dl*)((void*)&theAddr);
            if (IFT_ETHER != sdl->sdl_type)
            {
                PLOG(PL_WARN, "ProtoNet::SetSockAddr() error: non-Ethertype link address!\n");
                return false;
            }
            SetRawHostAddress(ETH, sdl->sdl_data + sdl->sdl_nlen, sdl->sdl_alen);        
            return true;
        }
#endif  // HAVE_IFDL
        default:
            PLOG(PL_ERROR, "ProtoAddress::SetSockAddr() warning: Invalid address type: %d\n", theAddr.sa_family);
            type = INVALID;
            length = 0;
            return false;
    }
}  // end ProtoAddress:SetAddress()

/**
*
* @brief Initializes the address from the buffer contents
*
* @param theType
* @param buffer
* @param buflen
*
* @retval Returns success/failure
*/
bool ProtoAddress::SetRawHostAddress(ProtoAddress::Type theType,
                                     const char*        buffer,
                                     UINT8              buflen)
{
    UINT16 thePort = GetPort();
    switch (theType)
    {       
        case IPv4:
            if (buflen > 4) return false;
            type = IPv4;
            length = 4;
            memset(&((struct sockaddr_in*)&addr)->sin_addr, 0, 4);
            memcpy(&((struct sockaddr_in*)&addr)->sin_addr, buffer, buflen);
#if defined(_SOCKLEN_T)  ||  defined(_SOCKLEN_T_DECLARED)   // for BSD systems
            ((struct sockaddr_in*)&addr)->sin_len = sizeof(sockaddr_in);
#endif // defined(_SOCKLENT_T)  || defined(_SOCKLEN_T_DECLARED)
            ((struct sockaddr_in*)&addr)->sin_family = AF_INET;
            break;
#ifdef HAVE_IPV6
        case IPv6:
            if (buflen > 16) return false;
            type = IPv6;
            length  = 16;
            memset(&((struct sockaddr_in6*)&addr)->sin6_addr, 0, 16);
            memcpy(&((struct sockaddr_in6*)&addr)->sin6_addr, buffer, buflen);
#if defined(_SOCKLEN_T)  ||  defined(_SOCKLEN_T_DECLARED)   // for BSD systems
            ((struct sockaddr_in6*)&addr)->sin6_len = sizeof(sockaddr_in6);
#endif // defined(_SOCKLENT_T)  || defined(_SOCKLEN_T_DECLARED)
            ((struct sockaddr_in6*)&addr)->sin6_family = AF_INET6;
            break;
#endif // HAVE_IPV6  
        case ETH:
            if (buflen > 6) return false;
            type = ETH;
            length = 6;
            memset((char*)&addr, 0, 6);
            memcpy((char*)&addr, buffer, buflen);
            break;
#ifdef SIMULATE          
        case SIM:
            if (buflen > sizeof(SIMADDR)) return false;
            type = SIM;
            length = sizeof(SIMADDR);
            memset(&((struct sockaddr_sim*)&addr)->addr, 0, sizeof(SIMADDR));
            memcpy(&((struct sockaddr_sim*)&addr)->addr, buffer, buflen);
            break;
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::SetRawHostAddress() Invalid address type!\n");
            return false;
    }
    SetPort(thePort);
    return true;
}  // end ProtoAddress::SetRawHostAddress()
        

const char* ProtoAddress::GetRawHostAddress() const
    {return AccessRawHostAddress();}

char* ProtoAddress::AccessRawHostAddress() const
{
    switch (type)
    {
        case IPv4:
            return ((char*)&((struct sockaddr_in*)&addr)->sin_addr);
#ifdef HAVE_IPV6
        case IPv6:
            return ((char*)&((struct sockaddr_in6*)&addr)->sin6_addr);
#endif // HAVE_IPV6  
        case ETH:
            return ((char*)&addr);
#ifdef SIMULATE          
        case SIM:
            return ((char*)&((struct sockaddr_sim*)&addr)->addr);
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::RawHostAddress() Invalid address type!\n");
            return NULL;
    }
}  // end ProtoAddress::GetRawHostAddress()

unsigned int ProtoAddress::SetCommonHead(const ProtoAddress& theAddr)
{
    unsigned int commonBytes = 0;
    if((GetType() != theAddr.GetType()) && (!IsValid()))
    {
        Reset(GetType());
        return commonBytes;
    }
    const char *myRawHost = GetRawHostAddress();
    const char *theirRawHost = theAddr.GetRawHostAddress();
    for(commonBytes = 1 ; commonBytes <= GetLength() ; commonBytes++)
    {
        if(memcmp(myRawHost,theirRawHost,commonBytes))
        {
            commonBytes--;
            ApplyPrefixMask(commonBytes*8); //ApplyPrefixMask takes size in bits
            return commonBytes;
        }
    }
    return GetLength();
}  // end ProtoAddress::SetCommonHead()

unsigned int ProtoAddress::SetCommonTail(const ProtoAddress& theAddr)
{
    unsigned int commonBytes = 0;
    if((GetType() != theAddr.GetType()) && (!IsValid()))
    {
        Reset(GetType());
        return commonBytes;
    }
    const char *myRawHost = GetRawHostAddress();
    const char *theirRawHost = theAddr.GetRawHostAddress();
    for(commonBytes = 1 ; commonBytes <= GetLength() ; commonBytes++)
    {
        if(memcmp(myRawHost-commonBytes+GetLength(),theirRawHost-commonBytes+GetLength(),commonBytes))
        {
            commonBytes--;
            ApplySuffixMask(commonBytes*8);
            return commonBytes;
        }
    }
    return GetLength();
}  // end ProtoAddress::SetCommonTail()


bool ProtoAddress::PrefixIsEqual(const ProtoAddress& theAddr, UINT8 prefixLen) const
{
    // Compare address "type" and "prefixLen" bits of address
    if (!IsValid() && !theAddr.IsValid()) return true;
    if (type == theAddr.type)
    {
        const char* ptr1 = GetRawHostAddress();
        const char* ptr2 = theAddr.GetRawHostAddress();
        size_t nbyte = prefixLen >> 3;
        if ((0 == nbyte) || (0 == memcmp(ptr1, ptr2, nbyte)))
        {
            UINT8 nbit = prefixLen & 0x07;
            if (0 == nbit) return true;
            char mask = 0xff << (8 - nbit);
            if ((mask & ptr1[nbyte]) == (mask & ptr2[nbyte]))
                return true;
        }
    }
    return false;
}  // end ProtoAddress::PrefixIsEqual()


UINT8 ProtoAddress::GetPrefixLength() const
{
    UINT8* ptr = NULL;
    UINT8 maxBytes = 0;
	switch (type)
    {
        case IPv4:
            ptr = ((UINT8*)&((struct sockaddr_in*)&addr)->sin_addr);
            maxBytes = 4;
            break;
#ifdef HAVE_IPV6
        case IPv6:
            ptr = ((UINT8*)&((struct sockaddr_in6*)&addr)->sin6_addr);
            maxBytes = 16;
            break;
#endif // HAVE_IPV6  
#ifdef SIMULATE          
        case SIM:
            ptr = ((UINT8*)&((struct sockaddr_sim*)&addr)->addr);
            maxBytes = sizeof(SIMADDR);
            break;
#endif // SIMULATE
        case ETH:
        default:
		  PLOG(PL_ERROR, "ProtoAddress::PrefixLength() Invalid address type of %d!\n",type);
            return 0;
    }
    UINT8 prefixLen = 0;
    for (UINT8 i = 0; i < maxBytes; i++)
    {
        if (0xff == *ptr)
        {
            prefixLen += 8;
            ptr++;   
        }
        else
        {
            UINT8 bit = 0x80;
            while (0 != (bit & *ptr))
            {
                bit >>= 1;
                prefixLen += 1;   
            }
            break;
        }   
    }
    return prefixLen;
}  // end ProtoAddress::GetPrefixLength()

void ProtoAddress::GeneratePrefixMask(ProtoAddress::Type type, UINT8 prefixLen)
{
    unsigned char* ptr;
    switch (type)
    {
        case IPv4:
            ptr = ((unsigned char*)&((struct sockaddr_in*)&addr)->sin_addr);
            break;
#ifdef HAVE_IPV6
        case IPv6:
            ptr = ((unsigned char*)&((struct sockaddr_in6*)&addr)->sin6_addr);
            break;
#endif // HAVE_IPV6  
        case ETH:
            ptr =  ((unsigned char*)&addr);
            break;
#ifdef SIMULATE          
        case SIM:
            ptr = ((unsigned char*)&((struct sockaddr_sim*)&addr)->addr);
            break;
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::GeneratePrefixMask() Invalid address type!\n");
            return ;
    }
    Reset(type, true);  // init to zero
    if (prefixLen > GetLength())
        prefixLen = GetLength();
    while (0 != prefixLen)
    {
        if (prefixLen < 8)
        {
            *ptr = 0xff << (8 - prefixLen);
            return;
        }
        else
        {
            *ptr++ = 0xff;
            prefixLen -= 8;
        }
    }
}  // end ProtoAddress::GeneratePrefixMask()

void ProtoAddress::ApplyPrefixMask(UINT8 prefixLen)
{
    UINT8* ptr = NULL;
    UINT8 maxLen = 0;
    switch (type)
    {
        case IPv4:
            ptr = ((UINT8*)&((struct sockaddr_in*)&addr)->sin_addr);
            maxLen = 32;
            break;
#ifdef HAVE_IPV6
        case IPv6:
            ptr = ((UINT8*)&((struct sockaddr_in6*)&addr)->sin6_addr);
            maxLen = 128;
            break;
#endif // HAVE_IPV6  
#ifdef SIMULATE          
        case SIM:
            ptr = ((UINT8*)&((struct sockaddr_sim*)&addr)->addr);
            maxLen = sizeof(SIMADDR) << 3;
            break;
#endif // SIMULATE
        case ETH:
        default:
            PLOG(PL_ERROR, "ProtoAddress::ApplyPrefixMask() Invalid address type!\n");
            return;
    }
    if (prefixLen >= maxLen) return;
    UINT8 nbytes = prefixLen >> 3;
    UINT8 remainder = prefixLen & 0x07;
    if (remainder) 
    {
        ptr[nbytes] &= (UINT8)(0x00ff << (8 - remainder));
        nbytes++;
    }
    memset(ptr + nbytes, 0, length - nbytes);    
}  // end ProtoAddress::ApplyPrefixMask()

void ProtoAddress::ApplySuffixMask(UINT8 suffixLen)
{
    UINT8* ptr = NULL;
    UINT8 maxLen = 0;
    switch (type)
    {
        case IPv4:
            ptr = ((UINT8*)&((struct sockaddr_in*)&addr)->sin_addr);
            maxLen = 32;
            break;
#ifdef HAVE_IPV6
        case IPv6:
            ptr = ((UINT8*)&((struct sockaddr_in6*)&addr)->sin6_addr);
            maxLen = 128;
            break;
#endif // HAVE_IPV6  
#ifdef SIMULATE          
        case SIM:
            ptr = ((UINT8*)&((struct sockaddr_sim*)&addr)->addr);
            maxLen = sizeof(SIMADDR) << 3;
            break;
#endif // SIMULATE
        case ETH:
        default:
            PLOG(PL_ERROR, "ProtoAddress::ApplyPrefixMask() Invalid address type!\n");
            return;
    }
    if (suffixLen >= maxLen) return;
    UINT8 nbytes = suffixLen >> 3;
    UINT8 remainder = suffixLen & 0x07;
    if (remainder) 
    {
        ptr[(maxLen >> 3)-nbytes-1] &= (UINT8)(0x00ff >> (8-remainder));
        nbytes++;
    }
    memset(ptr, 0, length - nbytes);
}
void ProtoAddress::GetSubnetAddress(UINT8         prefixLen, 
                                    ProtoAddress& subnetAddr) const
{
    subnetAddr = *this;
    UINT8* ptr = NULL;
    UINT8 maxLen = 0;
    switch (type)
    {
        case IPv4:
            ptr = ((UINT8*)&((struct sockaddr_in*)&subnetAddr.addr)->sin_addr);
            maxLen = 32;
            break;
#ifdef HAVE_IPV6
        case IPv6:
            ptr = ((UINT8*)&((struct sockaddr_in6*)&subnetAddr.addr)->sin6_addr);
            maxLen = 128;
            break;
#endif // HAVE_IPV6  
        case ETH:
            return;
#ifdef SIMULATE          
        case SIM:
            ptr = ((UINT8*)&((struct sockaddr_sim*)&subnetAddr.addr)->addr);
            maxLen = sizeof(SIMADDR) << 3;
            break;
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::GetSubnetAddress() Invalid address type!\n");
            return;
    }
    if (prefixLen >= maxLen) return;
    UINT8 nbytes = prefixLen >> 3;
    UINT8 remainder = prefixLen & 0x07;
    if (remainder) 
    {
        ptr[nbytes] &= (UINT8)(0xff << (8 - remainder));
        nbytes++;
    }
    memset(ptr + nbytes, 0, length - nbytes);           
}  // end ProtoAddress::GetSubnetAddress()

bool ProtoAddress::Increment()
{
    if (!IsValid()) 
    {
        PLOG(PL_ERROR, "ProtoAddress::Increment() error:  invalid address\n");
        return false;
    }
    int index = GetLength() - 1;
    UINT8* byte = (UINT8*)AccessRawHostAddress();
    while (index >= 0)
    {
        if (255 != byte[index])
        {
            byte[index] += 1;
            return true;
        }
        byte[index--] = 0;
    }
    return false;
}  // end ProtoAddress::Increment()
    

void ProtoAddress::GetBroadcastAddress(UINT8         prefixLen, 
                                       ProtoAddress& broadcastAddr) const
{
    broadcastAddr = *this;
    UINT8* ptr = NULL;
    UINT8 maxLen = 0;
    switch (type)
    {
        case IPv4:
            ptr = ((UINT8*)&((struct sockaddr_in*)&broadcastAddr.addr)->sin_addr);
            maxLen = 32;
            break;
#ifdef HAVE_IPV6
        case IPv6:
            ptr = ((UINT8*)&((struct sockaddr_in6*)&broadcastAddr.addr)->sin6_addr);
            maxLen = 128;
            break;
#endif // HAVE_IPV6  
        case ETH:
            ptr = (UINT8*)&broadcastAddr.addr;
            maxLen = 48;
            prefixLen = 0;
            break;
#ifdef SIMULATE          
        case SIM:
            ptr = ((UINT8*)&((struct sockaddr_sim*)&broadcastAddr.addr)->addr);
            maxLen = sizeof(SIMADDR) << 3;
            break;
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::GetBroadcastAddress() Invalid address type!\n");
            return;
    }
    if (prefixLen >= maxLen) return;
    UINT8 nbytes = prefixLen >> 3;
    UINT8 remainder = prefixLen & 0x07;
    if (remainder) 
    {
        ptr[nbytes] |= (0x00ff >> remainder);
        nbytes++;
    }
    memset(ptr + nbytes, 0xff, length - nbytes);           
}  // end ProtoAddress::GetBroadcastAddress()

ProtoAddress& ProtoAddress::GetEthernetMulticastAddress(const ProtoAddress& ipMcastAddr)
{
    if (!ipMcastAddr.IsMulticast())
    {
        Invalidate();
        return *this;
    }
    // Ethernet mcast addr begins with 00:00:5e ...
    UINT8 ethMcastAddr[6];
    const UINT8* ipAddrPtr;
    switch (ipMcastAddr.GetType())
    {
        // Point to lower 24-bits of IP mcast address
        case IPv4:
            ethMcastAddr[0] = 0x01;
            ethMcastAddr[1] = 0x00;
            ethMcastAddr[2] = 0x5e;
            ipAddrPtr = (const UINT8*)(ipMcastAddr.GetRawHostAddress() + 1);
            ethMcastAddr[3] = (0x7f & ipAddrPtr[0]);
            ethMcastAddr[4] = ipAddrPtr[1];
            ethMcastAddr[5] = ipAddrPtr[2];
            break;
        case IPv6:
            ethMcastAddr[0] = 0x33;
            ethMcastAddr[1] = 0x33;
            ipAddrPtr = (const UINT8*)(ipMcastAddr.GetRawHostAddress() + 12);
            ethMcastAddr[2] = ipAddrPtr[0];
            ethMcastAddr[3] = ipAddrPtr[1];
            ethMcastAddr[4] = ipAddrPtr[2];
            ethMcastAddr[5] = ipAddrPtr[3];
            break;
        default:
            PLOG(PL_ERROR, "ProtoAddress::GetEthernetMulticastAddress() error : non-IP address!\n");
            Invalidate();
            return *this;
    }
    SetRawHostAddress(ETH, (char*)ethMcastAddr, 6);
    return *this;
}  // end ProtoAddress::GetEthernetMulticastAddress()

void ProtoAddress::SetEndIdentifier(UINT32 endIdentifier)
{
    endIdentifier = htonl(endIdentifier);
    switch(type)
    {
        case IPv4:
            SetRawHostAddress(IPv4, (char*)&endIdentifier, 4);
            break;
#ifdef HAVE_IPV6
        case IPv6:
            // Set lowest 4 bytes to endIdentifier
            memcpy(((char*)&(((struct sockaddr_in6&)addr).sin6_addr))+12, &endIdentifier, 4);
#endif // HAVE_IPV6
            break;
        case ETH:
        {
            UINT8 vendorHash;
            memcpy(&vendorHash, (char*)&endIdentifier, 1);
            UINT8* addrPtr = (UINT8*)&addr;
            addrPtr[0] = addrPtr[1] = addrPtr[2] = vendorHash;
            memcpy(addrPtr+3, (((char*)&endIdentifier)+1), 3);
            break;
        }
#ifdef SIMULATE
        case SIM:
            ((struct sockaddr_sim*)&addr)->addr = endIdentifier;
            break;
#endif // SIMULATE
        default:
            SetRawHostAddress(IPv4, (char*)&endIdentifier, 4);
            break;
    }
}  // end ProtoAddress::SetEndIdentifier()

UINT32 ProtoAddress::GetEndIdentifier() const
{
    switch(type)
    {
        case IPv4:
        {
            return ntohl(((struct sockaddr_in*)&addr)->sin_addr.s_addr);
            
        }
#ifdef HAVE_IPV6
        case IPv6:
        {
            return ntohl(IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&addr)->sin6_addr)));
        }
#endif // HAVE_IPV6
        case ETH:  
        {
            // a dumb little hash: MSB is randomized vendor id, 3 LSB's is device id
            UINT8* addrPtr = (UINT8*)&addr;
            UINT8 vendorHash = addrPtr[0] ^ addrPtr[1] ^ addrPtr[2];
            UINT32 temp32;
            memcpy((char*)&temp32, &vendorHash, 1);
            memcpy((((char*)&temp32)+1), addrPtr+3, 3);
            return ntohl(temp32);
        }
#ifdef SIMULATE
        case SIM:
            return ((UINT32)((struct sockaddr_sim*)&addr)->addr);
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress::GetEndIdentifier(): Invalid address type!\n");
            return INADDR_NONE;
    }
}  // end ProtoAddress:GetEndIdentifier()

bool ProtoAddress::HostIsEqual(const ProtoAddress& theAddr) const
{
    if (!IsValid() && !theAddr.IsValid()) return true;
    switch(type)
    {
        case IPv4:
        {
            struct in_addr myAddrIn = ((struct sockaddr_in *)&addr)->sin_addr;
            struct in_addr theAddrIn = ((struct sockaddr_in *)&(theAddr.addr))->sin_addr;
            if ((IPv4 == theAddr.type) &&
                (myAddrIn.s_addr == theAddrIn.s_addr))
                return true;
            else
                return false;
        }
#ifdef HAVE_IPV6
        case IPv6:
            if ((IPv6 == theAddr.type) &&
                (0 == memcmp(((struct sockaddr_in6*)&addr)->sin6_addr.s6_addr,
                             ((struct sockaddr_in6*)&(theAddr.addr))->sin6_addr.s6_addr,
                             4*sizeof(UINT32))))
                return true;
            else
                return false;
#endif // HAVE_IPV6 
        case ETH:
            if ((ETH == theAddr.type) &&
                0 == memcmp((char*)&addr, (char*)&theAddr.addr, 6))
                return true;
            else
                return false;
#ifdef SIMULATE
        case SIM:
            if ((SIM == theAddr.type) &&
                (((struct sockaddr_sim*)&addr)->addr ==
                 ((struct sockaddr_sim*)&(theAddr.addr))->addr))
                return true;
            else
                return false;
#endif // SIMULATE

            default:
                PLOG(PL_ERROR, "ProtoAddress::HostIsEqual(): Invalid address type!\n");
                return false;
    }
}  // end ProtoAddress::HostIsEqual()

int ProtoAddress::CompareHostAddr(const ProtoAddress& theAddr) const
{
    switch(type)
    {
        case IPv4:
            return memcmp(&((struct sockaddr_in *)&addr)->sin_addr,
                          &((struct sockaddr_in *)&theAddr.addr)->sin_addr,
                          4);
#ifdef HAVE_IPV6
        case IPv6:
            return memcmp(&((struct sockaddr_in6*)&addr)->sin6_addr,
                          &((struct sockaddr_in6*)&theAddr.addr)->sin6_addr,
                          16);
#endif // HAVE_IPV6
        case ETH:
            return memcmp((char*)&addr, (char*)&theAddr.addr, 6);                
#ifdef SIMULATE
        case SIM:
        {
            SIMADDR addr1 = ((struct sockaddr_sim*)&addr)->addr;
            SIMADDR addr2 = ((struct sockaddr_sim*)&(theAddr.addr))->addr;  
            if (addr1 < addr2)
                return -1;
            else if (addr1 > addr2)
                return 1;
            else
                return 0;
        }
#endif // SIMULATE
        default:
            PLOG(PL_ERROR, "ProtoAddress: CompareHostAddr(): Invalid address type!\n");
            return -1;
     }
}  // end ProtoAddress::CompareHostAddress()

bool ProtoAddress::ResolveEthFromString(const char* text)
{
    unsigned int a[6];
    char b[6];
    if (6 != sscanf(text, "%x:%x:%x:%x:%x:%x", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]))
    {
        PLOG(PL_DEBUG, "ProtoAddress: ResolveEthFromString(%s): Invalid ETH address type!\n", text);
        return false;
    }
    for (int i=0;i<=5;i++)
        b[i] = (char) a[i];
    SetRawHostAddress(ETH, b, 6); 
    return true;
}  // end ProtoAddress::ResolveEthFromString()

// non-DNS (i.e numeric address notation)
bool ProtoAddress::ConvertFromString(const char* text)
{
#ifdef SIMULATE
    return ResolveFromString(text);
#else
	// inet_pton not available below Vista
#ifdef WIN32 
    // Initialize the address family
    // Startup WinSock for name lookup
    if (!Win32Startup())
    {
        PLOG(PL_ERROR, "ProtoAddress: GetHostString(): Error initializing WinSock!\n");
        return NULL;
    }
	struct sockaddr_in sa = (struct sockaddr_in&)addr;	
	sa.sin_family = AF_INET;
	int addrSize = sizeof(struct sockaddr_storage);

#ifdef _UNICODE
	WCHAR theString[256];
	mbstowcs(theString, text, 255);
#else
	const char* theString = text;
#endif // if/else _UNICODE

	if (0 == WSAStringToAddress((LPTSTR)theString, AF_INET, NULL,(LPSOCKADDR)&sa,&addrSize))
	{
		addr = (struct sockaddr_storage&)sa;
		length = 4;
		Win32Cleanup();
		return true;
	}

	// See if it's an ipv6 address
	struct sockaddr_in6 sa6 = (struct sockaddr_in6&)addr;
    sa6.sin6_family = AF_INET6;
	addrSize = sizeof(struct sockaddr_storage);

	if (0 == WSAStringToAddress((LPTSTR)theString, AF_INET6, NULL,(LPSOCKADDR)&sa6,&addrSize))
	{
		addr = (struct sockaddr_storage&)sa6;
		type = IPv6;
		length = 16;
		Win32Cleanup();
		return true;
	}
	Win32Cleanup();
	// Finally, see if it's an Ethertype addr string
    return ResolveEthFromString(text);
#else
    // First try for IPv4 addr
    struct sockaddr_in sa;
    if (1 == inet_pton(AF_INET, text, &sa.sin_addr))
    {
        sa.sin_family = AF_INET;
        return SetSockAddr((struct sockaddr&)sa);
    }  
    // Next try for an IPv6 addr 
    struct sockaddr_in6 sa6; 
    if (1 == inet_pton(AF_INET6, text, &sa6.sin6_addr))
    {
        sa6.sin6_family = AF_INET6;
        return SetSockAddr((struct sockaddr&)sa6);
    }  
    // Finally, see if it's an Ethertype addr string
    return ResolveEthFromString(text);
#endif  // if/else WIN32
#endif  // if/else SIMULATE
}  // end ProtoAddress::ConvertFromString()

// (TBD) Provide a mechanism for async lookup with optional user interaction
bool ProtoAddress::ResolveFromString(const char* text)
{
    UINT16 thePort = GetPort();
#ifdef SIMULATE
    // Assume no DNS for simulations
    SIMADDR theAddr;
#ifndef OPNET // JPH 2/8/06
    if (1 == sscanf(text, "%i", &theAddr))
#else
	if (theAddr = ip_address_create(text))
#endif // OPNET
	{
        ((struct sockaddr_sim*)&addr)->addr = theAddr;
        type = SIM;
        length = sizeof(SIMADDR);
        SetPort(thePort);  // restore port number
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoAddress::ResolveFromString() Invalid simulator address: \"%s\"\n", text);
        return false;
    }
#else
   // Use DNS to look it up
   // Get host address, looking up by hostname if necessary      
#ifdef WIN32
    // Startup WinSock for name lookup
    if (!Win32Startup())
    {
            PLOG(PL_ERROR, "ProtoAddress::ResolveFromString() Error initializing WinSock!\n");
            return false;
    }
#endif //WIN32
#if defined(HAVE_IPV6)// Try to get address using getaddrinfo()
    struct addrinfo* addrInfoPtr = NULL;
    if(0 == getaddrinfo(text, NULL, NULL, &addrInfoPtr)) 
    {
#ifdef WIN32
		Win32Cleanup();
#endif // WIN32
        struct addrinfo* ptr = addrInfoPtr;
        bool result = false;
        //while (NULL != ptr)  // We just use the first addr returned for now
        {
            if (ptr->ai_family == PF_INET)
            {
                SetSockAddr(*(ptr->ai_addr));
                type = IPv4;
                length =  4; // IPv4 host addresses are always 4 bytes in length
                result = true;
            }
            else if (ptr->ai_family == PF_INET6)
            {
                SetSockAddr(*(ptr->ai_addr));
                type = IPv6;
                length = 16; // IPv6 host addresses are always 16 bytes in length
                result = true;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoAddress::ResolveFromString() getaddrinfo() returned unsupported address family!\n");
            }
            ptr = ptr->ai_next;
        }  // end while
        freeaddrinfo(addrInfoPtr);
        SetPort(thePort);  // restore port number
        return result;
    }
    else
    {
        if (NULL != addrInfoPtr) freeaddrinfo(addrInfoPtr);
        PLOG(PL_WARN, "ProtoAddress::ResolveFromString() getaddrinfo() error: %s\n", GetErrorString());
#ifdef WIN32
        // on WinNT 4.0, getaddrinfo() doesn't work, so we check the OS version
        // to decide what to do.  Try "gethostbyaddr()" if it's an old OS (e.g. NT 4.0 or earlier)
        OSVERSIONINFO vinfo;
        vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&vinfo);
        if ((VER_PLATFORM_WIN32_NT == vinfo.dwPlatformId) &&
            ((vinfo.dwMajorVersion > 4) ||
             ((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion > 0))))
                    return false;  // it's a modern OS where getaddrinfo() should work!
#else
        return false;
#endif // if/else WIN32/UNIX
    }
#endif  // HAVE_IPV6
#if defined(WIN32) || !defined(HAVE_IPV6)
    // Use this approach as a backup for NT-4 or if !HAVE_IPV6
    // 1) is it a "dotted decimal" address?
    struct sockaddr_in* addrPtr = (struct sockaddr_in*)&addr;
    if (INADDR_NONE != (addrPtr->sin_addr.s_addr = inet_addr(text)))
    {
        addrPtr->sin_family = AF_INET;
    }
    else
    {
        // 2) Use "gethostbyname()" to lookup IPv4 address
        struct hostent *hp = gethostbyname(text);
#ifdef WIN32
		Win32Cleanup();
#endif // WIN32
        if(hp)
        {
            addrPtr->sin_family = hp->h_addrtype;
#if defined(_SOCKLEN_T)  ||  defined(_SOCKLEN_T_DECLARED)   // for BSD systems
           addrPtr->sin_len = hp->h_length;
#endif // defined(_SOCKLENT_T)  || defined(_SOCKLEN_T_DECLARED)
            memcpy((char*)&addrPtr->sin_addr,  hp->h_addr,  hp->h_length);
        }
        else
        {
            PLOG(PL_WARN, "ProtoAddress::ResolveFromString() gethostbyname() error: %s\n",
                    GetErrorString());
            return false;
        }
    }
    if (addrPtr->sin_family == AF_INET)
    {
        type = IPv4;
        length =  4;  // IPv4 addresses are always 4 bytes in length
        SetPort(thePort);  // restore port number
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoAddress::ResolveFromString gethostbyname() returned unsupported address family!\n");
        return false;
    }
#endif // WIN32 || !HAVE_IPV6
#endif // !SIMULATE
    
}  // end ProtoAddress::ResolveFromString()
#ifdef USE_GETHOSTBYADDR
bool ProtoAddress::ResolveToName(char* buffer, unsigned int buflen) const
{
#ifdef WIN32
        if (!Win32Startup())
        {
            PLOG(PL_ERROR, "ProtoAddress::ResolveToName() Error initializing WinSock!\n");
            GetHostString(buffer, buflen);
            return false;
        }
#endif // WIN32
    struct hostent* hp = NULL;
    switch (type)
    {
        case IPv4:
            hp = gethostbyaddr((char*)&(((struct sockaddr_in*)&addr)->sin_addr),
                               4, AF_INET);
            break;
#ifdef HAVE_IPV6
        case IPv6:
            hp = gethostbyaddr((char*)&(((struct sockaddr_in*)&addr)->sin_addr),
                               16, AF_INET6);
         break;
#endif // HAVE_IPV6
#ifdef SIMULATE
        case SIM:
            GetHostString(buffer, buflen);
            return true;
#endif // SIMULATE
        case ETH:
            GetHostString(buffer, buflen);
            return true;
        default:
            PLOG(PL_ERROR, "ProtoAddress::ResolveToName(): Invalid address type!\n");
            return false;
    }  // end switch(type)
#ifdef WIN32
		Win32Cleanup();
#endif // WIN32
        
    if (hp)
    {
        // Use the hp->h_name hostname by default
        size_t nameLen = 0;
        unsigned int dotCount = 0;
        strncpy(buffer, hp->h_name, buflen);
        nameLen = strlen(hp->h_name);
        nameLen = nameLen < buflen ? nameLen : buflen;
        const char* ptr = hp->h_name;
        while ((ptr = strchr(ptr, '.')))
        {
            ptr++;
            dotCount++;   
        }
        
        char** alias = hp->h_aliases;
        // Use first alias by default
        if (alias && *alias && buffer)
        {
            // Try to find the fully-qualified name 
            // (longest alias with most '.')
            while (NULL != *alias)
            {
                unsigned int dc = 0;
                ptr = *alias;
                bool isArpa = false;
                while (NULL != (ptr = strchr(ptr, '.')))
                {
                    ptr++;
                    // don't let ".arpa" aliases override
                    isArpa = (0 == strcmp(ptr, "arpa"));
                    dc++;   
                }
                size_t alen = strlen(*alias);
                bool override = (dc > dotCount) || 
                                ((dc == dotCount) && (alen >nameLen));
                if (isArpa) override = false;
                if (override)
                {
                    strncpy(buffer, *alias, buflen);
                    nameLen = alen;
                    dotCount = dc;
                    nameLen = nameLen < buflen ? nameLen : buflen;
                }
                alias++;
            }
        }
        return true;
    }
    else
    {
        PLOG(PL_WARN, "ProtoAddress::ResolveToName() gethostbyaddr() error: %s\n",
                GetErrorString());
        GetHostString(buffer, buflen);
        return false;
    }
}  // end ProtoAddress::ResolveToName() 

#else // Use newer getnameinfo rather than legacy gethostbyaddr
bool ProtoAddress::ResolveToName(char* buffer, unsigned int buflen) const
{
#ifdef WIN32
        if (!Win32Startup())
        {
            PLOG(PL_ERROR, "ProtoAddress::ResolveToName() Error initializing WinSock!\n");
            GetHostString(buffer, buflen);
            return false;
        }
        DWORD retval = 0;
#else // WIN32
        int retval = 0;
#endif // endif WIN32
    switch (type)
    {
        case IPv4:
        case IPv6:
            retval = getnameinfo((struct sockaddr *) &addr,
                                 sizeof(addr),
                                 buffer,
                                 buflen,NULL,0,NI_NAMEREQD);
            break;
#ifdef SIMULATE
        case SIM:
            GetHostString(buffer, buflen);
            return true;
#endif // SIMULATE
        case ETH:
            GetHostString(buffer, buflen);
            return true;
        default:
            PLOG(PL_ERROR, "ProtoAddress::ResolveToName(): Invalid address type!\n");
            return false;
    }  // end switch(type)
#ifdef WIN32
        Win32Cleanup();
#endif // WIN32
        if (retval != 0)
        {
            PLOG(PL_ERROR, "ProtoAddress::ResolveToName() error: %s\n", gai_strerror(retval));
            return false;
        }
        
        return true;
}  // end ProtoAddress::ResolveToName() 
#endif // endif USE_GETHOSTBYADDR

bool ProtoAddress::ResolveLocalAddress(char* buffer, unsigned int buflen)
{
    UINT16 thePort = GetPort();  // save port number
    // Try to get fully qualified host name if possible
    char hostName[256];
    hostName[0] = '\0';
    hostName[255] = '\0';
#ifdef WIN32
    if (!Win32Startup())
    {
        PLOG(PL_ERROR, "ProtoAddress::ResolveLocalAddress() error startup WinSock!\n");
        return false;
    }
#endif // WIN32
    int result = gethostname(hostName, 255);
#ifdef WIN32
	Win32Cleanup();
#endif  // WIN32
    if (result)
    {
        PLOG(PL_ERROR, "ProtoAddress::ResolveLocalAddress(): gethostname() error: %s\n", GetErrorString());
        return false;
    }
    else
    {
        // Terminate at first '.' in name, if any (e.g. MacOS .local)
        char* dotPtr = strchr(hostName, '.');
        if (NULL != dotPtr) *dotPtr = '\0';
        bool lookupOK = ResolveFromString(hostName);
        if (lookupOK)
        {
            // Invoke ResolveToName() to get fully qualified name and use that address
            ResolveToName(hostName, 255);
            lookupOK = ResolveFromString(hostName);
        }
        if (!lookupOK || IsLoopback())
        {
            // darn it! lookup failed or we got the loopback address ... 
            // So, try to troll interfaces for a decent address
            // using our handy-dandy ProtoSocket::GetInterfaceInfo routines
            gethostname(hostName, 255);
            if (!lookupOK)
            {
                // Set IPv4 loopback address as fallback id if no good address is found
                UINT32 loopbackAddr = htonl(0x7f000001); 
                SetRawHostAddress(IPv4, (char*)&loopbackAddr, 4);  
            }
            
            // Use "ProtoSocket::FindLocalAddress()" that trolls through network interfaces
            // looking for an interface with a non-loopback address assigned.
            if (!ProtoSocket::FindLocalAddress(IPv4, *this))
            {
#ifdef HAVE_IPV6
                // Try IPv6 if IPv4 wasn't assigned
                if (!ProtoSocket::FindLocalAddress(IPv6, *this))
                {
                    PLOG(PL_WARN, "ProtoAddress::ResolveLocalAddress() warning: no assigned addresses found\n");
                }
#endif // HAVE_IPV6
            }
            if (IsLoopback() || IsUnspecified())
                PLOG(PL_ERROR, "ProtoAddress::ResolveLocalAddress() warning: only loopback address found!\n");    
        }
        SetPort(thePort);  // restore port number
        buflen = buflen < 255 ? buflen : 255;
        if (buffer) strncpy(buffer, hostName, buflen);
        return true;
    }
}  // end ProtoAddress::ResolveLocalAddress()


ProtoAddressList::ProtoAddressList()
{
}

ProtoAddressList::~ProtoAddressList()
{
    Destroy();
}

void ProtoAddressList::Destroy()
{
    Item* entry;
    while (NULL != (entry = static_cast<Item*>(addr_tree.GetRoot())))
    {
        addr_tree.Remove(*entry);   
        delete entry;
    }
}  // end ProtoAddressList::Destroy()

bool ProtoAddressList::Insert(const ProtoAddress& theAddress, const void* userData)
{
    if (!theAddress.IsValid())
    {
        PLOG(PL_ERROR, "ProtoAddressList::Insert() error: invalid address\n");
        return false;
    }
    Item* entry = static_cast<Item*>(addr_tree.Find(theAddress.GetRawHostAddress(), theAddress.GetLength() << 3));
    if (NULL != entry)
    {
        // Just update user data
        entry->SetUserData(userData);
    }
    else
    {
        Item* entry = new Item(theAddress, userData);
        if (NULL == entry)
        {
            PLOG(PL_ERROR, "ProtoAddressList::Insert() new ProtoTree::Item error: %s\n", GetErrorString());
            return false;
        }
        addr_tree.Insert(*entry);
    }
    return true;
}  // end ProtoAddressList::Insert()

void ProtoAddressList::Remove(const ProtoAddress& addr)
{
    Item* entry = static_cast<Item*>(addr_tree.Find(addr.GetRawHostAddress(), addr.GetLength() << 3));
    if (NULL != entry)
    {
        addr_tree.Remove(*entry);
        delete entry;
    }   
}  // end ProtoAddressList::Remove()

bool ProtoAddressList::AddList(ProtoAddressList& addrList)
{
    ProtoAddressList::Iterator iterator(addrList);
    ProtoAddress addr;
    while (iterator.GetNextAddress(addr))
    {
        if (!Insert(addr)) return false;
    }
    return true;
}  // end ProtoAddressList::AddList()

void ProtoAddressList::RemoveList(ProtoAddressList& addrList)
{
    ProtoAddressList::Iterator iterator(addrList);
    ProtoAddress addr;
    while (iterator.GetNextAddress(addr))
        Remove(addr);
}  // end ProtoAddressList::RemoveList()


// Returns the root of the addr_tree
bool ProtoAddressList::GetFirstAddress(ProtoAddress& firstAddr) const
{
    Item* rootItem = static_cast<Item*>(addr_tree.GetRoot());
    if (NULL != rootItem)
    {
        firstAddr = rootItem->GetAddress();
        return true;
    }
    else
    {
        firstAddr.Invalidate();
        return false;
    }
}  // end ProtoAddressList::GetFirstAddress()

        
/*
ProtoAddressList::Item* ProtoAddressList::GetFirstItem()
{
    Item* rootItem = static_cast<Item*>(addr_tree.GetRoot());
    return rootItem;
}  // end ProtoAddressList::GetFirstItem()
*/


ProtoAddressList::Item::Item(const ProtoAddress& theAddr, const void* userData)
 : addr(theAddr), user_data(userData)
{   
}

ProtoAddressList::Item::~Item()
{
}


ProtoAddressList::Iterator::Iterator(ProtoAddressList& addrList)
 : ptree_iterator(addrList.addr_tree)
{
}

ProtoAddressList::Iterator::~Iterator()
{
}

bool ProtoAddressList::Iterator::GetNextAddress(ProtoAddress& nextAddr)
{
    Item* nextItem = static_cast<Item*>(ptree_iterator.GetNextItem());
    if (NULL != nextItem)
    {
        nextAddr = nextItem->GetAddress();
        return true;
    }
    else
    {
        nextAddr.Invalidate();
        return false;
    }   
}  // end ProtoAddressList::Iterator::GetNextAddress()

bool ProtoAddressList::Iterator::PeekNextAddress(ProtoAddress& nextAddr)
{
    Item* nextItem = static_cast<Item*>(ptree_iterator.PeekNextItem());
    if (NULL != nextItem)
    {
        nextAddr = nextItem->GetAddress();
        return true;
    }
    else
    {
        nextAddr.Invalidate();
        return false;
    }   
}  // end ProtoAddressList::Iterator::PeekNextAddress()
