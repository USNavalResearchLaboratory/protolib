#include "protoNet.h"
#include <Iphlpapi.h>

// When compiled with the v100_xp platorm on vista and above the winver
// check will fail(NotifyIpInterfaceChange is not available below Vista)

#if (WINVER >= 0x0600)  // Win32NetMonitor only works for Windows Vista and higher
class Win32NetMonitor : public ProtoNet::Monitor
{
public:
	Win32NetMonitor();
	~Win32NetMonitor();

	bool Open();
	void Close();
	bool GetNextEvent(Event& theEvent);
	bool GetEvent(PMIB_IPINTERFACE_ROW row,
		MIB_NOTIFICATION_TYPE notificationType);
	static bool FindIPAddr(NET_IFINDEX InterfaceIndex);
	const char* GetNotificationType(int type);
	HANDLE GetEventHandle() { return input_handle; }

private:
	// We cache mib changes to a linked list for
	// retrieval by the GetNextEvent() method
	class EventItem : public Event, public ProtoList::Item
	{
	public:
		EventItem();
		~EventItem();
	};  // end class Win32NetMontior::EventItem

	class EventList : public ProtoListTemplate<EventItem> {};
	EventList	event_list;
	EventList	event_pool;

	typedef CRITICAL_SECTION    Mutex;
	Mutex                       lock;
    
	static void Init(Mutex& m) {InitializeCriticalSection(&m);}
    static void Destroy(Mutex& m) {DeleteCriticalSection(&m);}
    static void Lock(Mutex& m) {EnterCriticalSection(&m);}
	static void Unlock(Mutex& m) {LeaveCriticalSection(&m);}

	HANDLE notification_handle;  // handle to subsequently stop notifications

}; // end class Win32NetMonitor

/// This is the implementation of the ProtoNet::Monitor::Create()
/// static method (our win32-specific factory)
ProtoNet::Monitor* ProtoNet::Monitor::Create()
{
	return static_cast<ProtoNet::Monitor*>(new Win32NetMonitor);
} // end ProtoNet::Monitor::Create()

Win32NetMonitor::Win32NetMonitor()
{
    Init(lock);
}

Win32NetMonitor::~Win32NetMonitor()
{
}

// Static callback function for NotifyIpInterfaceChange API
static void WINAPI IpInterfaceChangeCallback(PVOID callerContext,
	PMIB_IPINTERFACE_ROW row,
	MIB_NOTIFICATION_TYPE notificationType)
{
	Win32NetMonitor* monitor = (Win32NetMonitor*)callerContext;
	if (!monitor) 
	{
		PLOG(PL_ERROR,"IpInterfaceChangeCallback() Error: No callerContext.\n");
		return;
	}

	if (row)
	{
		// Get complete information for MIP_IPINTERFACE_ROW
		GetIpInterfaceEntry(row);
		// Add an event to our list for the notification
		if (!monitor->GetEvent(row,notificationType))
		{	
			PLOG(PL_ERROR,"MonitorEventHandler() GetEvent error\n");
			return;
		}

    }
	if (!SetEvent(monitor->GetEventHandle()))
		PLOG(PL_ERROR,"win32Net::MonitorEventHandler() Error setting event handle.\n");
}

bool Win32NetMonitor::Open()
{
 	// Not using a manual reset event?
	if (NULL == (input_handle = CreateEvent(NULL,FALSE,FALSE,NULL)))
    {
        input_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR,"Win32Monitor::Open() CreateEvent(event_handle) error: %s\n", ::GetErrorString());
        Close(); 
        return false;
    }
	// Initiate notifications ...
	notification_handle = NULL;
	if (!NotifyIpInterfaceChange(
				AF_UNSPEC,  // AF_INET
				(PIPINTERFACE_CHANGE_CALLBACK)IpInterfaceChangeCallback,
				this,
				false, // initialNofification 
				&notification_handle) == NO_ERROR)
	{
		PLOG(PL_ERROR,"Win32NetMonitor::Open() NotifyIpInterfaceChange failed\n");
		return false;
	}

	if (!ProtoNet::Monitor::Open())
	{
		Close();
		return false;
	}
	
	return true;
}

