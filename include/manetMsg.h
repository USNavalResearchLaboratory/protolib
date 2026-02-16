#ifndef _MANET_MESSAGE
#define _MANET_MESSAGE

/**
* @class ManetTlv
* @brief This C++ class is an implementation of the packet and message formats given
* in IETF RFC 5444
*
* Methods are provided to construct and parse the Type-Length-Value (TLV)
* based scheme described in that document.
*/
#include <protoPkt.h>
#include <protoDefs.h>
#include <protoDebug.h>
#include <protoAddress.h>

namespace ManetMsgTypes
{
    // These IANA registries are defined in RFC 5444 section 6 with specific assignment in various derived documents
    // Standard defined Packet TLV types
    enum PktTlv
    {
    };
    // Standard defined Message Types
    enum MsgType
    {
        MSG_TYPE_NHDP                   = 0, //RFC6130
        MSG_TYPE_DYMO                   = 2, //TBD dymo document
        MSG_TYPE_OLSRv2                 = 1  //TBD olsrv2 document
    };
    // Standard defined Message TLV types
    enum MsgTlv
    {
        MSG_TLV_INTERVAL_TIME          = 0, //RFC5497
        MSG_TLV_VALIDITY_TIME          = 1, //RFC5497
        MSG_TLV_NHDP_SMF               = 128,  //RFC6621
        MSG_TLV_OLSR_MPR_WILLING       = 129, //OLSRv2
        MSG_TLV_OLSR_CONT_SEQ_NUM      = 130  //OLSRv2
    };
    enum SubTypeSmfRelayAlgorithmID
    {
        SUB_TYPE_SMF_CF                = 0, //RFC6621
        SUB_TYPE_SMF_SMPR              = 1, //RFC6621
        SUB_TYPE_SMF_ECDS              = 2, //RFC6621
        SUB_TYPE_SMF_MPRCDS            = 3,  //RFC6621
        SUB_TYPE_SMF_ECDS_ETX          = 128 //TBD metric/ETX document
    };
    // Standard defined Address Block TLV types
    enum AddrBlkTlv
    {
        ADDR_TLV_INTERVAL_TIME         = 0, //RFC5497
        ADDR_TLV_VALIDITY_TIME         = 1, //RFC5497
        ADDR_TLV_LOCAL_IF              = 2, //RFC6130
        ADDR_TLV_LINK_STATUS           = 3, //RFC6130
        ADDR_TLV_OTHER_NEIGHB          = 4, //RFC6130
        ADDR_TLV_OLSR_MPR              = 5, //TBD OLSRv2
        ADDR_TLV_OLSR_NBR_ADDR_TYPE    = 6, //TBD OLSRv2
        ADDR_TLV_OLSR_LINK_METRIC      = 7, //TBD OLSRv2
        ADDR_TLV_NHDP_SMF              = 128, //RFC6621
        ADDR_TLV_LINK_ETX_DF           = 129, //TBD metric/ETX document (Delivery Ratio Forward/Outgoing)
        ADDR_TLV_LINK_ETX_DR           = 130 //TBD metric/ETX document (Delivery Ratio Reverse/Incoming)
    };
}

class ManetTlv : public ProtoPkt
{
    public:
        ManetTlv();
        ~ManetTlv();

        // (TBD) Will there be standard "namespace:ietf:manet:message:tlvTypes"
        //       that we should enumerate here?

        // Should this enum be moved to ManetMsg::TlvType???
        enum Type
        {
            // (TBD) Provide methods for dealing with fragmented TLVs
            MPR_WILLING = 10,
            CONTENT_SEQ_NUMBER  = 11,     // value length
            GATEWAY = 12
        };

        enum TypeFlag
        {
            TLV_USER     = 0X80,
            TLV_PROTOCOL = 0X40
        };

        // TLV building routines (these MUST be called in order given here)
        bool InitIntoBuffer(UINT8 type, void* bufferPtr = NULL, unsigned numBytes = 0);

        // Defined to use with any built-in enumerated TLV types (FRAGMENTATION, etc)
        bool InitIntoBuffer(Type type, void* bufferPtr = NULL, unsigned int numBytes = 0)
            {return InitIntoBuffer((UINT8)type, bufferPtr, numBytes);}
        
