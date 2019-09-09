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
                          const ProtoAddress&   vifAddr, 
                          unsigned int          maskLen) 
            {return ProtoChannel::Open();}

        virtual void Close()
            {ProtoChannel::Close();}
        
        // This lets us set a MAC addr. 
        // (vif MUST be open and iface is brought down/up to change)
        virtual bool SetHardwareAddress(const ProtoAddress& hwAddr) = 0;
        
        virtual bool SetARP(bool status) = 0;

        virtual bool Write(const char* buffer, unsigned int buflen) = 0;
        
        virtual bool Read(char* buffer, unsigned int& numBytes) = 0;
        
        const char* GetName() const
            {return vif_name;}
    
    protected:
        ProtoVif();
    
        // Implementations SHOULD fill this in with the whatever name
        // the virtual interface gets, whether equal to requested name or not
        char    vif_name[VIF_NAME_MAX+1];
          
}; // end class ProtoVif

#endif // _PROTO_VIF
