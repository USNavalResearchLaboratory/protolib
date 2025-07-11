#ifndef _PROTO_FLOW
#define _PROTO_FLOW

#include "protoAddress.h"
#include "protoPktIP.h"
#include "protoQueue.h"

// This module provides classes to support high performance lookup tables indexed by
// a "flow description" tuple consisting of IP packet dst/src addressing, protocol, and traffic class
// It also supports associating flows with specific interface indices, if desired.
// Both the the dst and src addresses may optionally have prefix masks applied. This enables
// flow table entries that encompass entire subnets, etc.  Similarly the addresses and other
// fields can be "wildcarded".  The FlowTable::Iterator provides an algorithm for iterating
// through a table for flows that match a given flow description.  Note the classes here are
// designed to provide base classes for derivation of more functional subclasses for specific
// purposes.  The nrlsmf code uses these for elastic routing flow-based forwarding and
// "membership" management.

// NOTES:
// 1) To create a wildcard address entry for a specific address family (i.e., by length),
//    set the address to a valid address, but then set a prefix mask length of zero.
//    (Otherwise, a true wildcard address entry will match _any_ address, regardless of type.)

namespace ProtoFlow
{
    class Description
    {
        public:
            // Flows are referenced by [<dst>][:<src>][:<protocol>][:<trafficClass>]
            // The ProtoFlow::Description key is organized so that prefix-based iterations
            // over dst[+src] addresses can be done.
            // flow_key:  dlen [+ dst + dmask] + slen [+ src + smask] + class + protocol + index
            // where:
            //  'dlen' and 'slen' are the length (in bytes) of the dst and src address, respectively
            //  'dmask' and 'smask' are the prefix mask length (in bits) of the dst and src address, respectively
            //  'dst + dmask' and 'src + smask' are present only for non-zero 'dlen' and 'slen', respectively
            //  'class' is the flow traffic class and value of '0x03' (ECN bits set) indicates "null" (or wildcard) traffic class field
            //  'protocol' is the flow IP protocol and value of 'ProtoPktIP::RESERVED' indicates "null" (or wildcard) protocol field
            //  'index' is the interface index and value of '0' indicates "null" (or wildcard) interface index field

            Description(const ProtoAddress&  dst = PROTO_ADDR_NONE, // invalid dst addr means "null" dst
                        const ProtoAddress&  src = PROTO_ADDR_NONE, // invalid src addr means "null" src
                        UINT8                trafficClass = 0x03,   // "null" trafficClass 
                        ProtoPktIP::Protocol theProtocol = ProtoPktIP::RESERVED, // "null" protocol
                        unsigned int         ifaceIndex = 0);       // "null" interface index 
            ~Description();

            // These two should only be called _before_ insertion
            // into any data structure that uses the "flow_key"
            void SetDstMaskLength(UINT8 dstMaskSize);
            void SetSrcMaskLength(UINT8 srcMaskSize);

            enum Flag
            {
                FLAG_NONE   = 0x00,
                FLAG_DST    = 0x01,
                FLAG_SRC    = 0x02,
                FLAG_CLASS  = 0x04,
                FLAG_PROTO  = 0x08,
                FLAG_INDEX  = 0x10,
                FLAG_ALL    = 0x1f
            };

            bool IsValid() const
                {return (0 != flow_keysize);}

            void Print(FILE* filePtr = NULL) const;  // to ProtoDebug by default

            // These parse the "flow_key", so only use if needed
            bool GetDstAddr(ProtoAddress& addr) const;
            bool GetSrcAddr(ProtoAddress& addr) const;

            UINT8 GetDstLength() const
                {return flow_key[OFFSET_DLEN];}
            const char* GetDstPtr() const
                {return (flow_key + OFFSET_DST);}
            UINT8 GetDstMaskLength() const
                {return flow_key[Offset_Dmask()];}
            UINT8 GetSrcLength() const
                {return flow_key[Offset_Slen()];}
            const char* GetSrcPtr() const
                {return (flow_key + Offset_Src());}
            UINT8 GetSrcMaskLength() const
                {return flow_key[Offset_Smask()];}