        void SetType(UINT8 type)
            {SetUINT8(OFFSET_TYPE,type); return;}
        
        bool SetExtendedType(UINT8 extType);

        void SetIsMultiValue() {return SetSemantic(MULTIVALUE);}
        // By default, TLV is non-indexed, single value (or no value) until this is called
        // (non-zero "numAddrs" indicates AddressBlock TLV and need for explicit index is determined)
        bool SetIndexRange(UINT8 start, UINT8 stop, bool multivalue = false, unsigned int numAddrs = 0);
        
        
        
        // By default, TLV is "no value" until this is called.
        // Note "index" MUST be >= GetIndexStart() and <= GetIndexStop() if MULTIVALUE or -1 otherwise
        // The optional "numAddrs" parameter is for multivalue AddressBlock TLVs where an index isn't present
        // (Note that for building up TLVs, etc the index is always included for AddressBlock multivalue TLVs 
        //  but this may change in the future.  For parsing AddressBlock TLVs from other PacketBB builders,
        //  it is important that the "numAddrs" be indicated for AddressBlock TLVs.
        bool SetValue(const char* value, UINT16 valueLength, int index = -1, unsigned int numAddrs = 0);

        /**
         * Here are some "shortcut" versions of SetValue()
         * The index values are indexed to the address block not the value field itself.
        **/
        bool SetValue(UINT8 value, int index = -1, unsigned int numAddrs = 0)
            {return (SetValue((char*)&value, sizeof(UINT8), index, numAddrs));}
        bool SetValue(UINT16 value, int index = -1, unsigned int numAddrs = 0)
            {value = htons(value); return (SetValue((char*)&value, sizeof(UINT16), index, numAddrs));}
        bool SetValue(UINT32 value, int index = -1, unsigned int numAddrs = 0)
            {value = htonl(value); return (SetValue((char*)&value, sizeof(UINT32), index, numAddrs));}

        // This lets you  edit the index range _after_ setting some values if desired
        bool UpdateIndexRange(UINT8 start, UINT8 stop, bool multivalue, unsigned int numAddrs = 0);
        
        // TLV parsing methods
        // Note:  This one is called by the ManetMsg and ManetAddrBlock "TlvIterator"
        //        classes so you shouldn't call this yourself, typically.
        bool InitFromBuffer(void* bufferPtr = NULL, unsigned int numBytes = 0);

        UINT8 GetType() const
            {return GetUINT8(OFFSET_TYPE);}
        bool HasExtendedType() const
            {return SemanticIsSet(EXTENDED_TYPE);}
        UINT8 GetExtendedType() const
            {return (HasExtendedType() ? GetUINT8(OFFSET_TYPE_EXT) : 0);}
        UINT16 GetFullType() const
            {return ((GetType() << 8) | GetExtendedType());}

        bool HasIndex() const
            {return !HasNoIndex();}
        bool HasMultiIndex() const
            {return SemanticIsSet(MULTI_INDEX);}
        bool HasSingleIndex() const
            {return SemanticIsSet(SINGLE_INDEX);}
        UINT8 GetIndexStart() const
            {return (HasIndex() ? GetUINT8(OffsetIndexStart()) : 0);}
        
        // IMPORTANT: The "numAddrs" parameter SHOULD be set appropriately to properly parse AddressBlock TLVs
        // (AddressBlock TLVs differ from Packet and Message TLVs in that an explicit index is _not_ required  
        //  for multivalue TLVs (the index range 0..numAddrs-1 is assumed according to RFC 5444
        UINT8 GetIndexStop(unsigned int numAddrs = 0) const
            {return (HasSingleIndex() ? GetIndexStart() : 
                                        (HasMultiIndex() ? GetUINT8(OffsetIndexStop()) : 
                                         (SemanticIsSet(MULTIVALUE) ? (numAddrs - 1) : 0)));}

        bool HasValue() const
            {return (SemanticIsSet(HAS_VALUE));}
        bool IsMultiValue() const
        {
            bool result = SemanticIsSet(MULTIVALUE);
            ASSERT(!result || (HasValue() && !HasSingleIndex()));
            return result;
        }

        // Returns _total_ length of tlv <value> field
        UINT16 GetTlvLength() const;

        // Returns length of _single_ tlv value (in bytes)
        UINT16 GetValueLength(unsigned int numAddrs = 0) const;
        
