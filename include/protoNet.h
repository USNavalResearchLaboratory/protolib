#ifndef _PROTO_NET
#define _PROTO_NET

// The ProtoNet classes provide APIs for getting information
// on the computer host's network interfaces, configured
// addresses, etc.
// It includes a class for actively monitoring changes in
// in the status of network interfaces and configuration
// 
// Common code for these functions and classes is implemented in
// "src/common/protoNet.cpp" and system-specific code is implemented
// in other files (e.g. src/unix/unixNet.cpp, src/win32/win32Net.cpp, etc)

// For classes defined here, general a "factory" design pattern is used where
// a static "Create()" method is called to create system-specific instantiations
// of a given class and any virtual interface methods defined here are invoked
// using system-specific code as needed.


#include "protoAddress.h"
#include "protoChannel.h"

namespace ProtoNet
{
    /**
     * @class ProtoNet::Monitor
     *
     * @brief This class provides a means
     * to receive notifications of changes
     * in the computer's network status or
     * configuration.
     *
     */
    
    enum InterfaceStatus
    {
        IFACE_UNKNOWN,
        IFACE_UP,
        IFACE_DOWN
    };
 
    class Monitor : public ProtoChannel
    {
        public:
            static Monitor* Create();
            ~Monitor();
            
            // NOTE:  Overrides of "Open()" and "Close()" MUST call the ProtoChannel 
            // Open/Close base class methods last/first, respectively.
            virtual bool Open() 
                {return ProtoChannel::Open();}

            virtual void Close()
                {ProtoChannel::Close();}
            
            // Can use the ProtoChannel "SetNotifier()" and "SetListener()" methods to
            // "wire up" the Monitor notifications to, respectively, ProtoDispatcher (or other) 
            // asynchronous I/O notification and a "listener" method contained
            // in another class
            
            // Use the GetNextEvent() to fetch the next network status update event
            // (This should be called upon a ProtoChannel::NOTIFY_INPUT notification event)
            // from the system.  Note that other functions may need to be invoked to
            // get details about a particular event (e.g., change in interface "flags", etc)
            
            // Note the set of Event types will be expanded over time as needed and depending
            // upon OS-support (i.e. not all events might be supported on all OS types)
            
            class Event
            {
                public:
                    enum Type
                    {
                        NULL_EVENT = 0,
                        IFACE_UP,
                        IFACE_DOWN,
                        IFACE_ADDR_NEW,
                        IFACE_ADDR_DELETE,
                        IFACE_STATE,  // change in flags, etc
                        UNKNOWN_EVENT
                    };
                        
                    enum {IFNAME_MAX = 255};
                        
                    Event();
                    ~Event();
                    
                    void SetType(Type eventType)
                        {event_type = eventType;}
                    void SetInterfaceIndex(int ifaceIndex)
                        {iface_index = ifaceIndex;}
                    void SetAddress(ProtoAddress& addr)
                        {iface_addr = addr;}
                    void SetInterfaceName(const char* name)
					{
#ifdef WIN32
						strncpy_s(iface_name, IFNAME_MAX, name, IFNAME_MAX);
#else
                        strncpy(iface_name, name, IFNAME_MAX);
#endif  // if/else WIN32
					}
                    
                    Type GetType() const
                        {return event_type;}
                    int GetInterfaceIndex() const
                        {return iface_index;}
                    const ProtoAddress& GetAddress() const
                        {return iface_addr;}
                    ProtoAddress& AccessAddress()
                        {return iface_addr;}
                    const char* GetInterfaceName() const
                        {return iface_name;}
                    
                    
                private:
                    Type            event_type;
                    int             iface_index;
                    ProtoAddress    iface_addr; 
                    char            iface_name[IFNAME_MAX+1];
                     
            };  // end class ProtoNet::Monitor::Event
            
            virtual bool GetNextEvent(Event& event) = 0;
                
        protected:
            Monitor();
                
    };  // end class ProtoNet::Monitor
    