            ProtoPktIP::Protocol GetProtocol() const
                {return ((ProtoPktIP::Protocol)((UINT8)flow_key[Offset_Protocol()]));}
            UINT8 GetTrafficClass() const
                {return ((UINT8)flow_key[Offset_TrafficClass()]);}
            unsigned int GetInterfaceIndex() const
            {
                // TBD - should this be in network byte order of specific size
                // for controller / forwarder on different platforms
                unsigned int ifaceIndex;
                memcpy(&ifaceIndex, flow_key + Offset_Index(), sizeof(unsigned int));
                return ifaceIndex;
            }
        
            // infers address type from non-zero address length field
            ProtoAddress::Type GetAddressType() const
            {
                unsigned int dstLen = GetDstLength();
                return ProtoAddress::GetType((0 != dstLen) ? dstLen : GetSrcLength());
            }
        
            const char* GetKey() const
                {return flow_key;}
            unsigned int GetKeysize() const
                {return  flow_keysize;}

            // Methods for setting description fields
            // flags = FLAG_ALL means _all_ fields are set
            bool InitFromPkt(ProtoPktIP& ipPkt, unsigned int ifaceIndex = 0, int flags = FLAG_ALL);  
            void InitFromDescription(const Description& description, int flags = FLAG_ALL);
            bool InitFromText(const char* text);  // format: <srcAddr>[/maskLen],<dstAddr>[/maskLen][,<protocol>[,<class>]]] (also can use X or * to wildcard fields)
            // flow_key:  dlen [+ dstAddr + dmask] + slen [+ srcAddr + smask] + class + protocol + index
            enum {KEY_MAX = (1 + 1 + 16 + 1 + 1 + 16 + 1 + 1 + sizeof(unsigned int))};
            void SetKey(const char*             dstAddr = NULL, 
                        UINT8                   dstLen = 0, 
                        UINT8                   dstMask = 0,
                        const char*             srcAddr = NULL, 
                        UINT8                   srcLen = 0, 
                        UINT8                   srcMask = 0,
                        UINT8                   trafficClass = 0x03, 
                        ProtoPktIP::Protocol    protocol = ProtoPktIP::RESERVED,
                        unsigned int            ifaceIndex = 0);
            void SetKeysize(unsigned int keysize)  // in bits
                {flow_keysize = keysize > (8*KEY_MAX) ? 8*KEY_MAX : keysize;}
            UINT16 GetPrefixSize() const
                {return prefix_size;}

        private:
            // Worst case flow_key length is 42 bytes:
            // 1 'DLEN' byte + 1 'DMASK' byte + 16 IPv6 'DST' bytes + 
            // 1 'SLEN" byte + 1 'SMASK' byte + 16 IPv6 'SRC' bytes + 
            // 1 'trafficClass' byte + 1 'protocol' byte + sizeof(unsigned int) 'ifaceIndex' bytes
            // (trafficClass w/ ECN bits set indicates "null" trafficClass
            //  and ProtoPktIP::RESERVED indicates "null" protocol)
            char                    flow_key[KEY_MAX];
            unsigned int            flow_keysize; // key size (in bits)
            UINT16                  prefix_size;  // non-wildcard key prefix size (in bits)
            // "Offset" methods for dereferencing flow_key fields
            enum
            {
                OFFSET_DLEN = 0,
                OFFSET_DST  = OFFSET_DLEN + 1
            };
            int Offset_Dmask() const
            {
                // only valid for non-zero dstLen
                int dstLen = GetDstLength();
                return (dstLen ? (OFFSET_DST + dstLen) : 0);
            } 
            int Offset_Slen() const
            {        
                int dstLen = GetDstLength();
                return (dstLen ? (2 + dstLen) : OFFSET_DST);
            }
            int Offset_Src() const
                {return (Offset_Slen() + 1);}
            int Offset_Smask() const            // only present for non-zero SLEN
            {
                // only valid for non-zero srcLen
                int srcLen = GetSrcLength();
                return (srcLen ? (Offset_Src() + srcLen) : 0);
            }
            int Offset_TrafficClass() const
            {
                int srcLen = GetSrcLength();
                return (srcLen ? (Offset_Smask() + 1) : (Offset_Slen() + 1));
            }
            int Offset_Protocol() const
                {return (Offset_TrafficClass() + 1);}
            int Offset_Index() const
                {return (Offset_Protocol() + 1);}

    };  // end class ProtoFlow::Description

