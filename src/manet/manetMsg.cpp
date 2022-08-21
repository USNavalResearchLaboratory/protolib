#include "manetMsg.h"
#include "protoDebug.h"

ManetTlv::ManetTlv()
{
}

ManetTlv::~ManetTlv()
{
}

bool ManetTlv::InitIntoBuffer(UINT8 type, void* bufferPtr, unsigned int numBytes)
{
    unsigned int minLength = OFFSET_SEMANTICS + 1; // "type" and "semantics" fields only
    if (NULL != bufferPtr)
    {
        if (numBytes < minLength)
            return false;
        else
            AttachBuffer(bufferPtr, numBytes);
    }
    else if (buffer_bytes < minLength)
    {
        return false;
    }
    ((UINT8*)buffer_ptr)[OFFSET_TYPE] = (UINT8)type;
    ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] = (UINT8)0;
    pkt_length = minLength;
    return true;
}  // end ManetTlv::InitIntoBuffer()

bool ManetTlv::SetExtendedType(UINT8 extType)
{
    unsigned int minLength = OFFSET_TYPE_EXT + 1;
    if (buffer_bytes < minLength) return false;
    SetSemantic(EXTENDED_TYPE);
    SetUINT8(OFFSET_TYPE_EXT, extType);
    return true;
}  // end ManetTlv::SetExtendedType()

bool ManetTlv::SetIndexRange(UINT8 start, UINT8 stop, bool multivalue, unsigned int numAddrs)
{
    ASSERT(start <= stop);
    ASSERT((0 == numAddrs) || (stop < numAddrs));
    if ((0 != numAddrs) && (0 == start) && (stop == (numAddrs - 1)))
    {
        // This is for a full address block, so no explicit index fields are needed   
        unsigned int minLength = OffsetIndexStart();
        if (buffer_bytes < minLength) return false;
        ClearSemantics(SINGLE_INDEX | MULTI_INDEX | MULTIVALUE);
        if (multivalue && (start != stop)) SetSemantic(MULTIVALUE);
        pkt_length = minLength;
    }
    else if (start == stop)
    {
        // single index
        unsigned int minLength = OffsetIndexStart() + 1;
        if (buffer_bytes < minLength) return false;
        ClearSemantics(MULTI_INDEX | MULTIVALUE);
        SetSemantic(SINGLE_INDEX);
        SetUINT8(OffsetIndexStart(), start);
        pkt_length = minLength;
    }
    else
    {
        // index range
        unsigned int minLength = OffsetIndexStart() + 2;
        if (buffer_bytes < minLength) return false;
        ClearSemantics(SINGLE_INDEX | MULTIVALUE);
        SetSemantic(MULTI_INDEX);
        if (multivalue) SetSemantic(MULTIVALUE);
        SetUINT8(OffsetIndexStart(), start);
        SetUINT8(OffsetIndexStop(), stop);
        pkt_length = minLength;
    }
    return true;
}  // end ManetTlv::SetIndexRange()

bool ManetTlv::UpdateIndexRange(UINT8 start, UINT8 stop, bool multivalue, unsigned int numAddrs)
{
    ASSERT(start <= stop);
    ASSERT((0 == numAddrs) || (stop < numAddrs));
    if (start < GetIndexStart()) 
        PLOG(PL_WARN, "ManetTlv::UpdateIndexRange() warning: start index is smaller than current start index.\n");
    
    if ((0 != numAddrs) && (0 == start) && (stop == (numAddrs - 1)))
    {
        // This is for a full address block, so no explicit index fields are needed 
        if (HasSingleIndex())
        {
            // slide content backwards one byte to omit former single index
            unsigned int contentStart = OffsetIndexStart() + 1;
            unsigned int contentLength = pkt_length - contentStart;
            memmove((char*)buffer_ptr+contentStart-1, (char*)buffer_ptr+contentStart, contentLength);
            pkt_length--; 
        }
        else if (HasMultiIndex())
        {
            // slide content backwards two byte to omit former start/stop indices
            unsigned int contentStart = OffsetIndexStart() + 2;
            unsigned int contentLength = pkt_length - contentStart;
            memmove((char*)buffer_ptr+contentStart-2, (char*)buffer_ptr+contentStart, contentLength);
            pkt_length -= 2; 
        } 
        ClearSemantics(SINGLE_INDEX | MULTI_INDEX | MULTIVALUE);
        if (multivalue && (start != stop)) SetSemantic(MULTIVALUE);
    }
    else if (start == stop)
    {
        if (HasMultiIndex())
        {
            // we need to slide any content backwards one byte to omit stop index
            unsigned int contentStart = OffsetIndexStart() + 2;
            unsigned int contentLength = pkt_length - contentStart;
            memmove((char*)buffer_ptr+contentStart-1, (char*)buffer_ptr+contentStart, contentLength);
            pkt_length--; 
        }
        else if (!HasSingleIndex())
        {
            // we need to slide any content forward a byte to add start index
            if ((pkt_length+1) > buffer_bytes) return false;
            unsigned int contentStart = OffsetIndexStart();
            unsigned int contentLength = pkt_length - contentStart;
            memmove((char*)buffer_ptr+contentStart+1, (char*)buffer_ptr+contentStart, contentLength);
            pkt_length++;
        }
        ClearSemantics(MULTI_INDEX | MULTIVALUE);
        SetSemantic(SINGLE_INDEX);
        SetUINT8(OffsetIndexStart(), start);
    }
    else
    {
        // TBD - add code to allow AddressBlockTLVs (0 != numAddrs) to be multivalue but without explict index
        if (HasSingleIndex())
        {
            // we need to slide any content forwards one byte to add stop index
            if ((pkt_length+1) > buffer_bytes) return false;  // no more room at the inn
            unsigned int contentStart = OffsetIndexStart() + 1;
            unsigned int contentLength = pkt_length - contentStart ;
            memmove((char*)buffer_ptr+contentStart+1, (char*)buffer_ptr+contentStart, contentLength);
            pkt_length++; 
        }
        else if (!HasMultiIndex())
        {
            // we need to slide any content forwards one byte to add start/stop indices
            if ((pkt_length+2) > buffer_bytes) return false;  // no more room at the inn
            unsigned int contentStart = OffsetIndexStart();
            unsigned int contentLength = pkt_length - contentStart ;
            memmove((char*)buffer_ptr+contentStart+2, (char*)buffer_ptr+contentStart, contentLength);
            pkt_length++; 
            
        }
        ClearSemantics(SINGLE_INDEX | MULTIVALUE);
        SetSemantic(MULTI_INDEX);
        if (multivalue) SetSemantic(MULTIVALUE);
        SetUINT8(OffsetIndexStart(), start);
        SetUINT8(OffsetIndexStop(), stop);
    }
    return true;
}  // end ManetTlv::UpdateIndexRange()

