
#include "protoNet.h"
#include "protoList.h"
#include "protoDebug.h"

#include <stdio.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <unistd.h>  // for getpid()

// Note that the remainder of the Linux ProtoNet stuff is
// implemented in the "src/unix/unixNet.cpp" file
// in the Protolib source tree and the common stuff is
// in "src/common/protoNet.cpp"

class LinuxNetMonitor : public ProtoNet::Monitor
{
    public:
        LinuxNetMonitor();
        ~LinuxNetMonitor();
        
        bool Open();
        void Close();
        bool GetNextEvent(Event& theEvent);
        
    private:
        // Since a Linux netlink message may have multiple 
        // network interface events, we cache them in a linked
        // list for retrieval by the GetNextEvent() method    
        class EventItem : public Event, public ProtoList::Item
        {
            public:
                EventItem();
                ~EventItem();
        };  // end class LinuxNetMonitor::EventItem         
            
        class EventList : public ProtoListTemplate<EventItem> {};   
            
        EventList   event_list;
        EventList   event_pool;
            
};  // end class LinuxNetMonitor


// This is the implementation of the ProtoNet::Monitor::Create()
// static method (our Linux-specific factory)
ProtoNet::Monitor* ProtoNet::Monitor::Create()
{
    return static_cast<ProtoNet::Monitor*>(new LinuxNetMonitor);
}  // end ProtoNet::Monitor::Create()
        
LinuxNetMonitor::LinuxNetMonitor()
{
}

LinuxNetMonitor::~LinuxNetMonitor()
{
}

bool LinuxNetMonitor::Open()
{
    if (IsOpen()) Close();
    if (0 > (descriptor = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)))
    {
        PLOG(PL_ERROR, "LinuxNetMonitor::Open() socket() error: %s\n", 
                GetErrorString());
        return false;
    }    
    
    // Send a netlink request message to subscribe to the
    // RTMGRP_IPV4_IFADDR and RTMGRP_IPV6_IFADDR groups for 
    // network interface status update messages
    struct sockaddr_nl localAddr;
    localAddr.nl_family = AF_NETLINK;
	localAddr.nl_pid = getpid();
	localAddr.nl_groups |= RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;// | RTMGRP_IPV6_IFINFO;
	if (0 > bind(descriptor, (struct sockaddr*) &localAddr, sizeof(localAddr)))
    {
        PLOG(PL_ERROR, "LinuxNetMonitor::Open() bind() error: %s\n", 
                GetErrorString());
        Close();
        return false;
    }
    
    if (!ProtoNet::Monitor::Open())
    {
        Close();
        return false;
    }
    return true;
}  // end LinuxNetMonitor::Open()

void LinuxNetMonitor::Close()
{
    if (IsOpen())
    {
        ProtoNet::Monitor::Close();
        close(descriptor);
        descriptor = INVALID_HANDLE;
    }
    event_list.Destroy();
    event_pool.Destroy();
}  // end LinuxNetMonitor::Close()