    // The ProtoFlow::Table class provides a _base_ class for
    // data structures that have items with a ProtoFlow::Description.
    class Table
    {
        public:
            class Entry
            {
                public:
                    Entry(const ProtoAddress&  dst = PROTO_ADDR_NONE, // invalid dst addr means "null" dst
                         const ProtoAddress&  src = PROTO_ADDR_NONE, // invalid src addr means "null" src
                         UINT8                trafficClass = 0x03,   // "null" trafficClass 
                         ProtoPktIP::Protocol theProtocol = ProtoPktIP::RESERVED, // "null" protocol
                         unsigned int         ifaceIndex = 0);     // "null" interface index 
                    Entry(const Description& flowDescription, int flags = Description::FLAG_ALL);
                    virtual ~Entry();
                
                    const Description& GetFlowDescription() const
                        {return flow_description;}
                
                    bool GetDstAddr(ProtoAddress& addr) const
                        {return flow_description.GetDstAddr(addr);}
                    unsigned int GetDstLength() const
                        {return flow_description.GetDstLength();}
                    UINT8 GetDstMaskLength() const
                        {return flow_description.GetDstMaskLength();}
                    bool GetSrcAddr(ProtoAddress& addr) const
                        {return flow_description.GetSrcAddr(addr);}
                    unsigned int GetSrcLength() const
                        {return flow_description.GetSrcLength();}
                    UINT8 GetSrcMaskLength() const
                        {return flow_description.GetSrcMaskLength();}
                    ProtoPktIP::Protocol GetProtocol() const
                        {return flow_description.GetProtocol();}
                    int GetTrafficClass() const
                        {return flow_description.GetTrafficClass();} 
                    unsigned int GetInterfaceIndex() const
                        {return flow_description.GetInterfaceIndex();}
                
                    ProtoAddress::Type GetAddressType() const
                        {return flow_description.GetAddressType();}
                
                    // Note these two should only be called _before_ insertion
                    // into any data structure that uses the "flow_key"
                    // (only necessary for partial prefix mask lengths)
                    void SetDstMaskLength(UINT8 maskLen)
                        {flow_description.SetDstMaskLength(maskLen);}
                    void SetSrcMaskLength(UINT8 maskLen)
                        {flow_description.SetSrcMaskLength(maskLen);}
                    
                    const char* GetKey() const
                        {return flow_description.GetKey();}
                    unsigned int GetKeysize() const
                        {return flow_description.GetKeysize();}
                
                    UINT16 GetPrefixSize() const
                        {return flow_description.GetPrefixSize();}
                
                    void PrintDescription(FILE* filePtr = NULL) const
                        {flow_description.Print(filePtr);}
                
                private:
                    Description flow_description;
            
            };  // end class ProtoFlow::Table::Entry
        
        protected:
            // Derived classes MUST call these as part of their
            // own respective insert / remove methods so the
            // FlowTable::mask_list is updated properly
            void Insert(UINT16 prefixLength)
                {mask_list.Insert(prefixLength);}
            void Remove(UINT16 prefixLength)
                {mask_list.Remove(prefixLength);}
        
            // The MaskLengthList is a helper class to keep track of the
            // set of different destination mask prefix lengths that 
            // are within the table. (for efficient matching iterator operation)
            class MaskLengthList
            {
                public:
                    MaskLengthList();
                    ~MaskLengthList();
                
                    enum {MASK_SIZE_MASK = Description::KEY_MAX*8};

                    void Insert(UINT16 maskLen);
                    void Remove(UINT16 maskLen);
                
                    class Iterator
                    {
                        public:
                            Iterator(const MaskLengthList& maskList) 
                                : mask_list(maskList), list_index(0) {}
                    
                            void Reset()
                                {list_index = 0;}
                        
                            int GetNextMaskSize()
                                {return ((list_index < mask_list.GetLength()) ? 
                                            mask_list.GetValue(list_index++) : -1);}
                    
                        private:
                            const MaskLengthList& mask_list;
                            unsigned int          list_index;
                        
                    };  // end class MulticastFIB::MaskLengthList::Iterator
                
                    friend class Iterator;

                private:
                    unsigned int GetLength() const
                        {return list_length;}
                    int GetValue(UINT16 index) const
                        {return ((index < list_length) ? (int)mask_list[index] : -1);}
                    