// Note: User MUST call SetIndexRange() _before_ this method for "index" >= 0
bool ManetTlv::SetValue(const char* value, UINT16 valueLength, int index, unsigned int numAddrs)
{
    if (index < 0)
    {
        if (SemanticIsSet(MULTIVALUE))
        {
            PLOG(PL_ERROR, "ManetTlv::SetValue() error: can't set non-indexed value for MultiValue TLV\n");
            return false;
        }
        index = 0;
    }
    //else if (!SemanticIsSet(SINGLE_INDEX) && !SemanticIsSet(MULTI_INDEX)) ((0 == numAddrs) || (
    else if (HasNoIndex() && !SemanticIsSet(MULTIVALUE))
    {
        ASSERT(index < 0);
        if (index >= 0)
        {
            PLOG(PL_ERROR, "ManetTlv::SetValue() error: can't set indexed value for NO_INDEX TLV\n");
            return false;
        }
        index = 0;  // ignore index
    }
    else if (SemanticIsSet(SINGLE_INDEX))
    {
        if (index != GetIndexStart())
        {
            PLOG(PL_ERROR, "ManetTlv::SetValue() error: can't set multiple values for SINGLE_INDEX TLV\n");
            return false;
        }
        index = 0;
    }
    else
    {
        // TBD - add code to allow multivalue AddressBlock TLVs to have no index
        UINT8 startIndex = GetIndexStart();
        if ((index < startIndex) || (index > GetIndexStop(numAddrs)))
        {
            PLOG(PL_ERROR, "ManetTlv::SetValue() error: out-of-range \"index\"\n");
            return false; // (TBD) print a warning here?
        }
        else if (SemanticIsSet(MULTIVALUE))
        {
            index -= startIndex;
        }
        else
        {
            PLOG(PL_WARN, "ManetTlv::SetValue() warning: index specified for single value TLV?\n");
            index = 0;
        }
    }
    

    unsigned int offsetValue = OffsetValue();
    UINT16 oldTlvLength = GetTlvLength();
    unsigned int newTlvOffset = valueLength * index;
    UINT16 newTlvLength = newTlvOffset + valueLength;
    if (newTlvLength > oldTlvLength)
    {
        // Make sure the "value" will fit into our buffer space
        unsigned int newPktLength = offsetValue + newTlvLength;
        if ((newTlvLength >= 256) && (!SemanticIsSet(EXTENDED_LENGTH)))
            newPktLength++;
        if (newPktLength > buffer_bytes)
        {
            PLOG(PL_WARN, "ManetTlv::SetValue() value length exceeds packet buffer size\n");
            return false;
        }

        if ((newTlvLength >= 256) && (!SemanticIsSet(EXTENDED_LENGTH)))
        {
            // Length necessitates "extended length" TLV
            if (0 != oldTlvLength)
            {
                // Shift existing TLV content by one byte
                char* ptrOld = (char*)buffer_ptr + offsetValue;
                char* ptrNew = ptrOld + 1;
                memmove(ptrNew, ptrOld, oldTlvLength);
            }
            SetSemantic(EXTENDED_LENGTH);
            offsetValue++;
        }

        SetTlvLength(newTlvLength);
        pkt_length = newPktLength;
    }
    // Copy new value to its position
    memcpy((char*)buffer_ptr + offsetValue + newTlvOffset, value, valueLength);
    if (!SemanticIsSet(HAS_VALUE)) SetSemantic(HAS_VALUE);
    return true;
}  // end ManetTlv::SetValue()

void ManetTlv::SetTlvLength(UINT16 tlvLength)
{
    if (SemanticIsSet(EXTENDED_LENGTH))
        SetUINT16(OffsetLength(), tlvLength);
    else if (tlvLength < 256)
        SetUINT8(OffsetLength(), (UINT8)tlvLength);
    else
        PLOG(PL_ERROR, "ManetTlv::SetTlvLength() error: tlvLength exceeds non-extended maximum\n");
}  // end ManetTlv::SetTlvLength()

