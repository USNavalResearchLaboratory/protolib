
/**
* @file protoSocket.cpp
* 
* @brief Network socket container class that provides consistent interface for use of operating system (or simulation environment) transport sockets.
*/



#ifdef UNIX

#include <unistd.h>
#include <stdlib.h>  // for atoi()
#ifdef HAVE_IPV6
#ifdef MACOSX
#define __APPLE_USE_RFC_3542 1  // needed to invoke IPV6_PKTINFO
#endif // MACOSX
#include <netinet/in.h>
#endif  // HAVE_IPV6
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>
#include <fcntl.h>

#ifndef SIOCGIFHWADDR
#if defined(SOLARIS) || defined(IRIX)
#include <sys/sockio.h> // for SIOCGIFADDR ioctl
#include <netdb.h>      // for res_init()
#include <sys/dlpi.h>
#include <stropts.h>
#else
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <ifaddrs.h> 
#endif  // if/else (SOLARIS || IRIX)
#endif  // !SIOCGIFHWADDR

#endif  // UNIX

#ifdef WIN32
#include <Winsock2.h>
#include <Ws2def.h>
#include <WS2tcpip.h>  // for extra socket options
#include <Mswsock.h>   // for even more
#include <Windows.h>
#include <Iphlpapi.h>
#include <Iptypes.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
typedef UINT32 in_addr_t;
#endif  // WIN32

// NOTE: This value should be confirmed.  It's the apparently Linux-only approach we currently
//       use to set the IPv6 flow info header fields.  Other approaches are under investigation
//       (We seem to be only able to affect the "traffic class" bits at this time)
#ifndef IPV6_FLOWINFO_SEND
#define IPV6_FLOWINFO_SEND 33 // from linux/in6.h
#endif // !IPV6_FLOWINFO_SEND

#include <stdio.h>
#include <string.h>

#include "protoSocket.h"
#include "protoNet.h"
#include "protoDebug.h"

// Hack for using with NRL IPSEC implementation
#ifdef HAVE_NETSEC
#include <net/security.h>
extern void* netsec_request;
extern int netsec_requestlen;
#endif // HAVE_NETSEC

// Use this macro (-DSOCKLEN_T=X) in your system's Makefile if the type socklen_t is 
// not defined for your system (use "man getsockname" to see what type is required).

#ifdef SOCKLEN_T
#define socklen_t SOCKLEN_T
#else
#ifdef WIN32
//#define socklen_t int
#endif // WIN32
#endif // if/else SOCKLEN_T

#include "protoDispatcher.h"

#ifdef WIN32
const ProtoSocket::Handle ProtoSocket::INVALID_HANDLE = INVALID_SOCKET;
LPFN_WSARECVMSG ProtoSocket::WSARecvMsg = NULL;
#else
const ProtoSocket::Handle ProtoSocket::INVALID_HANDLE = -1;
#endif  // if/else WIN32

ProtoSocket::IPv6SupportStatus ProtoSocket::ipv6_support_status = IPV6_UNKNOWN;

ProtoSocket::ProtoSocket(ProtoSocket::Protocol theProtocol)
    : domain(IPv4), protocol(theProtocol), raw_protocol(RAW), state(CLOSED), 
      handle(INVALID_HANDLE), port(-1), tos(0), ecn_capable(false), ip_recvdstaddr(false),
#ifdef HAVE_IPV6
      flow_label(0),
#endif // HAVE_IPV6
      notifier(NULL), notify_output(false), notify_input(true), notify_exception(false),
#ifdef WIN32
      input_event_handle(NULL), output_event_handle(NULL), 
      input_ready(false), output_ready(false), closing(false),
#endif // WIN32
      listener(NULL), user_data(NULL)
{
   
}

ProtoSocket::~ProtoSocket()
{
    Close();
    if (NULL != listener)
    {
        delete listener;
        listener = NULL; 
    }
}

bool ProtoSocket::SetBlocking(bool blocking)
{
#ifdef UNIX
    if (blocking)
    {
        if(-1 == fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) & ~O_NONBLOCK))
        {
            PLOG(PL_ERROR, "ProtoSocket::SetBlocking() fcntl(F_SETFL(~O_NONBLOCK)) error: %s\n", GetErrorString());
            return false;
        }
    }
    else
    {
        if(-1 == fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK))
        {
            PLOG(PL_ERROR, "ProtoSocket::SetBlocking() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
            return false;
        }
    }
#endif // UNIX
    return true;  //Note: taken care automatically under Win32 by WSAAsyncSelect(), etc
}  // end ProtoSocket::SetBlocking(bool blocking)

bool ProtoSocket::StartInputNotification()
{
    if (!notify_input)
    {
	    notify_input = true;
	    notify_input = UpdateNotification();
    }
	return notify_input;
}  // end ProtoSocket::StartInputNotification()

void ProtoSocket::StopInputNotification()
{
	if (notify_input)
    {
	    notify_input = false;
	    UpdateNotification();
    }
}  // end ProtoSocket::StopInputNotification()

bool ProtoSocket::StartOutputNotification()
{
	if (!notify_output)
    {
	    notify_output = true;
	    notify_output = UpdateNotification();
    }
	return notify_output;
}  // end ProtoSocket::StartOutputNotification()

void ProtoSocket::StopOutputNotification()
{
	 if (notify_output)
    {
	    notify_output = false;
	    UpdateNotification();
    }
}  // end ProtoSocket::StopOutputNotification()

bool ProtoSocket::StartExceptionNotification()
{
    notify_exception = true;
    notify_exception = UpdateNotification();
    return notify_exception;
}  // end ProtoSocket::StartExceptionNotification()

void ProtoSocket::StopExceptionNotification()
{   
    notify_exception = false;
    UpdateNotification();
}  // ProtoSocket::StopExceptionNotification()

bool ProtoSocket::SetNotifier(ProtoSocket::Notifier* theNotifier)
{
    if (notifier != theNotifier)
    {
        if (IsOpen())
        {
            // 1) Detach old notifier, if any
            if (NULL != notifier)
            {
                notifier->UpdateSocketNotification(*this, 0);
                if (NULL == theNotifier)
                {
                    // Reset socket to "blocking"
                    if(!SetBlocking(true))
                        PLOG(PL_ERROR, "ProtoSocket::SetNotifier() SetBlocking(true) error\n", GetErrorString());
                }
            }
            else
            {
                // Set socket to "non-blocking"
				if(!SetBlocking(false))
                {
                    PLOG(PL_ERROR, "ProtoSocket::SetNotifier() SetBlocking(false) error\n", GetErrorString());
                    return false;
                }
#ifdef WIN32
                // Reset input/output ready to initial state?
                input_ready = false;
                output_ready = true;
#endif // WIN32
            }   
            // 2) Set and update new notifier (if applicable)
            notifier = theNotifier;
            if (!UpdateNotification())
            {
                notifier = NULL;  
                return false;
            } 
        }
        else
        {
            notifier = theNotifier;
        }
    }
    return true;
}  // end ProtoSocket::SetNotifier()

ProtoAddress::Type ProtoSocket::GetAddressType()
{
    switch (domain)
    {
        case LOCAL:
            return ProtoAddress::INVALID;  
        case IPv4:
            return ProtoAddress::IPv4;
#ifdef HAVE_IPV6
        case IPv6:
            return ProtoAddress::IPv6; 
#endif // HAVE_IPV6
#ifdef SIMULATE
        case SIM:
            return ProtoAddress::SIM;
#endif // SIMULATE
        default:
            return ProtoAddress::INVALID; 
    }  
}  // end ProtoSocket::GetAddressType()

/**
 * WIN32 needs the address type determine IPv6 _or_ IPv4 socket domain
 * @note WIN32 can't do IPv4 on an IPV6 socket!
 * Vista and above support dual stack sockets
 */
