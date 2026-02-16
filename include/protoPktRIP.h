#ifndef _PROTO_PKT_RIP
#define _PROTO_PKT_RIP

#include "protoPkt.h"
#include "protoAddress.h"

class ProtoPktRIP : public ProtoPkt
{
    public:
        ProtoPktRIP(void*          bufferPtr = NULL, 
                    unsigned int   numBytes = 0,  
                    unsigned int   pktLength = 0,  // inits from buffer if non-zero
                    bool           freeOnDestruct = false); 
        ~ProtoPktRIP();

        enum Command
        {
            INVALID     = 0,
            REQUEST     = 1,
            RESPONSE    = 2
        };    
            
        enum {VERSION = 2};
        
        enum AddressFamily
        {
            NONE    = 0,
            IPv4    = 2,       // AF_INET
            AUTH    = 0xffff   // Indicates Authentication entry
        };
            
        enum AuthType
        {
            AUTH_NONE   = 0,
            AUTH_PASS   = 2
        };
        
        class RouteEntry : public ProtoPkt
        {
            public:
                RouteEntry(void*          bufferPtr = NULL, 
                           unsigned int   numBytes = 0,  
                           bool           initFromBuffer = false,
                           bool           freeOnDestruct = false); 
                ~RouteEntry();
                
                // Use these to build a route entry
                // (call them in order)
                bool InitIntoBuffer(void*          bufferPtr = NULL, 
                                    unsigned int   numBytes = 0, 
                                    bool           freeOnDestruct = false);
                void SetAddressFamily(AddressFamily family)
                    {SetUINT16((unsigned int)(2*OFFSET_FAMILY), (UINT16)family);}
                void SetRouteTag(UINT16 tag)
                    {SetUINT16(2*OFFSET_TAG, tag);}
                bool SetAddress(const ProtoAddress& addr);
                bool SetMask(const ProtoAddress& addr);
                bool SetMaskLength(UINT8 maskLen);
                bool SetNextHop(const ProtoAddress& addr);
                void SetMetric(UINT32 metric)
                    {SetUINT32(4*OFFSET_METRIC, metric);}
                
                // Use these to parse a route entry
                bool InitFromBuffer(unsigned int   pktLength = 0,
                                    void*          bufferPtr = NULL, 
                                    unsigned int   numBytes  = 0, 
                                    bool           freeOnDestruct = false);
                AddressFamily GetAddressFamily() const
                    {return (AddressFamily)(GetUINT16((unsigned int)(2*OFFSET_FAMILY)));}
                UINT16 GetRouteTag() const
                    {return GetUINT16(2*OFFSET_TAG);}
                bool GetAddress(ProtoAddress& addr) const;
                bool GetMask(ProtoAddress& addr) const;
                UINT8 GetMaskLength() const;
                bool GetNextHop(ProtoAddress& addr) const;
                UINT32 GetMetric() const
                    {return GetUINT32(4*OFFSET_METRIC);}
                    
            private:
                enum
                {
                    OFFSET_FAMILY = 0,                   // UINT16 offset
                    OFFSET_TAG    = OFFSET_FAMILY + 1,   // UINT16 offset
                    OFFSET_ADDR   = (OFFSET_TAG+1)/2,    // UINT32 offset
                    OFFSET_MASK   = OFFSET_ADDR + 1,     // UINT32 offset
                    OFFSET_NHOP   = OFFSET_MASK + 1,     // UINT32 offset
                    OFFSET_METRIC = OFFSET_NHOP + 1      // UINT32 offset
                };
        };  // end class ProtoPktRIP::RouteEntry
                    
        bool InitIntoBuffer(void*          bufferPtr = NULL, 
                            unsigned int   numBytes = 0, 
                            bool           freeOnDestruct = false);    
        void SetCommand(Command cmd)
            {SetUINT8(OFFSET_COMMAND, (UINT8)cmd);}
        void SetVersion(UINT8 version)
            {SetUINT8(OFFSET_VERSION, version);}
        
        bool AddRouteEntry(const ProtoAddress&  destAddr,
                           UINT32               maskLen,
                           const ProtoAddress&  nextHop,
                           UINT32               metric = 1,
                           UINT16               routeTag = 0);
        
        bool InitFromBuffer(unsigned int   pktLength = 0,
                            void*          bufferPtr = NULL, 
                            unsigned int   numBytes  = 0, 
                            bool           freeOnDestruct = false);
        Command GetCommand() const
            {return (Command)(GetUINT8(OFFSET_COMMAND));}
        UINT8 GetVersion() const
            {return GetUINT8(OFFSET_VERSION);}
        
        unsigned int GetNumEntry() const;
        // Note this does _not_ copy data, but wraps "entry" around
        // the appropriate internal ProtoPktRIP buffer area
        bool AccessRouteEntry(unsigned int index, RouteEntry& entry);
        
   
    private:
        enum
        {
            OFFSET_COMMAND  = 0,                     // UINT8 offset
            OFFSET_VERSION  = OFFSET_COMMAND+1,      // UINT8 offset   
            OFFSET_RESERVED = (OFFSET_VERSION+1)/2,  // UINT16 offset
            OFFSET_PAYLOAD  = (OFFSET_RESERVED+1)/2  // UINT32 offset
        };
        
};  // end class ProtoPktRIP


#endif // _PROTO_PKT_RIP