bool ManetTlv::InitFromBuffer(void* bufferPtr, unsigned numBytes)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes);
    UINT8 semantics = (buffer_bytes >= OFFSET_SEMANTICS) ? ((char*)buffer_ptr)[OFFSET_SEMANTICS] : 0;
    unsigned int minLength = GetMinLength(semantics);
    if (buffer_bytes < minLength)
    {
        pkt_length = 0;
        return false;
    }
    else
    {
        pkt_length =  minLength + GetTlvLength();
        return true;
    }
}  // end ManetTlv::InitFromBuffer()


UINT16 ManetTlv::GetTlvLength() const
{
    if (HasValue())
    {
        if (HasExtendedLength())
            return GetUINT16(OffsetLength());
        else
            return ((UINT16)GetUINT8(OffsetLength()));
    }
    else
    {
        return 0;
    }
}  // end ManetTlv::GetTlvLength()

UINT16 ManetTlv::GetValueLength(unsigned int numAddrs) const
{
    UINT16 tlvLength = GetTlvLength();
    if (IsMultiValue() && (0 != tlvLength))
    {
        ASSERT(GetIndexStop(numAddrs) >= GetIndexStart());
        int numberValues = GetIndexStop(numAddrs) - GetIndexStart() + 1;
        if (numberValues > 0)
        {
            if (0 != (tlvLength % numberValues))
            {
                PLOG(PL_WARN, "ManetTlv::GetValueLength() TLV value field not integral of num-values\n");
                return 0;
            }
            return (tlvLength / numberValues);
        }
        else
        {
            return 0; // (TBD) print warning here
        }
    }
    else
    {
        return tlvLength;
    }
}  // end ManetTlv::GetValueLength()

bool ManetTlv::GetValue(char* value, UINT16 valueLength, int index, unsigned int numAddrs) const
{
    const char* valuePtr = GetValuePtr(valueLength, index, numAddrs);
    if (NULL != valuePtr)
    {
        memcpy(value, valuePtr, valueLength);
        return true;
    }
    else
    {
        return false;
    }
}  // end ManetTlv::GetValue()


const char* ManetTlv::GetValuePtr(UINT16 valueLength, int index, unsigned int numAddrs) const
{
    if (!HasValue())
        return NULL; // (TBD) print a warning here?

    if (index < 0)
    {
        if (IsMultiValue())
        {
            PLOG(PL_ERROR, "ManetTlv::GetValuePtr() error: can't return non-indexed value for MultiValue TLV\n");
            return NULL;
        }
        index = 0;
    }
    else if (HasNoIndex() && (0 == numAddrs))
    {
        index = 0;  // ignore index
    }
    else if (HasSingleIndex())
    {
        if (index != GetIndexStart())
        {
            PLOG(PL_ERROR, "ManetTlv::GetValuePtr() error: invalid \"index\" for Single Index TLV\n");
            return NULL;
        }
        index = 0;
    }
    else
    {
        if (IsMultiValue() || HasIndex())
        {
            int startIndex = GetIndexStart();
            if ((index < startIndex) || (index > GetIndexStop(numAddrs)))
            {
                PLOG(PL_ERROR, "ManetTlv::GetValuePtr() error: invalid \"index\" out-of-range\n");
                return NULL; // (TBD) print a warning here?
            }
            if (IsMultiValue()) 
                index -= startIndex;
            else
                index = 0;
        }
        else
        {
            index = 0;  // ignore index for non-indexed, single value tlvs
        }
    }
    ASSERT((0 == index) || (valueLength == GetValueLength(numAddrs)));
    UINT16 offset = OffsetValue() + (index * valueLength);
    if ((offset + valueLength) > pkt_length)
    {
        PLOG(PL_ERROR, "ManetTlv::GetValuePtr() error: requested value exceeds packet size");
        return NULL;  // (TBD) print a warning here?
    }
    return ((char*)buffer_ptr + offset);
}  // end ManetTlv::GetValue()


ManetTlvBlock::ManetTlvBlock()
        : tlv_pending(false)
{
}

ManetTlvBlock::~ManetTlvBlock()
{
}

