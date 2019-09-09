#ifndef _PROTO_CAP
#define _PROTO_CAP

/** 
* @class ProtoCap 
* @brief This class is a generic base class for simple
 *       link layer packet capture (ala libpcap) and
 *       raw link layer packet transmission.
 */

#include "protoChannel.h"

class ProtoCap : public ProtoChannel
{
    public:
        static ProtoCap* Create();
        virtual ~ProtoCap();
        
        // Packet capture "direction"
        enum Direction
        {
            UNSPECIFIED,
            INBOUND,
            OUTBOUND   
        };
        
        // These must be overridden for different implementations
        // ProtoCap::Open() should also be called at the _end_ of derived
        // implementations' Open() method
        virtual bool Open(const char* interfaceName = NULL)
            {return ProtoChannel::Open();}
        virtual bool IsOpen() 
            {return ProtoChannel::IsOpen();}
        // ProtoCap::Close() should also be called at the _beginning_ of
        // derived implementations' Close() method
        virtual void Close() 
        {
            ProtoChannel::Close();
            if_index = -1;
        }
        
        int GetInterfaceIndex() const 
            {return if_index;}
        
        virtual bool Send(const char* buffer, unsigned int buflen) = 0;
        virtual bool Forward(char* buffer, unsigned int buflen) = 0;
        virtual bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL) = 0;
        
        void SetUserData(const void* userData) 
            {user_data = userData;}
        const void* GetUserData() const
            {return user_data;}
            
    protected:
        ProtoCap();
        int         if_index;
        
    private:
        const void* user_data;
            
};  // end class ProtoCap

#endif // _PROTO_CAP
