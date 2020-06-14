/**
* @file protoPktIP.cpp
* 
* @brief These classes provide representations of IP packet formats for IPv4 and IPv6 packets: 
*/

#include "protoPktIP.h"
#include "protoDebug.h"

// begin ProtoPktIP implementation
ProtoPktIP::ProtoPktIP(void*          bufferPtr, 
                       unsigned int   numBytes,
                       bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
}

ProtoPktIP::~ProtoPktIP()
{
}

unsigned int ProtoPktIP::GetHeaderLength()
{
    switch (GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ip4Pkt(*this);
            return ip4Pkt.GetHeaderLength();
        }
        case 6:
        {
            // IPv6 has fixed-length header always
            return ProtoPktIPv6::GetHeaderLength();
        }
        default:
            return 0;
    }
}  // end ProtoPktIP::GetHeaderLength()

bool ProtoPktIP::GetDstAddr(ProtoAddress& dst) 
{   
    switch (GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ip4Pkt(*this);
            ip4Pkt.GetDstAddr(dst);
            return true;
        }
        case 6:
        {
            ProtoPktIPv6 ip6Pkt(*this);
            ip6Pkt.GetDstAddr(dst);
            return true;
        }
        default:
            return false;
    }
}  // end ProtoPktIP::GetDstAddr() 

bool ProtoPktIP::SetDstAddr(ProtoAddress& dst) 
{   
    switch (GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ip4Pkt(*this);
            ip4Pkt.SetDstAddr(dst, true);
            return true;
        }
        case 6:
        {
            ProtoPktIPv6 ip6Pkt(*this);
            ip6Pkt.SetDstAddr(dst);
            return true;
        }
        default:
            return false;
    }
}  // end ProtoPktIP::SetDstAddr() 


bool ProtoPktIP::GetSrcAddr(ProtoAddress& src) 
{   
    switch (GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ip4Pkt(*this);
            ip4Pkt.GetSrcAddr(src);
            return true;
        }
        case 6:
        {
            ProtoPktIPv6 ip6Pkt(*this);
            ip6Pkt.GetSrcAddr(src);
            return true;
        }
        default:
            return false;
    }
}  // end ProtoPktIP::GetSrcAddr() 

bool ProtoPktIP::SetSrcAddr(ProtoAddress& src) 
{   
    switch (GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ip4Pkt(*this);
            ip4Pkt.SetSrcAddr(src, true);
            return true;
        }
        case 6:
        {
            ProtoPktIPv6 ip6Pkt(*this);
            ip6Pkt.SetSrcAddr(src);
            return true;
        }
        default:
            return false;
    }
}  // end ProtoPktIP::SetSrcAddr() 

// begin ProtoPktIPv4 implementation
ProtoPktIPv4::ProtoPktIPv4(void*        bufferPtr, 
                           unsigned int numBytes, 
                           bool         initFromBuffer,
                           bool         freeOnDestruct)
 : ProtoPktIP(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer();
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktIPv4::ProtoPktIPv4(ProtoPktIP & ipPkt)
 : ProtoPktIP(ipPkt.AccessBuffer(), ipPkt.GetBufferLength())
{
    InitFromBuffer();
}

ProtoPktIPv4::~ProtoPktIPv4()
{
}

bool ProtoPktIPv4::InitFromBuffer(void*   bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    else
        ProtoPkt::SetLength(0);
    if (GetBufferLength() > OFFSET_VERSION)
    {
        if (4 == (GetUINT8(OFFSET_VERSION) >> 4))
        {
            if (GetBufferLength() > (2*OFFSET_LEN + 2) &&
                ProtoPkt::InitFromBuffer(GetTotalLength()))
            {
                return true;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPktIPv4::InitFromBuffer() error: insufficient buffer space!\n");
            }
        }
        else
        {
            PLOG(PL_WARN, "ProtoPktIPv4::InitFromBuffer() error: invalid protocol version!\n");
        }
    }
    if (NULL != bufferPtr) DetachBuffer();
    return false;
}  // end ProtoPktIPv4::InitFromBuffer()


bool ProtoPktIPv4::InitIntoBuffer(void* bufferPtr, unsigned int bufferBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
    {
        if (bufferBytes < 20)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 20) 
    {
        return false;
    }
    SetVersion(4);
    SetHeaderLength(20);
    SetChecksum(0);
    SetWord16(OFFSET_FRAGMENT, 0);  // init flags & frag to ZERO
    SetTOS(0);
    SetUINT8(OFFSET_FLAGS, 0);      
    SetTTL(255);
    SetProtocol(RESERVED);
    // (TBD) Set some header fields to default values? (e.g. fragment, flags)
    // (TBD) should we set total length to 20 here? 
    return true;
}  // end ProtoPktIPv4::InitIntoBuffer()

void ProtoPktIPv4::SetTOS(UINT8 tos, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_TOS));
        UpdateChecksum(GetTOS(), tos, oddOffset);
    }
    SetUINT8(OFFSET_TOS, tos);
}  // end ProtoPktIPv4::SetTOS()

void ProtoPktIPv4::SetID(UINT16 id, bool updateChecksum) 
{
    if (updateChecksum) UpdateChecksum(GetID(), id);
    SetWord16(OFFSET_ID, id);
}  // end ProtoPktIPv4::SetID()
        
void ProtoPktIPv4::SetFlag(Flag flag, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_FLAGS));
        UINT8 oldFlags = GetUINT8(OFFSET_FLAGS);
        UINT8 newFlags = oldFlags | (UINT8)flag;
        SetUINT8(OFFSET_FLAGS, newFlags);
        UpdateChecksum(oldFlags, newFlags, oddOffset);
    }
    else
    {
        AccessUINT8(OFFSET_FLAGS) |= flag;
    }
}  // end ProtoPktIPv4::SetFlag()

void ProtoPktIPv4::ClearFlag(Flag flag, bool updateChecksum)
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_FLAGS));
        UINT8 oldFlags = GetUINT8(OFFSET_FLAGS);
        UINT8 newFlags = oldFlags & ~(UINT8)flag;
        SetUINT8(OFFSET_FLAGS, newFlags);
        UpdateChecksum(oldFlags, newFlags, oddOffset);
    }
    else
    {
        AccessUINT8(OFFSET_FLAGS) &= ~flag;
    }
} // end ProtoPktIPv4::ClearFlag()

void ProtoPktIPv4::SetFragmentOffset(UINT16 fragmentOffset, bool updateChecksum)  
{
    UINT16 fragOld = GetWord16(OFFSET_FRAGMENT);
    UINT16 fragNew = fragOld & 0xe000;
    fragNew |= (fragmentOffset & 0x1fff);
    if (updateChecksum) UpdateChecksum(fragOld, fragNew);
    SetWord16(OFFSET_FRAGMENT, fragNew);
} // end ProtoPktIPv4::SetFragmentOffset()


void ProtoPktIPv4::SetTTL(UINT8 ttl, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_TTL));
        UpdateChecksum(GetTTL(), ttl, oddOffset);
    }
    SetUINT8(OFFSET_TTL, ttl);
} // end ProtoPktIPv4::SetTTL()
   
void ProtoPktIPv4::SetProtocol(Protocol protocol, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_PROTOCOL));
        UpdateChecksum((UINT8)GetProtocol(), (UINT8)protocol, oddOffset);
    }
    SetUINT8(OFFSET_PROTOCOL, (UINT8)protocol);
}  // end ProtoPktIPv4::SetProtocol()