        /**
         * This copies the value content.  Index is related to the address block field and not the value field.
        **/
        bool GetValue(char* value, UINT16 valueLength, int index = -1, unsigned int numAddrs = 0) const;

        /**
         * Here are some "shortcut" methods for certain lengths. 
         * Index is related to the address block field and not the value field.
        **/
        bool GetValue(UINT8& value, int index = -1, unsigned int numAddrs = 0) const
            {return GetValue((char*)&value, sizeof(UINT8), index, numAddrs);}
        bool GetValue(UINT16& value, int index = -1, unsigned int numAddrs = 0) const
        {
            bool result = GetValue((char*)&value, sizeof(UINT16), index, numAddrs); 
            value = ntohs(value);
            return result;
        }
        
        bool GetValue(UINT32& value, int index = -1, unsigned int numAddrs = 0) const
        {
            bool result = GetValue((char*)&value, sizeof(UINT32), index, numAddrs); 
            value = ntohl(value);
            return result;
        }

        // This returns a pointer to the value portion of the tlv buffer_ptr
        const char* GetValuePtr(UINT16 valueLength = 0, int index = -1, unsigned int numAddrs = 0) const;

    private:

        enum Semantic
        {
            EXTENDED_TYPE       = 0x80, // Indicates TLV "type" has added 8-bits of extended type
            SINGLE_INDEX        = 0x40, // value for only "index-start" is given (no "index-stop" field)
            MULTI_INDEX         = 0x20, // value for only "index-start" is given (no "index-stop" field)
            HAS_VALUE           = 0x10, // no value content and no value length
            EXTENDED_LENGTH     = 0x08, // Indicates TLV "length" is 16-bit instead of 8-bit
            MULTIVALUE          = 0x04  // one value for each address in index range <startIndex:stopIndex>
        };

        bool HasNoIndex() const
            {return !SemanticIsSet(SINGLE_INDEX) && !SemanticIsSet(MULTI_INDEX);}

        bool HasExtendedLength() const
            {return SemanticIsSet(EXTENDED_LENGTH);}

        // Enumeration and methods to calculate offset of various TLV fields
        enum
        {
            OFFSET_TYPE         = 0,
            OFFSET_SEMANTICS    = OFFSET_TYPE + 1,
            OFFSET_TYPE_EXT     = OFFSET_SEMANTICS + 1
        };
        unsigned int OffsetIndexStart() const
            {return (OFFSET_TYPE_EXT + (HasExtendedType() ? 1 : 0));}
        unsigned int OffsetIndexStop() const
            {return (OffsetIndexStart() + 1);}
        unsigned int OffsetLength() const
            {return (OffsetIndexStart() + (HasNoIndex() ? 0 : (HasSingleIndex() ? 1 : 2)));}
        unsigned int OffsetValue() const
            {return (OffsetLength() + (HasExtendedLength() ? 2 : 1));}

        // Helper method used to in TLV build/parse initialization
        static unsigned int GetMinLength(UINT8 semantics)
        {
            unsigned int minLength = 2;                     // "type" and "semantics" field
            minLength += (0 != (semantics & EXTENDED_TYPE)) ? 1 : 0;  // opt. "type-ext" field
            minLength +=  (0 != (semantics & SINGLE_INDEX) )?   // opt. "index" fields
                          1 :
                          ((0 != (semantics & MULTI_INDEX)) ? 2 : 0);
            minLength += (0 != (semantics & HAS_VALUE)) ?    // opt. "length" field
                         ((0 != (semantics & EXTENDED_LENGTH)) ? 2 : 1) :
                                 0;
            return minLength;
        }

        void SetTlvLength(UINT16 tlvLength);

        bool SemanticIsSet(Semantic semantic) const
            {return (0 != ((UINT8)semantic & GetUINT8(OFFSET_SEMANTICS)));}//((UINT8*)buffer_ptr)[OFFSET_SEMANTICS]));}
        void SetSemantic(Semantic semantic)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] |= (UINT8)semantic;}
        void ClearSemantic(Semantic semantic)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= ~((UINT8)semantic);}
        void ClearSemantics(int flags)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= ~((UINT8)flags);}

};  // end class ManetTlv