bool ProtoSocket::Open(UINT16               thePort, 
                       ProtoAddress::Type   addrType,
                       bool                 bindOnOpen)
{
    if (IsOpen()) Close();
#ifdef HAVE_IPV6
    if(addrType == ProtoAddress::IPv6)
    {
        if (!HostIsIPv6Capable())
        {
            PLOG(PL_ERROR,"ProtoSocket::Open() system not IPv6 capable?\n");
            return false;
        }
        domain = IPv6;
    }
    else
#endif // HAVE_IPV6
    {
        domain = IPv4;
    }    
    
    int socketType = 0;
    switch (protocol)
    {
        case UDP:
            socketType = SOCK_DGRAM;
            break;
        case TCP:
            socketType = SOCK_STREAM;
            break;
        case RAW:  
            socketType = SOCK_RAW;
            break;
        default:
	        PLOG(PL_ERROR,"ProtoSocket::Open() error: Unsupported protocol\n");
	        return false;
    }
    
#ifdef WIN32
#ifdef HAVE_IPV6
    int family = (IPv6 == domain) ? AF_INET6: AF_INET;
#else
    int family = AF_INET;
#endif // if/else HAVE_IPV6
    // Startup WinSock
	if (!ProtoAddress::Win32Startup())
    {
        PLOG(PL_ERROR, "ProtoSocket::Open() WSAStartup() error: %s\n", GetErrorString());
        return false;
    }
    // Since we're might want QoS, we need find a QoS-capable UDP service provider
    WSAPROTOCOL_INFO* infoPtr = NULL;
    WSAPROTOCOL_INFO* protocolInfo = NULL;
    DWORD buflen = 0;
    // Query to properly size protocolInfo buffer
    WSAEnumProtocols(NULL, protocolInfo, &buflen);
    if (buflen)
    {
        int protocolType;
        switch (protocol)
        {
            case UDP:
                protocolType = IPPROTO_UDP;
                break;
            case TCP:
                protocolType = IPPROTO_TCP;
                break;
            case RAW:  
                switch (raw_protocol)
                {
                    case RAW:
                        protocolType = IPPROTO_RAW;
                        break;
                    case UDP:
                        protocolType = IPPROTO_UDP;
                        break;
                    case TCP:
                        protocolType = IPPROTO_TCP;
                        break;
                    default:
                        protocolType = IPPROTO_RAW;
                }
                break; 
        }
        
        // Enumerate, try to find multipoint _AND_ QOS-capable UDP, and create a socket
        if ((protocolInfo = (WSAPROTOCOL_INFO*) new char[buflen]))
        {
            int count = WSAEnumProtocols(NULL, protocolInfo, &buflen);
            if (SOCKET_ERROR != count)
            {
                for (int i = 0; i < count; i++)
                {
                    switch (protocol)
                    {
                        // This code tries to find a multicast-capable UDP/TCP socket
                        // providers _without_ QoS support, if possible (but note will 
                        // use one with QoS support if this is not possible.  The reason
                        // for this is that, ironically, it appears that a socket _without_ 
                        // QoS support allows for more flexible control of IP TOS setting 
                        // by the app?!  (Note that this may be revisited if we someday 
                        // again dabble with Win32 RSVP support (if it still exists) in future
                        // Protolib apps)
                        case UDP:
                            if ((IPPROTO_UDP == protocolInfo[i].iProtocol) &&
                                (0 != (XP1_SUPPORT_MULTIPOINT & protocolInfo[i].dwServiceFlags1)))
                            {
                                
                                if ((NULL == infoPtr) ||
                                    (0 == (XP1_QOS_SUPPORTED & protocolInfo[i].dwServiceFlags1)))
                                {
                                    infoPtr = protocolInfo + i;
                                }
                            }
                            break;
                        case TCP:
                            if (IPPROTO_TCP == protocolInfo[i].iProtocol)
                            {
                                if ((NULL == infoPtr) ||
                                    (0 == (XP1_QOS_SUPPORTED & protocolInfo[i].dwServiceFlags1)))
                                {
                                    infoPtr = protocolInfo + i;
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }  // end for (i=0..count)
            }
            else
            {
                PLOG(PL_ERROR, "ProtoSocket: WSAEnumProtocols() error2!\n");
                delete[] protocolInfo;
				ProtoAddress::Win32Cleanup();
                return false;
            }
        }
        else
        {
            PLOG(PL_ERROR, "ProtoSocket: Error allocating memory!\n");
			ProtoAddress::Win32Cleanup();
            return false;
        }        
    }
    else
    {
        PLOG(PL_WARN, "ProtoSocket::Open() WSAEnumProtocols() error1!\n");            
    }

    // Use WSASocket() to open right kind of socket
    // (Just a regular socket if infoPtr == NULL
    DWORD flags = WSA_FLAG_OVERLAPPED;
#ifndef _WIN32_WCE
    if (UDP == protocol) flags |= (WSA_FLAG_MULTIPOINT_C_LEAF | WSA_FLAG_MULTIPOINT_D_LEAF);
#endif // !_WIN32_WCE
    handle = WSASocket(family, socketType, 0, infoPtr, 0, flags);
    if (NULL != protocolInfo) delete[] protocolInfo;
    if (INVALID_HANDLE == handle)
    {
        PLOG(PL_ERROR, "ProtoSocket::Open() WSASocket() error: %s\n", GetErrorString());
	    ProtoAddress::Win32Cleanup();	
        return false;
    }
    if (NULL == (input_event_handle = WSACreateEvent()))
    {
        PLOG(PL_ERROR, "ProtoSocket::Open() WSACreateEvent() error: %s\n", GetErrorString());
        Close();
        return false;
    } 
	input_ready = false;
	output_ready = true;
#else 
#ifdef HAVE_IPV6
    int family = (IPv6 == domain) ? AF_INET6: AF_INET;
#else
    int family = AF_INET;
#endif // if/else HAVE_IPV6
    
    /*if (AF_INET6 == family)
        TRACE("opening an IPV6 socket\n");
    else
        TRACE("opening an IPV4 socket\n");*/
    int socketProtocol = 0;  // use default socket protocol type if not RAW
    if (SOCK_RAW == socketType)
    {
        switch (raw_protocol)
        {
            case RAW:
                socketProtocol = IPPROTO_RAW;
                break;
            case UDP:
                socketProtocol = IPPROTO_UDP;
                break;
            case TCP:
                socketProtocol = IPPROTO_TCP;
                break;
            default:
                socketProtocol = IPPROTO_RAW;
        }
    }
    if (INVALID_HANDLE == (handle = socket(family, socketType, socketProtocol)))
    {
       PLOG(PL_ERROR, "ProtoSocket::Open() socket() error: %s\n", GetErrorString());
       return false;
    }
    // (TBD) set IP_HDRINCL option for raw socket
#endif // if/else WIN32
    state = IDLE;
#ifdef NETSEC
    if (net_security_setrequest(handle, 0, netsec_request, netsec_requestlen))
    {
        PLOG(PL_ERROR, "ProtoSocket::Open() net_security_setrequest() error: %s\n", 
                GetErrorString());
        Close();
        return false;
    }
#endif // NETSEC

#ifdef UNIX
    // Don't pass descriptor to child processes
    if(-1 == fcntl(handle, F_SETFD, FD_CLOEXEC))
        PLOG(PL_ERROR, "ProtoSocket::Open() fcntl(FD_CLOEXEC) warning: %s\n", GetErrorString());
    // Make the socket non-blocking
    if (notifier)
    {
        if(-1 == fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK))
        {
            PLOG(PL_ERROR, "ProtoSocket::Open() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n",
                    GetErrorString());
            Close();
            return false;
        }
    }
#endif // UNIX
    if (bindOnOpen)
    {
        if (!Bind(thePort))
		{
			Close();
			return false;
		}
    }
    else
    {
        port = -1;
		// TBD - the UpdateNotification() call here may be unnecessary???
		//       (newly opened, unbound socket needs no notification?)
		if (!UpdateNotification())
		{
			PLOG(PL_ERROR, "ProtoSocket::Open() error installing async notification\n");
			Close();
			return false;
		}
    } 
    // Do the following in case TOS or "ecn_capable" was set _before_ 
    // ProtoSocket::Open() was called.  Note that the "IPv6" flow label
    // "traffic class" is also updated as needed for IPv6 sockets
    if ((0 != tos) || (ecn_capable)) SetTOS(tos);
#ifdef WIN32
    closing = false;
#endif //WIN32
    ip_recvdstaddr = false;  // make sure this is reset
    return true;
}  // end ProtoSocket::Open()

bool ProtoSocket::SetRawProtocol(Protocol theProtocol)
{
    bool wasOpen = false;
    UINT16 portSave = 0;
    if (IsOpen())
    {
        portSave = GetPort();
        Close();
        wasOpen = true;
    }
    protocol = RAW;
    raw_protocol = theProtocol;
    if (wasOpen)
        return Open(portSave);
    else
        return true;
}  // end ProtoSocket::SetRawProtocol()

bool ProtoSocket::UpdateNotification()
{    
	if (NULL != notifier)
    {
        if (IsOpen() && !SetBlocking(false))
        {
            PLOG(PL_ERROR, "ProtoSocket::UpdateNotification() SetBlocking() error\n");
            return false;   
        }
        int notifyFlags = NOTIFY_NONE;
        if (NULL != listener)
        {
            switch (protocol)
            {
                case UDP:
                case RAW:
                    switch (state)
                    {
                        case CLOSED:
                            break;
                        default:
                            if (notify_input && IsBound())
                                notifyFlags = NOTIFY_INPUT;
                            else
                                notifyFlags = NOTIFY_NONE;
                            if (notify_output) 
                                notifyFlags |= NOTIFY_OUTPUT;
                            if (IsOpen() && notify_exception)
                                notifyFlags |= NOTIFY_EXCEPTION;
                        break;
                    }
                  break;
                  
                case TCP:
                    switch(state)
                    {
                        case CLOSED:
                        case IDLE:
                            break;
                        case CONNECTING:
                            notifyFlags = NOTIFY_OUTPUT;
                            break;
                        case LISTENING:
                            notifyFlags = NOTIFY_INPUT;
                            break;
                        case CONNECTED:
			  	            if (notify_input)
                                notifyFlags = NOTIFY_INPUT;
                            if (notify_output) 
                                notifyFlags |= NOTIFY_OUTPUT;
                            break;  
                    }  // end switch(state)
                    break; 
                case ZMQ:
                    switch (state)
                    {
                        case CONNECTED:
                            if (notify_input)
                                notifyFlags = NOTIFY_INPUT;
                            break;
                        default:
                            break;        
                    }
                    break;
	            default:
                    PLOG(PL_ERROR,"ProtoSocket::UpdateNotification Error: Unsupported protocol: %d.\n", protocol);
	                break;
            }  // end switch(protocol)
        }  // end if(listener)
        return notifier->UpdateSocketNotification(*this, notifyFlags);
    }  
    else
    {
        return true;   
    }
}  // end ProtoSocket::UpdateNotification()

void ProtoSocket::OnNotify(NotifyFlag theFlag)
{
    ProtoSocket::Event event = INVALID_EVENT;
    if (NOTIFY_INPUT == theFlag)
    {
        switch (state)
        {
            case CLOSED:
                break;
            case IDLE:
                event = RECV;
                break;
            case CONNECTING:
                break;
            case LISTENING:
                // (TBD) check for error
                event = ACCEPT;
                break; 
            case CONNECTED:
                event = RECV;
                break;
        }        
    }
    else if (NOTIFY_OUTPUT == theFlag)
    {
        switch (state)
        {
            case CLOSED:
                break;
            case IDLE:
                event = SEND;
                break;
            case CONNECTING:
            {
#ifdef WIN32
                event = CONNECT;
                state = CONNECTED;
                UpdateNotification();
#else
                int err;
                socklen_t errsize = sizeof(err);
                if (getsockopt(handle, SOL_SOCKET, SO_ERROR, (char*)&err, &errsize))
                {
                    PLOG(PL_ERROR, "ProtoSocket::OnNotify() getsockopt() error: %s\n", GetErrorString());
                    
                } 
                else if (0 != err)
                {
		            PLOG(PL_DEBUG, "ProtoSocket::OnNotify() getsockopt() error: %s\n", GetErrorString()); 
                    Disconnect();
		            event = ERROR_;  // TBD - should this be DISCONNECT instead?
                }
                else
                {
                    event = CONNECT;
                    state = CONNECTED;
                    UpdateNotification();
                }
#endif  // if/else WIN32
                break;
            }
            case LISTENING: 
                break;
            case CONNECTED:
                event = SEND;
                break;
        }    
    }
    else if (NOTIFY_EXCEPTION == theFlag)
    {
        event = EXCEPTION;
    }
    else if (NOTIFY_ERROR == theFlag)
    {
		//TRACE("ProtoSocket NOTIFY_ERROR notification\n");
        switch(state)
    	{
	        case CONNECTING:
	        case CONNECTED:
	            Disconnect();
            default:
                event = ProtoSocket::ERROR_;
	            break;
	    }
    }
    else  // NOTIFY_NONE  (connection was purposefully ended)
    {
        switch(state)
        {
            case CONNECTING:
                //PLOG(PL_ERROR, "ProtoSocket::OnNotify() Connect() error: %s\n", GetErrorString());
            case CONNECTED:
                Disconnect();
                event = DISCONNECT;
                break;
            default:
                break;
        }
    }
    ASSERT(INVALID_EVENT != event);
    if (listener) listener->on_event(*this, event);
}  // end ProtoSocket::OnNotify()

bool ProtoSocket::Bind(UINT16 thePort, const ProtoAddress* localAddress)
{
    if (IsBound()) Close();
    if (IsOpen() && (NULL != localAddress) && (GetAddressType() != localAddress->GetType()))
    {
        Close();  // requires a different address family, so close for reopen
    }
    if (!IsOpen()) 
    {
        ProtoAddress::Type addrType = localAddress ? localAddress->GetType() : ProtoAddress::IPv4;
		if(!Open(thePort, addrType, false))
        {
			PLOG(PL_ERROR,"ProtoSocket::Bind() error opening socket on port %d\n",thePort);
			return false;
		}
	}
#ifdef HAVE_IPV6
    struct sockaddr_storage socketAddr;
    socklen_t addrSize;
    if (IPv6 == domain)
    {
	    addrSize = sizeof(struct sockaddr_in6);
        memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in6));    
        ((struct sockaddr_in6*)&socketAddr)->sin6_family = AF_INET6;
        ((struct sockaddr_in6*)&socketAddr)->sin6_port = htons(thePort);
        ((struct sockaddr_in6*)&socketAddr)->sin6_flowinfo = 0;
        if (NULL != localAddress)
        {
            ((struct sockaddr_in6*)&socketAddr)->sin6_addr = 
                ((const struct sockaddr_in6*)(&localAddress->GetSockAddrStorage()))->sin6_addr;
        }
        else
        {
            ((struct sockaddr_in6*)&socketAddr)->sin6_addr = in6addr_any;
        }
    }
    else
    {
        addrSize = sizeof(struct sockaddr_in);
        memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in));    
        ((struct sockaddr_in*)&socketAddr)->sin_family = AF_INET;
        ((struct sockaddr_in*)&socketAddr)->sin_port = htons(thePort);
        if (NULL != localAddress)
        {
            ((struct sockaddr_in*)&socketAddr)->sin_addr = 
                ((const struct sockaddr_in*)(&localAddress->GetSockAddrStorage()))->sin_addr;
        }
	    else
        {
            struct in_addr inAddr;
            inAddr.s_addr = htonl(INADDR_ANY);
            ((struct sockaddr_in*)&socketAddr)->sin_addr = inAddr;
        }
    }
#else
    struct sockaddr socketAddr;
    socklen_t addrSize = sizeof(struct sockaddr_in);
    memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in));    
    ((struct sockaddr_in*)&socketAddr)->sin_family = AF_INET;
    ((struct sockaddr_in*)&socketAddr)->sin_port = htons(thePort);
    if (NULL != localAddress)
    {
        ((struct sockaddr_in*)&socketAddr)->sin_addr = 
            ((struct sockaddr_in*)(&localAddress->GetSockAddr()))->sin_addr;
    }
	else
    {
        struct in_addr inAddr;
        inAddr.s_addr = htonl(INADDR_ANY);
        ((struct sockaddr_in*)&socketAddr)->sin_addr = inAddr;
    }