void ProtoPktIPv4::SetSrcAddr(const ProtoAddress& addr, bool calculateChecksum)
{
    memcpy((char*)AccessBuffer32(OFFSET_SRC_ADDR), addr.GetRawHostAddress(), 4); 
    if (calculateChecksum) CalculateChecksum();  // (TBD) is it worth it to incrementally update
}  // end ProtoPktIPv4::SetSrcAddr() 

void ProtoPktIPv4::SetDstAddr(const ProtoAddress& addr, bool calculateChecksum)
{
    memcpy((char*)AccessBuffer32(OFFSET_DST_ADDR), addr.GetRawHostAddress(), 4); 
    if (calculateChecksum) CalculateChecksum();  // (TBD) is it worth it to incrementally update
}  // end ProtoPktIPv4::SetDstAddr()  

void ProtoPktIPv4::SetPayloadLength(UINT16 numBytes, bool calculateChecksum)
{
    SetTotalLength(numBytes + GetHeaderLength());
    if (calculateChecksum) CalculateChecksum();
}  // end ProtoPktIPv4::SetPayloadLength()

/**
 * Return checksum in host byte order
 */
UINT16 ProtoPktIPv4::CalculateChecksum(bool set)
{
    UINT32 sum = 0;
    // Calculate checksum, skipping checksum field
    unsigned int i;
    for (i = 0; i < OFFSET_CHECKSUM; i++)
        sum += GetWord16(i);
    unsigned int hdrEndex = (GetUINT8(OFFSET_HDR_LEN) & 0x0f) << 1;
    for (i = OFFSET_CHECKSUM+1; i < hdrEndex; i++)
        sum += GetWord16(i);
    while (sum >> 16)
        sum = (sum & 0x0000ffff) + (sum >> 16);
    sum = ~sum;
    if (set) SetChecksum(sum);
    return sum;
}  // ProtoPktIPv4::CalculateChecksum()


// begin ProtoPktIPv4::Option implementation

ProtoPktIPv4::Option::Option(void* bufferPtr, unsigned int numBytes, bool initFromBuffer, bool freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer && (NULL != bufferPtr)) InitFromBuffer(); 
}

ProtoPktIPv4::Option::~Option()
{
}

int ProtoPktIPv4::Option::GetLengthByType(Type type)
{
    switch (type)
    {
        // fixed length options we support
        case EOOL:
        case NOP:
            return 1;
        case SEC:
            return 11;
        case SID:
        case MTUP:
        case MTUR:
        case RTRALT:
            return 4;
        // variable length options we support 
        case LSR:
        case SSR:  
        case RR:
        case ESEC:
        case TS:
        case TR:
        case SDB:
        case EIP:
        case CIPSO:
            return LENGTH_VARIABLE; // indicates variable length
        case UMP:
            return 8;
        // unsupported options 
        default: 
            return LENGTH_UNKNOWN;  // unsupported type
    }
}  // end ProtoPktIPv4::Option::GetLengthByType()

bool ProtoPktIPv4::Option::IsMutable(Type type)
{
    // per RFC4302
    switch (type)
    {
        // Immutable options
        case EOOL:
        case NOP:
        case SEC:
        case ESEC:
        case CIPSO:
        case RTRALT:
        case SDB:
            return false;
        
        // Assume others are mutable
        default:
            return true;
    } 
}  // end ProtoPktIPv4::Option::IsMutable()