// Note: Typically, you should not have to use the ManetTlvBlock class directly yourself
class ManetTlvBlock: public ProtoPkt
{
    public:
        ManetTlvBlock();
        ~ManetTlvBlock();

        // TLV Block building methods
        // Note: This one is called for you by the ManetMsg and ManetAddrBlock
        //       "AppendTlv()", etc routines, so you shouldn't normally
        //       need to make calls related to ManetTlvBlock yourself.
        bool InitIntoBuffer(void* bufferPtr = NULL, unsigned int numBytes = 0);

        // This returns a ManetTlv pointer initialized into the
        // tlv block's buffer_ptr space
        ManetTlv* AppendTlv(UINT8 type = 0);

        // This copies an existing ManetTlv into the tlv block's
        // buffer_ptr space if there is room.
        bool AppendTlv(ManetTlv& tlv);

        void Pack();

        // TLV Block parsing methods
        bool InitFromBuffer(void* bufferPtr = NULL, unsigned int numBytes = 0);

        UINT16 GetTlvBlockLength()
            {return (GetUINT16(OFFSET_LENGTH));}

        class Iterator
        {
            public:
                Iterator(ManetTlvBlock& tlvBlk);
                ~Iterator();

                void Reset() {tlv_temp.AttachBuffer(NULL, 0);}
                bool GetNextTlv(ManetTlv & tlv)
                {
                    bool result = tlv_block.GetNextTlv(tlv_temp);
                    tlv = tlv_temp;
                    return result;
                }


            private:
                ManetTlvBlock&    tlv_block;
                ManetTlv          tlv_temp;
        };  // end class ManetTlvBlock::Iterator


    private:
        bool GetNextTlv(ManetTlv& tlv) const;

        void SetTlvBlockLength(UINT16 tlvLength)
            {SetUINT16(OFFSET_LENGTH, tlvLength);}

        ManetTlv    tlv_temp;
        bool        tlv_pending;

        enum
        {
            OFFSET_LENGTH  = 0,  // 2-byte <tlv-length> field
            OFFSET_CONTENT = 2
        };

};  // end class ManetTlvBlock

class ManetAddrBlock : public ProtoPkt
{
    public:
        ManetAddrBlock();
        ManetAddrBlock(void* bufferPtr, unsigned int numBytes, bool freeOnDestroy = false);
        ~ManetAddrBlock();

        // Addr block building methods (These MUST be called in order given)
        // Note:  This is called for you by ManetMsg::AppendAddressBlock() so
        //        typically you shouldn't call this one yourself.
        bool InitIntoBuffer(void* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);

        // if <head-length> == addr-length, this sets the entire addr-block content
        // Note: "hlen" is the prefix length in bytes
        bool SetHead(const ProtoAddress& addr, UINT8 hlen);

        bool SetTail(const ProtoAddress& addr, UINT8 tlen);

        bool AppendAddress(const ProtoAddress& addr);

        bool SetPrefixLength(UINT8 length, UINT8 index = 0);

        // This returns a ManetTlv pointer initialized into the
        // address block's buffer_ptr space
        ManetTlv* AppendTlv(UINT8 type = 0);

        // This copies an _existing_ ManetTlv into the address block's
        // buffer_ptr space, if there is room.
        // (This lets us build TLV's in a separate buffer_ptr if we choose to)
        // (or copy TLVs from one block to another)
        bool AppendTlv(ManetTlv& tlv);

        // This finalizes the address block build
        // Note: This is taken care of for you by ManetMsg::Pack() which is called by
        //       ManetPkt::Pack() so you shouldn't call this yourself if you use
        //       those constructs to encapsulate your address block.
        void Pack();

        // Addr block parsing methods
        // Note:  This is called for you by ManetMsg::AddrBlockIterator() so typically
        //        you shouldn't have to call this yourself.
        bool InitFromBuffer(UINT8 addrLength, void* bufferPtr, unsigned int numBytes = 0);
        UINT8 GetAddressLength() const
            {return (addr_length);}
        UINT8 GetAddressCount() const
            {return GetUINT8(OFFSET_NUM_ADDR);}
        bool GetAddress(UINT8 index, ProtoAddress& theAddr) const;
        ProtoAddress::Type GetAddressType() const
            {return ProtoAddress::GetType(addr_length);}

