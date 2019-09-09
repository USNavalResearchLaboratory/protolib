#ifndef _PROTO_DETOUR
#define _PROTO_DETOUR

#include "protoChannel.h"
#include "protoAddress.h"

/**
 * @class ProtoDetour
 *
 * @brief Inbound/outbound packet _interception_ class. Platform 
 * implementations vary ... but generally leverages system firewall interfaces. 
 * A Win32 version based around NDIS intermediate driver is in progress.
 */

class ProtoDetour : public ProtoChannel
{
    public:
        static ProtoDetour* Create();
        virtual ~ProtoDetour() {}
        
        enum HookFlag
        {
            INPUT  =  0x01,  // packets coming in
            OUTPUT =  0x02,  // locally generated packets   
            FORWARD = 0x04,  // packets we might forward
            INJECT  = 0x80   // "inject-only" option (create a raw socket for send-only)
        };
            
        enum Direction
        {
            UNSPECIFIED,
            INBOUND,
            OUTBOUND   
        };
            
        
        virtual bool Open(int                 hookFlags     = 0, 
                          const ProtoAddress& srcFilterAddr = PROTO_ADDR_NONE, 
                          unsigned int        srcFilterMask = 0,
                          const ProtoAddress& dstFilterAddr = PROTO_ADDR_NONE,
                          unsigned int        dstFilterMask = 0,
                          int                 dscpValue     = -1)
        {
            return ProtoChannel::Open();
        }
        virtual bool IsOpen() {return ProtoChannel::IsOpen();}
        // ProtoCap::Close() should also be called at the _beginning_ of
        // any derived implementations' Close() method
        virtual void Close() {ProtoChannel::Close();}
        
        virtual bool Recv(char*         buffer, 
                          unsigned int& numBytes, 
                          Direction*    direction = NULL, // optionally learn INBOUND/OUTBOUND direction of pkt
                          ProtoAddress* srcMac = NULL,    // optionally learn previous hop source MAC addr 
                          unsigned int* ifIndex = NULL) = 0;     // optionally learn which iface (INBOUND only)
        
        virtual bool Allow(const char* buffer, unsigned int numBytes) = 0;
        virtual bool Drop() = 0;
        virtual bool Inject(const char* buffer, unsigned int numBytes) = 0;
        
        virtual bool SetMulticastInterface(const char* interfaceName) {return false;}
            
        void SetUserData(const void* userData) 
            {user_data = userData;}
        const void* GetUserData() const
            {return user_data;}
            
    protected:
        ProtoDetour() : user_data(NULL) 
        {
            // Enable input notification by default
            StartInputNotification();  
            // Our default handler "allows" all packets received
            // (Note that ProtoChannel::SetNotifier() _must_ be set _and_ a "running" notifier
            //  must be in place for this default listener to be automatically invoked!
            SetListener(this, &ProtoDetour::DefaultEventHandler);
        }
      
    private:
        void DefaultEventHandler(ProtoChannel& theChannel,
                                Notification  theNotification)
        {
            if (NOTIFY_INPUT == theNotification)
            {
                char buffer[8192];
                unsigned int numBytes = 8192; 
                while (Recv(buffer, numBytes))
                {
                    if (0 != numBytes)
                        Allow(buffer, numBytes);
                    else
                        break; 
                    numBytes = 8192;
                }                  
            }
        }   
        
        const void*     user_data;
};  // end class ProtoDetour

#endif // _PROTO_DETOUR