#endif  //  if/else HAVE_IPV6
    
#ifdef UNIX
#ifdef HAVE_IPV6
    if ((IPv6 == domain) && (0 != flow_label))
        ((struct sockaddr_in6*)&socketAddr)->sin6_flowinfo = flow_label;
#endif // HAVE_IPV6
#endif // UNIX

#ifdef HAVE_IPV6
#ifdef WIN32
	OSVERSIONINFO osvi;
	BOOL osVistaOrLater;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	osVistaOrLater = osvi.dwMajorVersion > 5;
	if (osVistaOrLater)
	{
		// On Windows Vista & above dual stack sockets are supported so
		// disable ipv6_v6only so we can listen to both ipv4 & ipv6 connections
		// simultaneously
		int ipv6only = 0;
		if (setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only)) < 0)
		{
			PLOG(PL_ERROR, "ProtoSocket::Bind() setsockopt(!IPV6_V6ONLY) error: %s\n", GetErrorString());
			return false;
		}
	}
#endif  // WIN32
#endif // HAVE_IPV6
    // Bind the socket to the given port     
    if (bind(handle, (struct sockaddr*)&socketAddr, addrSize) < 0)
    {
       PLOG(PL_ERROR, "ProtoSocket::Bind(%hu) bind() error: %s\n", thePort, GetErrorString());
       return false;
    }

    // Get socket name so we know our port number  
    socklen_t sockLen = addrSize;
    if (getsockname(handle, (struct sockaddr*)&socketAddr, &sockLen) < 0) 
    {    
        PLOG(PL_ERROR, "ProtoSocket::Bind() getsockname() error: %s\n", GetErrorString());
        return false;
    }
	source_addr.SetSockAddr((struct sockaddr&)socketAddr);
    switch(((struct sockaddr*)&socketAddr)->sa_family)
    {
        case AF_INET:    
            source_addr.SetSockAddr((struct sockaddr&)socketAddr);
            port = ntohs(((struct sockaddr_in*)&socketAddr)->sin_port);
            break;
#ifdef HAVE_IPV6        
        case AF_INET6:
            source_addr.SetSockAddr((struct sockaddr&)socketAddr);
            port = ntohs(((struct sockaddr_in6*)&socketAddr)->sin6_port);
            break;
#endif // HAVE_IPV6        
        default:
            PLOG(PL_ERROR, "ProtoSocket::Bind() error: getsockname() returned unknown address type\n");
            return false;
    }
#ifdef WIN32
	// reset input/output readiness since it is a newly bound socket
	input_ready = false;
	output_ready = true;
#endif  // WIN32
    return UpdateNotification();
}  // end ProtoSocket::Bind()