bool ProtoPktIPv4::Option::InitFromBuffer(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (GetBufferLength() > OFFSET_TYPE)
    {
        // Validate buffer length according to type
        Type type = (Type)GetUINT8(OFFSET_TYPE);
        int minLength = GetLengthByType(type);
        switch (minLength)
        {
            case LENGTH_UNKNOWN:
                 PLOG(PL_ERROR, "ProtoPktIPv4::Option::InitFromBuffer() error: unsupported type: %d\n", (UINT8)type);
                 return false;
            case LENGTH_VARIABLE:
                if (GetBufferLength() <= OFFSET_LENGTH)
                {
                    PLOG(PL_ERROR, "ProtoPktIPv4::Option::InitFromBuffer() error: incomplete buffer\n");
                    return false;
                }
                minLength = (int)GetUINT8(OFFSET_LENGTH);
                break;
            default:
                break;   
        }
        if (GetBufferLength() < (unsigned int)minLength)
        {
            PLOG(PL_ERROR, "ProtoPktIPv4::Option::InitFromBuffer() error: incomplete buffer\n");
            return false;
        }
        else
        {
            SetLength(minLength);
            return true;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktIPv4::Option::InitFromBuffer() error: insufficient buffer\n");
        return false;
    }
}  // end ProtoPktIPv4::Option::InitFromBuffer()


bool ProtoPktIPv4::Option::InitIntoBuffer(Type         type,
                                          void*        bufferPtr, 
                                          unsigned int numBytes, 
                                          bool         freeOnDestruct)
{
    int minLength = GetLengthByType(type);
    switch (minLength)
    {
        case LENGTH_UNKNOWN:
            PLOG(PL_ERROR, "ProtoPktIPv4::Option::InitIntoBuffer() error: unsupported type: %d\n", type);
            return false;
        case LENGTH_VARIABLE:
            minLength = 2;  // for type + length fields
            break;
        default:
            break;
    }
    if (NULL != bufferPtr)
    {
        if (numBytes < (unsigned int)minLength)
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < (unsigned int)minLength) 
    {
        return false;
    }
    // zero init "minLength" bytes?
    SetUINT8(OFFSET_TYPE, (UINT8)type);
    SetUINT8(OFFSET_LENGTH, (UINT8)minLength);
    SetLength(minLength);
    // Zero the remaining content
    if (minLength > OFFSET_LENGTH + 1)
        memset(AccessBuffer(OFFSET_LENGTH + 1), 0, minLength - (OFFSET_LENGTH+1));
    return true;
}  // end ProtoPktIPv4::Option::InitIntoBuffer()

bool ProtoPktIPv4::Option::SetData(const char* data, unsigned int length)
{
    if (0 == GetBufferLength())
    {
        PLOG(PL_ERROR, "ProtoPktIPv4::Option::SetData() error: no buffer attached\n");
        return false;
    }
    // 1) Make sure data will fit into buffer _and_ appropo for type
    int optLength = GetLengthByType(GetType());
    int maxLength = 0;
    switch (optLength)
    {
        case LENGTH_UNKNOWN:
            PLOG(PL_ERROR, "ProtoPktIPv4::Option::SetData() error: unsupported type: %d\n", GetType());
            return false;
            
        case LENGTH_VARIABLE:
            optLength = length + 2;
            maxLength = (GetBufferLength() < 2) ? 0 : (GetBufferLength() - 2);
            break;
            
        default:
            maxLength = (GetBufferLength() < (unsigned int)optLength) ? 0 : optLength - 2;
            break;
    } 
    if (length > (unsigned int)maxLength)
    {
        PLOG(PL_ERROR, "ProtoPktIPv4::Option::SetData() error: insufficient buffer space\n");
        return false;
    }
    SetUINT8(OFFSET_LENGTH, optLength);
    SetLength(optLength);
    memcpy((char*)AccessBuffer(2), data, length); 
    return true;
}  // end ProtoPktIPv4::Option::SetData()

ProtoPktIPv4::Option::Iterator::Iterator(const ProtoPktIPv4& ip4Pkt)
    : pkt_buffer((const char*)ip4Pkt.GetBuffer()), offset(20)
{
    if (ip4Pkt.GetLength() > 20)
    {
        unsigned int hlen = ip4Pkt.GetHeaderLength();
        if (hlen > 20)
            offset_end = hlen;
        else
            offset_end = 20;  // no options
    }
    else
    {
        offset_end = 20;
    }
}

ProtoPktIPv4::Option::Iterator::~Iterator()
{
}

bool ProtoPktIPv4::Option::Iterator::GetNextOption(Option& option)
{
    if (offset < offset_end)
    {
        bool result = option.InitFromBuffer((char*)pkt_buffer + offset, offset_end - offset);
        offset = result ? (offset + option.GetLength()) : offset_end;
        return result;
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv4::Option::Iterator::GetNextOption()


// begin ProtoPktIPv6 implementation

ProtoPktIPv6::ProtoPktIPv6(void*        bufferPtr, 
                           unsigned int numBytes, 
                           bool         initFromBuffer,
                           bool         freeOnDestruct)
 : ProtoPktIP(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer();
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktIPv6::ProtoPktIPv6(ProtoPktIP & ipPkt)
 : ProtoPktIP(ipPkt.AccessBuffer(), ipPkt.GetBufferLength()),
   ext_pending(false)
{
    InitFromBuffer();
}

ProtoPktIPv6::~ProtoPktIPv6()
{
}

bool ProtoPktIPv6::InitFromBuffer(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    ext_pending = false;
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    else
        ProtoPkt::SetLength(0);
    if (GetBufferLength() > OFFSET_VERSION)
    {
        if (6 != (GetUINT8(OFFSET_VERSION) >> 4)) 
        {
            PLOG(PL_ERROR, "ProtoPktIPv6::InitFromBuffer() error: invalid version number\n");
        }
        else if (GetBufferLength() > (2*OFFSET_LENGTH + 2))
        {
            // this validates that the embedded length is <= buffer_bytes
            if (ProtoPkt::InitFromBuffer(40 + GetPayloadLength()))
            {
                return true;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPktIPv6::InitFromBuffer() error: invalid packet length?\n");
            }
        }
        else
        {
            PLOG(PL_ERROR, "ProtoPktIPv6::InitFromBuffer() error: insufficient buffer space (2)\n");
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktIPv6::InitFromBuffer() error: insufficient buffer space (1)\n");
    }
    if (NULL != bufferPtr) DetachBuffer();
    return false;
}  // end ProtoPktIPv6::InitFromBuffer()


bool ProtoPktIPv6::InitIntoBuffer(void*         bufferPtr, 
                                  unsigned int  bufferBytes, 
                                  bool          freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (bufferBytes < 40)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 40) 
    {
        return false;
    }
    SetVersion(6);
    SetTrafficClass(0);
    SetFlowLabel(0);
    SetPayloadLength(0);
    SetNextHeader(NONE);
    SetLength(40);
    ext_pending = false;
    return true;
}  // end ProtoPktIPv6::InitIntoBuffer()

ProtoPktIP::Protocol ProtoPktIPv6::GetLastHeader()
{
    if (HasExtendedHeader())
    {
        Extension::Iterator iterator(*this);
        Extension lastExt;
        while (iterator.GetNextExtension(lastExt));
        return lastExt.GetNextHeader();
    }
    else
    {
        return GetNextHeader();
    }
}  // end ProtoPktIPv6::GetLastHeader()

/**
 * Map extension to end of current IPv6 packet header
 */
ProtoPktIPv6::Extension* ProtoPktIPv6::AddExtension(Protocol extensionType)
{
    if (NONE == extensionType) return NULL;
    unsigned int hdrBytes = 40;
    if (ext_pending)
    {
        PackHeader(extensionType);
        hdrBytes = ProtoPkt::GetLength();
    }
    else if (HasExtendedHeader())
    {
        Extension::Iterator iterator(*this);
        Extension lastExt, x;
        while (iterator.GetNextExtension(x))
        {
            hdrBytes += x.GetLength();
            lastExt = x;
        }
        lastExt.SetNextHeader(extensionType);
    }
    else
    {
        if (GetBufferLength() <=  hdrBytes) return NULL;
        // (TBD) Should we return NULL if (NONE != GetNextHeader())???
        ASSERT(NONE == GetNextHeader());
        SetNextHeader(extensionType);
    }
    ASSERT(0 == (hdrBytes & 0x03));
    ext_temp.AttachBuffer(AccessBuffer32(hdrBytes >> 2), GetBufferLength() - hdrBytes);
    ext_temp.SetType(extensionType);
    ext_pending = true;
    return &ext_temp;
}  // end ProtoPktIPv6::AddExtension()

bool ProtoPktIPv6::PackHeader(Protocol nextHeaderType)
{
    if (ext_pending)
    {
        if (!ext_temp.Pack()) return false;
        ext_temp.SetNextHeader(nextHeaderType);
        // Update IPv6 packet payload length field
        SetPayloadLength(GetPayloadLength() + ext_temp.GetLength());
        ext_pending = false;
    }
    else if (HasExtendedHeader())
    {
        // (TBD) Find last header extension and set next header type?
    }
    else
    {
        SetNextHeader(nextHeaderType);
    }
    return true;
}  // end ProtoPktIPv6::PackHeader()

/**
 * Copy extension to beginning of packet's set of header 
 * extensions (payload is moved if present)
 */
bool ProtoPktIPv6::PrependExtension(Extension& ext)
{
    if (ext_pending) PackHeader();
    // 1) Is there room in the IPv6 packer buffer_ptr for the extension?
    if (GetBufferLength() < (ProtoPkt::GetLength() + ext.GetLength())) return false;
    // 2) Copy current packet next header to our ext next header
    ext.SetNextHeader(GetNextHeader());
    // 3) Move the current payload to make room for the extension
    UINT16 payloadLength = GetPayloadLength();
    void* ptr = AccessBuffer(40);
    memmove((char*)ptr+ext.GetLength(), ptr, payloadLength);
    // 4) Copy extension buffer_ptr into space made available
    memcpy(ptr, (char*)ext.GetBuffer(), ext.GetLength());
    SetNextHeader(ext.GetType());
    SetPayloadLength(payloadLength + ext.GetLength());
    return true;
}  // end ProtoPktIPv6::PrependExtension()


bool ProtoPktIPv6::ReplaceExtension(Extension& oldExt, Extension& newExt)
{
    ASSERT((oldExt.GetBuffer() >= ((char*)GetBuffer() + 40)) && 
           (oldExt.GetBuffer() < ((char*)GetBuffer() + GetLength())));
    
    if (oldExt.GetType() == newExt.GetType())
    {
        int spaceAvailable = GetBufferLength() - GetLength();
        int spaceDelta = newExt.GetLength() - oldExt.GetLength();
        if (spaceDelta > spaceAvailable)
        {
            PLOG(PL_ERROR, "ProtoPktIPv6::ReplaceExtension() error: insufficient buffer space!\n");
            return false;
        }
        newExt.SetNextHeader(oldExt.GetNextHeader());
        char* dataPtr = (char*)oldExt.AccessBuffer() + oldExt.GetLength();
        UINT16 dataLen = (UINT16)((char*)GetBuffer() + GetLength() - dataPtr);
        memmove(dataPtr+spaceDelta, dataPtr, dataLen);
        memcpy((char*)oldExt.AccessBuffer(), (char*)newExt.GetBuffer(), newExt.GetLength());
        SetPayloadLength(GetPayloadLength() + spaceDelta);
        return true;
    }
    else
    {
        // (TBD) we could mod to allow a new type of extension to replace the old one
        PLOG(PL_ERROR, "ProtoPktIPv6::ReplaceExtension() error: new extension is of different type!\n");
        return false;
    }
}  // end ProtoPktIPv6::ReplaceExtension()

/**
 * Copy pre-built extension to end of current IPv6 header 
 * (payload is moved if present)
 */
bool ProtoPktIPv6::AppendExtension(Extension& ext)
{
    unsigned int hdrBytes = 40;
    if (ext_pending)
    {
        
        PackHeader(ext.GetType());
        // Is there room in the IPv6 packer buffer_ptr for the additional extension?
        if (GetBufferLength() < (ProtoPkt::GetLength() + ext.GetLength())) return false;
        hdrBytes = ProtoPkt::GetLength();
    }
    else if (HasExtendedHeader())
    {
        // Is there room in the IPv6 packer buffer_ptr for the additional extension?
        if (GetBufferLength() < (ProtoPkt::GetLength() + ext.GetLength())) return false;
        Extension::Iterator iterator(*this);
        Extension lastExt, x;
        while (iterator.GetNextExtension(x))
        {
            hdrBytes += x.GetLength();
            lastExt = x;
        }
        ext.SetNextHeader(lastExt.GetNextHeader());
        lastExt.SetNextHeader(ext.GetType());
    }
    else
    {
        // Is there room in the IPv6 packer buffer_ptr for the additional extension?
        if (GetBufferLength() < (ProtoPkt::GetLength() + ext.GetLength())) return false;
        ext.SetNextHeader(GetNextHeader());
        SetNextHeader(ext.GetType());
    }
    void* ptr = AccessBuffer(hdrBytes);
    UINT16 payloadLength = GetPayloadLength();
    UINT16 moveLength = payloadLength + 40 - hdrBytes;
    memmove((char*)ptr + ext.GetLength(), ptr, moveLength);
    memcpy(ptr, (char*)ext.GetBuffer(), ext.GetLength());
    SetPayloadLength(payloadLength + ext.GetLength());
    return true;
}  // end ProtoPktIPv6::AppendExtension()

bool ProtoPktIPv6::SetPayload(Protocol payloadType, const char* dataPtr, UINT16 dataLen)
{
    if (ext_pending)
    {
        // Is there room for the payload?
        if (GetBufferLength() < (ProtoPkt::GetLength() + ext_temp.GetLength() + dataLen)) return false;
        PackHeader(payloadType);
    }
    else if (HasExtendedHeader())
    {
        // Is there room for the payload?
        if (GetBufferLength() < (ProtoPkt::GetLength() + dataLen)) 
        {
            PLOG(PL_ERROR, "ProtoPktIPv6::SetPayload() error: insufficient buffer space (1)\n");
            return false;
        }
        ASSERT(GetPayloadLength() > 0);
        // Find last extension header
        Extension::Iterator iterator(*this);
        Extension lastExt, x;
        while (iterator.GetNextExtension(x))
            lastExt = x;
        ASSERT(NONE == lastExt.GetNextHeader());
        lastExt.SetNextHeader(payloadType);
    }
    else
    {
        // Is there already a payload set?
        ASSERT(NONE == GetNextHeader());
        // Is there room for the payload?
        if (GetBufferLength() < (ProtoPkt::GetLength() + dataLen)) 
        {
            PLOG(PL_ERROR, "ProtoPktIPv6::SetPayload() error: insufficient buffer space (2)\n");
            return false;
        }
        ASSERT(0 == GetPayloadLength());
        SetNextHeader(payloadType);
    }
    // 3) Copy payload into buffer 
    memcpy(AccessBuffer(ProtoPkt::GetLength()), dataPtr, dataLen);
    SetPayloadLength(GetPayloadLength() + dataLen);
    return true;
}  // end ProtoPktIPv6::SetPayload()
                    

ProtoPktIPv6::Extension::Extension(Protocol     extType, 
                                   void*        bufferPtr, 
                                   unsigned int numBytes, 
                                   bool         initFromBuffer, 
                                   bool         freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct), ext_type(NONE), opt_pending(false), opt_packed(false)
{
    if (initFromBuffer) 
        InitFromBuffer(extType);
    else
        InitIntoBuffer(extType);
}

ProtoPktIPv6::Extension::~Extension()
{
}

bool ProtoPktIPv6::Extension::Copy(const ProtoPktIPv6::Extension& ext)
{
    if (ext.GetLength() > GetBufferLength())
    {
        PLOG(PL_ERROR, "ProtoPktIPv6::Extension::Copy() error: insufficient buffer size\n");
        return false;
    }
    // a) save our buffer pointer, length, and allocation status
    UINT32 bufferBytes = GetBufferLength();
    bool freeOnDestruct = FreeOnDestruct();
    UINT32* bufferPtr = DetachBuffer();
    // b) copy all 'ext' member variable values
    *this = ext;
    // c) restore buffer pointer, buffer length, and set to 'ext' length
    DetachBuffer(); // detach the 'ext' buffer pointers copied
    ProtoPkt::InitFromBuffer(ext.GetLength(), bufferPtr, bufferBytes, freeOnDestruct);
    // d) copy 'ext' buffer content
    memcpy(bufferPtr, ext.GetBuffer(), ext.GetLength());
    return true;
}  // end ProtoPktIPv6::Extension::Copy()

bool ProtoPktIPv6::Extension::InitIntoBuffer(Protocol       extType,
                                             void*          bufferPtr, 
                                             unsigned int   numBytes, 
                                             bool           freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    ext_type = extType;
    if (0 == GetBufferLength())
        return true;  // don't worry if no buffer_ptr for moment
    else if (GetBufferLength() > OFFSET_NEXT_HDR) 
        SetNextHeader(NONE);
    switch (extType)
    {
        // These types have length fields
        default:
            PLOG(PL_WARN, "ProtoPktIPv6::Extension::InitIntoBuffer() warning: unknown extension type\n");
            // For now we'll assume it has a length field ala the options and routing extensions
        case NONE:
        case HOPOPT:
        case DSTOPT:
        case RTG:
        case AUTH:
            if (GetBufferLength() > OFFSET_LENGTH) 
            {
                ProtoPkt::SetLength(2);
                break;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPktIPv6::Extension::InitIntoBuffer() error: insufficient buffer space\n");
                SetLength(0);
                if (NULL != bufferPtr)
                    DetachBuffer();
                return false;
            }
        case FRAG:
            if (GetBufferLength() > OFFSET_LENGTH) SetExtensionLength(8);
            break;
    }
    opt_pending = opt_packed = false;
    return true;
}  // ProtoPktIPv6::Extension::InitIntoBuffer()

/**
 * Set length field (as applicable) of extension header from units of bytes
 */
void ProtoPktIPv6::Extension::SetExtensionLength(UINT16 numBytes)
{
    switch (GetType())
    {
        default:
            PLOG(PL_ERROR, "ProtoPktIPv6::Extension::SetExtensionLength() unknown extension type\n");
            // For now we'll assume it has a length field ala the options and routing extensions
        case HOPOPT:
        case DSTOPT:
        case RTG:
            ASSERT(0 == (0x07 & numBytes));
            SetUINT8(OFFSET_LENGTH, (numBytes - 8) >> 3);
            break;
        case AUTH:
            ASSERT(0 == (0x03 & numBytes));
            SetUINT8(OFFSET_LENGTH, (numBytes - 4) >> 2);
        case FRAG:
            ASSERT(8 == numBytes);
            break;
    }
    ProtoPkt::SetLength(numBytes);
}  // end ProtoPktIPv6::Extension::SetExtensionLength()

ProtoPktIPv6::Option* ProtoPktIPv6::Extension::AddOption(Option::Type optType)
{
    if (opt_packed)
    {
        ASSERT(!opt_pending);
        // Find first PADx option and truncate extension length
        Option::Iterator iterator(*this);
        Option opt;
        while (iterator.GetNextOption(opt))
        {
            Option::Type otype = opt.GetType();
            if ((Option::PAD1 == otype) || (Option::PADN == otype))
            {
                size_t extLen = (char*)opt.GetBuffer() - (char*)GetBuffer();
                if (extLen != (ProtoPkt::GetLength() - opt.GetLength()))
                    PLOG(PL_ERROR, "ProtoPktIPv6::Extension::AddOption() warning: extension used multiple PADS ?!\n");
                ProtoPkt::SetLength((unsigned int)extLen);
                break;
            }
        }
        opt_packed = false;
    }
    else if (opt_pending) 
    {
        PackOption();
    }
    // If there is any room in the buffer, init the new option into the available space
    unsigned int minLength = (Option::PAD1 == optType) ? 1 : 2;
    unsigned int bufferSpace = GetBufferLength() - ProtoPkt::GetLength();
    if (bufferSpace < minLength)
    {
        PLOG(PL_ERROR, "ProtoPktIPv6::Extension::AddOption() error: insufficient buffer space\n");
        return NULL;
    }        
    opt_temp.InitIntoBuffer(optType, (char*)AccessBuffer(ProtoPkt::GetLength()), bufferSpace);
    if (Option::PAD1 != optType) opt_temp.SetData(NULL, 0);
    opt_pending = true;
    return &opt_temp;
}  // end ProtoPktIPv6::Extension::AddOption()

bool ProtoPktIPv6::Extension::ReplaceOption(Option& oldOpt, Option& newOpt)
{
    ASSERT((oldOpt.GetBuffer() >= ((char*)GetBuffer() + 2)) && 
           (oldOpt.GetBuffer() < ((char*)GetBuffer() + GetLength())));
    if (opt_packed)
    {
        ASSERT(!opt_pending);
        // Find first PADx option and truncate extension length
        Option::Iterator iterator(*this);
        Option opt;
        while (iterator.GetNextOption(opt))
        {
            Option::Type otype = opt.GetType();
            if ((Option::PAD1 == otype) || (Option::PADN == otype))
            {
                size_t extLen = (char*)opt.GetBuffer() - (char*)GetBuffer();
                if (extLen != (ProtoPkt::GetLength() - opt.GetLength()))
                    PLOG(PL_ERROR, "ProtoPktIPv6::Extension::AddOption() warning: extension used multiple PADS ?!\n");
                ProtoPkt::SetLength((unsigned int)extLen);
                break;
            }
        }
        opt_packed = false;
    }
    else if (opt_pending) 
    {
        PackOption();
    }
    int spaceAvailable = GetBufferLength() - GetLength();
    int spaceDelta = newOpt.GetLength() - oldOpt.GetLength();
    if (spaceDelta > spaceAvailable)
    {
        PLOG(PL_ERROR, "ProtoPktIPv6::Extension::ReplaceOption() error: insufficient buffer space!\n");
        return false;
    }
    char* dataPtr = (char*)oldOpt.AccessBuffer() + oldOpt.GetLength();
    UINT16 dataLen = (UINT16)((char*)GetBuffer() + GetLength() - dataPtr);
    memmove(dataPtr+spaceDelta, dataPtr, dataLen);
    memcpy(oldOpt.AccessBuffer(), newOpt.GetBuffer(), newOpt.GetLength());
    ProtoPkt::SetLength(ProtoPkt::GetLength() + spaceDelta);
    return Pack();
}  // end ProtoPktIPv6::Extension::ReplaceOption()

bool ProtoPktIPv6::Extension::Pack()
{
    if (IsOptionHeader()) 
    {
        if (opt_pending) PackOption();
        if (PadOptionHeader())
        {
            opt_packed = true;
            SetExtensionLength(ProtoPkt::GetLength());
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        opt_packed = true;
        SetExtensionLength(ProtoPkt::GetLength());
        return true;
    }   
}  // end ProtoPktIPv6::Extension::Pack()

void ProtoPktIPv6::Extension::PackOption()
{
    if (opt_pending)
    {
        // Update Extension payload length field
        //SetExtensionLength(pkt_length + opt_temp.GetLength());
        ProtoPkt::SetLength(ProtoPkt::GetLength() + opt_temp.GetLength());
        opt_pending = false;
    }
}  // end ProtoPktIPv6::Extension::PackOption()

bool ProtoPktIPv6::Extension::PadOptionHeader()
{
    PackOption();
    UINT8 padBytes = (UINT8)(GetLength() & 0x07);
    if (0 != padBytes)
        padBytes = 8 - padBytes;
    else
        return true;  // no padding was needed
    Option* opt = AddOption((1 == padBytes) ? Option::PAD1 : Option::PADN);
    if ((NULL != opt) && opt->MakePad(padBytes))
    {
        PackOption();
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktIPv6::Extension::PadOptionHeader() error: insufficient buffer space\n");
        return false;
    }
}  // end ProtoPktIPv6::Extension::PadOptionHeader()


bool ProtoPktIPv6::Extension::InitFromBuffer(Protocol extType, void*   bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (GetBufferLength() >= OFFSET_LENGTH)
    {
        // set ProtoPkt "pkt_length" and check that it is bounds of buffer_ptr
        ext_type = extType;
        opt_pending = false;
        opt_packed = true;
        return ProtoPkt::InitFromBuffer(GetExtensionLength());
    }
    else
    {
        if (NULL != bufferPtr) 
            DetachBuffer();
        ProtoPkt::SetLength(0);
        return false;
    }
}  // end ProtoPktIPv6::Extension::InitFromBuffer()

/**
 * Return length of extension header in bytes
 */
UINT16 ProtoPktIPv6::Extension::GetExtensionLength() const
{
    switch (GetType())
    {
        default:
            PLOG(PL_ERROR, "ProtoPktIPv6::Extension::GetExtensionLength() unknown extension type\n");
            // For now we'll assume it has a length field ala the options and routing extensions
        case HOPOPT:
        case DSTOPT:
        case RTG:
            return (8 + (GetUINT8(OFFSET_LENGTH) << 3));
        case AUTH:
            return (4 + (GetUINT8(OFFSET_LENGTH) << 2));
        case FRAG:
            return 8;
    }
}  // end ProtoPktIPv6::Extension::GetExtensionLength()

// class ProtoPktIPv6::Extension::Iterator implementation

ProtoPktIPv6::Extension::Iterator::Iterator(ProtoPktIPv6& pkt)
 : ipv6_pkt(pkt), next_hdr(pkt.GetNextHeader()), offset(40)
{   
}

ProtoPktIPv6::Extension::Iterator::~Iterator()
{
}

bool ProtoPktIPv6::Extension::Iterator::GetNextExtension(Extension& extension)
{
    if ((6 != ipv6_pkt.GetVersion()) || (offset >= ipv6_pkt.GetLength())) return false;
    if (IsExtension(next_hdr))
    {
        if (extension.InitFromBuffer(next_hdr, ipv6_pkt.AccessBuffer32(offset >> 2), ipv6_pkt.GetLength()-offset))
        {
            next_hdr = extension.GetNextHeader();
            offset += extension.GetLength();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Extension::GetNextExtension()

ProtoPktIPv6::Option::Option(void* bufferPtr, unsigned int numBytes, bool initFromBuffer, bool freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer && (NULL != bufferPtr)) InitFromBuffer();
}

ProtoPktIPv6::Option::~Option()
{
}

bool ProtoPktIPv6::Option::InitFromBuffer(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (GetBufferLength() > 0)
    {
        if (PAD1 == GetType())
        {
            return true;
        }
        else if (GetBufferLength() > 1)
        {
            // check that the embedded option length is in bounds
            if ((unsigned int)(OFFSET_DATA + GetDataLength()) > numBytes)  
            {
                return false;       
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Option::InitFromBuffer()

bool ProtoPktIPv6::Option::InitIntoBuffer(Type         type,
                                          void*        bufferPtr, 
                                          unsigned int numBytes, 
                                          bool         freeOnDestruct)
{
    UINT8 minLen = (type == PAD1) ? 1 : 2;
    if (NULL != bufferPtr)
    {
        if (numBytes < minLen)
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < minLen) 
    {
        return false;
    }
    SetUnknownPolicy(SKIP);
    SetMutable(false);
    SetType(type);
    if (PAD1 != type) SetDataLength(0);
    return true;
}  // end ProtoPktIPv6::Option::InitIntoBuffer()

bool ProtoPktIPv6::Option::SetData(char* dataPtr, UINT8 dataLen)
{
    if (dataLen < (GetBufferLength() - OFFSET_DATA))
    {
        memcpy(AccessBuffer(OFFSET_DATA), dataPtr, dataLen);
        SetDataLength(dataLen);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Option::SetData()

bool ProtoPktIPv6::Option::MakePad(UINT8 numBytes)
{
    if (GetBufferLength() > OFFSET_TYPE)
    {
        if (numBytes > 1)
        {
            if (GetBufferLength() > (unsigned int)(OFFSET_TYPE + numBytes))
            {
                SetType(PADN);
                memset(AccessBuffer(OFFSET_DATA), 0, numBytes-2);
                SetDataLength(numBytes - (OFFSET_TYPE+2));
                return true;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPktIPv6::Option::MakePad() error: insufficient buffer space\n");
                return false;
            }
        }
        else if (1 == numBytes)
        {
            SetType(PAD1);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktIPv6::Option::MakePad() error: no buffer space allocated\n");
        return false;
    }
}  // end ProtoPktIPv6::Option::MakePad()

ProtoPktIPv6::Option::Iterator::Iterator(const Extension& extension)
 : hdr_extension(extension), offset(2)
{
    
}

ProtoPktIPv6::Option::Iterator::~Iterator()
{
}

bool ProtoPktIPv6::Option::Iterator::GetNextOption(Option& option)
{
    if (offset >= hdr_extension.GetLength())
    {
    
        return false;
    }
    else if (option.InitFromBuffer(((char*)hdr_extension.GetBuffer()) + offset,
                                   hdr_extension.GetLength() - offset))
    {
        offset += option.GetLength();
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Option::Iterator::GetNextOption()


ProtoPktFRAG::ProtoPktFRAG(void*         bufferPtr, 
                           unsigned int  numBytes, 
                           bool          initFromBuffer,
                           bool          freeOnDestruct)
 : ProtoPktIPv6::Extension(ProtoPktIP::FRAG, bufferPtr, numBytes, initFromBuffer, freeOnDestruct)
{
}

ProtoPktFRAG::~ProtoPktFRAG()
{
}

bool ProtoPktFRAG::InitIntoBuffer(void*         bufferPtr, 
                                  unsigned int  numBytes, 
                                  bool          freeOnDestruct)
{
    if (Extension::InitIntoBuffer(ProtoPktIP::FRAG, bufferPtr, numBytes, freeOnDestruct))
    {
        // Make sure it's big enough (8 bytes)
        unsigned int minLength = 8;
        if (GetBufferLength() < minLength)
        {
            SetLength(0);
            if (bufferPtr != NULL)
                DetachBuffer();
            return false;
        }
        // zero init everything (all 8 bytes)
        memset(AccessBuffer(), 0, 8);
        SetLength(minLength);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktFRAG::InitIntoBuffer()


ProtoPktAUTH::ProtoPktAUTH(void*         bufferPtr, 
                           unsigned int  numBytes, 
                           bool          initFromBuffer,
                           bool          freeOnDestruct)
 : ProtoPktIPv6::Extension(ProtoPktIP::AUTH, bufferPtr, numBytes, initFromBuffer, freeOnDestruct)
{
}

ProtoPktAUTH::~ProtoPktAUTH()
{
}

bool ProtoPktAUTH::InitIntoBuffer(void*         bufferPtr, 
                                  unsigned int  numBytes, 
                                  bool          freeOnDestruct)
{
    if (Extension::InitIntoBuffer(ProtoPktIP::AUTH, bufferPtr, numBytes, freeOnDestruct))
    {
        // Make sure it's big enough for at least reserved, spi, and sequence fields
        unsigned int minLength = ((OFFSET_SEQUENCE+1) << 2);
        if (GetBufferLength() < minLength)
        {
            SetLength(0);
            if (bufferPtr != NULL)
                DetachBuffer();
            return false;
        }
        SetWord16(OFFSET_RESERVED, 0);
        SetLength(minLength);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktAUTH::InitIntoBuffer()

bool ProtoPktAUTH::InitFromBuffer(void*   bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (Extension::InitFromBuffer(ProtoPktIP::AUTH, bufferPtr, numBytes, freeOnDestruct))
    {
        if (GetBufferLength() >= ((OFFSET_SEQUENCE+1) << 2))
        {
            return true;
        }
        else
        {
            SetLength(0);
            if (NULL != bufferPtr)
                DetachBuffer();
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktAUTH::InitFromBuffer()


ProtoPktESP::ProtoPktESP(void*         bufferPtr, 
                         unsigned int  numBytes, 
                         bool          freeOnDestruct)
: ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
}
        
ProtoPktESP::~ProtoPktESP()
{
}
/**
 * This function makes sure the buffer is big enough for at least
 * spi and sequence fields.
 */
bool ProtoPktESP::InitIntoBuffer(void*         bufferPtr, 
                                 unsigned int  numBytes, 
                                 bool          freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (0 == GetBufferLength()) return true;  // don't worry if no buffer_ptr for moment
    // Make sure it's big enough for at least spi and sequence fields
    unsigned int minLength = ((OFFSET_SEQUENCE+1) << 2);
    if (GetBufferLength() >= minLength)
    {
        ProtoPkt::SetLength(minLength);
        return true;
    }
    else
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr)
            DetachBuffer();
        return false;   
    }
}  // end ProtoPktESP::InitIntoBuffer()
/**
 * This function makes sure the buffer is big enough for at least
 * spi and sequence fields.
 */

bool ProtoPktESP::InitFromBuffer(UINT16 espLength, void*   bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    // Make sure it's big enough for at least spi and sequence fields
    unsigned int minLength = ((OFFSET_SEQUENCE+1) << 2);
    if (GetBufferLength() >= minLength)
    {
        SetLength(espLength);
        return true;
    }
    else
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr)
            DetachBuffer();
        return false;  
    }
}  // end ProtoPktESP::InitFromBuffer()


// (TBD) - move "ProtoPktDPD" implementation to the protolib/manet tree?
ProtoPktDPD::ProtoPktDPD(void*        bufferPtr, 
                         unsigned int numBytes, 
                         bool         initFromBuffer,
                         bool         freeOnDestruct)
 : ProtoPktIPv6::Option(bufferPtr, numBytes, false, freeOnDestruct)
{
    if (initFromBuffer)
        InitFromBuffer();
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktDPD::~ProtoPktDPD()
{
}

bool ProtoPktDPD::InitFromBuffer(void*          bufferPtr,
                                 unsigned int   numBytes,
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if ((numBytes < (OFFSET_DATA_LENGTH + 1)) ||
        (SMF_DPD != GetType()))
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    unsigned int dataLength = GetDataLength();
    if ((dataLength < 1) || (numBytes < (OFFSET_DATA_LENGTH + 1 + dataLength)))
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    if (GetTaggerIdLength() >= dataLength)
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    return true;
}  // end ProtoPktDPD::InitFromBuffer()

bool ProtoPktDPD::GetTaggerId(ProtoAddress& addr) const
{
    switch (GetTaggerIdType())
    {
        case TID_IPv4_ADDR:
            if (4 == GetTaggerIdLength())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv4, GetTaggerId(), 4);
                return true;
            }
            else
            {
                return false;
            }
        case TID_IPv6_ADDR:
            if (4 == GetTaggerIdLength())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv6, GetTaggerId(), 16);
                return true;
            }
            else
            {
                return false;
            }
            return true;
        default:
            return false;
    }
}  // end ProtoPktDPD::GetTaggerId(by address)

bool ProtoPktDPD::GetPktId(UINT8& value) const
{
    if (1 == GetPktIdLength())
    {
        value = GetUINT8(OffsetPktId());
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktDPD::GetPktId(UINT8& value)

bool ProtoPktDPD::GetPktId(UINT16& value) const
{
    if (2 == GetPktIdLength())
    {
        memcpy(&value, GetBuffer(OffsetPktId()), 2);
        value = ntohs(value);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktDPD::GetPktId(UINT16& value)

bool ProtoPktDPD::GetPktId(UINT32& value) const
{
    if (4 == GetPktIdLength())
    {
        memcpy(&value, GetBuffer(OffsetPktId()), 4);
        value = ntohl(value);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktDPD::GetPktId(UINT16& value)

bool ProtoPktDPD::InitIntoBuffer(void*          bufferPtr,
                                 unsigned int   numBytes,
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (numBytes < (OFFSET_DATA_LENGTH + 1))
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    // Set NULL tagger id
    SetUINT8(OFFSET_TID_TYPE, 0);
    SetDataLength(1);
    return 0;
}  // end ProtoPktDPD::InitIntoBuffer()

bool ProtoPktDPD::SetHAV(const char* hashAssistValue, UINT8 numBytes)
{
    unsigned int minLength = OFFSET_HAV + numBytes;
    if (GetBufferLength() < minLength) return false;
    char* ptr = (char*)AccessBuffer(OFFSET_HAV);
    memcpy(ptr, hashAssistValue, numBytes);
    *ptr |= (char)0x80;  // make sure 'H' bit is set
    SetDataLength(numBytes);
    return true;
}  // end ProtoPktDPD::SetHAV()

bool ProtoPktDPD::SetTaggerId(TaggerIdType type, const char* taggerId, UINT8 taggerIdLength)
{
    if ((TID_NULL != type) && (0 != taggerIdLength))
    {
        unsigned int minLength = OFFSET_TID_VALUE + taggerIdLength;
        if (GetBufferLength() < minLength) return false;
        SetUINT8(OFFSET_TID_TYPE, (UINT8)(type << 4));
        AccessUINT8(OFFSET_TID_LENGTH) |= ((taggerIdLength - 1) & 0x0f);
        memcpy(AccessBuffer(OFFSET_TID_VALUE), taggerId, taggerIdLength);
        SetDataLength(1 + taggerIdLength);
    }
    else
    {
        SetUINT8(OFFSET_TID_TYPE, 0);
        SetDataLength(1);
    }
    return true;
}  // end ProtoPktDPD::SetTaggerId(by buffer)


bool ProtoPktDPD::SetTaggerId(const ProtoAddress& ipAddr)
{
    switch (ipAddr.GetType())
    {
        case ProtoAddress::IPv4:
            return SetTaggerId(TID_IPv4_ADDR, ipAddr.GetRawHostAddress(), 4);
        case ProtoAddress::IPv6:
            return SetTaggerId(TID_IPv6_ADDR, ipAddr.GetRawHostAddress(), 16);
        default:
            PLOG(PL_ERROR, "ProtoPktDPD::SetTaggerId() error: invalid address type\n");
            return false;
    }
}  // ProtoPktDPD::SetTaggerId(by address)



// begin ProtoPktMobile implementation
ProtoPktMobile::ProtoPktMobile(void*          bufferPtr, 
                               unsigned int   numBytes,
                               bool           initFromBuffer,
                               bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (initFromBuffer) 
            InitFromBuffer();
        else
            InitIntoBuffer();
    }
}

ProtoPktMobile::~ProtoPktMobile()
{
}

bool ProtoPktMobile::InitIntoBuffer(void*   bufferPtr, unsigned int bufferBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
    {
        if (bufferBytes < 8)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 8) 
    {
        return false;
    }
    SetProtocol(ProtoPktIP::RESERVED);
    SetUINT8(OFFSET_RESERVED, 0);
    SetChecksum(0);
    SetLength(8);
    return true;
}  // end ProtoPktMobile::InitIntoBuffer()


void ProtoPktMobile::SetDstAddr(const ProtoAddress& addr, bool calculateChecksum) 
{   
    memcpy((char*)AccessBuffer32(OFFSET_DST_ADDR), addr.GetRawHostAddress(), 4); // (TBD) leverage alignment?     
    if (calculateChecksum) CalculateChecksum();  // (TBD) is it worth it to incrementally update
}  // end ProtoPktMobile::SetDstAddr() 

bool ProtoPktMobile::SetSrcAddr(const ProtoAddress& addr, bool calculateChecksum) 
{   
    if (GetBufferLength() < 12) return false;
    memcpy((char*)AccessBuffer32(OFFSET_SRC_ADDR), addr.GetRawHostAddress(), 4); // (TBD) leverage alignment?     
    if (calculateChecksum) CalculateChecksum();  // (TBD) is it worth it to incrementally update
    SetFlag(FLAG_SRC);
    SetLength(12);
    return true;
}  // end ProtoPktMobile::SetDstAddr() 

// Return header checksum in host byte order
UINT16 ProtoPktMobile::CalculateChecksum(bool set)
{
    UINT32 sum = 0;
    UINT16 savedSum = GetChecksum();
    SetChecksum(0);
    // Calculate checksum, skipping checksum field
    unsigned int headerLen = FlagIsSet(FLAG_SRC) ? 12/2 : 8/2;
    for (unsigned int i = 0; i < headerLen; i++)
        sum += GetWord16(i);
    while (sum >> 16)
        sum = (sum & 0x0000ffff) + (sum >> 16);
    sum = ~sum;
    if (set) 
        SetChecksum(sum);
    else
        SetChecksum(savedSum);
    return sum;
}  // ProtoPktMobile::CalculateChecksum()

bool ProtoPktMobile::InitFromBuffer(void*           bufferPtr, 
                                    unsigned int    numBytes, 
                                    bool            freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);    
    UINT16 minBytes = 8;
    if (GetBufferLength() > OFFSET_FLAGS)
    {
        if (FlagIsSet(FLAG_SRC))
            minBytes = 12;
    }
    if (GetBufferLength() < minBytes)
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    ProtoPkt::SetLength(numBytes);
    return true;
}  // end ProtoPktMobile::InitFromBuffer()

bool ProtoPktMobile::GetSrcAddr(ProtoAddress& src) const
{       
    if (FlagIsSet(FLAG_SRC))
    {
        src.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer32(OFFSET_DST_ADDR), 4);
        return true;
    }
    else
    {
        src.Invalidate();
        return false;
    }
}  // end ProtoPktMobile::GetSrcAddr() 


bool ProtoPktDPD::SetPktId(const char* pktId, UINT8 pktIdLength)
{
    unsigned int taggerIdLength = GetTaggerIdLength();
    unsigned int offsetPktId = OFFSET_TID_VALUE + taggerIdLength;
    unsigned int minLength = offsetPktId + pktIdLength;
    if (GetBufferLength() < minLength) return false;
    memcpy(AccessBuffer(offsetPktId), pktId, pktIdLength);
    SetDataLength(1 + taggerIdLength + pktIdLength);
    return true;   
}

ProtoPktUDP::ProtoPktUDP(void*          bufferPtr, 
                         unsigned int   numBytes, 
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (initFromBuffer)
            InitFromBuffer(); 
        else 
            InitIntoBuffer();
    }
}

ProtoPktUDP::~ProtoPktUDP()
{
}

bool ProtoPktUDP::InitFromPacket(ProtoPktIP& ipPkt)
{
    switch (ipPkt.GetVersion())
    {
        case 4:
        {
            // (TBD) support IPv4 extended headers
            ProtoPktIPv4 ip4Pkt(ipPkt);
            if (ProtoPktIP::UDP == ip4Pkt.GetProtocol())
            {
                return InitFromBuffer(ip4Pkt.AccessPayload(), ip4Pkt.GetPayloadLength(), false);
            }
            else
            {
                return false;  // not a UDP packet
            }
            break;
        }
        case 6:
        {
            ProtoPktIPv6 ip6Pkt(ipPkt);
            if (ip6Pkt.HasExtendedHeader())
            {
                unsigned int extHeaderLength = 0;
                ProtoPktIPv6::Extension::Iterator extIterator(ip6Pkt);
                ProtoPktIPv6::Extension ext;
                while (extIterator.GetNextExtension(ext))
                {
                    extHeaderLength += ext.GetLength();
                    if (ProtoPktIP::UDP == ext.GetNextHeader())
                    {
                        void* udpBuffer = (char*)ip6Pkt.AccessPayload() + extHeaderLength;
                        unsigned int udpLength = ip6Pkt.GetPayloadLength() - extHeaderLength;
                        return InitFromBuffer(udpBuffer, udpLength, false);
                    }
                }
                return false;  // not a UDP packet
            }
            else if (ProtoPktIP::UDP == ip6Pkt.GetNextHeader())
            {
                return InitFromBuffer(ip6Pkt.AccessPayload(), ip6Pkt.GetPayloadLength(), false);
            }
            else
            {
                return false;  // not a UDP packet
            }   
            break;
        }
        default:
            PLOG(PL_ERROR, "ProtoPktUDP::InitFromPacket() error: bad IP packet version: %d\n", ipPkt.GetVersion());
            return false;
    }
    return true;
}  // end ProtoPktUDP::InitFromPacket()

bool ProtoPktUDP::InitFromBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    UINT16 totalLen = GetPayloadLength() + 8;
    if (totalLen > GetBufferLength())
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    else
    {
        // (TBD) We could validate the checksum, too?
        ProtoPkt::SetLength(totalLen);
        return true;
    }
}  // end bool ProtoPktUDP::InitFromBuffer()

bool ProtoPktUDP::InitIntoBuffer(void*          bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct) 
{
    if (NULL != bufferPtr)
    {
        if (numBytes < 8)  // UDP header size
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    if (GetBufferLength() < 8) return false;
    SetChecksum(0);
    return true;
}  // end ProtoPktUDP::InitIntoBuffer()
                
UINT16 ProtoPktUDP::ComputeChecksum(ProtoPktIP& ipPkt) const
{
    UINT32 sum = 0;
    // 1) Calculate pseudo-header part
    switch(ipPkt.GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ipv4Pkt(ipPkt);
            // a) src/dst addr pseudo header portion
            const UINT16* ptr = (const UINT16*)ipv4Pkt.GetSrcAddrPtr();
            int addrEndex = ProtoPktIPv4::ADDR_LEN;  // note src + dst = 2 addresses
            for (int i = 0; i < addrEndex; i++)
                sum += (UINT16)ntohs(ptr[i]);
            // b) protocol & "total length" pseudo header portions
            sum += (UINT16)ipv4Pkt.GetProtocol();
            sum += (UINT16)GetLength(); // UDP length
            break;
        }
        case 6:
        {
            ProtoPktIPv6 ipv6Pkt(ipPkt);
            // a) src/dst addr pseudo header portion
            const UINT16* ptr = (const UINT16*)ipv6Pkt.GetSrcAddrPtr();
            int addrEndex = ProtoPktIPv6::ADDR_LEN;  // note src + dst = 2 addresses
            for (int i = 0; i < addrEndex; i++)
                sum += (UINT16)ntohs(ptr[i]);
            sum += (UINT16)GetLength(); // UDP length
            sum += (UINT16)ipv6Pkt.GetNextHeader();
            break;
        }
        default:
            return 0;   
    }
    // 2) UDP header part, sans "checksum" field
    unsigned int i;
    for (i = 0; i < OFFSET_CHECKSUM; i++)
        sum += GetWord16(i);
    // 3) UDP payload part (note adjustment for odd number of payload bytes)
    unsigned int dataEndex = GetLength();
    if (0 != (dataEndex & 0x01))
        sum += (UINT16)((UINT16)GetUINT8(dataEndex-1) << 8);
    dataEndex >>= 1;  // convert from bytes to UINT16 index
    for (i = (OFFSET_CHECKSUM+1); i < dataEndex; i++)  
        sum += GetWord16(i);
    // 4) Carry as needed
    while (0 != (sum >> 16))
        sum = (sum & 0x0000ffff) + (sum >> 16);
    sum = ~sum;
    
    // 5) ZERO check/correct as needed
    if (0 == sum) sum = 0x0000ffff;
    return (UINT16)sum;
}  // end ProtoPktUDP::CalculateChecksum()


