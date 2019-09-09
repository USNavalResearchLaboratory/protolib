
// This file contains BSD (and MacOS) specific implementations of 
// ProtoNet features.

// Note that the remainder of the ProtoNet stuff is
// implemented in the "src/unix/unixNet.cpp" file
// in the Protolib source tree and the common stuff is
// in "src/common/protoNet.cpp"

#include "protoNet.h"
#include "protoDebug.h"
#include "protoTree.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <string.h>
#include <net/if_dl.h>

#include <unistd.h>  // for close()
#include <stdio.h>   // for sprintf()
#ifdef MACOSX
#include <sys/kern_event.h>  // // for kernel event stuff
#endif
#include <sys/ioctl.h>  // for ioctl()
#include <net/if.h>  // for KEV_DL_SUBCLASS, etc
#include <net/if_var.h> // needed for freebsd
#include <netinet/in.h>      // for KEV_INET_SUBCLASS, etc
#include <netinet/in_var.h>  // for KEV_INET_SUBCLASS, etc
#include <netinet6/in6_var.h>  // for KEV_INET6_SUBCLASS, etc
 
bool ProtoNet::GetGroupMemberships(const char* ifaceName, ProtoAddress::Type addrType, ProtoAddressList& addrList)
{
    int family;
    switch (addrType)
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
    // use getifmaddrs() to fetch memberships for given interface
    struct ifmaddrs* ifmap;
    if (0 == getifmaddrs(&ifmap))
    {
        // Look for addrType address for given "interfaceName"
        struct ifmaddrs* ptr = ifmap;
        while (NULL != ptr)
        {
            if ((NULL != ptr->ifma_name) && 
                (NULL != ptr->ifma_addr) && 
                (family == ptr->ifma_addr->sa_family))
            {
                if (ptr->ifma_name->sa_family != AF_LINK) 
                {
                    PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: invalid interface data from kernel!\n");
                    freeifmaddrs(ifmap);
                    return false;
                }
                // This union hack avoids cast-align warning
                typedef struct SockAddrPtr 
                {
                    union
                    {
                        struct sockaddr*    sa;
                        struct sockaddr_dl* sd;
                    };
                } SockAddrPtr;
                SockAddrPtr sap;
                sap.sa = ptr->ifma_name;
                char ifName[64];
                unsigned int nameLen = sap.sd->sdl_nlen;
                if (nameLen > 63) nameLen = 63;
                strncpy(ifName, sap.sd->sdl_data, nameLen);
                ifName[nameLen] = '\0';
                if (0 == strcmp(ifaceName, ifName))
                {
                    ProtoAddress groupAddr;
                    if (!groupAddr.SetSockAddr(*(ptr->ifma_addr)))
                    {
                        PLOG(PL_WARN, "ProtoNet::GetGroupMemberships() error: invalid address family\n");
                        ptr = ptr->ifma_next;
                        continue;
                    }
                    if (!addrList.Insert(groupAddr))
                    {
                        PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() error: unable to add address to list!\n");
                        freeifmaddrs(ifmap);
                        return false;
                    }
                }
            }
            ptr = ptr->ifma_next;
        }
        if (NULL != ifmap)
            freeifmaddrs(ifmap);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoNet::GetGroupMemberships() getifmaddrs() error: %s\n", GetErrorString());
        return false;
    }
}   // end ProtoNet::GetGroupMemberships()

// IMPORTANT - The BsdNetMonitor implementation here is currently
//             specific to Mac OSX.  TBD - Provide implementation that
//             works on other BSD platforms, too.
#ifdef MACOSX

class BsdNetMonitor : public ProtoNet::Monitor
{
    public:
        BsdNetMonitor();
        ~BsdNetMonitor();
        
        bool Open();
        void Close();
        bool GetNextEvent(Event& theEvent);
        
    private:
        // We keep a list of "up" interfaces that is populated upon "Open()"  
        // and modified as interfaces go up and down  
            
            
        class Interface : public ProtoTree::Item
        {
            public:
                Interface(const char* name, unsigned int index);
                ~Interface();
                
                const char* GetName() const
                    {return iface_name;}
                unsigned int GetIndex() const
                    {return iface_index;}
                void SetIndex(unsigned int index)
                    {iface_index = index;}
                
            private:
                const char* GetKey() const
                    {return iface_name;}   
                unsigned int GetKeysize() const
                    {return iface_name_bits;} 
                    
                char            iface_name[IFNAMSIZ+1];
                unsigned int    iface_name_bits;
                unsigned int    iface_index;
        };  // end class BsdNetMonitor::Interface
        class InterfaceList : public ProtoTreeTemplate<Interface> {};
            
