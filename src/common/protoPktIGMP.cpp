#include "protoPktIGMP.h"

const UINT8 ProtoPktIGMP::DEFAULT_QRV = 2;       // default query robustness value
const double ProtoPktIGMP::DEFAULT_QQIC = 125.0; // default query interval (seconds)
const double ProtoPktIGMP::DEFAULT_MAX_RESP = 10.0; // default query response time (seconds)

ProtoPktIGMP::ProtoPktIGMP(void*          bufferPtr, 
                           unsigned int   numBytes, 
                           bool           initFromBuffer,
                           bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) InitFromBuffer(numBytes);
}
 
ProtoPktIGMP::~ProtoPktIGMP()
{
}

bool ProtoPktIGMP::InitFromBuffer(UINT16        pktLength,
                                  void*         bufferPtr, 
                                  unsigned int  bufferBytes, 
                                  bool          freeOnDestruct)
{
    unsigned int minLength = OFFSET_RESERVED;
    if (NULL != bufferPtr)
    {
        if (bufferBytes < minLength)  // IGMPv2 msg size
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    if (GetBufferLength() < pktLength)
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::InitFromBuffer() error: insufficient buffer size\n");
        return false;
    }
    SetLength(pktLength);
    if (GetVersion() > 2)
    {
        minLength = OFFSET_SRC_LIST;
        if (pktLength <  minLength)
        {
            PLOG(PL_ERROR, "ProtoPktIGMP::InitFromBuffer() error: invalid IGMPv3 packet\n");
            return false;
        }
        minLength += 4*GetNumSources();
        if (pktLength <  minLength)
        {
            PLOG(PL_ERROR, "ProtoPktIGMP::InitFromBuffer() error: truncated IGMPv3 packet\n");
            return false;
        }
    }
    return true;
}  // end ProtoPktIGMP::InitFromBuffer()

UINT8 ProtoPktIGMP::GetVersion() const
{
    switch (GetType())
    {
        case REPORT_V1:
            return 1;
        case REPORT_V2:
            return 2;
        case REPORT_V3:
            return 3;
        case QUERY:
        case LEAVE:
            if (GetLength() > OFFSET_RESERVED)
                return 3;
            else
                return 2;
        default:
            return 0;
    }
}  // end ProtoPktIGMP::GetVersion() 

double ProtoPktIGMP::GetMaxResponseTime() const
{
    UINT8 value = GetUINT8(OFFSET_MAX_RESP);
    if ((value < 128) || (GetVersion() < 3))
    {
        // value is in units of 0.1 seconds
        return (0.1*(double)value);
    }
    else
    {
        unsigned int exp = (value & 0x70) >> 4;
        unsigned int mant = value & 0x0f;
        unsigned int maxRespTime = (mant | 0x10) << (exp + 3);
        return (0.1*(double)maxRespTime);
    }
}  // end ProtoPktIGMP::GetMaxResponseTime()

double ProtoPktIGMP::GetQQIC() const
{
    UINT8 qqic = GetUINT8(OFFSET_QQIC);
    if (qqic < 128)
    {
        return (double)qqic;
    }
    else
    {
        unsigned int exp = (qqic & 0x70) >> 4;
        unsigned int mant = qqic & 0x0f;
        return (double)((mant | 0x10) << (exp + 3));
    }
}  // end ProtoPktIGMP::GetQQIC()

bool ProtoPktIGMP::GetSourceAddress(UINT16 index, ProtoAddress& srcAddr) const
{
    if (index <  GetNumSources())
    {
        srcAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer(OFFSET_SRC_LIST + index*4), 4);
        return true;
    }
    else
    {
        srcAddr.Invalidate();
        return false;
    }
}  // end ProtoPktIGMP::GetSourceAddress()

bool ProtoPktIGMP::GetNextGroupRecord(ProtoPktIGMP::GroupRecord& groupRecord, bool first)
{
    if (0 == GetNumRecords()) return false;
    void* recordPtr;
    unsigned int bufferSpace;
    if (first || (NULL == groupRecord.GetBuffer()))
    {
        // Point to first record
        recordPtr = AccessBuffer(OFFSET_REC_LIST);
        bufferSpace = GetLength() - OFFSET_REC_LIST;
    }    
    else
    {   
        recordPtr = groupRecord.AccessBuffer(groupRecord.GetLength());
        // Make sure it's in scope of this IGMP message size.
        size_t offset = (char*)recordPtr - (char*)GetBuffer();
        if (offset > GetLength())
            return false;  // out of bounds
        bufferSpace = GetLength() - (unsigned int)offset;
    }
    if (0 == bufferSpace)
        return false;
    else
        return groupRecord.InitFromBuffer(recordPtr, bufferSpace);
}  // end ProtoPktIGMP::GetGroupRecord()