bool ProtoSocket::Shutdown()
{
#ifdef WIN32 
    if (TCP == protocol)
#else
    if (IsConnected() && TCP == protocol)
#endif // WIN32
    {
        bool notifyOutput = notify_output;
        if (notifyOutput)
        {
            notify_output = false;
            UpdateNotification();
        }
#ifdef WIN32
        if (SOCKET_ERROR == shutdown(handle, SD_SEND))
#else
        if (0 != shutdown(handle, SHUT_WR))
#endif // if/else WIN32/UNIX
        {
            // TBD - should we not bother to re-enable output notification here?
            //  (i.e. since the user intended to shutdown the socket?!)
            if (notifyOutput)
            {
                notify_output = true;
                UpdateNotification();
            }
            PLOG(PL_ERROR, "ProtoSocket::Shutdown() error: %s\n", GetErrorString());
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoSocket::Shutdown() error: socket not connected\n");
        return false;
    }
}  // end ProtoSocket::Shutdown()

void ProtoSocket::Close()
{
    if (IsOpen()) 
    {
        if (IsConnected()) Disconnect();
        state = CLOSED;
        UpdateNotification();   
#ifdef WIN32
        if (NULL != input_event_handle)
        {
            WSACloseEvent(input_event_handle);
            input_event_handle = NULL;
        }
#endif // if WIN32
        if (INVALID_HANDLE != handle)
        {
#ifdef WIN32
            if (SOCKET_ERROR == closesocket(handle))
                PLOG(PL_WARN, "ProtoSocket::Close() warning: closesocket() error: %s\n", GetErrorString());
	        ProtoAddress::Win32Cleanup();
            input_ready = output_ready = false;  
#else
            close(handle);
#endif // if/else WIN32/UNIX
	        handle = INVALID_HANDLE;
        }
        port = -1;
    }
}  // end Close() 

bool ProtoSocket::Connect(const ProtoAddress& theAddress)
{
    TRACE("enter ProtoSocket::Connect() ...\n");
    if (IsConnected()) Disconnect();
    if (!IsOpen() && !Open(0, theAddress.GetType()))
    {
        PLOG(PL_ERROR, "ProtoSocket::Connect() error opening socket!\n");
        return false;
    }
#ifdef HAVE_IPV6
    socklen_t addrSize = (IPv6 == domain) ? sizeof(sockaddr_storage) : 
                                            sizeof(struct sockaddr_in);
#else
    socklen_t addrSize = sizeof(struct sockaddr_in);
#endif // if/else HAVE_IPV6
#ifdef WIN32
    //int result = WSAConnect(handle, &theAddress.GetSockAddr(), addrSize,
    //                      NULL, NULL, NULL, NULL);
	int result = connect(handle, &theAddress.GetSockAddr(), addrSize);
    if (SOCKET_ERROR == result)
    {
	if (WSAEWOULDBLOCK != WSAGetLastError())
        {
            PLOG(PL_ERROR, "ProtoSocket::Connect() WSAConnect() error: (%s)\n", GetErrorString());
            return false;
        } 
        output_ready = false;  // not yet connected
        state = CONNECTING;
    }
#else  // UNIX
#ifdef HAVE_IPV6
    if (flow_label && (ProtoAddress::IPv6 == theAddress.GetType()))
        ((struct sockaddr_in6*)(&theAddress.GetSockAddrStorage()))->sin6_flowinfo = flow_label;
#endif // HAVE_IPV6
    int result = connect(handle, &theAddress.GetSockAddr(), addrSize);
    if (0 != result)
    {
        if (EINPROGRESS != errno)
        {
            PLOG(PL_ERROR, "ProtoSocket::Connect() connect() error: %s\n", GetErrorString());
            return false;
        }
        state = CONNECTING;
    }
#endif // if/else WIN32/UNIX
    else
    {
        state = CONNECTED;
    }
    if (!UpdateNotification())
    {
        PLOG(PL_ERROR, "ProtoSocket::Connect() error updating notification\n");
        state = IDLE;
        UpdateNotification();
        return false;
    }
    // Use getsockname() to get local "port" and "source_addr"
#ifdef HAVE_IPV6
	struct sockaddr_in6 socketAddr;
#else
    struct sockaddr socketAddr;
#endif // if/else HAVE_IPV6
    socklen_t addrLen = sizeof(socketAddr);
    if (getsockname(handle, (struct sockaddr*)&socketAddr, &addrLen) < 0) 
    {  
        // getsockname() failed, so issue a warning
        PLOG(PL_ERROR, "ProtoSocket::Connect() getsockname() error: %s\n", GetErrorString());
        source_addr.Invalidate();
    }
    else
    {
        switch(((struct sockaddr*)&socketAddr)->sa_family)
        {
	        case AF_INET:  
                source_addr.SetSockAddr((struct sockaddr&)socketAddr);  
	            port = ntohs(((struct sockaddr_in*)&socketAddr)->sin_port);
	            break;
#ifdef HAVE_IPV6	    
	        case AF_INET6:
                source_addr.SetSockAddr((struct sockaddr&)socketAddr); 
	            port = ntohs(((struct sockaddr_in6*)&socketAddr)->sin6_port);
	            break;
#endif // HAVE_IPV6	   
#ifndef WIN32
            case AF_UNIX:
                source_addr.Invalidate();
                port = -1;
                break;
#endif // !WIN32 
	        default:
	            PLOG(PL_ERROR, "ProtoSocket::Connect() error: getsockname() returned unknown address type");
                break;
        }  // end switch()
    }
    destination = theAddress;  // cache the destination address
    return true;
}  // end bool ProtoSocket::Connect()

void ProtoSocket::Disconnect()
{
    if (IsConnected() || IsConnecting())
    {
		state = IDLE;
        UpdateNotification();
        //source_addr.Invalidate();
        //destination.Invalidate();
        struct sockaddr nullAddr;
        memset(&nullAddr, 0 , sizeof(struct sockaddr));
#ifdef UNIX
        if (TCP != protocol)
        {
            nullAddr.sa_family = AF_UNSPEC;
            if (connect(handle, &nullAddr, sizeof(struct sockaddr)))
            {
                if (EAFNOSUPPORT != errno)
                    PLOG(PL_WARN, "ProtoSocket::Disconnect() connect() error: %s)\n", GetErrorString());
                // (TBD) should we Close() and re-Open() the socket here?
            }
        }
        else
        {
            // TCP doesn't like the connect(nullAddr) trick, best to just close
            // (TBD) should we Close() and re-Open() the socket here?
            // (but can't easily recall all socket options, so best not I guess)
            //UINT16 savePort = GetPort();
	  
	        // MACOSX & LINUX trigger server resets differently...
#ifdef MACOSX
            Close();
#else
	        nullAddr.sa_family = AF_UNSPEC;
	        if (connect(handle,&nullAddr,sizeof(struct sockaddr)))
	        {
		        if (EAFNOSUPPORT != errno)
		            PLOG(PL_WARN, "ProtoSocket::Disconnect() connect() error (%s)\n", GetErrorString());
				Close();
	        }
#endif
            //Open(savePort);
        }
#else
        if (SOCKET_ERROR == WSAConnect(handle, &nullAddr, sizeof(struct sockaddr),
                                       NULL, NULL, NULL, NULL))
        {
            //PLOG(PL_WARN, "ProtoSocket::Disconnect() WSAConnect() error: %s\n", GetErrorString());
			Close();  // if windows doesn't like the null-connect disconnect trick either
        }
#endif  // WIN32
    }
}  // end ProtoSocket::Disconnect()

bool ProtoSocket::Listen(UINT16 thePort)
{
    if (IsBound())
    {
        if ((0 != thePort) && (thePort != port))
        {
            PLOG(PL_ERROR, "ProtoSocket::Listen() error: socket bound to different port.\n");
            return false;
        }
    }
    else
    {
        if (!Bind(thePort))
        {
            PLOG(PL_ERROR, "ProtoSocket::Listen() error binding socket.\n");
            return false; 
        } 
    } 
    if (TCP == protocol)
		state = LISTENING;
	else
		return true; // UDP sockets don't "listen"
    if (!UpdateNotification())
    {
        state = IDLE;
        PLOG(PL_ERROR, "ProtoSocket::Listen() error updating notification\n");
        return false;
    }
#ifdef WIN32
    if (SOCKET_ERROR == listen(handle, 5))
#else
    if (listen(handle, 5) < 0)
#endif // if/else WIN32/UNIX
    {
        PLOG(PL_ERROR, "ProtoSocket: listen() error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end ProtoSocket::Listen()

bool ProtoSocket::Accept(ProtoSocket* newSocket)
{
	if (TCP != protocol)
	{
		PLOG(PL_ERROR, "ProtoSocket::Accept() error non-TCP socket!\n");
		return false;
	}
    ProtoSocket& theSocket = (NULL != newSocket) ? *newSocket : *this;
    // Clone server socket
    if (this != &theSocket) 
    {
        // TBD - should override ProtoSocket copy operator to delete old listener???
        if (NULL != theSocket.listener) delete theSocket.listener;
        theSocket = *this;
        theSocket.listener = NULL; // this->listener is duplicated later (or should we save our old one?) 
    }
#ifdef HAVE_IPV6
	struct sockaddr_in6 socketAddr;
#else
    struct sockaddr socketAddr;
#endif // if/else HAVE_IPV6
    socklen_t addrLen = sizeof(socketAddr);
#ifdef WIN32
    if (this != &theSocket)
    {
        if (!ProtoAddress::Win32Startup())
        {
            PLOG(PL_ERROR, "ProtoSocket::Accept() WSAStartup() error: %s\n", GetErrorString());
            return false;
        }
    }
    Handle theHandle = WSAAccept(handle, (struct sockaddr*)&socketAddr, &addrLen, NULL, NULL);
#else
    Handle theHandle = accept(handle, (struct sockaddr*)&socketAddr, &addrLen);
#endif // if/else WIN32/UNIX
    if (INVALID_HANDLE == theHandle)
    {
#ifdef WIN32
        switch (WSAGetLastError())
        {
            case WSAEWOULDBLOCK:
                if (input_ready)
                {
                    input_ready = false; 
                    UpdateNotification();
                }
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::Accept() accept() error: %s\n", GetErrorString());
                break;
        }
#endif
        PLOG(PL_ERROR, "ProtoSocket::Accept() accept() error: %s\n", GetErrorString());
        if (this != &theSocket)
        {
#ifdef WIN32
            ProtoAddress::Win32Cleanup();
#endif // WIN32
            theSocket.handle = INVALID_HANDLE;
            theSocket.state = CLOSED;
        }
        return false;
    }
    // Don't think we need make "input_ready" true on accept?
    if (LOCAL != domain)
        theSocket.destination.SetSockAddr((struct sockaddr&)socketAddr);
    // Get the socket name so we know our port number
    addrLen = sizeof(socketAddr);
    if (getsockname(theHandle, (struct sockaddr*)&socketAddr, &addrLen) < 0) 
    {    
        PLOG(PL_ERROR, "ProtoSocket::Accept() getsockname() error: %s\n", GetErrorString());
        if (this != &theSocket)
        {
#ifdef WIN32
            closesocket(theHandle);
			ProtoAddress::Win32Cleanup();
#endif // WIN32
            theSocket.handle = INVALID_HANDLE;
            theSocket.state = CLOSED;
        }
	    return false;
    }
    switch(((struct sockaddr*)&socketAddr)->sa_family)
    {
	    case AF_INET:  
            theSocket.source_addr.SetSockAddr((struct sockaddr&)socketAddr);
	        theSocket.port = ntohs(((struct sockaddr_in*)&socketAddr)->sin_port);
	        break;
#ifdef HAVE_IPV6	    
	    case AF_INET6:
            theSocket.source_addr.SetSockAddr((struct sockaddr&)socketAddr);
	        theSocket.port = ntohs(((struct sockaddr_in6*)&socketAddr)->sin6_port);
	        break;
#endif // HAVE_IPV6	   
#ifndef WIN32
        case AF_UNIX:
            theSocket.source_addr.Invalidate();
            theSocket.port = -1;
            break;
#endif // !WIN32 
	    default:
	        PLOG(PL_ERROR, "ProtoSocket::Accept() error: getsockname() returned unknown address type");
            if (this != &theSocket)
            {
 #ifdef WIN32
                closesocket(theHandle);
				ProtoAddress::Win32Cleanup();
#endif // WIN32
                theSocket.handle = INVALID_HANDLE;
                theSocket.state = CLOSED;
            }
	        return false;
    }  // end switch()
    if (this == &theSocket)
    {  
        state = CLOSED;
        UpdateNotification();
#ifdef WIN32
        closesocket(theSocket.handle);
		input_ready = false;  // will be notified when there's something to read
		output_ready = true;  // assume new socket ready for writing
#else
        close(theSocket.handle);
#endif // if/else WIN32/UNIX
    }
    else
    {   
#ifdef WIN32
		// Need a new event handle for this new, unopened socket
        if (NULL == (theSocket.input_event_handle = WSACreateEvent()))
        {
            PLOG(PL_ERROR, "ProtoSocket::Accept() WSACreateEvent error: %s\n", GetErrorString());
            theSocket.Close();
            return false;
        }
		theSocket.input_ready = false; // will be notified when there's something to read
		theSocket.output_ready = true; // assume new socket ready for writing
#endif // WIN32
        // TBD - keep old listener / notifier and just do an UpdateNotification() here
        if (NULL != listener)
        {
            if (NULL == (theSocket.listener = listener->duplicate()))
            {
                PLOG(PL_ERROR, "ProtoSocket::Accept() listener duplication error: %s\n", ::GetErrorString());
                theSocket.Close();
                return false;
            }   
        }
        if (NULL != notifier)
        {
            theSocket.handle = theHandle;
            if(!theSocket.SetBlocking(false))
            {
                PLOG(PL_ERROR, "ProtoSocket::Accept() SetBlocking(false) error\n");
                theSocket.Close();
                return false;
	        }
        }
    }  // end if/else (this == &theSocket)
    theSocket.handle = theHandle;  // the socket gets the new handle/descriptor from accept()
    theSocket.state = CONNECTED;
    theSocket.UpdateNotification();
    return true;
}  // end ProtoSocket::Accept()

bool ProtoSocket::Send(const char*         buffer, 
                       unsigned int&       numBytes)
{
    if (IsConnected())
    {
#ifdef WIN32
        WSABUF sendBuf;
        sendBuf.buf = (char*)buffer;
        sendBuf.len = numBytes;
	    DWORD bytesSent;
        if (SOCKET_ERROR == WSASend(handle, &sendBuf, 1, &bytesSent, 0, NULL, NULL))
        {
            numBytes = 0; 
            switch (WSAGetLastError())
            {
                case WSAEINTR:
                    return true;
                case WSAEWOULDBLOCK:
                    if (output_ready)
                    {
						output_ready = false;  
                        UpdateNotification();  // because no longer "output ready"
                    }
                    return true;
                case WSAENETRESET:
                case WSAECONNABORTED:
                case WSAECONNRESET:
                case WSAESHUTDOWN:
                case WSAENOTCONN:
                    if (output_ready)
                    {
                        output_ready = false;
                        UpdateNotification();  // because no longer "output ready"
                    }
                    OnNotify(NOTIFY_ERROR);
                    break;
                default:
                    PLOG(PL_ERROR, "ProtoSocket::Send() WSASend() error: %s\n", GetErrorString());
                    break;
            }
            return false;
        }
        else
        {
            if (bytesSent < numBytes)
			{
                if (output_ready)
                {
                    output_ready = false;
				    UpdateNotification();  // because no longer "output ready"
                }
			}
            else if (!output_ready)
            {
				output_ready = true;
                UpdateNotification();
            }
            numBytes = (unsigned int)bytesSent;
            return true;
        }
#else
        ssize_t result = send(handle, buffer, (size_t)numBytes, 0);
        if (result < 0)
	    {
            numBytes = 0;
	        switch (errno)
            {
                case EINTR:
                case EAGAIN:
                    return true;
                case ENETRESET:
                case ECONNABORTED:
                case ECONNRESET:
                case ESHUTDOWN:
                case ENOTCONN:
                case EPIPE:
                    OnNotify(NOTIFY_ERROR);
                    break;
                case ENOBUFS:
                    PLOG(PL_DEBUG, "ProtoSocket::Send() send() error: %s\n", GetErrorString());
                    return false;
                default:
                    PLOG(PL_ERROR, "ProtoSocket::Send() send() error: %s\n", GetErrorString());
                    break;
            }
            return false;  
        }
        else
        {
            numBytes = (unsigned int)result;
            return true;
        }
#endif // if/else WIN32/UNIX
    }
    else
    {
        PLOG(PL_ERROR, "ProtoSocket::Send() error unconnected socket\n");
        numBytes = 0;
        return false;   
    }
}  // end ProtoSocket::Send()

bool ProtoSocket::Recv(char*            buffer, 
                       unsigned int&    numBytes)
{
#ifdef WIN32
    WSABUF recvBuf;
    recvBuf.buf = buffer;
    recvBuf.len = numBytes;
    DWORD bytesReceived;
    DWORD flags = 0;
    if (SOCKET_ERROR == WSARecv(handle, &recvBuf, 1, &bytesReceived, &flags, NULL, NULL))
    {
        numBytes = 0;
        switch (WSAGetLastError())
        {
            case WSAEINTR:
            case WSAEWOULDBLOCK:
                if (input_ready)
                {
                    input_ready = false;
                    UpdateNotification();  // because no longer "input ready"
                }
                return true; // not really an error, just no bytes read
                break;
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
            case WSAESHUTDOWN:
            case WSAENOTCONN:
                PLOG(PL_ERROR, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                OnNotify(NOTIFY_ERROR);
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                break;
        }
        return false;
    }
    else
    {
        if ((bytesReceived < numBytes) && (TCP == protocol))
	  {
            input_ready = false;  
	    UpdateNotification();  // because no longer "input ready"
	  }
        else if (!input_ready)
        {
	  input_ready = true;
	  UpdateNotification();
        }
        numBytes = bytesReceived;
        // TBD - Should we do a NOTIFY_NONE here like we do for UNIX?
        return true;
    }
#else
    ssize_t result = recv(handle, buffer, numBytes, 0);
    if (result < 0)
    {
        numBytes = 0;
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                PLOG(PL_WARN, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());   
                return true;            
                break;
            case ENETRESET:
            case ECONNABORTED:
            case ECONNRESET:
            case ESHUTDOWN:
            case ENOTCONN:
                OnNotify(NOTIFY_ERROR);
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                break;
        }
        return false;
    }
    else
    {
        numBytes = (unsigned int)result;
        if (0 == result) OnNotify(NOTIFY_NONE);
        return true;
    }
#endif // if/else WIN32/UNIX
}  // end ProtoSocket::Recv()

// Note the function prototype and behavior here is changing slightly
// from the prior / original ProtoSocket::SendTo() behavior such that
// the EWOULDBLOCK condition no longer returns "false" but instead returns
// "true", but with setting the referenced "buflen" value to zero.
// This lets us differentiate this condition, mainly as a cue for when
// (and when not to) enable socket output notification for async i/o mgmnt.s
bool ProtoSocket::SendTo(const char*         buffer, 
                         unsigned int&       buflen,
                         const ProtoAddress& dstAddr)
{
    if (!IsOpen())
    {
        if (!Open(0, dstAddr.GetType()))
        {
            PLOG(PL_ERROR, "ProtoSocket::SendTo() error: socket not open\n");
            return false;
        }
    }
    if (IsConnected())
    {
        unsigned int numBytes = buflen;
        if (!Send(buffer, numBytes))
        {

	        PLOG(PL_DEBUG, "ProtoSocket::SendTo() error: Send() error\n");
            buflen = 0;
            return false;
        }
        if (numBytes != buflen)
        {
            PLOG(PL_DEBUG, "ProtoSocket::SendTo() error: Send() incomplete\n");
            buflen = 0;
            return true;
        }
        else
        {
            return true;
        }
    }
	else
	{
		socklen_t addrSize;
#ifdef HAVE_IPV6
		if (flow_label && (ProtoAddress::IPv6 == dstAddr.GetType()))
			((struct sockaddr_in6*)(&dstAddr.GetSockAddrStorage()))->sin6_flowinfo = flow_label;
		if (ProtoAddress::IPv6 == dstAddr.GetType())
			addrSize = sizeof(struct sockaddr_in6);
		else
#endif //HAVE_IPV6
			addrSize = sizeof(struct sockaddr_in);
#ifdef WIN32
        WSABUF wsaBuf;
        wsaBuf.len = buflen;  
        wsaBuf.buf = (char*)buffer;
        DWORD numBytes; 
        if (SOCKET_ERROR == WSASendTo(handle, &wsaBuf, 1, &numBytes, 0, 
                                      &dstAddr.GetSockAddr(), addrSize, NULL, NULL))
        {
            buflen = 0;
            switch (WSAGetLastError())
            {
                case WSAEINTR:
                  return true;
                case WSAEWOULDBLOCK:
                  output_ready = false;
                  return true;
                default:
                  break;
            }
            PLOG(PL_ERROR, "ProtoSocket::SendTo() WSASendTo() error: %s\n", GetErrorString());
            return false;
        }
        else
        {
            //ASSERT(numBytes == buflen);
            return true;
        }
#else
        ssize_t result = sendto(handle, buffer, (size_t)buflen, 0, &dstAddr.GetSockAddr(), addrSize);	
        if (result < 0)
        {
            buflen = 0;
            switch (errno)
            {
                case EINTR:
                case EAGAIN:
                    return true;
                case ENOBUFS:
                    PLOG(PL_DEBUG, "ProtoSocket::SendTo() sendto() error: %s\n", GetErrorString());
                    return false;
                default:
                    break;
            }
	        PLOG(PL_ERROR, "ProtoSocket::SendTo() sendto() error: %s\n", GetErrorString());
            return false;
        }
        else
        {   
            //ASSERT(result == buflen);
            return true;
        }
#endif // if/else WIN32/UNIX
	}
}  // end ProtoSocket::SendTo()

bool ProtoSocket::RecvFrom(char*            buffer, 
                           unsigned int&    numBytes, 
                           ProtoAddress&    sourceAddr)
{
	if (!IsBound())
    {
        PLOG(PL_ERROR, "ProtoSocket::RecvFrom() error: socket not bound\n");
        numBytes = 0;    
    }
#ifdef HAVE_IPV6    
    struct sockaddr_storage sockAddr;
#else
    struct sockaddr sockAddr;
#endif  // if/else HAVE_IPV6
    socklen_t addrLen = sizeof(sockAddr);
    // Just do a regular recvfrom()  (TBD - for Win32, should we be using WSARecvFrom()???)
    ssize_t result = recvfrom(handle, buffer, (size_t)numBytes, 0, (struct sockaddr*)&sockAddr, &addrLen);
    if (result < 0)
    {
        numBytes = 0;
#ifdef WIN32
        switch (WSAGetLastError())
        {
            case WSAEINTR:
            case WSAEWOULDBLOCK:
                if (input_ready)
                {
                    input_ready = false;   
				    UpdateNotification(); // because no longer "input ready"
                }
                return true;
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::RecvFrom() recvfrom() error: %s\n", GetErrorString());
                break;
        }
#else
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                //PLOG(PL_WARN, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());   
                return true;            
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                break;
        }
#endif // UNIX
        return false;
    }
    else
    {
#ifdef WIN32
        if (!input_ready)
        {
            input_ready = true;
            UpdateNotification();
        }
#endif  // WIN32
        numBytes = (unsigned int)result;
        sourceAddr.SetSockAddr(*((struct sockaddr*)&sockAddr));
        if (!sourceAddr.IsValid())
        {
            PLOG(PL_ERROR, "ProtoSocket::RecvFrom() Unsupported address type!\n");
            return false;
        }
        return true;
    }
}  // end ProtoSocket::RecvFrom()

void ProtoSocket::EnableRecvDstAddr()
{
    if (!ip_recvdstaddr)
    {
        int enable = 1; 
#ifdef IP_RECVDSTADDR
        if (setsockopt(handle, IPPROTO_IP, IP_RECVDSTADDR, (char*)&enable, sizeof(enable)) < 0)
            PLOG(PL_WARN, "ProtoSocket::EnableRecvDstAddr() setsocktopt(IP_RECVDSTADDR) error: %s\n", GetErrorString());
#else
        if (setsockopt(handle, IPPROTO_IP, IP_PKTINFO, (char*)&enable, sizeof(enable)) < 0)
            PLOG(PL_WARN, "ProtoSocket::EnableRecvDstAddr() setsocktopt(IP_PKTINFO) error: %s\n", GetErrorString());
#endif // if/else IP_RECVDSTADDR        
#ifdef HAVE_IPV6
#ifdef IPV6_RECVDSTADDR
        if (setsockopt(handle, IPPROTO_IPV6, IPV6_RECVDSTADDR, (char*)&enable, sizeof(enable)) < 0)
            PLOG(PL_WARN, "ProtoSocket::EnableRecvDstAddr() setsocktopt(IPV6_RECVDSTADDR) error: %s\n", GetErrorString());
#else
        if (setsockopt(handle, IPPROTO_IPV6, IPV6_RECVPKTINFO, (char*)&enable, sizeof(enable)) < 0)
            PLOG(PL_WARN, "ProtoSocket::EnableRecvDstAddr() setsocktopt(IPV6_PKTINFO) error: %s\n", GetErrorString());
#endif // if/else IPV6_RECVDSTADDR
#endif // HAVE_IPV6
        ip_recvdstaddr = true;
    }
#ifdef WIN32
    // On Windows, you have to "load" the WsaRecvMsg() function pointer
    if (NULL == WSARecvMsg)
    {
        // On first call, we need to fetch the WSARecvMsg() function pointer
        GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
        DWORD NumberOfBytes;
        if (SOCKET_ERROR == WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, 
                                     &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID),
                                     &WSARecvMsg, sizeof WSARecvMsg,
                                     &NumberOfBytes, NULL, NULL))
        {
            PLOG(PL_ERROR, "ProtoSocket::EnableRecvDstAddr() error: WSARecvMsg() not supported on this platform!\n");
            WSARecvMsg = NULL;
	    }
    }
#endif // WIN32
}  // end ProtoSocket::EnableRecvDstAddr()

#ifndef WIN32
// Variant RecvFrom() that uses recvmsg() to get destAddr information
bool ProtoSocket::RecvFrom(char*            buffer, 
                           unsigned int&    numBytes, 
                           ProtoAddress&    sourceAddr,
                           ProtoAddress&    destAddr)
{
    if (!IsBound())
    {
        PLOG(PL_ERROR, "ProtoSocket::RecvFrom() error: socket not bound\n");
        numBytes = 0;    
    }
    if (!ip_recvdstaddr) EnableRecvDstAddr();  // should enable ahead of time to make sure you don't miss any
    
#ifdef HAVE_IPV6    
    struct sockaddr_storage sockAddr;
#else
    struct sockaddr sockAddr;
#endif  // if/else HAVE_IPV6
    socklen_t addrLen = sizeof(sockAddr);
    
    char cdata[64];
    struct msghdr msg;  // TBD - should we bzero() our "cdata" and "msg" here???
    struct iovec iov[1];
    iov[0].iov_base = buffer;
    iov[0].iov_len = numBytes;
    msg.msg_name = &sockAddr;
    msg.msg_namelen = addrLen;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cdata;
    msg.msg_controllen = 64;//sizeof(cdata);
    msg.msg_flags = 0;
    destAddr.Invalidate();
    ssize_t result = recvmsg(handle, &msg, 0);
    if (result < 0)
    {
        numBytes = 0;
#ifdef WIN32
        switch (WSAGetLastError())
        {
            case WSAEINTR:
            case WSAEWOULDBLOCK:
				//PLOG(PL_WARN, "ProtoSocket::RecvFrom() recvmsg() error: %s\n", GetErrorString());
				input_ready = false;  
                return true;
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::RecvFrom() recvmsg() error: %s\n", GetErrorString());
                break;
        }
#else
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                //PLOG(PL_WARN, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());   
                return true;            
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                break;
        }
#endif // UNIX
        return false;
    }
    else
    {
        numBytes = (unsigned int)result;
        sourceAddr.SetSockAddr(*((struct sockaddr*)&sockAddr));
        if (!sourceAddr.IsValid())
        {
            PLOG(PL_ERROR, "ProtoSocket::RecvFrom() Unsupported address type!\n");
            return false;
        }
        // Get destAddr info
        for (struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg); cmptr != NULL; cmptr = CMSG_NXTHDR(&msg, cmptr)) 
        {
            if (cmptr->cmsg_level == IPPROTO_IP)
            {
#ifdef IP_RECVDSTADDR
                if ((cmptr->cmsg_level == IPPROTO_IP) && (cmptr->cmsg_type == IP_RECVDSTADDR))
                {
                    destAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)CMSG_DATA(cmptr), 4);
                }
#else
                if (cmptr->cmsg_type == IP_PKTINFO)
                {
                    struct in_pktinfo* pktInfo = (struct in_pktinfo*)((void*)CMSG_DATA(cmptr));
                    destAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)(&pktInfo->ipi_addr), 4);
                }