void Win32NetMonitor::Close()
{
	if (IsOpen())
	{
		ProtoNet::Monitor::Close();	
		input_handle = INVALID_HANDLE;
	}
	if (notification_handle != INVALID_HANDLE)
		CancelMibChangeNotify2(notification_handle);

	event_list.Destroy();
	event_pool.Destroy();

	Unlock(lock);
	Destroy(lock);
}

const char* Win32NetMonitor::GetNotificationType(int type)
    {
        static const char* names[] = {
        "ParameterNotification",
        "AddInstance",
        "DeleteInstance",
        "InitialNotification" 
        };

        const char* name = "";
        if (type >=0 && type < sizeof(names)) {
            name = names[type];
        }
        return name;
    }
bool Win32NetMonitor::GetNextEvent(Event& theEvent)
{
	// 0) Initialize event instance
	theEvent.SetType(Event::UNKNOWN_EVENT);
	theEvent.SetInterfaceIndex(0);
	theEvent.AccessAddress().Invalidate();

	// 1) Get next event from list
	Lock(lock);
	EventItem* eventItem = event_list.RemoveHead();
	if (eventItem == NULL)
	{
		Unlock(lock);
		theEvent.SetType(Event::NULL_EVENT);
		return true;
	}
	theEvent = static_cast<Event&>(*eventItem);
	event_pool.Append(*eventItem);
	Unlock(lock);
	return true;
}

bool Win32NetMonitor::GetEvent(PMIB_IPINTERFACE_ROW row,
	MIB_NOTIFICATION_TYPE notificationType)
{
	EventItem* eventItem = event_pool.RemoveHead();
	if (NULL == eventItem) eventItem = new EventItem();
	if (NULL == eventItem)
	{
		PLOG(PL_ERROR,"Win32NetMonitor::GetEvent() new EventItem error: %s\n", GetErrorString());
		return false;
	}

	eventItem->SetInterfaceIndex(row->InterfaceIndex);

	switch (notificationType)
    {
	case 0:
		//eventItem->SetType(Event::IFACE_STATE);
		// not interested in windows state changes at the moment
		eventItem->SetType(Event::UNKNOWN_EVENT);
		break;
	case 1:
		eventItem->SetType(Event::IFACE_UP);
		break;
	case 2:
		eventItem->SetType(Event::IFACE_DOWN);
		break;
	case 3:
		eventItem->SetType(Event::UNKNOWN_EVENT);
		break;
	default:
		eventItem->SetType(Event::UNKNOWN_EVENT);
		PLOG(PL_ERROR,"Win32NetMonitor::GetEvent() warning: unhandled network event: %d\n",notificationType);
		break;
	}


	// Iterate through addresses looking for our interface index
	ULONG bufferSize = 0;
    ULONG index = 0;
    if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
    {
            char* tableBuffer = new char[bufferSize];
			if (NULL == tableBuffer)
	        {   
				PLOG(PL_ERROR, "Win32NetMonitor::GetEvent() new tableBuffer error: %s\n", ::GetErrorString());
				return false;
			}
			MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
		    if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
			{
				 for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
				{

				   MIB_IPADDRROW* entry = &(addrTable->table[i]);
				   if (entry->dwIndex == row->InterfaceIndex)
				   {
					  if (row->Family == AF_INET)
					     eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr,4);
					  else
					     eventItem->AccessAddress().SetRawHostAddress(ProtoAddress::IPv6, (char*)&entry->dwAddr,16);
					  // TBD Ignore link local addr new/delete events?
					  
				   }

			    }

			}
	}
    else
        {
            PLOG(PL_WARN, "Win32NetMonitor::GetEvent(%u) warning GetIpAddrTable() error: %s\n", row->InterfaceIndex, GetErrorString());
        }


	Lock(lock);
	event_list.Append(*eventItem);
	Unlock(lock);
	return true;

} // end Win32NetMonitor::GetNextEvent();

Win32NetMonitor::EventItem::EventItem()
{
}

Win32NetMonitor::EventItem::~EventItem()
{
}

#endif // (WINVER >= 0x00600)

