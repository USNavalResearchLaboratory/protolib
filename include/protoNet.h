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
                        
                    Event();
                    ~Event();
                    
                    void SetType(Type eventType)
                        {event_type = eventType;}
                    void SetInterfaceIndex(int ifaceIndex)
                        {iface_index = ifaceIndex;}
                    void SetAddress(ProtoAddress& addr)
                        {iface_addr = addr;}
                    
                    Type GetType() const
                        {return event_type;}
                    int GetInterfaceIndex() const
                        {return iface_index;}
                    const ProtoAddress& GetAddress() const
                        {return iface_addr;}
                    ProtoAddress& AccessAddress()
                        {return iface_addr;}
                    
                    
                private:
                    Type            event_type;
                    int             iface_index;
                    ProtoAddress    iface_addr; 
                     
            };  // end class ProtoNet::Monitor::Event
            
            virtual bool GetNextEvent(Event& event) = 0;
                
        protected:
            Monitor();
                
    };  // end class ProtoNet::Monitor
    
    
    // These appends addresses of type "addrType" to the "addrList"
	bool GetHostAddressList(ProtoAddress::Type  addrType,
						    ProtoAddressList&   addrList);

    bool GetInterfaceAddressList(const char*         ifName, 
				                 ProtoAddress::Type  addrType,
				                 ProtoAddressList&   addrList,
                                 unsigned int*       ifIndex = NULL); 
    
    bool GetInterfaceAddress(const char*         ifName, 
				             ProtoAddress::Type  addrType,
				             ProtoAddress&       theAddress,
                             unsigned int*       ifIndex = NULL);
    
    // Returns mask length for given interface address (0 if not valid iface or addr)
    unsigned int GetInterfaceAddressMask(const char* ifName, const ProtoAddress& ifAddr);

    
    unsigned int GetInterfaceCount();
    unsigned int GetInterfaceIndices(unsigned int* indexArray, unsigned int indexArraySize);
    
    unsigned int GetInterfaceIndex(const char* interfaceName);
    bool FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress);
    bool GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen);   
    bool GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen);
    
    bool AddInterfaceAddress(const char* ifaceName, const ProtoAddress& addr, unsigned int maskLen);
    bool RemoveInterfaceAddress(const char* ifaceName, const ProtoAddress& addr, unsigned int maskLen = 0);
    
    
}  // end namespace ProtoNet


#endif // !_PROTO_NET
