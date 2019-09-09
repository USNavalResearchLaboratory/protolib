#ifndef _PROTO_PKT_ARP
#define _PROTO_PKT_ARP

#include "protoPktETH.h"

/**
 * @class ProtoPktARP
 *
 * @brief Parses ARP packets.
 */
class ProtoPktARP : public ProtoPkt
{
    public:
        ProtoPktARP(UINT32*        bufferPtr = NULL, 
                    unsigned int   numBytes = 0, 
                    bool           initFromBuffer = false,
                    bool           freeOnDestruct = false); 
        ~ProtoPktARP();
        
        
        enum HardwareType
        {
            ETHERNET    = 1,
            IEEE802     = 6,
            ARCNET      = 7,
            FRAME_RELAY = 15,
            ATM         = 16,
            HDLC        = 17,
            FIBRE_CHAN  = 18,
            ATM2        = 19,
            SERIAL      = 20
        };
            
        enum Opcode
        {
            ARP_REQ     = 1,
            ARP_REP     = 2,
            RARP_REQ    = 3,
            RARP_REP    = 4,
            DRARP_REQ   = 5,
            DRARP_REP   = 6,
            DRARP_ERR   = 7,
            IN_ARP_REQ  = 8,
            IN_ARP_REP  = 9  
        };
        
        // Use these to parse the ARP message
        bool InitFromBuffer(UINT32* bufferPtr       = NULL, 
                            unsigned int numBytes   = 0, 
                            bool freeOnDestruct     = false);
        
        HardwareType GetHardwareType() const
            {return ((HardwareType)ntohs(((UINT16*)buffer_ptr)[OFFSET_HRD]));}
        
        ProtoPktETH::Type GetEtherType() const
            {return ((ProtoPktETH::Type)ntohs(((UINT16*)buffer_ptr)[OFFSET_PRO]));}
        
        UINT8 GetHardwareAddrLen() const
            {return ((UINT8*)buffer_ptr)[OFFSET_HLN];}
        
        UINT8 GetProtocolAddrLen() const
            {return ((UINT8*)buffer_ptr)[OFFSET_PLN];}
        
        Opcode GetOpcode() const
            {return ((Opcode)ntohs(((UINT16*)buffer_ptr)[OFFSET_OP]));}
        
        bool GetSenderHardwareAddress(ProtoAddress& addr) const;
        
        bool GetSenderProtocolAddress(ProtoAddress& addr) const;
        
        bool GetTargetHardwareAddress(ProtoAddress& addr) const;
        
        bool GetTargetProtocolAddress(ProtoAddress& addr) const;
        
        
        // Use these to build the ARP message 
        // (should be called in order of appearance here)
        bool InitIntoBuffer(UINT32*        bufferPtr = 0, 
                            unsigned int   numBytes = 0, 
                            bool           freeOnDestruct = false);
        void SetOpcode(Opcode opcode)
            {((UINT16*)buffer_ptr)[OFFSET_OP] = htons((UINT16)opcode);}
        bool SetSenderHardwareAddress(const ProtoAddress& addr);
        bool SetSenderProtocolAddress(const ProtoAddress& addr);
        bool SetTargetHardwareAddress(const ProtoAddress& addr);
        bool SetTargetProtocolAddress(const ProtoAddress& addr);
        
        
    private:
        void SetHardwareType(HardwareType hwType)
            {((UINT16*)buffer_ptr)[OFFSET_HRD] = htons((UINT16)hwType);}
        void SetEtherType(ProtoPktETH::Type etherType)  // protocol address type
            {((UINT16*)buffer_ptr)[OFFSET_PRO] = htons((UINT16)etherType);}
         void SetHardwareAddrLen(UINT8 numBytes) const
            {((UINT8*)buffer_ptr)[OFFSET_HLN] = numBytes;}
         void SetProtocolAddrLen(UINT8 numBytes) const
            {((UINT8*)buffer_ptr)[OFFSET_PLN] = numBytes;}
           
        enum
        {
            OFFSET_HRD           = 0,                    // UINT16 offset
            OFFSET_PRO           = OFFSET_HRD + 1,       // UINT16 offset
            OFFSET_HLN           = (OFFSET_PRO + 1)*2,   // UINT8 offset
            OFFSET_PLN           = OFFSET_HLN + 1,       // UIN8 offset
            OFFSET_OP            = (OFFSET_PLN + 1)/2,   // UINT16 offset
            OFFSET_SNDR_HRD_ADDR = (OFFSET_OP+1)*2       // UINT8 offset
        };
        
        unsigned int OffsetSenderHardwareAddr() const
            {return OFFSET_SNDR_HRD_ADDR;}    
        
        unsigned int OffsetSenderProtocolAddr() const
            {return (OFFSET_SNDR_HRD_ADDR + GetHardwareAddrLen());}   
        
        unsigned int OffsetTargetHardwareAddr() const
            {return (OffsetSenderProtocolAddr() + GetProtocolAddrLen());}    
         
        unsigned int OffsetTargetProtocolAddr() const
            {return (OffsetTargetHardwareAddr() + GetHardwareAddrLen());}   
            
};  // end class ProtoPktARP


#endif // _PROTO_PKT_ARP