                    UINT16          mask_list[MASK_SIZE_MASK+1];
                    UINT16          list_length;
                    unsigned int    ref_count[MASK_SIZE_MASK+1];
            };  // end class FlowTable::MaskLengthList
        
            // The BaseIterator provides an abstraction for the different data
            // structure types that may be used for derived ProtoFlow::Table classes.
            class BaseIterator
            {
                public:
                    virtual void Reset(bool         reverse = false,
                                       const char*  prefix = NULL,
                                       unsigned int prefixSize = 0) = 0;

                    virtual Entry* GetNextEntry() = 0;

            };  // end class ProtoFlow::Table::BaseIterator
        
            class Iterator
            {
                public:
                    virtual ~Iterator();
            
                protected:
                    // The appropriate BaseIterator subclass is passed into
                    // this common iterator over ProtoFlow::Description entries.
                    Iterator(BaseIterator&    tableIterator, 
                             MaskLengthList&  maskLengthList);
            
                    void Reset(const Description* flowDescription = NULL, int flags = Description::FLAG_ALL, bool bimatch = true);
                
                    Entry* GetNextEntry();
                
                    Entry* FindBestMatch(const Description& flowDescription, bool deepSearch=false);
            
                    int GetCurrentMaskLength() const
                        {return current_mask_size;}
                
                private:
                    BaseIterator&               table_iterator;
                    MaskLengthList::Iterator    mask_iterator;
                    UINT16                      prefix_mask_size;
                    char                        prefix_mask[Description::KEY_MAX];
                    int                         current_mask_size;
                    int                         next_mask_size;
                    ProtoAddress                src_addr;
                    UINT8                       src_mask_size;
                    UINT8                       traffic_class;
                    ProtoPktIP::Protocol        protocol;
                    unsigned int                iface_index;
                    bool                        bi_match;  // if true, do wildcard matching using both "flowDescription" param and candidates wildcard fields.
                                                           // else do wildcard match using only "flowDescription" param wildcard fields.
                
            };  // end class ProtoFlow::Table::Iterator
        
        protected:
            MaskLengthList  mask_list;
        
    };  // end class ProtoFlow::Table

    // Separate ProtoFlow::EntryTemplate and ProtoFlow::TableTemplate classes are provided 
    // so that subclasses may be easily created as needed.

    // "TABLE_TYPE" should be one of the "ProtoTree" or "ProtoIndexedQueue" for the moment
    // (The set of data structure types supported may be expanded if needed.) 
    template <class TABLE_TYPE>
    class EntryTemplate : public Table::Entry, public TABLE_TYPE::Item
    {
        public:
            EntryTemplate(const ProtoAddress&  dst = PROTO_ADDR_NONE, // invalid dst addr means "null" dst
                          const ProtoAddress&  src = PROTO_ADDR_NONE, // invalid src addr means "null" src
                          UINT8                trafficClass = 0x03,   // "null" trafficClass 
                          ProtoPktIP::Protocol theProtocol = ProtoPktIP::RESERVED, // "null" protocol
                          unsigned int         ifaceIndex = 0)        // "null" interface index 
                : Table::Entry(dst, src, trafficClass, theProtocol, ifaceIndex) {}
            EntryTemplate(const Description& flowDescription, int flags = Description::FLAG_ALL)
                : Table::Entry(flowDescription, flags) {}
            ~EntryTemplate() {}
        
            const char* GetKey() const
                {return Table::Entry::GetKey();}
            unsigned int GetKeysize() const
                {return Table::Entry::GetKeysize();}
    };  // end class ProtoFlow::EntryTemplate

    // "ENTRY_TYPE" MUST be of type derived from ProtoFlow::EntryTemplate for same given "TABLE_TYPE"
    template <class ENTRY_TYPE, class TABLE_TYPE>
    class TableTemplate : public Table, public TABLE_TYPE
    { 
        private:
            class BaseIterator : public Table::BaseIterator, public TABLE_TYPE::Iterator
            {
                public:
                    BaseIterator(TABLE_TYPE& table)
                      : TABLE_TYPE::Iterator(table) {}
                    ~BaseIterator() {}
                    void Reset(bool         reverse = false,
                               const char*  prefix = NULL,
                               unsigned int prefixSize = 0)
                    {
                        TABLE_TYPE::Iterator::Reset(reverse, prefix, prefixSize);
                    }
                    ENTRY_TYPE* GetNextEntry()
                    {
                        return static_cast<ENTRY_TYPE*>(TABLE_TYPE::Iterator::GetNextItem());
                    }
            };  // end class ProtoFlow::TableTemplate::BaseIterator
        