#endif // if/else IP_RECVDSTADDR
            }
#ifdef HAVE_IPV6
            if (cmptr->cmsg_level == IPPROTO_IPV6)   
            {             
#ifdef IPV6_RECVDSTADDR
                if (cmptr->cmsg_type == IPV6_RECVDSTADDR)
                {
                    destAddr.SetRawHostAddress(ProtoAddress::IPv6, (char*)CMSG_DATA(cmptr), 16);
                }
#else
                if (cmptr->cmsg_type == IPV6_PKTINFO)
                {
                    struct in6_pktinfo* pktInfo = (struct in6_pktinfo*)((void*)CMSG_DATA(cmptr));
                    destAddr.SetRawHostAddress(ProtoAddress::IPv6, (char*)(&pktInfo->ipi6_addr), 16);
                }
#endif // if/else IPV6_RECVDSTADDR
            }
#endif // HAVE_IPV6    
        } 
        return true;
    }
}  // end ProtoSocket::RecvFrom(w/ destAddr)
#else
// WIN32 implementation
bool ProtoSocket::RecvFrom(char*            buffer, 
                           unsigned int&    numBytes, 
                           ProtoAddress&    sourceAddr,
                           ProtoAddress&    destAddr)
{
	if (!IsBound())
    {
        PLOG(PL_ERROR, "ProtoSocket::RecvFrom() error: socket not bound\n");
        numBytes = 0;    
    }
    if (!ip_recvdstaddr) EnableRecvDstAddr();  // should enable ahead of time to make sure you don't miss any
    destAddr.Invalidate();  // will be filled in if possible
    
    if (NULL == WSARecvMsg)
    {
        PLOG(PL_WARN, "ProtoSocket::RecvFrom() warning: WSARecvMsg() not supported\n");
        return RecvFrom(buffer, numBytes, sourceAddr);
    }
    struct sockaddr sockAddr;
    socklen_t addrLen = sizeof(sockAddr);
    
    // Buffer to receive control data (destAddr info)
    char cdata[256];
    // Buffer to receive data
    WSABUF dbuf[1];
    dbuf[0].len = numBytes;
    dbuf[0].buf = buffer;
    
    WSAMSG msg;
    msg.name = &sockAddr;
    msg.namelen = addrLen;
    msg.lpBuffers = dbuf;
    msg.dwBufferCount = 1;
    msg.Control.len = 64;
    msg.Control.buf = cdata;
    msg.dwFlags = 0;
    destAddr.Invalidate();
    
    DWORD bytesRecvd;
    if (SOCKET_ERROR == WSARecvMsg(handle, &msg, &bytesRecvd, NULL, NULL))
    {
        numBytes = 0;
        switch (WSAGetLastError())
        {
            case WSAEINTR:
            case WSAEWOULDBLOCK:
				//PLOG(PL_WARN, "ProtoSocket::RecvFrom() WSARecvMsg() error: %s\n", GetErrorString());
				input_ready = false;  
                return true;
                break;
            default:
                PLOG(PL_ERROR, "ProtoSocket::RecvFrom() WSARecvMsg() error: %s\n", GetErrorString());
                break;
        }
        return false;
    }
    else
    {
        numBytes = bytesRecvd;
        sourceAddr.SetSockAddr(*((struct sockaddr*)&sockAddr));
        if (!sourceAddr.IsValid())
        {
            PLOG(PL_ERROR, "ProtoSocket::RecvFrom() error: Unsupported address type!\n");
            return false;
        }
        // Get destAddr info
		for (WSACMSGHDR* cmptr = WSA_CMSG_FIRSTHDR(&msg); cmptr != NULL; cmptr = WSA_CMSG_NXTHDR(&msg, cmptr)) 
        {
			if (cmptr->cmsg_level == IPPROTO_IP)
            {
#ifdef IP_RECVDSTADDR
                if ((cmptr->cmsg_level == IPPROTO_IP) && (cmptr->cmsg_type == IP_RECVDSTADDR))
                {
                    destAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)WSA_CMSG_DATA(cmptr), 4);
                }
#else
                if (cmptr->cmsg_type == IP_PKTINFO)
                {
                    struct in_pktinfo* pktInfo = (struct in_pktinfo*)((void*)WSA_CMSG_DATA(cmptr));
                    destAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)(&pktInfo->ipi_addr), 4);
                }
