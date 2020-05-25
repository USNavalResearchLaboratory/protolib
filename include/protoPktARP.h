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
        ProtoPktARP(void*          bufferPtr = NULL, 
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
        bool InitFromBuffer(void*   bufferPtr       = NULL, 
                            unsigned int numBytes   = 0, 
                            bool freeOnDestruct     = false);
        
        HardwareType GetHardwareType() const
            {return (HardwareType)GetWord16(OFFSET_HRD);}
        
        ProtoPktETH::Type GetEtherType() const
            {return (ProtoPktETH::Type)GetWord16(OFFSET_PRO);} 
        
        UINT8 GetHardwareAddrLen() const
            {return GetUINT8(OFFSET_HLN);}
        
        UINT8 GetProtocolAddrLen() const
            {return GetUINT8(OFFSET_PLN);}
        
        Opcode GetOpcode() const
            {return (Opcode)GetWord16(OFFSET_OP);}
        
        bool GetSenderHardwareAddress(ProtoAddress& addr) const;
        
        bool GetSenderProtocolAddress(ProtoAddress& addr) const;
        
        bool GetTargetHardwareAddress(ProtoAddress& addr) const;
        
        bool GetTargetProtocolAddress(ProtoAddress& addr) const;
        
        
        // Use these to build the ARP message 
        // (MUST be called in order of appearance here)
        bool InitIntoBuffer(void*          bufferPtr = 0, 
                            unsigned int   numBytes = 0, 
                            bool           freeOnDestruct = false);
        void SetOpcode(Opcode opcode)
            {SetWord16(OFFSET_OP, (UINT16)opcode);}
        bool SetSenderHardwareAddress(const ProtoAddress& addr);
        bool SetSenderProtocolAddress(const ProtoAddress& addr);
        bool SetTargetHardwareAddress(const ProtoAddress& addr);
        bool SetTargetProtocolAddress(const ProtoAddress& addr);
        
        
    private:
        void SetHardwareType(HardwareType hwType)
            {SetWord16(OFFSET_HRD, (UINT16)hwType);}
        void SetEtherType(ProtoPktETH::Type etherType)  // protocol address type
            {SetWord16(OFFSET_PRO, (UINT16)etherType);}
        void SetHardwareAddrLen(UINT8 numBytes) 
            {SetUINT8(OFFSET_HLN, numBytes);}
        void SetProtocolAddrLen(UINT8 numBytes)
            {SetUINT8(OFFSET_PLN, numBytes);}
           
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


// Data structure for both MAC->IP and IP->MAC lookups (dually indexed)
// (Note multiple IPs per MAC is allowed, but one MAC per IP)
class ProtoArpTable
{
    public:
        ProtoArpTable();
        ~ProtoArpTable();

        bool AddEntry(const ProtoAddress& ipAddr, const ProtoAddress& macAddr);
        void RemoveEntryByIP(const ProtoAddress& ipAddr);
        void RemoveEntryByMAC(const ProtoAddress& macAddr); 
        
        bool GetMacAddress(const ProtoAddress& ipAddr, ProtoAddress& macAddr);
        bool GetAddressList(const ProtoAddress& macAddr, ProtoAddressList addrList);
        
        void Destroy()
        {
            mac_list.Destroy();
            ip_list.Destroy();
        }

    private:
        // Record of IP address listings for a given MAC address
        class MacItem : public ProtoTree::Item
        {
            public:
                MacItem(const ProtoAddress& macAddr);
                ~MacItem();
                
                const ProtoAddress& GetMacAddr()
                    {return mac_addr;}
                ProtoAddressList& AccessAddressList()
                    {return ip_addr_list;}
                
                bool AddAddress(const ProtoAddress& ipAddr)
                    {return ip_addr_list.Insert(ipAddr);}
                void RemoveAddress(const ProtoAddress& ipAddr)
                    {ip_addr_list.Remove(ipAddr);}
                
            private:
                const char* GetKey() const
                    {return mac_addr.GetRawHostAddress();}
                unsigned int GetKeysize() const
                    {return (8 * mac_addr.GetLength());}
                
                ProtoAddress     mac_addr;
                ProtoAddressList ip_addr_list;
        };  // end class ProtoArpTable::MacItem
        class MacList : public ProtoTreeTemplate<MacItem> 
        {
            public:
                MacItem* FindItem(const ProtoAddress& macAddr)
                    {return Find(macAddr.GetRawHostAddress(), 8 * macAddr.GetLength());}
        };  // end class ProtoArpTable::IPList
        
        // MAC address indexed by IP address
        class IPItem : public ProtoTree::Item
        {
            public:
                IPItem(const ProtoAddress& ipAddr, MacItem* macItem)
                    : ip_addr(ipAddr), mac_item(macItem) {}
                ~IPItem() {}
                
                const ProtoAddress& GetAddress()const
                    {return ip_addr;}
                const ProtoAddress& GetMacAddr() const
                    {return mac_item->GetMacAddr();}
                
                MacItem* GetMacItem()
                    {return mac_item;}
                
            private:
                const char* GetKey() const
                    {return ip_addr.GetRawHostAddress();}
                unsigned int GetKeysize() const
                    {return (8 * ip_addr.GetLength());}
                
                ProtoAddress    ip_addr;
                MacItem*        mac_item;
        };  // end class ProtoArpTable::IPItem
        
        
        // List of MAC address item indexed by IP address
        class IPList : public ProtoTreeTemplate<IPItem>
        {
            public:
                IPItem* FindItem(const ProtoAddress& ipAddr)
                    {return Find(ipAddr.GetRawHostAddress(), 8 * ipAddr.GetLength());}
        };  // end class ProtoArpTable::IPList

        void DeleteIPItem(IPItem* ipItem);
        void DeleteMacItem(MacItem* macItem);

        MacList     mac_list;
        IPList      ip_list;

};  // end class ProtoArpTable

#endif // _PROTO_PKT_ARP