bool ManetTlvBlock::InitIntoBuffer(void* bufferPtr, unsigned int numBytes)
{
    tlv_pending = false;
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    if (buffer_bytes >= 2)
    {
        SetTlvBlockLength(0);
        pkt_length = 2;
        return true;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetTlvBlock::InitIntoBuffer()

ManetTlv* ManetTlvBlock::AppendTlv(UINT8 type)
{
    if (tlv_pending)
        pkt_length += tlv_temp.GetLength();
    tlv_pending =
        tlv_temp.InitIntoBuffer(type, (char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);

    return tlv_pending ? &tlv_temp : NULL;
}  // end ManetTlvBlock::AppendTlv()

bool ManetTlvBlock::AppendTlv(ManetTlv& tlv)
{
    if (tlv_pending)
    {
        pkt_length += tlv_temp.GetLength();
        tlv_pending = false;
    }
    unsigned int tlvLength = tlv.GetLength();
    unsigned int newLength = pkt_length + tlvLength;
    if (buffer_bytes < newLength) return false;
    memcpy((char*)buffer_ptr+pkt_length, (char*)tlv.GetBuffer(), tlvLength);
    pkt_length += tlvLength;
    return true;
}  // end ManetTlvBlock::AppendTlv()

void ManetTlvBlock::Pack()
{
    ASSERT(pkt_length >= 2);
    if (tlv_pending)
    {
        pkt_length += tlv_temp.GetLength();
        tlv_pending = false;
    }
    SetTlvBlockLength(pkt_length - 2);
}  // end ManetTlvBlock::Pack()

bool ManetTlvBlock::InitFromBuffer(void* bufferPtr, unsigned numBytes)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes);
    if (buffer_bytes >= 2)
    {
        pkt_length = GetTlvBlockLength() + OFFSET_CONTENT;
        return true;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetTlvBlock::InitFromBuffer()

bool ManetTlvBlock::GetNextTlv(ManetTlv& tlv) const
{
    char* currentBuffer = (char*)tlv.GetBuffer();
    unsigned int nextOffset;
    if (NULL == currentBuffer)
        nextOffset = OFFSET_CONTENT;
    else
        nextOffset = currentBuffer - (char*)buffer_ptr + tlv.GetLength();

    if (nextOffset < pkt_length)
        return tlv.InitFromBuffer((char*)buffer_ptr + nextOffset, pkt_length - nextOffset);
    else
        return false;
}  // end ManetTlvBlock::GetNextTlv()

ManetTlvBlock::Iterator::Iterator(ManetTlvBlock& tlvBlock)
        : tlv_block(tlvBlock)
{
}

ManetTlvBlock::Iterator::~Iterator()
{
}


ManetAddrBlock::ManetAddrBlock()
  : addr_length(0), tlv_block_pending(false)
{
}

ManetAddrBlock::ManetAddrBlock(void*        bufferPtr,
                               unsigned int numBytes,
                               bool         freeOnDestruct)
  : ProtoPkt(bufferPtr, numBytes, freeOnDestruct),
    addr_length(0), tlv_block_pending(false)
{
}

ManetAddrBlock::~ManetAddrBlock()
{
}



bool ManetAddrBlock::InitIntoBuffer(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    // minLength = num-addr field (1) + addr-semantics (1)
    unsigned int minLength = 2;
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (buffer_bytes < minLength) return false;
    addr_length = 0;
    // Init "num-addr" and "addr-semantics" fields
    SetAddressCount(0);
    ClearAllSemantics();
    tlv_block.AttachBuffer(NULL, 0);
    tlv_block_pending = false;
    pkt_length = 2;  // don't include tlv-block part yet.
    return true;
}  // end ManetAddrBlock::InitIntoBuffer()
bool ManetAddrBlock::SetHead(const ProtoAddress& addr, UINT8 hlen)
{
    // Re-init to be safe since head must be set first
    ASSERT(addr.IsValid());
    if ((hlen < 1) || (hlen > 127) || (hlen > addr.GetLength())) return false;
    // minLength: 1 (num-addr) + 1 (addr-semantics) + 1 (head-length) + hlen (head)
    unsigned int minLength = 3 + hlen;
    if (buffer_bytes < minLength) return false;
    // Save the addr_length, hlen, and head
    addr_length = addr.GetLength();
    // set HAS_HEAD semantic since there is a "head"
    SetSemantic(HAS_HEAD);
    SetUINT8(OFFSET_HEAD_LENGTH, hlen);
    memcpy((char*)buffer_ptr+ OFFSET_HEAD, addr.GetRawHostAddress(), hlen);
    pkt_length = OFFSET_HEAD + hlen;  // don't include tlv-block part yet.
    return true;
}  // end ManetAddrBlock::SetHead()

bool ManetAddrBlock::SetTail(const ProtoAddress& addr, UINT8 tlen)
{
    if (tlen > addr.GetLength()) return false;
    unsigned int hlen = GetHeadLength();
    if (0 == hlen)
    {
        // In case SetTail() is called first
        //if (!InitIntoBuffer((char*)buffer_ptr, buffer_bytes)) return false;
        addr_length = addr.GetLength();
    }
    else if (addr_length != addr.GetLength())
    {
        return false;
    }
    // minLength: 1 (num-addr) + 1 (addr-semantics) + [1 (head-length) + hlen (head)] + 1 (tail-length) + tlen (tail)
    unsigned int minLength = 2 + (hlen ? (hlen + 1) : 0) + 1 + tlen;
    if (tlen > 0)
    {
        // Is it an all-zero tail?
        bool zeroTail = true;
        const char* tailPtr = addr.GetRawHostAddress() + addr.GetLength() - tlen;
        for (UINT8 i = 0; i < tlen; i++)
        {
            if (0 != tailPtr[i])
            {
                zeroTail = false;
                break;
            }
        }
        unsigned int offsetTail = OffsetTail();
        if (zeroTail)
        {
            minLength -= tlen;
            if (minLength > buffer_bytes) return false;
            SetSemantic(HAS_ZERO_TAIL);
            pkt_length = offsetTail;   // don't include tlv-block part yet.
        }
        else
        {
            if (minLength > buffer_bytes) return false;
            memcpy((char*)buffer_ptr + offsetTail, tailPtr, tlen);
            pkt_length = offsetTail + tlen;  // don't include tlv-block part yet.
            SetSemantic(HAS_FULL_TAIL);
        }
        SetUINT8(OffsetTailLength(), tlen);
    }
    else
    {
        minLength -= 1;  // don't need "tail-length" field
        ClearSemantic(HAS_ZERO_TAIL);
        ClearSemantic(HAS_FULL_TAIL);
        pkt_length = HasHead() ? (OFFSET_HEAD + hlen) : (OFFSET_SEMANTICS + 1);  // don't include tlv-block part yet.
    }
    return true;
}  // end ManetAddrBlock::SetTail()

bool ManetAddrBlock::AppendAddress(const ProtoAddress& addr)
{
    // (TBD) Should we make sure this addr prefix matches our "head" _and_ "tail"
    if (!HasHead() && !HasTail())
    {
        addr_length = addr.GetLength();
    }
    else if (addr.GetLength() != addr_length)
    {
        PLOG(PL_ERROR,"ManetAddrBlock::AppendAddress() error appending address %s because the address length %d is not equal to the message addr length %d!\n",addr.GetHostString(),addr.GetLength(),addr_length);
        return false;
    }
    UINT8 mlen = GetMidLength();
    if (0 == mlen)
    {
        PLOG(PL_ERROR, "ManetAddrBlock::AppendAddress() error: address mid-length is zero\n");
        return false;
    }
    UINT8 numAddr = GetAddressCount();
    unsigned int offset = (0 == numAddr) ? OffsetMid() : pkt_length;
    unsigned int minLength = offset + mlen;
    if (buffer_bytes < minLength) 
    {
        PLOG(PL_WARN,"ManetAddrBlock::AppendAddress() warn: buffer_bytes(%u) < minLength(%u) no room for packing message\n",buffer_bytes,minLength);
        return false;
    }
    memcpy((char*)buffer_ptr + offset, addr.GetRawHostAddress() + GetHeadLength(), mlen);
    ((UINT8*)buffer_ptr)[OFFSET_NUM_ADDR]++;
    pkt_length = offset + mlen;
    return true;
}  // end ManetAddrBlock::AppendAddress()

bool ManetAddrBlock::SetPrefixLength(UINT8 length, UINT8 index)
{
    ASSERT(length <= (GetAddressLength() << 3));
    if (0 == GetAddressCount())
    {
        // Check for "head-only" or "tail-only" address block
        if ((0 == GetHeadLength()) && (0 == GetTailLength()))
            return false;
        else
            SetAddressCount(1);
    }
    ASSERT(index < GetAddressCount());
    unsigned int offset = OffsetPrefixLength() + index;
    unsigned int newLength =  offset + 1;
    if (newLength > buffer_bytes) return false;
    if (!HasPrefixLength() && (0 == index))
    {
        SetSemantic(HAS_SINGLE_PREFIX_LEN);
    }
    else if (!HasMultiPrefixLength() && (index > 0))
    {
        ClearSemantic(HAS_SINGLE_PREFIX_LEN);
        SetSemantic(HAS_MULTI_PREFIX_LEN);
    }
    SetUINT8(offset, length);
    if (pkt_length < newLength) pkt_length = newLength;
    return true;
}  // end ManetAddrBlock::SetPrefixLength()

ManetTlv* ManetAddrBlock::AppendTlv(UINT8 type)
{
    if (0 == GetAddressCount())
    {
        // Check for "head-only" or "tail-only" address block
        if ((0 == GetHeadLength()) && (0 == GetTailLength()))
            return NULL;
        else
            SetAddressCount(1);
    }
    if (!tlv_block_pending)
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);
    return tlv_block_pending ? tlv_block.AppendTlv(type) : NULL;
}  // end ManetAddrBlock::AppendTlv()

bool ManetAddrBlock::AppendTlv(ManetTlv& tlv)
{
    if (0 == GetAddressCount())
    {
        // Check for "head-only" or "tail-only" address block
        if ((0 == GetHeadLength()) && (0 == GetTailLength()))
            return false;
        else
            SetAddressCount(1);
    }
    if (!tlv_block_pending)
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);
    return (tlv_block_pending ? tlv_block.AppendTlv(tlv) : false);
}  // end ManetAddrBlock::AppendTlv()

void ManetAddrBlock::Pack()
{
    if (0 == GetAddressCount())
    {
        // Check for "head-only" or "tail-only" address block
        if ((0 == GetHeadLength()) && (0 == GetTailLength()))
        {
            pkt_length = 0;
            return;
        }
        else
        {
            SetAddressCount(1);
        }
    }
    if (!tlv_block_pending)
        tlv_block.InitIntoBuffer((char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);
    tlv_block.Pack();
    pkt_length += tlv_block.GetLength();
    tlv_block_pending = false;
}  // end ManetAddrBlock::Pack()

bool ManetAddrBlock::InitFromBuffer(UINT8 addrLength, void* bufferPtr, unsigned numBytes)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes);
    addr_length = addrLength;
    pkt_length = 0;
    unsigned int minLength = 2;
    if (buffer_bytes < minLength) return false;
    if (HasHead())
    {
        minLength +=1;
        if (buffer_bytes < minLength) return false;
        minLength += GetHeadLength();
        if (buffer_bytes < minLength) return false;
    }
    if (HasTail())
    {
        minLength += 1;
        if (buffer_bytes < minLength) return false;
        minLength += HasZeroTail() ? 0 : GetTailLength();
        if (buffer_bytes < minLength) return false;
    }

    // Make sure big enough for any listed address mid-sections, prefixes,
    // and at least tlv-block-length
    minLength += GetAddressCount() * GetMidLength();
    if (buffer_bytes < (minLength + 2)) return false;
    if (HasPrefixLength())
    {
        minLength += HasSinglePrefixLength() ? 1 : GetAddressCount();
        if (buffer_bytes < minLength) return false;
    }
    if (tlv_block.InitFromBuffer((char*)buffer_ptr+minLength, buffer_bytes - minLength))
    {
        pkt_length = minLength + tlv_block.GetLength();
        return true;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetAddrBlock::InitFromBuffer()

bool ManetAddrBlock::GetAddress(UINT8 index, ProtoAddress& theAddr) const
{
    ASSERT(index < GetAddressCount());
    char addrBuffer[16]; // supports up to IPv6
    // Copy head into addrBuffer
    UINT8 hlen = GetHeadLength();
    ASSERT(hlen <= GetAddressLength());
    if (hlen > 0)
        memcpy(addrBuffer, (char*)buffer_ptr+OFFSET_HEAD, hlen);
    // Copy mid into addrBuffer
    UINT8 mlen = GetMidLength();
    ASSERT(mlen <= GetAddressLength());
    if (mlen > 0)
        memcpy(addrBuffer+hlen, (char*)buffer_ptr + OffsetMid() + index*GetMidLength(), mlen);
    UINT8 tlen = GetTailLength();
    // Copy tail into addrBuffer
    if (tlen > 0)
    {
        if (HasZeroTail())
            memset(addrBuffer+hlen+mlen, 0, tlen);
        else
            memcpy(addrBuffer+hlen+mlen, (char*)buffer_ptr + OffsetTail(), tlen);
    }
    ASSERT((hlen + mlen + tlen) == GetAddressLength());
    theAddr.SetRawHostAddress(GetAddressType(), addrBuffer, GetAddressLength());
    return theAddr.IsValid();
}  // end ManetAddrBlock::GetAddress()

ManetAddrBlock::TlvIterator::TlvIterator(ManetAddrBlock& addrBlk)
        : ManetTlvBlock::Iterator(addrBlk.tlv_block)
{
}

ManetAddrBlock::TlvIterator::~TlvIterator()
{
}

ManetMsg::ManetMsg()
{
}

ManetMsg::ManetMsg(void*        bufferPtr,
                   unsigned int numBytes,
                   bool         freeOnDestruct)
  : ProtoPkt(bufferPtr, numBytes, freeOnDestruct),
    tlv_block_pending(false), addr_block_pending(false)
{
}

ManetMsg::~ManetMsg()
{
}

bool ManetMsg::InitIntoBuffer(void* bufferPtr, unsigned int numBytes)
{
    addr_block_pending = false;
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    // At least 4 bytes required for "type", "semantics" and "size"
    if (buffer_bytes >= 4)
    {
        pkt_length = 4;
        ClearAllSemantics();  // addr length & semantic flags will be set w/ corresponding 'Set' calls
        // Assume initial msg-tlv-block location
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr+4, buffer_bytes-4);
        return tlv_block_pending;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetMsg::InitIntoBuffer()

bool ManetMsg::SetOriginator(const ProtoAddress& addr)
{
    if (!addr.IsValid())
    {
        PLOG(PL_ERROR, "ManetMsg::SetOriginator() error: invalid address!\n");
        return false;
    }
    UINT8 addrLen = addr.GetLength();
    SetAddressLength(addrLen);
    unsigned int offsetOriginator = OffsetOriginator();
    if (!HasOriginator())
    {
        UINT16 newLength = offsetOriginator + addrLen;
        if (newLength > buffer_bytes) return false;
        // Try to move the msg-tlv-block location
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr+newLength, buffer_bytes-newLength);
        if (tlv_block_pending)
        {
            pkt_length = newLength;
            SetSemantic(HAS_ORIGINATOR);
        }
        else
        {
            return false;
        }
    }
    memcpy((char*)buffer_ptr + offsetOriginator, addr.GetRawHostAddress(), addrLen);
    return true;
}  // end ManetMsg::SetOriginator()

bool ManetMsg::SetHopLimit(UINT8 hopLimit)
{
    unsigned int offsetHopLimit = OffsetHopLimit();
    if (!HasHopLimit())
    {
        UINT16 newLength = offsetHopLimit + 1;
        if (newLength > buffer_bytes) return false;
        // Try to move the msg-tlv-block location
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr+newLength, buffer_bytes-newLength);
        if (tlv_block_pending)
        {
            pkt_length = newLength;
            SetSemantic(HAS_HOP_LIMIT);
        }
        else
        {
            return false;
        }
    }
    SetUINT8(offsetHopLimit, hopLimit);
    return true;
}  // end ManetMsg::SetHopLimit()

bool ManetMsg::SetHopCount(UINT8 hopCount)
{
    unsigned int offsetHopCount = OffsetHopCount();
    if (!HasHopCount())
    {
        UINT16 newLength = offsetHopCount + 1;
        if (newLength > buffer_bytes) return false;
        // Try to move the msg-tlv-block location
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr+newLength, buffer_bytes-newLength);
        if (tlv_block_pending)
        {
            pkt_length = newLength;
            SetSemantic(HAS_HOP_COUNT);
        }
        else
        {
            return false;
        }
    }
    SetUINT8(offsetHopCount, hopCount);
    return true;
}  // end ManetMsg::SetHopCount()

