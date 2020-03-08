#ifndef _PROTO_VIF
#define _PROTO_VIF

#include "protoChannel.h"
#include "protoAddress.h"

/**
 * @class ProtoVif
 *
 * @brief Supports virtual interfaces.
 */
class ProtoVif : public ProtoChannel
{
    public:
        static ProtoVif* Create();  // for our "factory" pattern
        virtual ~ProtoVif();
        
        enum {VIF_NAME_MAX = 255};

        // Overrides of "Open()" and "Close()" MUST call the ProtoChannel 
        // Open/Close base class methods last/first, respectively
        // Note: on some OS's, you may not get your requested "vifName"
        virtual bool Open(const char*           vifName, 
                          const ProtoAddress&   ipAddr, // optional IP addr for interface
                          unsigned int          maskLen) // maskLen for optional IP addr
            {return ProtoChannel::Open();}

        virtual void Close()
            {ProtoChannel::Close();}
        
        const ProtoAddress& GetHardwareAddress() const
            {return hw_addr;}
        
        // This lets us set a MAC addr. 
        // (vif MUST be open and iface is brought down/up to change)
        virtual bool SetHardwareAddress(const ProtoAddress& hwAddr) = 0;
        
        virtual bool SetARP(bool status) = 0;

        virtual bool Write(const char* buffer, unsigned int buflen) = 0;
        
        virtual bool Read(char* buffer, unsigned int& numBytes) = 0;
        const char* GetName() const
            {return vif_name;}
    
        void SetUserData(const void* userData) 
            {user_data = userData;}
        const void* GetUserData() const
            {return user_data;}
            
    protected:
        ProtoVif();
    
        // Implementations SHOULD fill this in with the whatever name
        // the virtual interface gets, whether equal to requested name or not
        char            vif_name[VIF_NAME_MAX+1];
        ProtoAddress    hw_addr;  // should be filled in with device hardware addr on Open()
        
    private:
        const void*     user_data;
          
}; // end class ProtoVif

#endif // _PROTO_VIF
