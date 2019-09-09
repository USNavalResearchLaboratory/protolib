
// This file contains BSD (and MacOS) specific implementations of 
// ProtoNet features.

// Note that the remainder of the Linux ProtoNet stuff is
// implemented in the "src/unix/unixNet.cpp" file
// in the Protolib source tree and the common stuff is
// in "src/common/protoNet.cpp"

#include "protoNet.h"
#include "protoDebug.h"

#include <unistd.h>  // for close()
#include <stdio.h>   // for sprintf()
#include <sys/socket.h>  // for socket()
#include <sys/kern_event.h>  // // for kernel event stuff
#include <sys/ioctl.h>  // for ioctl()
#include <net/if.h>  // for KEV_DL_SUBCLASS, etc
#include <netinet/in.h>      // for KEV_INET_SUBCLASS, etc
#include <netinet/in_var.h>  // for KEV_INET_SUBCLASS, etc
#include <netinet6/in6_var.h>  // for KEV_INET6_SUBCLASS, etc
 

class BsdNetMonitor : public ProtoNet::Monitor
{
    public:
        BsdNetMonitor();
        ~BsdNetMonitor();
        
        bool Open();
        void Close();
        bool GetNextEvent(Event& theEvent);
        
    private:
        u_int32_t*  msg_buffer;
        u_int32_t   msg_buffer_size;
            
};  // end class BsdNetMonitor


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
    return true;
}  // end BsdNetMonitor::Open()

void BsdNetMonitor::Close()
{
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
            // If it was a supported event, fill in ifaceIndex
            //if (Event::UNKNOWN_EVENT != theEvent.GetType())
            {
                
                char ifName[IFNAMSIZ];
                sprintf(ifName, "%s%d", dat->if_name, dat->if_unit);
                unsigned int ifIndex = ProtoNet::GetInterfaceIndex(ifName);
                if (0 == ifIndex)
                {
                    PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() unable to get index for iface \"%s\"\n", ifName);
                    return false;
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
                
                char ifName[IFNAMSIZ];
                sprintf(ifName, "%s%d", dat->link_data.if_name, dat->link_data.if_unit);
                unsigned int ifIndex = ProtoNet::GetInterfaceIndex(ifName);
                if (0 == ifIndex)
                {
                    PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() unable to get index for iface \"%s\"\n", ifName);
                    return false;
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
                
                char ifName[IFNAMSIZ];
                sprintf(ifName, "%s%d", dat->link_data.if_name, dat->link_data.if_unit);
                unsigned int ifIndex = ProtoNet::GetInterfaceIndex(ifName);
                if (0 == ifIndex)
                {
                    PLOG(PL_ERROR, "BsdNetMonitor::GetNextEvent() unable to get index for iface \"%s\"\n", ifName);
                    return false;
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