        // These methods are provided for more optimal (but stateful)
        // parsing by application code using this class
        // (i.e, directly copy addresses into a buffer_ptr)
        bool HasHead() const
            {return (SemanticIsSet(HAS_HEAD));}
        UINT8 GetHeadLength() const
            {return (HasHead() ? GetUINT8(OFFSET_HEAD_LENGTH) : 0);}
        const char* GetHead() const
            {return ((char*)buffer_ptr + OFFSET_HEAD);}

        bool HasTail() const
            {return (SemanticIsSet(HAS_FULL_TAIL));}
        bool HasZeroTail() const
            {return SemanticIsSet(HAS_ZERO_TAIL);}
        UINT8 GetTailLength() const
            {return ((HasTail() || HasZeroTail()) ? GetUINT8(OffsetTailLength()) : 0);}
        const char* GetTail(UINT8 index) const
            {return ((char*)buffer_ptr + OffsetTail());}

        UINT8 GetMidLength() const
            {return (GetAddressLength() - GetHeadLength() - GetTailLength());}
        const char* GetMid(UINT8 index) const
        {
            ASSERT(index < GetAddressCount());
            return ((char*)buffer_ptr + OffsetMid() + index*GetMidLength());
        }

        bool HasSinglePrefixLength() const
            {return SemanticIsSet(HAS_SINGLE_PREFIX_LEN);}
        bool HasMultiPrefixLength() const
            {return SemanticIsSet(HAS_MULTI_PREFIX_LEN);}
        bool HasPrefixLength() const
            {return (HasSinglePrefixLength() || HasMultiPrefixLength());}
        // returns "prefix length" in bits
        UINT8 GetPrefixLength(UINT8 index = 0)
        {
            if (HasSinglePrefixLength())
                return GetUINT8(OffsetPrefixLength());
            else if (HasMultiPrefixLength())
                return GetUINT8(OffsetPrefixLength() + index);
            else
                return (GetAddressLength() << 3);
        }

        class TlvIterator : public ManetTlvBlock::Iterator
        {
            public:
                TlvIterator(ManetAddrBlock& addrBlk);
                ~TlvIterator();

                void Reset()
                    {ManetTlvBlock::Iterator::Reset();}

                bool GetNextTlv(ManetTlv& tlv)
                    {return ManetTlvBlock::Iterator::GetNextTlv(tlv);}

        }; //  end class ManetAddrBlock::TlvIterator

    private:
        UINT8               addr_length;
        ManetTlvBlock       tlv_block;
        bool                tlv_block_pending;

        enum SemanticFlag
        {
            HAS_HEAD              = 0x80,
            HAS_FULL_TAIL         = 0x40,
            HAS_ZERO_TAIL         = 0x20,
            HAS_SINGLE_PREFIX_LEN = 0x10,
            HAS_MULTI_PREFIX_LEN  = 0x08
        };

        bool SemanticIsSet(SemanticFlag flag) const
            {return (0 != (flag & GetUINT8(OFFSET_SEMANTICS)));}
        void SetSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] |= (UINT8)flag;}
        void ClearSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= (UINT8)(~flag);}
        void SetSemantics(UINT8 semantics)
            {SetUINT8(OFFSET_SEMANTICS, semantics);}
        void ClearAllSemantics()
            {SetSemantics(0);}

        enum
        {
            OFFSET_NUM_ADDR     = 0,
            OFFSET_SEMANTICS    = OFFSET_NUM_ADDR + 1,
            OFFSET_HEAD_LENGTH  = OFFSET_SEMANTICS + 1,  // optional
            OFFSET_HEAD         = OFFSET_HEAD_LENGTH + 1 // optional
        };
        unsigned int OffsetTailLength() const
            {return (OFFSET_HEAD_LENGTH + (HasHead() ? (1 + GetHeadLength()) : 0));}
        unsigned int OffsetTail() const
            {return (OffsetTailLength() + 1);}
        unsigned int OffsetMid() const
        {
            UINT16 offset = OffsetTailLength();
            offset += (HasTail() ? (1 + (HasZeroTail() ? 0 : GetTailLength())) : 0);
            return offset;
        }
        unsigned int OffsetPrefixLength() const
            {return (OffsetMid() + (GetAddressCount() * GetMidLength()));}

        void SetAddressCount(UINT8 numAddrs)
            {SetUINT8(OFFSET_NUM_ADDR, numAddrs);}

};  // end class ManetAddrBlock