#endif // if/else IP_RECVDSTADDR
            }
#ifdef HAVE_IPV6
            if (cmptr->cmsg_level == IPPROTO_IPV6)   
            {             
#ifdef IPV6_RECVDSTADDR
                if (cmptr->cmsg_type == IPV6_RECVDSTADDR)
                {
                    destAddr.SetRawHostAddress(ProtoAddress::IPv6, (char*)WSA_CMSG_DATA(cmptr), 16);
                }
#else
                if (cmptr->cmsg_type == IPV6_PKTINFO)
                {
                    struct in6_pktinfo* pktInfo = (struct in6_pktinfo*)((void*)WSA_CMSG_DATA(cmptr));
                    destAddr.SetRawHostAddress(ProtoAddress::IPv6, (char*)(&pktInfo->ipi6_addr), 16);
                }
#endif // if/else IPV6_RECVDSTADDR
            }
#endif // HAVE_IPV6    
        } 
        return true;
    }
}  // end ProtoSocket::RecvFrom(w/ destAddr) [WIN32]

#endif // if/else !WIN32

#ifdef HAVE_IPV6
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif // !IPV6_ADD_MEMBERSHIP
#endif // HAVE_IPV6 

/**
 * @note On WinNT 4.0 (or earlier?), we seem to need WSAJoinLeaf() for multicast to work
 * Thus NT 4.0 probably doesn't support IPv6 multicast???
 * So, here we use WSAJoinLeaf() iff the OS is NT and version 4.0 or earlier.
 */
 // Full SSM support is still work in progress (only supported on Linux and Mac OSX for moment)
bool ProtoSocket::JoinGroup(const ProtoAddress& groupAddress, 
                            const char*         interfaceName,  // optional interface name for group join
                            const ProtoAddress* sourceAddress)  // optional source address for SSM support
{
    if (!IsOpen() && !Open(0, groupAddress.GetType(), false))
    {
        PLOG(PL_ERROR, "ProtoSocket::JoinGroup() error: unable to open socket\n");
        return false;
    }    
#ifdef WIN32
    if (NULL != sourceAddress)
    {
        // WIN32 SSM support will be added in near future
        PLOG(PL_ERROR, "ProtoSocket::JoinGroup() error: Source-specific Multicast (SSM) for WIN32 not yet supported\n");
        return false;
    }
    // on WinNT 4.0 (or earlier?), we seem to need WSAJoinLeaf() for multicast to work
    // Thus NT 4.0 probably doesn't support IPv6 multicast???
    // So, here we use WSAJoinLeaf() iff the OS is NT and version 4.0 or earlier.
    bool useJoinLeaf = false;
    OSVERSIONINFO vinfo;
    vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&vinfo);
    if ((VER_PLATFORM_WIN32_NT == vinfo.dwPlatformId) &&
        ((vinfo.dwMajorVersion < 4) ||
            ((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion == 0))))
                useJoinLeaf = true;
    if (useJoinLeaf)
    {
        if (interfaceName && !SetMulticastInterface(interfaceName))
            PLOG(PL_ERROR, "ProtoSocket::JoinGroup() warning: error setting socket multicast interface\n");
        SOCKET result = WSAJoinLeaf(handle, &groupAddress.GetSockAddr(), sizeof(struct sockaddr), 
                                    NULL, NULL, NULL, NULL, JL_BOTH);
        if (INVALID_SOCKET == result)
        {
            PLOG(PL_ERROR, "WSAJoinLeaf() error: %d\n", WSAGetLastError());
            return false;
        }
        else
        {
            return true;
        }
    }  // end if (useJoinLeaf)
#endif // WIN32
    int result;
#ifdef HAVE_IPV6
    if  (ProtoAddress::IPv6 == groupAddress.GetType())
    {
        if (NULL != sourceAddress)
        {
            // IPv6 SSM support will be added in near future
            PLOG(PL_ERROR, "ProtoSocket::JoinGroup() error: Source-specific Multicast (SSM) for IPv6 not yet supported\n");
            return false;
        }
        if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr))
        {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = 
                (in_addr_t)IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr));
            if (NULL != interfaceName)
            {
                ProtoAddress interfaceAddress;
#if defined(WIN32) && (WINVER < 0x0500)
                if (interfaceAddress.ResolveFromString(interfaceName))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#else
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#endif // if/else defined(WIN32) && (WINVER < 0x0500)
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoSocket::JoinGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                mreq.imr_interface.s_addr = INADDR_ANY;  
            } 
            result = setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
        else
        {
            struct ipv6_mreq mreq;
            mreq.ipv6mr_multiaddr = ((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr;
            if (NULL != interfaceName)
                mreq.ipv6mr_interface = GetInterfaceIndex(interfaceName);               
            else
                mreq.ipv6mr_interface = 0;
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
       }
    }
    else
#endif // HAVE_IPV6
    {
        // IPv4 group join
        if (NULL == sourceAddress)
        {
            // non-SSM
        
            struct ip_mreq mreq;
#ifdef HAVE_IPV6
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddrStorage())->sin_addr;
#else
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddr())->sin_addr;
#endif  // end if/else HAVE_IPV6
            if (NULL != interfaceName)
            {
                ProtoAddress interfaceAddress;
#if defined(WIN32) && (WINVER < 0x0500)
                if (interfaceAddress.ResolveFromString(interfaceName))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#else
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#endif // if/else defined(WIN32) && (WINVER < 0x0500)
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoSocket::JoinGroup() error: invalid interface name\n");
                    return false;
                }
            }
            else
            {
                mreq.imr_interface.s_addr = INADDR_ANY;  
            }
            result = setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
        else
        {
            // SSM join   
#if defined(_PROTOSOCKET_IGMPV3_SSM) && defined(IP_ADD_SOURCE_MEMBERSHIP)
            struct ip_mreq_source mreq;
#ifdef HAVE_IPV6
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddrStorage())->sin_addr;
            mreq.imr_sourceaddr = ((struct sockaddr_in*)&sourceAddress->GetSockAddrStorage())->sin_addr;
#else
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddr())->sin_addr;
            mreq.imr_sourceaddr = ((struct sockaddr_in*)&sourceAddress->GetSockAddr())->sin_addr;
#endif // if/else HAVE_IPV6
            if (NULL != interfaceName)
            {
                ProtoAddress interfaceAddress;
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoSocket::JoinGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                mreq.imr_interface.s_addr = INADDR_ANY;  
            }
            result = setsockopt(handle, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
            if (result  < 0) 
            {
                   PLOG(PL_ERROR, "ProtoSocket::JoinGroup() setsockopt(IP_ADD_SOURCE_MEMBERSHIP) error: %s\n", strerror( errno ));
                   return false;
            }
		    return true;
#else
            PLOG(PL_ERROR, "ProtoSocket:JoinGroup() error: SSM support not included in this build\n");
            return false;
#endif  // end if/else _PROTOSOCKET_IGMPV3_SSM && IP_ADD_SOURCE_MEMBERSHIP
        }
    }
    if (result < 0)
    { 
        PLOG(PL_ERROR, "ProtoSocket:JoinGroup() setsockopt(add membership) error: %s\n", GetErrorString());
        return false;
    }  
    return true;
}  // end ProtoSocket::JoinGroup() 

bool ProtoSocket::LeaveGroup(const ProtoAddress& groupAddress,
                             const char*         interfaceName,
                             const ProtoAddress* sourceAddress)
{    
    if (!IsOpen()) return true;
    int result;
#ifdef HAVE_IPV6
    if (ProtoAddress::IPv6 == groupAddress.GetType())
    {
        // IPv6 leave
        if (NULL != sourceAddress)
        {
            // IPv6 SSM support will be added in near future
            PLOG(PL_ERROR, "ProtoSocket::LeaveGroup() error: Source-specific Multicast (SSM) for IPv6 not yet supported\n");
            return false;
        }
        if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr))
        {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = 
                (in_addr_t)IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr));
            if (interfaceName)
            {
                ProtoAddress interfaceAddress;
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoSocket::LeaveGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                mreq.imr_interface.s_addr = INADDR_ANY; 
            }
            result = setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
        else
        {
            struct ipv6_mreq mreq;
            mreq.ipv6mr_multiaddr = ((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr;
            if (interfaceName)
                mreq.ipv6mr_interface = GetInterfaceIndex(interfaceName);
            else
                mreq.ipv6mr_interface = 0;
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
    }
    else
#endif // HAVE_IPV6
    {
        // IPv4 leave
        if (NULL == sourceAddress)
        {
            // non-SSM
            struct ip_mreq mreq;
#ifdef HAVE_IPV6
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddrStorage())->sin_addr;
#else
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddr())->sin_addr;
#endif  // end if/else HAVE_IPV6
            if (NULL != interfaceName)
            {
                ProtoAddress interfaceAddress;
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, 
                                        interfaceAddress))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoSocket::LeaveGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                    mreq.imr_interface.s_addr = INADDR_ANY; 
            }
            result = setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
        else
        {
            // SSM Leave
#if defined(_PROTOSOCKET_IGMPV3_SSM) && defined(IP_DROP_SOURCE_MEMBERSHIP)
            struct ip_mreq_source   mreq;
#ifdef HAVE_IPV6
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddrStorage())->sin_addr;
            mreq.imr_sourceaddr = ((struct sockaddr_in*)&sourceAddress->GetSockAddrStorage())->sin_addr;
#else
            mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddr())->sin_addr;
            mreq.imr_sourceaddr = ((struct sockaddr_in*)&sourceAddress->GetSockAddr())->sin_addr;
#endif  // if/else HAVE_IPV6
            if (interfaceName)
            {
                ProtoAddress interfaceAddress;
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, 
                                        interfaceAddress))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    PLOG(PL_ERROR, "ProtoSocket::LeaveGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                    mreq.imr_interface.s_addr = INADDR_ANY; 
            }
            result = setsockopt(handle, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
#else
            PLOG(PL_ERROR, "ProtoSocket:LeaveGroup() error: SSM support not included in this build\n");
            return false;
#endif  // end if/else _PROTOSOCKET_IGMPV3_SSM && IP_DROP_SOURCE_MEMBERSHIP
        }
    }
    if (result < 0)
    {
        PLOG(PL_ERROR, "ProtoSocket::LeaveGroup() error leaving multicast group: %s\n", GetErrorString());
        return false;
    }
    else
    {
        return true;
    }
}  // end ProtoSocket::LeaveGroup() 

// Set MulticastTTL Function name retained for backwards compatability
// See new ProtoSocket::SetUnicastTTL
bool ProtoSocket::SetTTL(unsigned char ttl)
{   
#if defined(WIN32) && !defined(_WIN32_WCE)
    DWORD dwTTL = (DWORD)ttl; 
    DWORD dwBytesXfer;
    int optVal = ttl;
    int optLen = sizeof(int);
    if (WSAIoctl(handle, SIO_MULTICAST_SCOPE, &dwTTL, sizeof(dwTTL),
			    NULL, 0, &dwBytesXfer, NULL, NULL))

#else
    int result = 0;
#ifdef HAVE_IPV6
    if (IPv6 == domain)
    {
#ifdef MACOSX
    // v6 multicast TTL socket option must be an int
        int hops = (int) ttl;

        if (protocol != ProtoSocket::TCP)
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 
                                &hops, sizeof(hops));

        if (result == 0)
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
                                &hops, sizeof(hops));
#else

        socklen_t hops = (socklen_t) ttl;
        
        if (protocol != ProtoSocket::TCP)
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 
                                &hops, sizeof(hops));
        
        if (result == 0)
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
                                &hops, sizeof(hops));