            // To support use of ProtoIndexedQueue as TABLE_TYPE              
            const char* GetKey(const ProtoQueue::Item& item) const
                {return static_cast<const ENTRY_TYPE&>(item).GetKey();}
            unsigned int GetKeysize(const ProtoQueue::Item& item) const
                {return static_cast<const ENTRY_TYPE&>(item).GetKeysize();}
        
        public:
            // Here is the full public interface that can be used
            TableTemplate() {};
            virtual ~TableTemplate() {}
        
            bool InsertEntry(ENTRY_TYPE& entry)
            {
                if (TABLE_TYPE::Insert(entry))
                {
                    Table::Insert(entry.GetPrefixSize());
                    return true;
                }
                return false;
            }
        
            void RemoveEntry(ENTRY_TYPE& entry)
            {
                Table::Remove(entry.GetPrefixSize());
                TABLE_TYPE::Remove(entry);
            }
        
            ENTRY_TYPE* FindEntry(const Description& flowDescription)
                {return static_cast<ENTRY_TYPE*>(TABLE_TYPE::Find(flowDescription.GetKey(), flowDescription.GetKeysize()));}
        
            void Destroy()
                {TABLE_TYPE::Destroy();}
        
            // If "deepSearch" is false, the best match for the longest matching destination prefix is
            // returned.  Otherwise, a "deeper" search is conducted where possibly a longer source 
            // address prefix match will trump another match that has a longer destination prefix match.
            // (It is expected the default (deepSearch=false) will usually be the desired behavior.)
            // (TBD - add optional "flags" parameter to 'soft edit' the flowDescription passed in)
            ENTRY_TYPE* FindBestMatch(const Description& flowDescription, bool deepSearch=false)
            {
                TableTemplate<ENTRY_TYPE, TABLE_TYPE>::Iterator iterator(*this, &flowDescription);
                return iterator.FindBestMatch(flowDescription, deepSearch);
            }
        
            class Iterator : public Table::Iterator
            {
                public:
                    Iterator(TableTemplate&         table, 
                             const Description*     flowDescription = NULL, 
                             int                    flags = Description::FLAG_ALL,
                             bool                   bimatch = true) 
                      : Table::Iterator(base_iterator, table.mask_list), base_iterator(table)
                        {Table::Iterator::Reset(flowDescription, flags, bimatch);}
            
                    ~Iterator() {}
                
                    // "bimatch=true" does fully reciprocal matching of flowDescription with wildcard to/from entries with wildcards,
                    // while "bimatch=false" ignores entry wildcard values, thus matching only "subordinate" entries ...
                
                    void Reset(const Description* flowDescription = NULL, int flags = Description::FLAG_ALL, bool bimatch = true)
                        {Table::Iterator::Reset(flowDescription, flags, bimatch);}
                
                    ENTRY_TYPE* GetNextEntry()
                        {return static_cast<ENTRY_TYPE*>(Table::Iterator::GetNextEntry());}
                
                    int GetCurrentMaskLength() const
                        {return Table::Iterator::GetCurrentMaskLength();}
                
                private:
                    friend class TableTemplate;
                    ENTRY_TYPE* FindBestMatch(const Description& flowDescription, bool deepSearch=false)
                        {return static_cast<ENTRY_TYPE*>(Table::Iterator::FindBestMatch(flowDescription, deepSearch));}
                
                    TableTemplate::BaseIterator    base_iterator;
            
            };  // end class ProtoFlow::TableTemplate::Iterator
    };  // end ProtoFlow:P:TableTemplate
    
    
    // Another data structure using ProtoIndexedQueue we may want to support is one where each 
    // ProtoFlow::Description entry is inserted into multiple indexed queues (i.e. trees)
    // with tree indexed with a different attribute (src, dst, protocol, etc) and then matching
    // could be done by building  a temporary set of each non-wildcard attribute match and then 
    // assessing set intersection membership (via discard) to identify valid matches.  
    
}  // end namespace ProtoFlow
#endif // !_PROTO_FLOW