/**
* @class ManetMsg
*
* @brief Class that implements the general packet format of IETF RFC 5444 
*/
class ManetMsg : public ProtoPkt
{
    public:
        ManetMsg();
        ManetMsg(void* bufferPtr, unsigned int numBytes, bool freeOnDestroy = false);
        virtual ~ManetMsg();

        // (TBD) Will there be standard "namespace:ietf:manet:message:types"
        //       that we should enumerate here?
        // Message Building (all header & info fields MUST be set
        // _before_ tlv's, etc).  Also message header/info fields
        // MUST be set in order given here!

        // Init into given "bufferPtr" space
        // Note:: ManetPkt::AppendMessage() will call this one for you so
        //        typically you shouldn't need to call this
        bool InitIntoBuffer(void* bufferPtr = NULL, unsigned int numBytes = 0);

        // This MUST be called if "SetOriginator()" is not called.
        void SetAddressLength(UINT8 addrLength)
        {
            UINT8 semantics = ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS];
            semantics &= 0xf0;
            semantics |= ((addrLength-1) & 0x0f);
            ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] = semantics;
        }
        void SetType(UINT8 type)
            {SetUINT8(OFFSET_TYPE, type);}

        // <msg-semantics> flags are set as corresponding fields are set
        // <msg-size> is set when message is Pack()'d

        bool SetOriginator(const ProtoAddress& addr);

        bool SetHopLimit(UINT8 hopLimit);

        bool SetHopCount(UINT8 hopCount);

        bool SetSequence(UINT16 sequence);

        // Use next to append any message TLV's (and set their indexes, values, etc as needed)
        // (TBD - add code to enforce "order contraint" as they are added)

        // Append a <msg-tlv>
        // This returns a ManetTlv pointer initialized into the ManetMsg buffer_ptr space
        ManetTlv* AppendTlv(UINT8 type = 0)
            {return (tlv_block_pending ? tlv_block.AppendTlv(type) : NULL);}

        // This copies an existing ManetTlv into the message's buffer_ptr space
        // if there is room.  (This lets us build TLV's in a separate
        // buffer_ptr if we so choose)
        bool AppendTlv(ManetTlv& tlv)
        {
            ASSERT(!tlv.HasIndex());  // msg-tlv's never have index!
            return (tlv_block_pending ? tlv_block.AppendTlv(tlv) : false);
        }

        // Then, append any address block(s)
        // This returns a pointer to an address-block initialized into the message's buffer_ptr
        // (This pointer is for temporary use and should not be deleted)
        ManetAddrBlock* AppendAddressBlock();

        // Note:  ManetPkt::Pack() will call this for you, so you don't have to!
        void Pack();

        // Message parsing
        // Note:  InitFromBuffer() is called for you by the ManetPkt::Iterator()
        //        so you shouldn't need to call this (perhaps this should be private
        //        assuming the message is encapsulated in an ManetPkt?)

        bool InitFromBuffer(void* bufferPtr, unsigned int numBytes = 0);

        UINT8 GetType() const
            {return GetUINT8(OFFSET_TYPE);}

        UINT16 GetMsgSize() const
            {return GetUINT16(OffsetSize());}
        
        UINT8 GetAddressLength() const
            {return ((GetUINT8(OFFSET_SEMANTICS) & 0x0F) + 1);}

        ProtoAddress::Type GetAddressType() const
            {return ProtoAddress::GetType(GetAddressLength());}

        bool HasOriginator() const
            {return (SemanticIsSet(HAS_ORIGINATOR));}

        bool GetOriginator(ProtoAddress& addr) const;

        void GetOriginatorAsBytes(char* target) const
            {memcpy(target, (char*)buffer_ptr+OffsetOriginator(), GetAddressLength());}

        bool HasHopLimit() const
            {return (SemanticIsSet(HAS_HOP_LIMIT));}

        UINT8 GetHopLimit() const
            {return GetUINT8(OffsetHopLimit());}

        bool HasHopCount() const
            {return (SemanticIsSet(HAS_HOP_COUNT));}

        UINT8 GetHopCount() const
            {return GetUINT8(OffsetHopCount());}

        bool HasSequence() const
            {return (SemanticIsSet(HAS_SEQ_NUM));}

        UINT16 GetSequence() const
            {return GetUINT16(OffsetSequence());}

        class TlvIterator : public ManetTlvBlock::Iterator
        {
            public:
                TlvIterator(ManetMsg& msg);
                ~TlvIterator();

                void Reset() {ManetTlvBlock::Iterator::Reset();}

                bool GetNextTlv(ManetTlv& tlv)
                {return ManetTlvBlock::Iterator::GetNextTlv(tlv);}
        }; //  end class ManetAddrBlock::TlvIterator

        class AddrBlockIterator
        {
            public:
                AddrBlockIterator(ManetMsg& theMsg);
                ~AddrBlockIterator();

                void Reset()
                    {addr_block_temp.AttachBuffer(NULL, 0);}

                bool GetNextAddressBlock(ManetAddrBlock& addrBlk)
                {
                    bool result = msg.GetNextAddressBlock(addr_block_temp);
                    addrBlk = addr_block_temp;
                    return result;
                }
            private:
                ManetMsg&         msg;
                ManetAddrBlock    addr_block_temp;
        };  // end class ManetMessage::AddrBlockIterator()

    protected:
        bool GetTlvBlock(ManetTlvBlock& tlvBlk) const
        {
            UINT16 tlvBlockOffset = OffsetTlvBlock();
            char* tlvBuffer =  (char*)buffer_ptr + tlvBlockOffset;
            return tlvBlk.InitFromBuffer(tlvBuffer, pkt_length - tlvBlockOffset);
        }

        bool GetNextAddressBlock(ManetAddrBlock& addrBlk) const;

        enum SemanticFlag
        {
            HAS_ORIGINATOR       = 0x80,
            HAS_HOP_LIMIT        = 0x40,
            HAS_HOP_COUNT        = 0x20,
            HAS_SEQ_NUM          = 0x10
        };
            
        bool SemanticIsSet(SemanticFlag flag) const
            {return (0 != (flag & ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS]));}
        void SetSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] |= (UINT8)flag;}
        void ClearSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= (UINT8)(~flag);}
        void ClearAllSemantics()
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] = 0;}

        void SetMsgSize(UINT16 numBytes)
            {SetUINT16(OffsetSize(), numBytes);}

        ManetTlvBlock       tlv_block;  // msg-tlv-blk
        bool                tlv_block_pending;
        ManetAddrBlock      addr_block_temp;
        bool                addr_block_pending;

        // Byte offsets to various message header fields, etc
        enum
        {
            OFFSET_TYPE         = 0,
            OFFSET_SEMANTICS    = OFFSET_TYPE + 1,
            OFFSET_SIZE         = OFFSET_SEMANTICS + 1
        };
        unsigned int OffsetSize() const
            {return OFFSET_SIZE;}
        unsigned int OffsetOriginator() const
            {return (OffsetSize() + 2);}
        unsigned int OffsetHopLimit() const
            {return (OffsetOriginator() + (HasOriginator() ? GetAddressLength() : 0));}
        unsigned int OffsetHopCount() const
            {return (OffsetHopLimit() + (HasHopLimit() ? 1 : 0));}
        unsigned int OffsetSequence() const
            {return (OffsetHopCount() + (HasHopCount() ? 1 : 0));}
        UINT16 OffsetTlvBlock() const
            {return (OffsetSequence() + (HasSequence() ? 2 : 0));}

}; // end class ManetMsg