#endif // MACOSX
    }
    else
#endif // HAVE_IPV6
    {
#ifdef _WIN32_WCE
        int hops = (int)ttl;
#else
	socklen_t hops = (socklen_t)ttl;
#endif // if/else _WIN32_WCE/UNIX
#ifdef MACOSX
	if (protocol != ProtoSocket::TCP)
	  result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, 
                            (char*)&hops, sizeof(hops));

	if (result == 0)
	  result = setsockopt(handle,IPPROTO_IP, IP_TTL,
			      (char*)&hops, sizeof(hops));

#else
	if (protocol != ProtoSocket::TCP)
	  result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, 
			      (char*)&hops, sizeof(hops));
#endif 
    }
    if (result < 0) 
#endif // if/else WIN32/UNIX | _WIN32_WCE
    { 
		
		PLOG(PL_ERROR, "ProtoSocket: setsockopt(IP_MULTICAST_TTL) error: %s\n", GetErrorString()); 
        return false;
    }
    else
    {   
        return true;
    }
}  // end ProtoSocket::SetTTL()

bool ProtoSocket::SetUnicastTTL(unsigned char ttl)
{   
#if defined(WIN32) && !defined(_WIN32_WCE)
    DWORD dwTTL = (DWORD)ttl; 
    DWORD dwBytesXfer;
    int optVal = ttl;
    int optLen = sizeof(int);
    if (setsockopt(handle,IPPROTO_IP,IP_TTL,(char*)&optVal,optLen) == SOCKET_ERROR)
	{
      PLOG(PL_ERROR, "ProtoSocket: setsockopt(IP_TTL) error: %s\n", GetErrorString()); 
	  return false;
	}
#else
    int result = 0;
#ifdef HAVE_IPV6
    if (IPv6 == domain)
    {
        socklen_t hops = (socklen_t) ttl;

#ifdef MACOSX
	result = setsockopt(handle, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			      &hops, sizeof(&hops));
#else
	result = setsockopt(handle, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			      &hops, sizeof(hops));
#endif // MACOSX
    }
    else
#endif // HAVE_IPV6
    {
#ifdef _WIN32_WCE
        int hops = (int)ttl;
#else
	socklen_t hops = (socklen_t)ttl;
#endif // if/else _WIN32_WCE/UNIX
#ifdef MACOSX
	result = setsockopt(handle,IPPROTO_IP, IP_TTL,
			    (char*)&hops, sizeof(&hops));

#else
	result = setsockopt(handle,IPPROTO_IP, IP_TTL,
			    (char*)&hops, sizeof(hops));
#endif 
    }
    if (result < 0) 
    { 
		
	PLOG(PL_ERROR, "ProtoSocket: setsockopt(IP_MULTICAST_TTL) error: %s\n", GetErrorString()); 
        return false;
    }
#endif // if/else WIN32/UNIX | _WIN32_WCE
    else
    {   
        return true;
    }
}  // end ProtoSocket::SetUnicastTTL()

bool ProtoSocket::SetTOS(UINT8 theTOS)
{ 
    if (!IsOpen())
    {
        tos = theTOS;
        return true;
    }
    if (ecn_capable)
    {
        theTOS |= ((UINT8)ECN_ECT0);  // set ECT0 bit
        theTOS &= ~((UINT8)ECN_ECT1); // clear ECT1 bit
    }
#ifdef NEVER_EVER // (for older LINUX?)                  
   int precedence = IPTOS_PREC(theTOS);
   if (setsockopt(handle, SOL_SOCKET, SO_PRIORITY, (char*)&precedence, sizeof(precedence)) < 0)           
   {     
       PLOG(PL_ERROR, "ProtoSocket: setsockopt(SO_PRIORITY) error: %s\n", GetErrorString()); 
       return false;
    }                          
   int tosBits = IPTOS_TOS(theTOS);
   if (setsockopt(handle, SOL_IP, IP_TOS, (char*)&tosBits, sizeof(tosBits)) < 0) 
#else
    int tosBits = theTOS;
    int result = -1;
#ifdef HAVE_IPV6
    if (IPv6 == domain)
    {
#ifdef IPV6_TCLASS
        result = setsockopt(handle, IPPROTO_IPV6, IPV6_TCLASS, (char*)&tosBits, sizeof(tosBits));
		if (result < 0)
#endif // IPV6_TCLASS
		{
			result = setsockopt(handle, IPPROTO_IPV6, IP_TOS, (char*)&tosBits, sizeof(tosBits));
			if (result < 0)
				PLOG(PL_ERROR,"ProtoSocket::SetTOS() Error setting IPV6 tos/tclass %s",GetErrorString());
		}
        SetFlowLabel(((UINT32)theTOS) << 20);  // set flow label "traffic class" to "tos" value
    }
    else 
#endif // HAVE_IPV6  
    {
        result =  setsockopt(handle, IPPROTO_IP, IP_TOS, (char*)&tosBits, sizeof(tosBits));    
    }
#endif  // if/else NEVER_EVER
    if (result < 0)
    {               
        PLOG(PL_ERROR, "ProtoSocket: setsockopt(IP_TOS) error\n");
        return false;
    }
    tos = theTOS;
    return true; 
}  // end ProtoSocket::SetTOS()

/**
 * (TBD) add mechanism to dither ECN ECT0/ECT1 1-bit "nonce" 
 * of RFC3168 (e.g. RFC3540).  For now we set ECT0 bit only.
 */
bool ProtoSocket::SetEcnCapable(bool state)
{
    if (state)
    {
        if (!ecn_capable)
        {
            ecn_capable = true;
            bool result = SetTOS(tos);  // will update saved "tos" value accordingly
            if (!result) ecn_capable = false;
            return result;
        }
    }
    else if (ecn_capable)
    {
        ecn_capable = false;
        bool result = SetTOS(tos);
        if (!result) ecn_capable = true;
        return result;
    }
    return true;
}  // end ProtoSocket::SetEcnCapable()

bool ProtoSocket::SetBroadcast(bool broadcast)
{
#ifdef SO_BROADCAST
#ifdef WIN32
    BOOL state = broadcast ? TRUE : FALSE;
#else
    int state = broadcast ? 1 : 0;
#endif // if/else WIN32
    if (setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char*)&state, sizeof(state)) < 0)
    {
        PLOG(PL_ERROR, "ProtoSocket::SetBroadcast(): setsockopt(SO_BROADCAST) error: %s\n",
                GetErrorString());
        return false;
    }
#endif // SO_BROADCAST
    return true;
}  // end ProtoSocket::SetBroadcast()


bool ProtoSocket::SetFragmentation(bool enable)
{
#ifdef WIN32
    DWORD df = enable ? FALSE : TRUE;
    if (setsockopt(handle, IPPROTO_IP, IP_DONTFRAGMENT, (char*)&df, sizeof(df)) < 0)
    {
        PLOG(PL_ERROR, "ProtoSocket::SetFragmentation() setsockopt(IP_DONTFRAGMENT) error: %s\n", GetErrorString());
        return false;
    }
#else  // UNIX
#if defined(IP_MTU_DISCOVER)
    // Note PMTUDISC_DONT clears DF flag while PMTUDISC_DO sets DF flag for MTU discovery
    // (thus, IP_PMTUDISC_DONT allows fragmentation to occur)
    int val = enable ? IP_PMTUDISC_DONT : IP_PMTUDISC_DO;
    if (setsockopt(handle, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
    {
#if defined(HAVE_IPV6) && defined(IPV6_DONTFRAG)
        if (IPv6 == domain)
        {
            // For IPv6 sockets, we can try "IPV6_DONTFRAG" as a backup
            // (clear DF to allow fragmenation to occur)
            int df = enable ? 0 : 1;
            if (setsockopt(handle, IPPROTO_IPV6, IPV6_DONTFRAG, &df, sizeof(df)) < 0)
            {
                PLOG(PL_ERROR, "ProtoSocket::SetFragmentation() setsockopt(IPV6_DONTFRAG) error: %s\n", GetErrorString());
                return false;
            }
        }
        else
#endif // HAVE_IPV6 && IPV6_DONTFRAG
        {
            PLOG(PL_ERROR, "ProtoSocket::SetFragmentation() setsockopt(IP_MTU_DISCOVER) error: %s\n", GetErrorString());
            return false;
        }
    }
#elif defined(IP_DONTFRAG)
    int df = enable ? 0 : 1;
    int result;
#if defined(HAVE_IPV6) && defined(IPV6_DONTFRAG)
    if (IPv6 == domain)
        result = setsockopt(handle, IPPROTO_IP, IPV6_DONTFRAG, (char*)&df, sizeof(df));
    else
#endif
        result = setsockopt(handle, IPPROTO_IP, IP_DONTFRAG, (char*)&df, sizeof(df));
    if (result < 0)
    {
        PLOG(PL_ERROR, "ProtoSocket::SetFragmentation() setsockopt(IP_DONTFRAG) error: %s\n", GetErrorString());
        return false;
    }
#else
    PLOG(PL_WARN, "ProtoSocket::SetFragmentation() warning: IP_MTU_DISCOVER or IP_DONTFRAG socket option not supported!\n");
    return false;
#endif // if/else IP_MTU_DISCOVER / IP_DONTFRAG / none
#endif // if/else WIN32 / UNIX
    return true;
}  // end ProtoSocket::SetFragmentation()

bool ProtoSocket::SetLoopback(bool loopback)
{
    ASSERT(IsOpen());
#ifdef WIN32
    DWORD dwLoop = loopback ? TRUE: FALSE;
    DWORD dwBytesXfer;
    if (WSAIoctl(handle, SIO_MULTIPOINT_LOOPBACK , &dwLoop, sizeof(dwLoop),
                 NULL, 0, &dwBytesXfer, NULL, NULL))
#else 
    int result;
    char loop = loopback ? 1 : 0;
#ifdef HAVE_IPV6
    unsigned int loop6 = loopback ? 1 : 0;  // this is needed at least for FreeBSD
    if (IPv6 == domain)
        result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, 
                            (char*)&loop6, sizeof(loop6));
    else
#endif // HAVE_IPV6
    result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_LOOP, 
                        (char*)&loop, sizeof(loop));
    if (result < 0)
#endif // if/else WIN32
    {
#ifdef WIN32
        // On NT, this the SIO_MULTIPOINT_LOOPBACK always seems to return an error
        // so we'll ignore the error and return success on NT4 and earlier
        OSVERSIONINFO vinfo;
        vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&vinfo);
        if ((VER_PLATFORM_WIN32_NT == vinfo.dwPlatformId) &&
            ((vinfo.dwMajorVersion < 4) ||
                ((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion == 0))))
                    return true;
#endif // WIN32
        PLOG(PL_ERROR, "ProtoSocket: setsockopt(IP_MULTICAST_LOOP) error: %s\n", GetErrorString());
        return false;
    } 
    else
    {
        return true;
    }
}  // end ProtoSocket::SetLoopback() 

bool ProtoSocket::SetBindInterface(const char* ifaceName)
{
#ifdef SO_BINDTODEVICE
    size_t nameSize = strlen(ifaceName) + 1;  // includes NULL termination
    if (setsockopt(handle, SOL_SOCKET, SO_BINDTODEVICE, ifaceName, nameSize) < 0)
    {
        PLOG(PL_ERROR, "ProtoSocket::SetBindInterface() error: %s\n", GetErrorString());
        return false;
    }
    return true;
#else
    // TBD - For MacOS/BSD, we can use the IP_RECVIF socket options and recvmsg() calls
    //       (under the hood) and filter incoming datagrams for a specific interface
    //       to _simulate_ the behavior of SO_BINDTODEVICE.  Not yet sure what to do
    //       here for WIN32 systems.
    
    PLOG(PL_ERROR, "ProtoSocket::SetBindInterface() error: SO_BINDTODEVICE socket option not supported!\n", GetErrorString());
    return false;
#endif  // if/else SO_BINDTODEVICE
}  // end ProtoSocket::SetBindInterface()