        InterfaceList   iface_list;
        u_int32_t*      msg_buffer;
        u_int32_t       msg_buffer_size;
            
};  // end class BsdNetMonitor

BsdNetMonitor::Interface::Interface(const char* name, unsigned int index)
 : iface_index(index)
{
    iface_name[IFNAMSIZ] = '\0';
    strncpy(iface_name, name, IFNAMSIZ);
    iface_name_bits = (unsigned int)strlen(iface_name) << 3;
}

BsdNetMonitor::Interface::~Interface()
{
}


// This is the implementation of the ProtoNet::Monitor::Create()
// static method (our BSD-specific factory)
ProtoNet::Monitor* ProtoNet::Monitor::Create()
{
    return static_cast<ProtoNet::Monitor*>(new BsdNetMonitor);
}  // end ProtoNet::Monitor::Create()
        
BsdNetMonitor::BsdNetMonitor()
 : msg_buffer(NULL), msg_buffer_size(0)
{
}

BsdNetMonitor::~BsdNetMonitor()
{
    Close();
}

bool BsdNetMonitor::Open()
{
    if (IsOpen()) Close();
    if (0 > (descriptor = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT)))
    {
        PLOG(PL_ERROR, "BsdNetMonitor::Open() socket() error: %s\n", GetErrorString());
        return false;
    }
    // NOTE: This stuff here is currently MACOS only!!!
    // (TBD - implement BSD equivalent)
    // Call ioctl(SIOCSKEVFILT) to monitor for network events
    struct kev_request req;
    req.vendor_code = KEV_VENDOR_APPLE;
    req.kev_class = KEV_NETWORK_CLASS;
    req.kev_subclass = KEV_ANY_SUBCLASS; //KEV_DL_SUBCLASS;
    int result = ioctl(descriptor, SIOCSKEVFILT, (void*)&req);
    if (0 != result)
    {
        PLOG(PL_ERROR, "BsdNetMonitor::Open() ioctl(SIOSKEVFILT) error");
        Close();
        return false;
    }
    if (!ProtoNet::Monitor::Open())
    {
        Close();
        return false;
    }
    
    // Populate our iface_list with "up" interfaces
    unsigned int ifaceCount = ProtoNet::GetInterfaceCount();
    if (ifaceCount > 0)
    {
        unsigned int* indexArray = new unsigned int[ifaceCount];
        if (NULL == indexArray)
        {
            PLOG(PL_ERROR, "BsdNetMonitor::Open() new indexArray[%u] error: %s\n", ifaceCount, GetErrorString());
            Close();
            return false;
        }
        if (ProtoNet::GetInterfaceIndices(indexArray, ifaceCount) != ifaceCount)
        {
            PLOG(PL_ERROR, "BsdNetMonitor::Open() GetInterfaceIndices() error?!\n");
            delete[] indexArray;
            Close();
            return false;
        }
        for (unsigned int i = 0; i < ifaceCount; i++)
        {
            char ifaceName[IFNAMSIZ+1];
            ifaceName[IFNAMSIZ] = '\0';
            if (!ProtoNet::GetInterfaceName(indexArray[i], ifaceName, IFNAMSIZ))
            {
                PLOG(PL_ERROR, "BsdNetMonitor::Open() error: unable to get interface name for index: %u\n", indexArray[i]);
                delete[] indexArray;
                Close();
                return false;
            }
            Interface* iface = iface_list.FindString(ifaceName);
            if (NULL != iface)
            {
                PLOG(PL_ERROR, "BsdNetMonitor::Open() warning: interface \"%s\" index:%u already listed as index:%u\n",
                                ifaceName, indexArray[i], iface->GetIndex());
            }
            else if (NULL == (iface = new Interface(ifaceName, indexArray[i])))
            {
                PLOG(PL_ERROR, "BsdNetMonitor::Open() new Interface[ error: %s\n", GetErrorString());
                Close();
                return false;
            }
            iface_list.Insert(*iface);
        }
    }
    return true;
}  // end BsdNetMonitor::Open()

void BsdNetMonitor::Close()
{
    iface_list.Destroy();
    if (IsOpen())
    {
        ProtoNet::Monitor::Close();
        close(descriptor);
        descriptor = INVALID_HANDLE;
    }
    if (NULL != msg_buffer)
    {
        delete[] msg_buffer;
        msg_buffer_size = 0;
        msg_buffer = NULL;
    }
}  // end BsdNetMonitor::Open()