bool ProtoPktIGMP::InitIntoBuffer(Type         type,
                                  unsigned int version,
                                  void*        bufferPtr, 
                                  unsigned int bufferBytes, 
                                  bool         freeOnDestruct)
{
    UINT16 minLength ;
    switch (type)
    {
        case QUERY:
            if (version < 3)
                minLength = OFFSET_RESERVED;
            else
                minLength = OFFSET_SRC_LIST;
        case REPORT_V1:
            version = 1;
            minLength = OFFSET_RESERVED;
            break;
        case REPORT_V2:
        case LEAVE:
            version = 2;
            minLength = OFFSET_RESERVED;
            break;
        case REPORT_V3:
            version = 3;
            minLength = OFFSET_REC_LIST;
            break;
        default:
            break;
    }
    if (NULL != bufferPtr) 
    {
        if (bufferBytes < minLength)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    if (GetBufferLength() < minLength) return false;
    memset(AccessBuffer(), 0, minLength);
    SetUINT8(OFFSET_TYPE, type);
    SetLength(minLength);
    if (QUERY == type)
    {
        SetMaxResponseTime(DEFAULT_MAX_RESP);
        if (3 == version)
        {
            SetQRV(DEFAULT_QRV);
            SetQQIC(DEFAULT_QQIC);
        }
    }
    return true;
    
}  // end ProtoPktIGMP::InitIntoBuffer()

void ProtoPktIGMP::SetMaxResponseTime(double seconds, bool updateChecksum)
{
    seconds *= 10.0;  // convert to 1/10 sec units
    UINT8 code;
    if (seconds < 128)
    {
        code = (unsigned int)seconds;
    }
    else if (seconds > 31743.0)
    {
        code = 255;
    }
    else
    {
        unsigned int value = (unsigned int)seconds;
        unsigned exp = 0;
        value >>= 3;
        while (value > 31)
        {
            exp++;
            value >>= 1;
        }
        exp <<= 4;
        code = (0x80 | exp | (value & 0x0f));
        
    }
    SetUINT8(OFFSET_MAX_RESP, code);
    if (updateChecksum) ComputeChecksum();
}  // end ProtoPktIGMP::SetMaxResponseTime()

void ProtoPktIGMP::SetGroupAddress(ProtoAddress* groupAddr, bool updateChecksum)
{
    void* ptr = AccessBuffer(OFFSET_GROUP);
    if ((NULL == groupAddr) || (ProtoAddress::IPv4 != groupAddr->GetType()) || (!groupAddr->IsMulticast()))
        memset(ptr, 0, 4);
    else 
        memcpy(ptr, groupAddr->GetRawHostAddress(), 4);
    if (updateChecksum) ComputeChecksum();
}  // end ProtoPktIGMP::SetGroupAddress()


bool ProtoPktIGMP::SetSuppress(bool state, bool updateChecksum)
{
    if (GetBufferLength() <= OFFSET_RESERVED)
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::SetSuppress() error: insufficient buffer space\n");
        return false;
    }
    UINT8 code = GetUINT8(OFFSET_S);
    if (state)
        code |= 0x08;
    else
        code = (code & ~0x08) & 0x0f;
    SetUINT8(OFFSET_S, code);
    if (updateChecksum) ComputeChecksum();
    return true;
}  // end ProtoPktIGMP::SetSuppress()

bool ProtoPktIGMP::SetQRV(UINT8 qrv, bool updateChecksum)
{
    if (GetBufferLength() <= OFFSET_RESERVED)
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::SetQRV() error: insufficient buffer space\n");
        return false;
    }
    UINT8 code = GetUINT8(OFFSET_QRV);
    code &= 0x08;  // clear old qrv
    qrv &= 0x07;   // mask new qrv
    code |= qrv;   // set new qrv
    SetUINT8(OFFSET_QRV, code);
    if (updateChecksum) ComputeChecksum();
    return true;
}  // end ProtoPktIGMP::SetQRV()