bool ManetMsg::SetSequence(UINT16 sequence)
{
    unsigned int offsetSequence = OffsetSequence();
    if (!HasSequence())
    {
        UINT16 newLength = offsetSequence + 2;
        if (newLength > buffer_bytes) return false;
        // Try to move the msg-tlv-block location
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr+newLength, buffer_bytes-newLength);
        if (tlv_block_pending)
        {
            pkt_length = newLength;
            SetSemantic(HAS_SEQ_NUM);
        }
        else
        {
            return false;
        }
    }
    SetUINT8(offsetSequence, sequence);
    return true;
}  // end ManetMsg::SetSequence()

ManetAddrBlock* ManetMsg::AppendAddressBlock()
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (addr_block_pending)
    {
        addr_block_temp.Pack();
        pkt_length += addr_block_temp.GetLength();
    }
    unsigned int bufferMax = (buffer_bytes > pkt_length) ? buffer_bytes - pkt_length : 0;
    char* bufferPtr = (char*)buffer_ptr + pkt_length;
    addr_block_pending = addr_block_temp.InitIntoBuffer(bufferPtr, bufferMax);
    return (addr_block_pending ? &addr_block_temp : NULL);
}  // end ManetMsg::AppendAddressBlock()

void ManetMsg::Pack()
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (addr_block_pending)
    {
        addr_block_temp.Pack();
        pkt_length += addr_block_temp.GetLength();
        addr_block_pending = false;
    }
    SetMsgSize((UINT16)pkt_length);  // includes message header _and_ body
}  // end ManetMsg::Pack()