bool BsdNetMonitor::GetNextEvent(Event& theEvent)
{
    // 0) Initialize event instance
    theEvent.SetType(Event::UNKNOWN_EVENT);
    theEvent.SetInterfaceIndex(0);
    theEvent.AccessAddress().Invalidate();
    
    // 1) "peek" at the kern_event msg header to see how
    // big the message data buffer needs to be.
    struct kern_event_msg tmpMsg;
    ssize_t result = recv(descriptor, (void*)&tmpMsg, sizeof(tmpMsg), MSG_PEEK);
    if (result < 0)
    {
        switch(errno)
        {
            case EINTR:
            case EAGAIN:
                theEvent.SetType(Event::NULL_EVENT);
                return true;
            default:
                PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() recv(PEEK) error: %s", GetErrorString());
                return false;
        }
    }
    // 2) alloc/realloc "msgBuffer" as needed and "recv()" message
    if (tmpMsg.total_size > msg_buffer_size)
    {
        if (NULL != msg_buffer) delete[] msg_buffer;
        if (NULL == (msg_buffer = new u_int32_t[tmpMsg.total_size / sizeof(u_int32_t)]))
        {
            PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent()  new msg_buffer[tmpMsg.total_size / sizeof(u_int32_t)] error: %s\n",
                    tmpMsg.total_size / sizeof(u_int32_t), GetErrorString()); 
            msg_buffer_size = 0;
            return false;
        }
        msg_buffer_size = tmpMsg.total_size;
    }
    result = recv(descriptor, (void*)msg_buffer, msg_buffer_size, 0);
    if (result < 0)
    {
        switch(errno)
        {
            case EINTR:
            case EAGAIN:
                theEvent.SetType(Event::NULL_EVENT);
                return true;
            default:
                PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() recv() error: %s", GetErrorString());
                return false;
        }
    }
    // 3) Parse the received kernel event message to see what happened
    struct kern_event_msg* kmsg = (struct kern_event_msg*)msg_buffer;
    switch (kmsg->kev_subclass)
    {
        case KEV_DL_SUBCLASS:
        {
            // The events we handle use "struct net_event_data" for the "event_data" portion
            struct net_event_data* dat = (struct net_event_data*)kmsg->event_data;
            switch (kmsg->event_code)
            {
                case KEV_DL_LINK_ON:
                    theEvent.SetType(Event::IFACE_UP);
                    break;
                case KEV_DL_LINK_OFF:
                    theEvent.SetType(Event::IFACE_DOWN);
                    break;
                case KEV_DL_SIFFLAGS:  // iface flags have changed (addr assigned, etc?)
                    theEvent.SetType(Event::IFACE_STATE);   
                    break;
                default:
                    PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() warning: unhandled iface network event:%d\n", kmsg->event_code);
                    break;
            }
            // If it was a supported event, fill in ifaceIndex and ifaceName
            //if (Event::UNKNOWN_EVENT != theEvent.GetType())
            {
                // Do we already know this interface?
                char ifName[IFNAMSIZ+1];
                ifName[IFNAMSIZ] = '\0';
                sprintf(ifName, "%s%d", dat->if_name, dat->if_unit);
                theEvent.SetInterfaceName(ifName);
                Interface* iface =iface_list.FindString(ifName);
                // Fetch the system index in case it's not consistent
                unsigned int ifIndex = ProtoNet::GetInterfaceIndex(ifName);
                if (0 == ifIndex)
                {
                    if (NULL != iface)
                    {
                        // If a known iface has lost its index, assume it's gone DOWN?
                        if (Event::IFACE_DOWN != theEvent.GetType())
                        {
                            PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() known interface has lost it's index (assuming IFACE_DOWN)\n");
                            theEvent.SetType(Event::IFACE_DOWN);
                        }
                        ifIndex = iface->GetIndex();
                    }
                    else
                    {
                        // Unknown interface with no index, so ignore
                        PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() unable to get index for iface \"%s\"\n", ifName);
                        return GetNextEvent(theEvent);
                    }
                }
                else
                {
                    if (NULL != iface)
                    {
                        if (ifIndex != iface->GetIndex())
                        {
                            PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() warning: index changed for known interface \"%s\"\n",ifName);
                            iface->SetIndex(ifIndex);
                        }
                    }
                    else if (Event::IFACE_DOWN != theEvent.GetType() && (ProtoNet::IFACE_UP == ProtoNet::GetInterfaceStatus(ifName)))
                    {
                        if (NULL == (iface = new Interface(ifName, ifIndex)))
                        {
                            PLOG(PL_ERROR, "BsdNetMonitor::Open() new Interface[ error: %s\n", GetErrorString());
                        }
                        else
                        {
                            if (Event::IFACE_UP != theEvent.GetType())
                            {
                                PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() event for unknown interface (assuming IFACE_UP)\n");
                                theEvent.SetType(Event::IFACE_UP);
                            }
                            TRACE("Inserting iface \"%s\"\n", ifName);
                            iface_list.Insert(*iface);
                        }
                    }
                }             
                if (Event::IFACE_STATE == theEvent.GetType())
                {
                    // See if state changed from UP to DOWN or vice versa
                    if (ProtoNet::IFACE_UP != ProtoNet::GetInterfaceStatus(ifName) && (NULL != iface))
                    {
                        PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() status event for known, but DOWN, interface (assuming IFACE_DOWN)\n");
                        theEvent.SetType(Event::IFACE_DOWN);
                    }
                }
                
                if ((Event::IFACE_DOWN == theEvent.GetType()) && (NULL != iface))
                {
                    // Remove "DOWN"" iface from iface_list
                    iface_list.Remove(*iface);
                    delete iface;
                }       
                theEvent.SetInterfaceIndex(ifIndex);
            }
            break;
        }  // end case KEV_DL_SUBCLASS
        case KEV_INET_SUBCLASS:
        {
            // The events we handle use "struct kev_in_data" for the "event_data" portion
            struct kev_in_data* dat = (struct kev_in_data*)kmsg->event_data;
            switch (kmsg->event_code)
            {
                case KEV_INET_NEW_ADDR:
                //case KEV_INET_CHANGED_ADDR:
                    theEvent.SetType(Event::IFACE_ADDR_NEW);
                    break;
                case KEV_INET_ADDR_DELETED:
                    theEvent.SetType(Event::IFACE_ADDR_DELETE);
                    break;
                default:
                    PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() warning: unhandled ipv4 network event:%d\n", kmsg->event_code);
                    break;
            }
            // If it was a supported event, fill in ifaceIndex and ifaceAddress
            //if (Event::UNKNOWN_EVENT != theEvent.GetType())
            {
                
                char ifName[IFNAMSIZ+1];
                ifName[IFNAMSIZ] = '\0';
                sprintf(ifName, "%s%d", dat->link_data.if_name, dat->link_data.if_unit);
                theEvent.SetInterfaceName(ifName);
                unsigned int ifIndex = ProtoNet::GetInterfaceIndex(ifName);
                if (0 == ifIndex)
                {
                    PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() unable to get index for ip4 iface \"%s\"\n", ifName);
                    return GetNextEvent(theEvent);
                }                    
                theEvent.SetInterfaceIndex(ifIndex);
                theEvent.AccessAddress().SetRawHostAddress(ProtoAddress::IPv4, (char*)&(dat->ia_addr), 4);
                // TBD - for consistency with Linux, etc we may want to ignore Link Local addr new/delete events???
            }
            break;
        }  // end KEV_INET_SUBCLASS
        case KEV_INET6_SUBCLASS:
        {
            // The events we handle use "struct kev_in6_data" for the "event_data" portion
            struct kev_in6_data* dat = (struct kev_in6_data*)kmsg->event_data;
            switch (kmsg->event_code)
            {
                case KEV_INET6_NEW_USER_ADDR:
                case KEV_INET6_NEW_LL_ADDR:
                case KEV_INET6_NEW_RTADV_ADDR:
                case KEV_INET6_CHANGED_ADDR:
                    theEvent.SetType(Event::IFACE_ADDR_NEW);
                    break;
                case KEV_INET6_ADDR_DELETED:
                    theEvent.SetType(Event::IFACE_ADDR_DELETE);
                    break;
                default:
                    PLOG(PL_INFO, "BsdNetMonitor::GetNextEvent() warning: unhandled ipv6 network event:%d\n", kmsg->event_code);
                    break;
            }
            // If it was a supported event, fill in ifaceIndex and ifaceAddress
            //if (Event::UNKNOWN_EVENT != theEvent.GetType())
            {
                
                char ifName[IFNAMSIZ+1];
                ifName[IFNAMSIZ] = '\0';
                sprintf(ifName, "%s%d", dat->link_data.if_name, dat->link_data.if_unit);
                theEvent.SetInterfaceName(ifName);
                unsigned int ifIndex = ProtoNet::GetInterfaceIndex(ifName);
                if (0 == ifIndex)
                {
                    PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() unable to get index for ip6 iface \"%s\"\n", ifName);
                    return GetNextEvent(theEvent);
                }                    
                theEvent.SetInterfaceIndex(ifIndex);
                theEvent.AccessAddress().SetRawHostAddress(ProtoAddress::IPv6, (char*)&(dat->ia_addr), 16);
                // TBD - for consistency with Linux, etc we may want to ignore Link Local addr new/delete events???
            }
            break;
        }
        default:
            printf("unhandled network event subclass:%d code:%d\n", kmsg->kev_subclass, kmsg->event_code);
            break;
    }
    return true;
} // end BsdNetMonitor::GetNextEvent()

#endif // MACOSX (TBD - make this implementation work on other BSD systems, too)