bool ProtoPktIGMP::SetQQIC(double seconds, bool updateChecksum)
{
    if (GetBufferLength() <= OFFSET_RESERVED)
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::SetQQIC() error: insufficient buffer space\n");
        return false;
    }
    UINT8 code;
    if (seconds < 128)
    {
        code = (unsigned int)seconds;
    }
    else if (seconds > 31743.0)
    {
        code = 255;
    }
    else
    {
        unsigned int value = (unsigned int)seconds;
        unsigned exp = 0;
        value >>= 3;
        while (value > 31)
        {
            exp++;
            value >>= 1;
        }
        exp <<= 4;
        code = (0x80 | exp | (value & 0x0f));
    }
    SetUINT8(OFFSET_QQIC, code);
    if (updateChecksum) ComputeChecksum();
    return true;
}  // end ProtoPktIGMP::SetQQIC()

bool ProtoPktIGMP::AppendSourceAddress(ProtoAddress& srcAddr, bool updateChecksum)
{
    if ((ProtoAddress::IPv4 != srcAddr.GetType()) || !srcAddr.IsUnicast())
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::AppendSourceAddress() error: invalid source address\n");
        return false;
    }
    // Is there room for another source address?
    UINT16 numSrc = GetNumSources();
    unsigned int offset = OFFSET_SRC_LIST + 4*numSrc;
    ASSERT(offset == GetLength());
    unsigned int newLength = offset + 4;
    if (newLength > GetBufferLength())
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::AppendSourceAddress() error: insufficient buffer space\n");
        return false;
    }
    memcpy(AccessBuffer(offset), srcAddr.GetRawHostAddress(), 4);
    SetUINT16(OFFSET_NUM_SRC, numSrc + 1);
    SetLength(newLength);
    if (updateChecksum) ComputeChecksum();
    return true;
}  // end ProtoPktIGMP::AppendSourceAddress()

// This inits the groupRecord into buffer space at the
// end of the IGMP message being built (Need to call AppendGroupRecord() to commit it)
// (avoids need for additional allocation and copying when building up message)
bool ProtoPktIGMP::AttachGroupRecord(GroupRecord& groupRecord)
{
    UINT32 currentLength = GetLength();
    ASSERT(0 == (currentLength & 0x00000003));  // should be multiple of 4
    void* bufferPtr = AccessBuffer(currentLength);
    unsigned int bufferSpace = GetBufferLength() - currentLength;
    if (!groupRecord.InitIntoBuffer(bufferPtr, bufferSpace))
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::AttachGroupRecord() error: insufficient buffer space\n");
        return false;
    }
    return true;
}  // end ProtoPktIGMP::AttachGroupRecord()

bool ProtoPktIGMP::AppendGroupRecord(const GroupRecord& groupRecord, bool updateChecksum)
{
    UINT32 currentLength = GetLength();
    ASSERT(0 == (currentLength & 0x00000003));  // should be multiple of 4
    unsigned int bufferSpace = GetBufferLength() - currentLength;
    if (bufferSpace < groupRecord.GetLength())
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::AppendGroupRecord() error: insufficient buffer space\n");
        return false;
    }    
    void* ptr = AccessBuffer(currentLength);
    // Check if we need to copy since this is _not_ an attached group record
    if (ptr != groupRecord.GetBuffer())
        memcpy(ptr, groupRecord.GetBuffer(), groupRecord.GetLength());
    SetUINT16(OFFSET_NUM_REC, GetNumRecords() + 1);    
    SetLength(currentLength + groupRecord.GetLength());
    if (updateChecksum) ComputeChecksum();
    return true;
}  // end ProtoPktIGMP::AppendGroupRecord()

UINT16 ProtoPktIGMP::ComputeChecksum(bool set)
{
    UINT32 sum = 0;
    if (set) SetUINT16(OFFSET_CHECKSUM, (UINT16)0);
    // Compute before checksum
    unsigned int end = OFFSET_CHECKSUM/2;
    end = GetLength() / 2;
    for (unsigned int i = 0; i < end; i++)
        sum += GetWord16(i);
    
    // Carry as needed
    while (0 != (sum >> 16))
        sum = (sum & 0x0000ffff) + (sum >> 16);
    // Complement
    sum = ~sum;
    // ZERO check/correct as needed
    if (0 == sum) sum = 0x0000ffff;
    if (set) SetUINT16(OFFSET_CHECKSUM, (UINT16)sum);
    return (UINT16)sum;
}  // end ProtoPktIGMP::ComputeChecksum()

ProtoPktIGMP::GroupRecord::GroupRecord(void*          bufferPtr, 
                                       unsigned int   numBytes, 
                                       bool           initFromBuffer,
                                       bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer)
        InitFromBuffer();
    else
        InitIntoBuffer();
}