bool LinuxNetMonitor::GetNextEvent(Event& theEvent)
{
    // 0) Initialize event instance
    theEvent.SetType(Event::UNKNOWN_EVENT);
    theEvent.SetInterfaceIndex(0);
    theEvent.AccessAddress().Invalidate();
    
    // 1) Get next event from list or recv() from netlink
    EventItem* eventItem = event_list.RemoveHead();
    if (NULL == eventItem)
    {
        // There was not any existing events in our list, so
        // get more from netlink
        char buffer[4096];
        struct nlmsghdr* nlh = (struct nlmsghdr*)buffer;
        ssize_t result = recv(descriptor, buffer, 4096, 0);
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
        else if (0 == result)
        {
            theEvent.SetType(Event::NULL_EVENT);
            return true;
        }
        unsigned int msgLen = (unsigned int)result;
        for (;(NLMSG_OK(nlh, msgLen)) && (nlh->nlmsg_type != NLMSG_DONE); nlh = NLMSG_NEXT(nlh, msgLen))
        {
            switch (nlh->nlmsg_type)
            {
                case RTM_NEWLINK:
                case RTM_DELLINK:
                {
                    eventItem = event_pool.RemoveHead();
                    if (NULL == eventItem) eventItem = new EventItem();
                    if (NULL == eventItem)
                    {
                        PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new EventItem error: %s\n", GetErrorString());
                        theEvent.SetType(Event::NULL_EVENT);
                        return false;
                    }
                    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(nlh);
                    if (RTM_NEWLINK == nlh->nlmsg_type)
                        eventItem->SetType(Event::IFACE_UP);
                    else
                        eventItem->SetType(Event::IFACE_DOWN);
                    eventItem->SetInterfaceIndex(ifi->ifi_index);
                    event_list.Append(*eventItem);
                    break;
                }
                case RTM_NEWADDR:
                case RTM_DELADDR:
                {
                    struct ifaddrmsg* ifa = (struct ifaddrmsg*) NLMSG_DATA(nlh);
                    // For now, we only look for IPv4 or Ipv6 addresses (TBD - other addr families?)
                    if ((AF_INET != ifa->ifa_family) && (AF_INET6 != ifa->ifa_family))  continue;
                    struct rtattr* rth = IFA_RTA(ifa);
                    int rtl = IFA_PAYLOAD(nlh);
                    for (;rtl && RTA_OK(rth, rtl); rth = RTA_NEXT(rth,rtl))
                    {
                        if (IFA_LOCAL == rth->rta_type)
                        {
                            eventItem = event_pool.RemoveHead();
                            if (NULL == eventItem) eventItem = new EventItem();
                            if (NULL == eventItem)
                            {
                                PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new EventItem error: %s\n", GetErrorString());
                                theEvent.SetType(Event::NULL_EVENT);
                                return false;
                            }
                            if (RTM_NEWADDR == nlh->nlmsg_type)
                                eventItem->SetType(Event::IFACE_ADDR_NEW);
                            else
                                eventItem->SetType(Event::IFACE_ADDR_DELETE);
                            eventItem->SetInterfaceIndex(ifa->ifa_index);
                            if (AF_INET == ifa->ifa_family)
                                eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv4, (char*)RTA_DATA(rth), 4);
                            else //if (AF_INET6 == ifa->ifa_family)
                                eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv6, (char*)RTA_DATA(rth), 16);
                            event_list.Append(*eventItem);
                        }
                        else if (IFA_ADDRESS == rth->rta_type)
                        {
                            // Note that Linux doesn't seem to reliably (or at all) issue RTM_DELLINK messages
                            // So - as a cheat hack, we're going to issue IFACE_DOWN event when the link local
                            // address is deleted (fingers crossed!)
                            if ((RTM_DELADDR == nlh->nlmsg_type) &&
                                (RT_SCOPE_LINK == ifa->ifa_scope))
                            {
                                eventItem = event_pool.RemoveHead();
                                if (NULL == eventItem) eventItem = new EventItem();
                                if (NULL == eventItem)
                                {
                                    PLOG(PL_ERROR, "LinuxNetMonitor::GetNextEvent() new EventItem error: %s\n", GetErrorString());
                                    theEvent.SetType(Event::NULL_EVENT);
                                    return false;
                                }
                                eventItem->SetType(Event::IFACE_DOWN);
                                eventItem->SetInterfaceIndex(ifa->ifa_index);
                                event_list.Append(*eventItem);
                                break; // we break because multiple link down indications aren't need
                                       // BUT if we want to link local addr changes here ...
                            }
                        }
                    }
                    break;
                }
                default:
                    //TRACE("OTHER message type %d\n", nlh->nlmsg_type);
                    break;
            }  // end switch(nlh->nlmsg_type) 
        }  // end for NLMSG_OK ...
        eventItem = event_list.RemoveHead();
    }  // end if (NULL == eventItem)
    if (NULL == eventItem)
    {
        theEvent.SetType(Event::NULL_EVENT);
    }
    else
    {    
        theEvent = static_cast<Event&>(*eventItem);
        event_pool.Append(*eventItem);
    }
    return true;
}  // end LinuxNetMonitor::GetNextEvent()

LinuxNetMonitor::EventItem::EventItem()
{
}

LinuxNetMonitor::EventItem::~EventItem()
{
}