    // These are the base ProtoNet methods that must be implemented in
    // with specific operating system calls (i.e. in unixNet.cpp, win32Net.cpp, etc)
    unsigned int GetInterfaceIndices(unsigned int* indexArray, unsigned int indexArraySize);
    
    // get iface name by index
    unsigned int GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen);  
    
    // get iface index by name
    unsigned int GetInterfaceIndex(const char* interfaceName);
    
    // get all addrs of "addrType" for given given "ifName"
    bool GetInterfaceAddressList(const char*         ifName, 
				                 ProtoAddress::Type  addrType,
				                 ProtoAddressList&   addrList,
                                 unsigned int*       ifIndex = NULL); 
    
    // get name that matches given ifAddr (may be an alias name)
    // (returns name length so you can verify buflen was sufficient)
    unsigned int GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen);
#ifdef WIN32
	unsigned int GetInterfaceFriendlyName(const ProtoAddress& ifaceAddress, char* buffer, unsigned int buflen);
	unsigned int GetInterfaceFriendlyName(unsigned int ifaceIndex, char* buffer, unsigned int buflen);
	bool GetInterfaceAddressDhcp(const char* ifName, const ProtoAddress& ifAddr);
	// TODO: Fix functions to have one definition
	bool AddInterfaceAddress(const char* ifaceName, const ProtoAddress& addr, unsigned int maskLen, bool dhcp_enabled=false);
	bool GetInterfaceIpAddress(unsigned int index, ProtoAddress& ifAddr);
#else
	bool AddInterfaceAddress(const char* ifaceName, const  ProtoAddress& addr, unsigned int maskLen);

#endif //WIN32
	unsigned int GetInterfaceAddressMask(const char* ifName, const ProtoAddress& ifAddr);  // returns masklen?
	bool RemoveInterfaceAddress(const char* ifaceName, const ProtoAddress& addr, unsigned int maskLen = 0);

#ifndef WIN32  // TBD - implement these for WIN32
      
    
    bool GetGroupMemberships(const char* ifaceName, ProtoAddress::Type addrType, ProtoAddressList& addrList);
    
#endif  // !WIN32    
    
    /////////////////////////////////////////////////////////
    // These are implemented in "protoNet.cpp" using the above
    // 'base' functions.
    
    unsigned int GetInterfaceCount();
    
    unsigned int GetInterfaceIndex(const ProtoAddress& ifAddr);
    
    bool GetInterfaceAddress(const char*         ifName, 
				             ProtoAddress::Type  addrType,
				             ProtoAddress&       theAddress,
                             unsigned int*       ifIndex = NULL);
    
    bool GetInterfaceAddress(unsigned int        ifIndex, 
				             ProtoAddress::Type  addrType,
				             ProtoAddress&       theAddress);
    
    bool GetHostAddressList(ProtoAddress::Type  addrType,
						    ProtoAddressList&   addrList);

    bool GetInterfaceAddressList(unsigned int ifIndex,
				                 ProtoAddress::Type  addrType,
				                 ProtoAddressList&   addrList);
    
    bool FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress);
    
    InterfaceStatus GetInterfaceStatus(const char* ifaceName);
    InterfaceStatus GetInterfaceStatus(unsigned int ifaceIndex);

#ifdef WIN32
    // TODO: make common function
    bool AddInterfaceAddress(unsigned int ifaceIndex, const ProtoAddress& addr, unsigned int maskLen, bool dhcp_enabled=false);

#else
    bool AddInterfaceAddress(unsigned int ifaceIndex, const ProtoAddress& addr, unsigned int maskLen);
#endif // WIN32

    bool RemoveInterfaceAddress(unsigned int ifaceIndex, const ProtoAddress& addr, unsigned int maskLen = 0);

    unsigned int GetInterfaceAddressMask(unsigned int ifIndex, const ProtoAddress& ifAddr);
#ifdef WIN32
    bool GetInterfaceAddressDhcp(unsigned int ifIndex, const ProtoAddress& ifAddr);
#endif // WIN32
}  // end namespace ProtoNet


#endif // !_PROTO_NET