bool ManetMsg::InitFromBuffer(void* bufferPtr, unsigned int numBytes)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes);
    if (buffer_bytes < 1) return false;  // not even enough for type/semantic fields
    //pkt_length = (buffer_bytes >= (OffsetSize() + 2)) ? GetMsgSize() : 0;
    pkt_length = GetMsgSize();

    if ((0 == pkt_length) || (pkt_length > buffer_bytes))
    {
        PLOG(PL_ERROR, "ManetMsg::InitFromBuffer() error: msg size:%u larger than buffer size:%u\n",
             pkt_length, buffer_bytes);
        pkt_length = 0;
        return false;
    }
    UINT16 tlvBlockOffset = OffsetTlvBlock();
    if (pkt_length < tlvBlockOffset)
    {
        PLOG(PL_ERROR, "ManetMsg::InitFromBuffer() error: msg too short?!\n");
        pkt_length = 0;
        return false;
    }
    if (tlv_block.InitFromBuffer((char*)buffer_ptr + tlvBlockOffset, pkt_length - tlvBlockOffset))
    {
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ManetMsg::InitFromBuffer() error: invalid msg-tlv-block?!\n");
        pkt_length = 0;
        return false;
    }
}  // end ManetMsg::InitFromBuffer()

bool ManetMsg::GetOriginator(ProtoAddress& addr) const
{
    if (HasOriginator())
    {
        UINT8 addrLen = GetAddressLength();
        addr.SetRawHostAddress(ProtoAddress::GetType(addrLen),  // infers from msg-addr-length 
                               (char*)buffer_ptr+OffsetOriginator(), 
                               addrLen);
    }
    else
    {
        addr.Invalidate();
    }
    return addr.IsValid();
}  // ManetMsg::GetOriginator()