ProtoPktIGMP::GroupRecord::~GroupRecord()
{
}

bool ProtoPktIGMP::GroupRecord::InitFromBuffer(void*        bufferPtr, 
                                               unsigned int bufferBytes, 
                                               bool         freeOnDestruct)
{
    unsigned int minLength = OFFSET_SRC_LIST;
    if (NULL != bufferPtr)
    {
        if (bufferBytes < minLength)  // IGMPv2 msg size
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    minLength += GetAuxDataLen();
    minLength += 4*GetNumSources();
    if (GetBufferLength() < minLength)
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::GroupRecord::InitFromBuffer() error: truncated IGMPv3 group record?!\n");
        return false;
    }
    SetLength(minLength);
    return true;
}  // end ProtoPktIGMP::GroupRecord::InitFromBuffer()


bool ProtoPktIGMP::GroupRecord::GetSourceAddress(UINT16 index, ProtoAddress& srcAddr) const
{
    if (index >= GetNumSources())
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::GroupRecord::GetSourceAddress() error: index out of range\n");
        srcAddr.Invalidate();        
        return false;
    }
    const void* ptr = GetBuffer(OFFSET_SRC_LIST + (index << 2));
    srcAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)ptr, 4);
    return true;
}  // end ProtoPktIGMP::GroupRecord::GetSourceAddress()

bool ProtoPktIGMP::GroupRecord::InitIntoBuffer(void*        bufferPtr, 
                                               unsigned int bufferBytes, 
                                               bool         freeOnDestruct)
{
    UINT16 minLength = OFFSET_SRC_LIST;
    if (NULL != bufferPtr) 
    {
        if (bufferBytes < minLength)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    if (GetBufferLength() < minLength) return false;
    memset(AccessBuffer(), 0, OFFSET_SRC_LIST);
    SetLength(minLength);
    return true;
}  // end ProtoPktIGMP::GroupRecord::InitIntoBuffer()

void ProtoPktIGMP::GroupRecord::SetGroupAddress(const ProtoAddress* groupAddr)
{
    void* ptr = AccessBuffer(OFFSET_GROUP);
    if ((NULL == groupAddr) || (ProtoAddress::IPv4 != groupAddr->GetType()) || (!groupAddr->IsMulticast()))
        memset(ptr, 0, 4);
    else 
        memcpy(ptr, groupAddr->GetRawHostAddress(), 4);
}  // end ProtoPktIGMP::GroupRecord::SetGroupAddress()

bool ProtoPktIGMP::GroupRecord::AppendSourceAddress(const ProtoAddress& srcAddr)
{
    if ((ProtoAddress::IPv4 != srcAddr.GetType()) || !srcAddr.IsUnicast())
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::GroupRecord::AppendSourceAddress() error: invalid source address\n");
        return false;
    }
    // Is there room for another source address?
    UINT16 numSrc = GetNumSources();
    unsigned int offset = OFFSET_SRC_LIST + 4*numSrc;
    ASSERT(offset == GetLength());
    unsigned int newLength = offset + 4;
    if (newLength > GetBufferLength())
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::GroupRecord::AppendSourceAddress() error: insufficient buffer space\n");
        return false;
    }
    memcpy(AccessBuffer(offset), srcAddr.GetRawHostAddress(), 4);
    SetUINT16(OFFSET_NUM_SRC, numSrc + 1);
    SetLength(newLength);
    return true;
}  // end ProtoPktIGMP::GroupRecord::AppendSourceAddress()

bool ProtoPktIGMP::GroupRecord::AppendAuxiliaryData(const char* data, UINT16 len)
{
    unsigned int currentLength = GetLength();
    unsigned int bufferSpace = GetBufferLength() - currentLength;
    if (0 != (len % 0xfffc))
    {
        // aux data len must by multiple of 4 bytes (32-bit words)
        PLOG(PL_ERROR, "ProtoPktIGMP::GroupRecord::AppendAuxiliaryData() error: invalid data length\n");
        return false;
    }
    if (bufferSpace < len)
    {
        PLOG(PL_ERROR, "ProtoPktIGMP::GroupRecord::AppendAuxiliaryData() error: insufficient buffer space\n");
        return false;
    }
    void* ptr = AccessBuffer(currentLength);
    memcpy(ptr, data, len);
    SetUINT8(OFFSET_AUX_LEN,len >> 2);
    SetLength(currentLength + len);
    return true;
}  // end ProtoPktIGMP::GroupRecord::AppendAuxiliaryData()
                