/**
* @class ManetPkt
*
* @brief Manet Packet building/parsing class of IETF RFC 5444
*
*/
class ManetPkt : public ProtoPkt
{
    public:
        ManetPkt();
        ManetPkt(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct = false);
        ~ManetPkt();

        // Packet building methods (IMPORTANT: Call these in the order given)
        bool InitIntoBuffer(void* bufferPtr = NULL, unsigned int numBytes = 0);

        void SetVersion(UINT8 version);

        void SetSequence(UINT16 sequence);

        // Two possible methods to append TLVs to packet tlv block
        ManetTlv* AppendTlv(UINT8 type = 0);

        // This copies an existing ManetTlv into the packets's buffer_ptr space
        // if there is room.  (This lets us build TLV's in a separate
        // buffer_ptr if we so choose for whatever reason)
        bool AppendTlv(ManetTlv& tlv);

        // "Attach", "build", then "pack" messages into ManetPkt
        // This returns a pointer to a ManetMsg initialized into the packet's buffer_ptr space
        ManetMsg* AppendMessage();

        // This one appends, by copying, an existing "ManetMsg" to the pkt
        // (Can be used to copy a message into a "ManetPkt")
        bool AppendMessage(ManetMsg& msg);

        // Must call this last to "finalize" the packet and its contents
        void Pack();