bool ManetMsg::GetNextAddressBlock(ManetAddrBlock& addrBlk) const
{
    char* nextBuffer = (char*)addrBlk.GetBuffer();
    if (NULL == nextBuffer)
    {
        // Get _first_ address block
        ManetTlvBlock tlvBlk;
        if (GetTlvBlock(tlvBlk))
            nextBuffer = (char*)tlvBlk.GetBuffer() + tlvBlk.GetLength();
        else
            return false; // there was no <msg-tlv-block>
    }
    else
    {
        nextBuffer += addrBlk.GetLength();
    }
    unsigned int offset = nextBuffer - (char*)buffer_ptr;
    if (offset < pkt_length)
        return addrBlk.InitFromBuffer(GetAddressLength(), nextBuffer, pkt_length - offset);
    else
        return false;
}  // end ManetMsg::GetNextAddressBlock()


ManetMsg::TlvIterator::TlvIterator(ManetMsg& msg)
        : ManetTlvBlock::Iterator(msg.tlv_block)
{
}

ManetMsg::TlvIterator::~TlvIterator()
{
}

ManetMsg::AddrBlockIterator::AddrBlockIterator(ManetMsg& theMsg)
        : msg(theMsg)
{
}

ManetMsg::AddrBlockIterator::~AddrBlockIterator()
{
}


ManetPkt::ManetPkt()
{
}

ManetPkt::ManetPkt(void*        bufferPtr,
                   unsigned int numBytes,
                   bool         freeOnDestruct)
  : ProtoPkt(bufferPtr, numBytes, freeOnDestruct),
    tlv_block_pending(false), msg_pending(false)
{
}

ManetPkt::~ManetPkt()
{
}

bool ManetPkt::InitIntoBuffer(void* bufferPtr, unsigned int numBytes)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    if (buffer_bytes < 1) return false;
    ClearAllSemantics();
    //ClearOrderConstraint();   // cleared by default because we don't like it
    pkt_length = 1;
    tlv_block_pending = false;
    msg_pending = false;
    return true;
}  // end ManetPkt::InitIntoBuffer()