bool ProtoSocket::SetMulticastInterface(const char* interfaceName)
{    
    if (interfaceName)
    {
        int result;
#ifdef HAVE_IPV6
        if (IPv6 == domain)  
        {
#ifdef WIN32
            // (TBD) figure out Win32 IPv6 multicast interface
            PLOG(PL_ERROR, "ProtoSocket::SetMulticastInterface() not yet supported for IPv6 on WIN32?!\n");
            return false;
#else
            unsigned int interfaceIndex = GetInterfaceIndex(interfaceName);
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                (char*)&interfaceIndex, sizeof(interfaceIndex));
#endif // if/else WIN32
        }
        else 
#endif // HAVE_IPV6 
        {  
            struct in_addr localAddr;
            ProtoAddress interfaceAddress;
#if defined(WIN32) && (WINVER < 0x0500)
            // First check to see if "interfaceName" is the IP address on older Win32 versions
            // since there seem to be issues with iphlpapi.lib (and hence GetInterfaceAddress()) on those platforms
            if (interfaceAddress.ResolveFromString(interfaceName))
            {
                localAddr.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#else
            if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#endif // if/else WIN32 &&  (WINVER < 0x0500)
			{
                localAddr.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else
            {
                PLOG(PL_ERROR, "ProtoSocket::SetMulticastInterface() invalid interface name: %s\n", interfaceName);
                return false;
            }
            result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localAddr, 
                                sizeof(localAddr));
        }
        if (result < 0)
        { 
            PLOG(PL_ERROR, "ProtoSocket: setsockopt(IP_MULTICAST_IF) error: %s\n", GetErrorString());
            return false;
        }         
    }     
    return true;
}  // end ProtoSocket::SetMulticastInterface()

bool ProtoSocket::SetReuse(bool state)
{
#if defined(SO_REUSEADDR) || defined(SO_REUSEPORT)
    bool success = true;
#else
    return false;
#endif
    int reuse = state ? 1 : 0;
#ifdef SO_REUSEADDR
#ifdef WIN32
    BOOL reuseAddr = (BOOL)reuse;
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr)) < 0)
#else
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0)
#endif // if/else WIN32
    {
        PLOG(PL_ERROR, "ProtoSocket: setsockopt(REUSE_ADDR) error: %s\n", GetErrorString());
        success = false;
    }
#endif // SO_REUSEADDR            
            
#ifdef SO_REUSEPORT  // not defined on Linux for some reason?
#ifdef WIN32
    BOOL reusePort = (BOOL)reuse;
	if (setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, (char*)&reusePort, sizeof(reusePort)) < 0)
#else
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse, sizeof(reuse)) < 0)
#endif // if/else WIN32
    {
        PLOG(PL_ERROR, "ProtoSocket: setsockopt(SO_REUSEPORT) error: %s\n", GetErrorString());
        success = false;
    }
#endif // SO_REUSEPORT
    return success;
}  // end ProtoSocketError::SetReuse()

#ifdef HAVE_IPV6
bool ProtoSocket::HostIsIPv6Capable()
{
#ifdef WIN32
    if (!ProtoAddress::Win32Startup())
    {
        PLOG(PL_ERROR, "ProtoSocket::HostIsIPv6Capable() WSAStartup() error: %s\n", GetErrorString());
        return false;
    }
    SOCKET handle = socket(AF_INET6, SOCK_DGRAM, 0);
    closesocket(handle);
	ProtoAddress::Win32Cleanup();
    if(INVALID_SOCKET == handle)
        return false;
    else
        return true;
#else
    if (IPV6_UNKNOWN == ipv6_support_status)
    {
        ProtoAddressList addrList;
        ProtoNet::GetHostAddressList(ProtoAddress::IPv6, addrList);
        if (addrList.IsEmpty())
        {
            ipv6_support_status = IPV6_UNSUPPORTED;
        }
        else
        {
            ipv6_support_status = IPV6_SUPPORTED;
        }
    }
    return (IPV6_SUPPORTED == ipv6_support_status);
#endif  // if/else WIN32
}  // end ProtoSocket::HostIsIPv6Capable()

#endif // HAVE_IPV6


bool ProtoSocket::SetTxBufferSize(unsigned int bufferSize)
{
   if (!IsOpen())
   {
        PLOG(PL_ERROR, "ProtoSocket::SetTxBufferSize() error: socket closed\n");     
        return false;
   }
   unsigned int oldBufferSize = GetTxBufferSize();
   if (setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char*)&bufferSize, sizeof(bufferSize)) < 0) 
   {
        setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char*)&oldBufferSize, sizeof(oldBufferSize));
        PLOG(PL_ERROR, "ProtoSocket::SetTxBufferSize() setsockopt(SO_SNDBUF) error: %s\n",
                GetErrorString());
        return false;
   }
   return true;
}  // end ProtoSocket::SetTxBufferSize()

unsigned int ProtoSocket::GetTxBufferSize()
{
    if (!IsOpen()) return 0;
    unsigned int txBufferSize = 0;
    socklen_t len = sizeof(txBufferSize);
    if (getsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char*)&txBufferSize, &len) < 0) 
    {
        PLOG(PL_ERROR, "ProtoSocket::GetTxBufferSize() getsockopt(SO_SNDBUF) error: %s\n",
                 GetErrorString());
	    return 0; 
    }
    return ((unsigned int)txBufferSize);
}  // end ProtoSocket::GetTxBufferSize()

bool ProtoSocket::SetRxBufferSize(unsigned int bufferSize)
{
   if (!IsOpen())
   {
        PLOG(PL_ERROR, "ProtoSocket::SetRxBufferSize() error: socket closed\n");    
        return false;
   }
   unsigned int oldBufferSize = GetRxBufferSize();
   if (setsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char*)&bufferSize, sizeof(bufferSize)) < 0) 
   {
        setsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char*)&oldBufferSize, sizeof(oldBufferSize));
        PLOG(PL_ERROR, "ProtoSocket::SetRxBufferSize() setsockopt(SO_RCVBUF) error: %s\n",
                GetErrorString());
        return false;
   }
   return true;
}  // end ProtoSocket::SetRxBufferSize()

unsigned int ProtoSocket::GetRxBufferSize()
{
    if (!IsOpen()) return 0;
    unsigned int rxBufferSize = 0;
    socklen_t len = sizeof(rxBufferSize);
    if (getsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char*)&rxBufferSize, &len) < 0) 
    {
        PLOG(PL_ERROR, "ProtoSocket::GetRxBufferSize() getsockopt(SO_RCVBUF) error: %s\n",
                GetErrorString());
	    return 0; 
    }
    return ((unsigned int)rxBufferSize);
}  // end ProtoSocket::GetRxBufferSize()

#ifdef HAVE_IPV6
bool ProtoSocket::SetFlowLabel(UINT32 label)  
{
    int result = 0; 
#ifdef SOL_IPV6
    if (label && !flow_label)
    {
       int on = 1;
       result = setsockopt(handle, SOL_IPV6, IPV6_FLOWINFO_SEND, (void*)&on, sizeof(on));
    }
    else if (!label && flow_label)
    {
        int off = 0;
        result = setsockopt(handle, SOL_IPV6, IPV6_FLOWINFO_SEND, (void*)&off, sizeof(off));
    }
#endif  // SOL_IPV6
    if (0 == result)
    {
        if (ecn_capable)
        {
            label |= (((UINT32)ECN_ECT0) << 20);    // set ECN_ECT0 bit
            label &= ~(((UINT32)ECN_ECT1) << 20);   // clear ECN_ECT1 bit
        }
        flow_label = htonl(label);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoSocket::SetFlowLabel() setsockopt(SOL_IPV6) error\n");
        return false;
    }    
}  // end ProtoSocket::SetFlowLabel()
#endif  //HAVE_IPV6



ProtoSocket::List::List()
 : head(NULL)
{
}

ProtoSocket::List::~List()
{
    Destroy();
}

void ProtoSocket::List::Destroy()
{
    Item* next = head;
    while (next)
    {
        Item* current = next;
        next = next->GetNext();
        delete current->GetSocket();
        delete current;
    }   
    head = NULL;
}  // end ProtoSocket::List::Destroy()

bool ProtoSocket::List::AddSocket(ProtoSocket& theSocket)
{
    Item* item = new Item(&theSocket);
    if (item)
    {
        item->SetPrev(NULL);
        item->SetNext(head);
        head = item;
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoSocket::List::AddSocket() new Item error: %s\n", GetErrorString());
        return false;
    }
}  // end ProtoSocket::List::AddSocket()

void ProtoSocket::List::RemoveSocket(ProtoSocket& theSocket)
{
    Item* item = head;
    while (item)
    {
        if (&theSocket == item->GetSocket())
        {
            Item* prev = item->GetPrev();
            Item* next = item->GetNext();
            if (prev) 
                prev->SetNext(next);
            else
                head = next;
            if (next) next->SetPrev(prev);
            delete item;
            break;
        }
        item = item->GetNext();   
    }
}  // end ProtoSocket::List::RemoveSocket()

ProtoSocket::List::Item* ProtoSocket::List::FindItem(const ProtoSocket& theSocket) const
{
    Item* item = head;
    while (item)
    {
        if (&theSocket == item->GetSocket())
            return item;
        else
            item = item->GetNext(); 
    }  
    return NULL; 
}  // end ProtoSocket::List::FindItem()

ProtoSocket::List::Item::Item(ProtoSocket* theSocket)
 : socket(theSocket), prev(NULL), next(NULL)
{
}

ProtoSocket::List::Iterator::Iterator(const ProtoSocket::List& theList)
 : next(theList.head)
{
}


////////////////////////////////////////////////////////////////////
// These are some network "helper" functions that get information
// about the system network devices (interfaces), addresses, etc
//
// Note that the "ProtoSocket" implementation of this is being replaced
// by implementation in the "ProtoNet" namespace (see "protoNet.h") and
// the implementations here will eventually be removed _and_ the ProtoSocket
// static method declarations themselves will be eventually deprecated

#include "protoNet.h"

bool ProtoSocket::GetHostAddressList(ProtoAddress::Type  addrType,
				     ProtoAddressList&   addrList)
{
	return ProtoNet::GetHostAddressList(addrType, addrList);
}  // end ProtoSocket::GetHostAddressList()


bool ProtoSocket::GetInterfaceAddress(const char*         ifName, 
				                      ProtoAddress::Type  addrType,
				                      ProtoAddress&       theAddress,
                                      unsigned int*       ifIndex)
{
	return ProtoNet::GetInterfaceAddress(ifName, addrType, theAddress, ifIndex);
}  // end ProtoSocket::GetInterfaceAddress()

bool ProtoSocket::GetInterfaceAddressList(const char*         interfaceName,
                                          ProtoAddress::Type  addressType,
                                          ProtoAddressList&   addrList,
                                          unsigned int*       interfaceIndex)
                                          
{
    return ProtoNet::GetInterfaceAddressList(interfaceName, addressType, addrList, interfaceIndex);
}  // end ProtoSocket::GetInterfaceAddressList()


bool ProtoSocket::FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress)
{
	return ProtoNet::FindLocalAddress(addrType, theAddress);
}  // end ProtoSocket::FindLocalAddress()

unsigned int ProtoSocket::GetInterfaceIndex(const char* interfaceName)
{
    return ProtoNet::GetInterfaceIndex(interfaceName);
}  // end ProtoSocket::GetInterfaceIndex()

unsigned int ProtoSocket::GetInterfaceIndices(unsigned int* indexArray, unsigned int indexArraySize)
{
    return ProtoNet::GetInterfaceIndices(indexArray, indexArraySize);
}  // end ProtoSocket::GetInterfaceIndices()


bool ProtoSocket::GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen)
{
    return (0 != ProtoNet::GetInterfaceName(index, buffer, buflen));
}  // end ProtoSocket::GetInterfaceName(by index)


bool ProtoSocket::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{  
    return (0 != ProtoNet::GetInterfaceName(ifAddr, buffer, buflen));
}  // end ProtoSocket::GetInterfaceName(by address)