        // Packet parsing methods
        bool InitFromBuffer(unsigned int pktLength, void* bufferPtr = NULL, unsigned int numBytes = 0);

        UINT8 GetVersion() const
            {return GetUINT8(OFFSET_SEMANTICS) & 0xF0;}

        UINT16 GetPktSize() const
            {return GetLength();}

        bool HasSequence() const
            {return SemanticIsSet(HAS_SEQ_NUM);}
        UINT16 GetSequence() const
        {
            ASSERT(SemanticIsSet(HAS_SEQ_NUM));
            return GetUINT16(OffsetSequence());
        }

        bool HasTlvBlock() const
            {return SemanticIsSet(HAS_TLV_BLOCK);}

        const char* GetPayload() const
            {return ((char*)buffer_ptr + OffsetPayload());}

        UINT16 GetPayloadLength() const
            {return ((pkt_length > OffsetPayload()) ? (pkt_length - OffsetPayload()) : 0);}

        class TlvIterator : public ManetTlvBlock::Iterator
        {
            public:
                TlvIterator(ManetPkt& pkt);
                ~TlvIterator();

                void Reset() {ManetTlvBlock::Iterator::Reset();}

                bool GetNextTlv(ManetTlv& tlv)
                {return ManetTlvBlock::Iterator::GetNextTlv(tlv);}
        }; //  end class ManetPkt::TlvIterator


        class MsgIterator
        {
            public:
                MsgIterator(ManetPkt& thePkt);
                ~MsgIterator();

                void Reset() 
                    {msg_temp.AttachBuffer(NULL, 0);}

                bool GetNextMessage(ManetMsg& msg)
                {
                    bool result = pkt.GetNextMessage(msg_temp);
                    msg = msg_temp;
                    return result;
                }

            private:
                ManetPkt& pkt;
                ManetMsg  msg_temp;
        }; // end class ManetPkt::MsgIterator


    private:
        // IMPORTANT NOTE: All of the offsets are
        //                 _byte_ offsets, since there
        //                 is not much consideration
        //                 towards alignment in the
        //                 "PacketBB" specification.
        enum
        {
            OFFSET_SEMANTICS = 0,
            OFFSET_SEQUENCE_NUMBER = 1
        };
        unsigned OffsetSequence() const
            {return OFFSET_SEQUENCE_NUMBER;}
        unsigned int OffsetTlvBlock() const
            {return (OffsetSequence() + (HasSequence() ? 2 : 0));}
        unsigned int OffsetPayload() const
            {return (OffsetTlvBlock() + (HasTlvBlock() ? tlv_block.GetLength() : 0));}

        enum SemanticFlag
        {
            HAS_SEQ_NUM         = 0x08,  // sequence (2 bytes) if set
            HAS_TLV_BLOCK       = 0x04  // packet tlv block present if set
        };

        bool SemanticIsSet(SemanticFlag flag) const
            {return (0 != (flag & GetUINT8(OFFSET_SEMANTICS)));}
        void SetSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] |= (UINT8)flag;}
        void ClearSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= (UINT8)(~flag);}
        void ClearAllSemantics()
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= 0xF0;}

        // This is provided for our friend, the "ManetPkt::MsgIterator"
        bool GetNextMessage(ManetMsg& msg);

        static unsigned int GetMinLength(UINT8 semantics)
        {
            unsigned int minLength = 1;  // "semantics" fields
            minLength += (0 != ((UINT8)HAS_SEQ_NUM & semantics)) ? 2 : 0;
            minLength += (0 != ((UINT8)HAS_TLV_BLOCK & semantics)) ? 2 : 0;
            return minLength;
        }

        ManetTlvBlock   tlv_block;         // pkt-tlv-blk
        bool            tlv_block_pending;
        ManetMsg        msg_temp;
        bool            msg_pending;

};  // end class ManetPkt

#endif // _MANET_MESSAGE