void ManetPkt::SetVersion(UINT8 version)
{
    uint8_t flags = GetUINT8(OFFSET_SEMANTICS) & 0x0F; // clear version
    SetUINT8(OFFSET_SEMANTICS, (version << 4) | flags);
}  // end ManetPkt::SetVersion()

void ManetPkt::SetSequence(UINT16 sequence)
{
    if (!SemanticIsSet(HAS_SEQ_NUM))
    {
        SetSemantic(HAS_SEQ_NUM);
        pkt_length += 2;
    }
    SetUINT16(OffsetSequence(), sequence);
}  // end ManetPkt::SetSequence()

ManetTlv* ManetPkt::AppendTlv(UINT8 type)
{
    if (msg_pending)
        return NULL; // Can't append Tlv after appending a message!
    if (!tlv_block_pending)
    {
        // Set semantic and init tlv_block
        SetSemantic(HAS_TLV_BLOCK);
        unsigned int offsetTlvBlock = OffsetTlvBlock();
        tlv_block_pending =
            tlv_block.InitIntoBuffer((char*)buffer_ptr+offsetTlvBlock, buffer_bytes-offsetTlvBlock);
    }
    return (tlv_block_pending ? tlv_block.AppendTlv(type) : NULL);
}  // end ManetPkt::AppendTlv()

bool ManetPkt::AppendTlv(ManetTlv& tlv)
{
    if (msg_pending)
        return false; // Can't append Tlv after appending a message!
    if (!tlv_block_pending)
    {
        // Set semantic and init tlv_block
        SetSemantic(HAS_TLV_BLOCK);
        unsigned int offsetTlvBlock = OffsetTlvBlock();
        tlv_block_pending =
            tlv_block.InitIntoBuffer((char*)buffer_ptr+offsetTlvBlock, buffer_bytes-offsetTlvBlock);
    }
    return (tlv_block_pending ? tlv_block.AppendTlv(tlv) : false);
}  // end ManetPkt::AppendTlv()

ManetMsg* ManetPkt::AppendMessage()
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (msg_pending)
    {
        msg_temp.Pack();
        pkt_length += msg_temp.GetLength();
    }
    unsigned int bufferMax = (buffer_bytes > pkt_length) ? buffer_bytes - pkt_length : 0;
    char* bufferPtr = (char*)buffer_ptr + pkt_length;
    msg_pending = msg_temp.InitIntoBuffer(bufferPtr, bufferMax);
    return msg_pending ? &msg_temp : NULL;
}  // end ManetPkt::AppendMessage()


bool ManetPkt::AppendMessage(ManetMsg& msg)
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (msg_pending)
    {
        msg_temp.Pack();
        pkt_length += msg_temp.GetLength();
    }
    msg.Pack();  // just to be safe
    unsigned int msgLength = msg.GetLength();
    unsigned int newLength = pkt_length + msgLength;
    if (buffer_bytes < newLength) return false;
    memcpy((char*)buffer_ptr + pkt_length, (char*)msg.GetBuffer(), msgLength);
    pkt_length = newLength;
    return true;
}  // end ManetPkt::AppendMessage()


void ManetPkt::Pack()
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (msg_pending)
    {
        msg_temp.Pack();
        pkt_length += msg_temp.GetLength();
        msg_pending = false;
    }
    //if (HasSize()) SetPktSize(pkt_length);
}  // end ManetPkt::Pack()

bool ManetPkt::InitFromBuffer(unsigned int pktLength, void* bufferPtr, unsigned int numBytes)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    msg_pending = false;
    if ((pktLength < 1) || (buffer_bytes < pktLength))
        return false;
    if (pktLength < GetMinLength(GetUINT8(OFFSET_SEMANTICS)))
        return false;
    else
        pkt_length = pktLength;

    if (HasTlvBlock())
    {
        unsigned tlvBlockOffset = OffsetTlvBlock();
        char* tlvBuffer =  (char*)buffer_ptr + tlvBlockOffset;
        return tlv_block.InitFromBuffer(tlvBuffer, pkt_length - tlvBlockOffset);
    }
    else
    {
        tlv_block.InitFromBuffer(NULL, 0);
        return true;
    }
}  // end ManetPkt::InitFromBuffer()

bool ManetPkt::GetNextMessage(ManetMsg& msg)
{
    char* currentBuffer = (char*)msg.GetBuffer();
    char* nextBuffer = currentBuffer ?
                       (currentBuffer + msg.GetLength()) :
                       ((char*)buffer_ptr + OffsetPayload());
    unsigned int nextOffset = nextBuffer - (char*)buffer_ptr;
    if (nextOffset < pkt_length)
    {
        return msg.InitFromBuffer(nextBuffer, pkt_length - nextOffset);
    }
    else
        return false;  // no more messages in packet
}  // end ManetPkt::GetNextMessage()

ManetPkt::TlvIterator::TlvIterator(ManetPkt& pkt)
        : ManetTlvBlock::Iterator(pkt.tlv_block)
{
}

ManetPkt::TlvIterator::~TlvIterator()
{
}

ManetPkt::MsgIterator::MsgIterator(ManetPkt& thePkt)
        : pkt(thePkt)
{
}

ManetPkt::MsgIterator::~MsgIterator()
{
}
