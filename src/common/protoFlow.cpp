#include "protoFlow.h"
#include "protoDebug.h"
#include "protoString.h"  // for ProtoTokenator
#include "protoNet.h"     // for ProtoNet::GetInterfaceAddress() for optional flow initialization
#include <ctype.h>  // for isspace()

ProtoFlow::Description::Description(const ProtoAddress&  dst,            // invalid dst addr means any dst
                                    const ProtoAddress&  src,            // invalid src addr means any src
                                    UINT8                trafficClass,   // 0x03 is "null" trafficClass (only ECN bits set)
                                    ProtoPktIP::Protocol protocol,       // 255 is "null" protocol
                                    unsigned int         ifaceIndex)     // 0 is "null" interface index
{
    // flow_key:  dlen [+ dst + dmask] + slen [+ src + smask] + class + protocol + index
    UINT8 dstLen, dstMaskLen, srcLen, srcMaskLen;
    const char* dstPtr;
    const char* srcPtr;
    if (dst.IsValid())
    {
        dstPtr = dst.GetRawHostAddress();
        dstLen = dst.GetLength();   // in bytes
        dstMaskLen = dstLen << 3;   // in bits
    }
    else
    {
        dstPtr = NULL;
        dstLen = 0;
        dstMaskLen = 0;
    }
    if (src.IsValid())
    {
        srcPtr = src.GetRawHostAddress();
        srcLen = src.GetLength();  // in bytes
        srcMaskLen = srcLen << 3;  // in bits
    }
    else
    {
        srcPtr = NULL;
        srcLen = 0;
        srcMaskLen = 0;
    }
    SetKey(dstPtr, dstLen, dstMaskLen, srcPtr, srcLen, srcMaskLen, trafficClass, protocol, ifaceIndex);
}

ProtoFlow::Description::~Description()
{
}

void ProtoFlow::Description::Print(FILE* filePtr) const
{
    if (NULL == filePtr)
        filePtr = GetDebugLog();
    // Format is "src->dst,protocol,class,ifaceIndex"
    //  where 'src' and 'dst' are addr{mask}/port
    //  as 'mask' or 'port' are applicable.)
    if (0 == flow_keysize)
    {
        fprintf(filePtr, "*");
        return;
    }
    // The "numFields" is used to determine the minimum number of fields to print
    unsigned int numFields;
    if (0 != GetInterfaceIndex())
        numFields = 5;
    else if (ProtoPktIP::RESERVED != GetProtocol())
        numFields = 4;
    else if (0x03 != GetTrafficClass())
        numFields = 3;
    else if ((0 != GetDstLength()) ||
             (0 != GetSrcLength()))
        numFields = 2;
    else
        numFields = 1;
    if (0 != GetSrcLength())
    {
        ProtoAddress addr;
        GetSrcAddr(addr);
        fprintf(filePtr, "%s", addr.GetHostString());
        if (GetSrcMaskLength() != GetSrcLength() << 3)
            fprintf(filePtr, "/%d", GetSrcMaskLength());
        //if (0 != addr.GetPort())
        //    fprintf(filePtr, "/%hu", addr.GetPort());
    }
    else
    {
        fprintf(filePtr, "*");
    }
    if (0 == --numFields) return;
    if (0 != GetDstLength())
    {
        ProtoAddress addr;
        GetDstAddr(addr);
        fprintf(filePtr, "->%s", addr.GetHostString());
        if (GetDstMaskLength() != GetDstLength() << 3)
            fprintf(filePtr, "/%d", GetDstMaskLength());
        //if (0 != addr.GetPort())
        //    fprintf(filePtr, "/%hu", addr.GetPort());
    }
    else
    {
        fprintf(filePtr, "->*");
    }
    if (0 == --numFields) return;
    if (ProtoPktIP::RESERVED != GetProtocol())
        fprintf(filePtr, ",%d", GetProtocol());
    else
        fprintf(filePtr, ",*");
    if (0 == --numFields) return;
    if (0x03 != GetTrafficClass())
        fprintf(filePtr, ",%02x", GetTrafficClass());
    else
        fprintf(filePtr, ",*");
    if (0 == --numFields) return;
    if (0 != GetInterfaceIndex())
        fprintf(filePtr, ",%d", GetInterfaceIndex());
}  // end ProtoFlow::Description::Print()

void ProtoFlow::Description::SetKey(const char*             dstAddr,
                                    UINT8                   dstLen,  // bytes
                                    UINT8                   dstMask, // bits
                                    const char*             srcAddr,
                                    UINT8                   srcLen,  // bytes
                                    UINT8                   srcMask, // bits
                                    UINT8                   trafficClass,
                                    ProtoPktIP::Protocol    protocol,
                                    unsigned int            ifaceIndex)
{
    // flow_key:  dlen [+ dst + dmask] + slen [+ src + smask] + class + protocol + index
    unsigned int keylen = dstLen ? (2 + dstLen) : 1;
    keylen += srcLen ? (2 + srcLen) : 1;
    keylen += 2 + sizeof(unsigned int);
    if (keylen > KEY_MAX)
    {
        PLOG(PL_ERROR, "ProtoFlow::Description::SetKey() error: invalid address length\n");
        flow_keysize = 0;
        return;
    }
    unsigned int offset = 0;
    flow_key[offset++] = dstLen;
    prefix_size = 0;
    bool extendPrefix = true;
    if (0 != dstLen)
    {
        memcpy(flow_key+offset, dstAddr, dstLen);
        offset += dstLen;
        flow_key[offset++] = dstMask;
        prefix_size = 8 + dstMask;  // dstLen + dstAddr
        if (dstMask == (dstLen << 3))
            prefix_size += 8;  // dstMask field
        else
            extendPrefix = false;
    }
    else
    {
        extendPrefix = false;
    }
    flow_key[offset++] = srcLen;
    if (0 != srcLen)
    {
        memcpy(flow_key+offset, srcAddr, srcLen);
        offset += srcLen;
        flow_key[offset++] = srcMask;
        if (extendPrefix)
        {
            prefix_size += 8 + srcMask;  // srcLen + srcAddr
            if (srcMask == (srcLen << 3))
                prefix_size += 8;  // srcMask field
            else
                extendPrefix = false;
        }
    }
    else if (extendPrefix)
    {
        extendPrefix = false;
    }
    flow_key[offset++] = trafficClass;
    if (extendPrefix)
    {
        if (0x03 != trafficClass)
            prefix_size += 8;  // trafficClass field
        else
            extendPrefix = false;
    }
    flow_key[offset++] = protocol;
    if (extendPrefix)
    {
        if (ProtoPktIP::RESERVED != protocol)
            prefix_size += 8;  // protocol field
        else
            extendPrefix = false;
    }
    memcpy(flow_key+offset, &ifaceIndex, sizeof(unsigned int));
    if (extendPrefix && (0 != ifaceIndex))
        prefix_size += sizeof(unsigned int) << 3;  // ifaceIndex field
    flow_keysize = keylen << 3;  // convert to length in bits
}  // end ProtoFlow::Description::SetKey()

void ProtoFlow::Description::InitFromDescription(const Description& description, int flags)
{
    // flow_key:  dlen [+ dst + dmask] + slen [+ src + smask] + class + protocol + index
    if (flags < FLAG_ALL)
    {
        // Custom flags, so do an "edited" copy
        unsigned int keylen = 0;
        unsigned int dstLen = (0 != (flags & FLAG_DST)) ? description.GetDstLength() : 0;
        flow_key[keylen++] = dstLen;
        prefix_size = 0;
        bool extendPrefix = true;
        if (0 != dstLen)
        {
            memcpy(flow_key+keylen, description.GetDstPtr(), dstLen);
            keylen += dstLen;
            UINT8 dstMask = description.GetDstMaskLength();
            flow_key[keylen++] = dstMask;
            prefix_size = 8 + dstMask;  // dstLen + dstAddr
            if (dstMask == (dstLen << 3))
                prefix_size += 8;  // dstMask field
            else
                extendPrefix = false;
        }
        else
        {
            extendPrefix = false;
        }
        unsigned int srcLen = (0 != (flags & FLAG_SRC)) ? description.GetSrcLength() : 0;
        flow_key[keylen++] = srcLen;
        if (0 != srcLen)
        {
            memcpy(flow_key+keylen, description.GetSrcPtr(), srcLen);
            keylen += srcLen;
            UINT8 srcMask = description.GetSrcMaskLength();
            flow_key[keylen++] = srcMask;
            if (extendPrefix)
            {
                prefix_size += 8 + srcMask;  // srcLen + srcAddr
                if (srcMask == (srcLen << 3))
                    prefix_size += 8;  // srcMask field
                else
                    extendPrefix = false;
            }
        }
        else if (extendPrefix)
        {
            extendPrefix = false;
        }
        UINT8 trafficClass = (0 != (flags & FLAG_CLASS)) ? description.GetTrafficClass() : 0x03;
        flow_key[keylen++] = trafficClass;
        if (extendPrefix)
        {
            if (0x03 != trafficClass)
                prefix_size += 8;  // trafficClass field
            else
                extendPrefix = false;
        }
        UINT8 protocol = (0 != (flags & FLAG_PROTO)) ? description.GetProtocol() : ProtoPktIP::RESERVED;
        flow_key[keylen++] = protocol;
        if (extendPrefix)
        {
            if (ProtoPktIP::RESERVED != protocol)
                prefix_size += 8;  // protocol field
            else
                extendPrefix = false;
        }
        unsigned int ifaceIndex =  (0 != (flags & FLAG_INDEX)) ? description.GetInterfaceIndex() : 0;
        memcpy(flow_key+keylen, &ifaceIndex, sizeof(unsigned int));
        keylen += sizeof(unsigned int);
        if (extendPrefix && (0 != ifaceIndex))
            prefix_size += sizeof(unsigned int) << 3;  // ifaceIndex field
        flow_keysize = keylen << 3;
    }
    else
    {
        // use all descriptors (exact copy)
        memcpy(flow_key, description.GetKey(), description.GetKeysize() >> 3);
        flow_keysize = description.flow_keysize;
        prefix_size = description.prefix_size;
    }
}  // end ProtoFlow::Description::InitFromDescription()

bool ProtoFlow::Description::InitFromPkt(ProtoPktIP& ipPkt, unsigned int ifaceIndex, int flags)
{
    // flow_key:  dlen [+ dst + dmask] + slen [+ src + smask] + class + protocol + index
    switch (ipPkt.GetVersion())
    {
        case 4:
        {
            const ProtoPktIPv4 ip4Pkt(ipPkt);
            UINT8 dstLen = 4;
            UINT8 srcLen = 4;
            UINT8 trafficClass = ip4Pkt.GetTOS() & 0xfc;
            ProtoPktIP::Protocol protocol = ip4Pkt.GetProtocol();
            if (flags < FLAG_ALL)
            {
                // optional "flags" were set
                if (0 == (flags & FLAG_DST)) dstLen = 0;
                if (0 == (flags & FLAG_SRC)) srcLen = 0;
                if (0 == (flags & FLAG_CLASS)) trafficClass = 0x03;
                if (0 == (flags & FLAG_PROTO)) protocol = ProtoPktIP::RESERVED;
            }
            SetKey((const char*)ip4Pkt.GetDstAddrPtr(), dstLen, 32,
                   (const char*)ip4Pkt.GetSrcAddrPtr(), srcLen, 32,
                   trafficClass, protocol, ifaceIndex);
            return true;
        }
        case 6:
        {
            const ProtoPktIPv6 ip6Pkt(ipPkt);
            UINT8 dstLen = 16;
            UINT8 srcLen = 16;
            UINT8 trafficClass = ip6Pkt.GetTrafficClass() & 0xfc;
            ProtoPktIP::Protocol protocol = ip6Pkt.GetNextHeader();
            if (flags < FLAG_ALL)
            {
                // optional "flags" were set
                if (0 == (flags & FLAG_DST)) dstLen = 0;
                if (0 == (flags & FLAG_SRC)) srcLen = 0;
                if (0 == (flags & FLAG_CLASS)) trafficClass = 0x03;
                if (0 == (flags & FLAG_PROTO)) protocol = ProtoPktIP::RESERVED;
            }
            SetKey((const char*)ip6Pkt.GetDstAddrPtr(), dstLen, 128,
                   (const char*)ip6Pkt.GetSrcAddrPtr(), srcLen, 128,
                   trafficClass, protocol, ifaceIndex);
            return true;
        }
        default:
            PLOG(PL_ERROR, "ProtoFlow::Description::InitFromPkt() error: invalid IP version!\n");
            SetKeysize(0); // invalid IP version
            return false;
    }
}  // end ProtoFlow::Description::InitFromPkt()

bool ProtoFlow::Description::InitFromText(const char* theText)
{
    // format: [<srcAddr>[/maskLen]->]<dstAddr>[/maskLen][,<protocol>[,<class>]] (also can use X or * to wildcard fields)
    // Notes:
    // 1) <srcAddr> can an interface name
    // 2) Can consist of just a single destination address with no delimiters (e.g., "224.1.2.3")
    
    ProtoTokenator tk(theText, ',');
    // 1) Check for presence of "->" src->dst delimiter as alternative syntax to comma delimiter
    //    (that is the format used by Description::Print(), trpr, etc, i.e., an Adamson-ism)
    const char* text;
    char* srcText = NULL;
    bool dstOnly = false;
    const char* dptr = strstr(theText, "->");
    if (NULL != dptr)
    {
        // Copy the <srcAddr> field (prior to "->" delimiter ptr)
        int srcLen = dptr - theText;
        // Set the tokenator "cursor" to after the "->" delimiter
        tk.Reset(dptr + 2, ',');
        if (NULL == (srcText = new char[srcLen + 1]))
        {
            PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() new char[] error: %s\n", GetErrorString());
            return false;
        }
        strncpy(srcText, theText, srcLen);
        srcText[srcLen] = '\0';
        // strip trailing whitespace
        char* ptr = srcText + srcLen - 1;
        while ((ptr >= srcText) && isspace(*ptr))
            *ptr-- = '\0';
        // strip leading whitespace
        ptr = srcText;
        while (isspace(*ptr) && ('\0' != *ptr))
            ptr++;
        text = ptr;
    }
    else if (NULL == strchr(theText, ','))
    {
        dstOnly = true;
    }
    else
    {
        text = tk.GetNextItem();
    }
    ProtoAddress srcAddr;
    int srcMaskValue = -1;
    if (dstOnly)
    {
        srcMaskValue = 0;  // no source was specified
    }
    else if (NULL != text)
    {   
        ProtoTokenator tk2(text, '/');
        const char* text2 = tk2.GetNextItem();
        if (('*' == text2[0]) || ('X' == text2[0]))
        {
            srcMaskValue = 0;  // wildcarded srcAddr
            text2 = NULL;
        }
        else if ('/' != text[0])
        {
            if (!srcAddr.ConvertFromString(text2))
            {
                if (!ProtoNet::GetInterfaceAddress(text2, ProtoAddress::IPv4, srcAddr) &&
                    !ProtoNet::GetInterfaceAddress(text2, ProtoAddress::IPv6, srcAddr))
                {
                    PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: invalid srcAddr or interface name \"%s\"\n", text2);
                    if (NULL != srcText) delete[] srcText; // free the array that was allocated
                    return false;
                }
            }
            text2 = tk2.GetNextItem();
        }
        if (NULL != text2)
        {
            if (1 != sscanf(text2, "%d", &srcMaskValue) || (srcMaskValue < 0) || (srcMaskValue > 8*srcAddr.GetLength()))
            {
                PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: invalid srcAddr mask length \"%s\"\n", text2);
                if (NULL != srcText) delete[] srcText; // free the array that was allocated
                return false;
            }
        }   
    }
    else
    {
        PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: empty string?!\n");
        if (NULL != srcText) delete[] srcText; // free the array that was allocated
        return false;
    }
    if (NULL != srcText) delete[] srcText; // free the array that was allocated
    text = tk.GetNextItem();
    ProtoAddress dstAddr;
    int dstMaskValue = -1;
    if (NULL != text)
    {   
        ProtoTokenator tk2(text, '/');
        const char* text2 = tk2.GetNextItem();
        if (('*' == text2[0]) || ('X' == text2[0]))
        {
            dstMaskValue = 0;  // wildcarded dstAddr
            text2 = NULL;
        }
        else if ('/' != text[0])
        {
            if (!dstAddr.ConvertFromString(text2))
            {
                PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: invalid dstAddr \"%s\"\n", text2);
                return false;
            }
            text2 = tk2.GetNextItem();
        }
        if (NULL != text2)
        {
            if (1 != sscanf(text2, "%d", &dstMaskValue) || (dstMaskValue < 0) || (dstMaskValue > 8*dstAddr.GetLength()))
            {
                PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: invalid dstAddr mask length \"%s\"\n", text2);
                return false;
            }
        }   
    }
    else
    {
        PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: missing dstAddr value\n");
        return false;
    }
    text = tk.GetNextItem();
    ProtoPktIP::Protocol protocol = ProtoPktIP::RESERVED;
    if (NULL != text)
    {
        // TBD - support protocols by name (e.g. 'udp', 'tcp', etc)
        if (('*' != text[0]) && ('X' != text[0]))
        {
            int value;
            if ((1 != sscanf(text, "%d", &value)) || (value < 0) || (value > 254))
            {
                PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: invalid numeric protocol type value \"%s\"\n", text);
                return false;
            }
            protocol = (ProtoPktIP::Protocol)value;
        }
    }
    text = tk.GetNextItem();
    UINT8 trafficClass = 0x03;  // default traffic class
    if (NULL != text)
    {
        // TBD - support protocols by name (e.g. 'udp', 'tcp', etc)
        if (('*' != text[0]) && ('X' != text[0]))
        {
            int value;
            if ((1 != sscanf(text, "%d", &value)) || (value < 0) || (value > 255))
            {
                PLOG(PL_ERROR, "ProtoFlow::Description::InitFromText() error: invalid numeric traffic class value \"%s\"\n", text);
                return false;
            }
            trafficClass = (UINT8)value;
        }
    }
    // Use information collected from parsing to init ProtoFlow::Description via its flow_key
    UINT8 dstLen, dstMaskLen, srcLen, srcMaskLen;
    const char* dstPtr;
    const char* srcPtr;
    if (dstAddr.IsValid())
    {
        dstPtr = dstAddr.GetRawHostAddress();
        dstLen = dstAddr.GetLength();   // in bytes
        if (dstMaskValue < 0)
            dstMaskLen = dstLen << 3;   // in bits
        else
            dstMaskLen = (UINT8)dstMaskValue;
    }
    else
    {
        dstPtr = NULL;
        dstLen = 0;
        dstMaskLen = 0;
    }
    if (srcAddr.IsValid())
    {
        srcPtr = srcAddr.GetRawHostAddress();
        srcLen = srcAddr.GetLength();  // in bytes
        if (srcMaskValue < 0)
            srcMaskLen = srcLen << 3;  // to bits
        else
            srcMaskLen = (UINT8)srcMaskValue;
    }
    else
    {
        srcPtr = NULL;
        srcLen = 0;
        srcMaskLen = 0;
    }
    SetKey(dstPtr, dstLen, dstMaskLen, srcPtr, srcLen, srcMaskLen, trafficClass, protocol);
    return true;
}  // end ProtoFlow::Description::InitFromText()

void ProtoFlow::Description::SetDstMaskLength(UINT8 dstMask)
{
    UINT8 dstLen = GetDstLength();
    if (0 != dstLen)
    {
        // Set dst mask field
        flow_key[Offset_Dmask()] = dstMask;
        // Update prefix_size
        prefix_size = 8 + dstMask;  // dstLen + dstAddr
        if (dstMask == (dstLen << 3))
            prefix_size += 8; // dstMask field
        else
            return;
        UINT8 srcLen = GetSrcLength();
        if (0 != srcLen)
        {
            UINT8 srcMask = GetSrcMaskLength();
            prefix_size += 8 + srcMask;  // srcLen + srcAddr
            if (srcMask == (srcLen << 3))
                prefix_size += 8;  // srcMask field
            else
                return;
            if (0x03 != GetTrafficClass())
                prefix_size += 8;  // trafficClass field
            else
                return;
            if (ProtoPktIP::RESERVED != GetProtocol())
                prefix_size += 8;  // protocol field
            else
                return;
            if (0 != GetInterfaceIndex())
                prefix_size += sizeof(unsigned int) << 3;
        }
    }
}  // end ProtoFlow::Description::SetDstMaskLength()

void ProtoFlow::Description::SetSrcMaskLength(UINT8 srcMask)
{
    UINT8 srcLen = GetSrcLength();
    if (0 == srcLen) return;  // no change
    flow_key[Offset_Smask()] = srcMask;
    // Update prefix_size
    UINT8 dstLen = GetDstLength();
    if (0 != dstLen)
    {
        UINT8 dstMask = GetDstMaskLength();
        prefix_size = 8 + dstMask;  // dstLen + dstAddr
        if (dstMask == (dstLen << 3))
            prefix_size += 8; // dstMask field
        else
            return;
        prefix_size += 8 + srcMask;  // srcLen + srcAddr
        if (srcMask == (srcLen << 3))
            prefix_size += 8;  // srcMask field
        else
            return;
        if (0x03 != GetTrafficClass())
            prefix_size += 8;  // trafficClass field
        else
            return;
        if (ProtoPktIP::RESERVED != GetProtocol())
            prefix_size += 8;  // protocol field
        else
            return;
        if (0 != GetInterfaceIndex())
            prefix_size += sizeof(unsigned int) << 3;
    }
}  // end ProtoFlow::Description::SetSrcMaskLength()

bool ProtoFlow::Description::GetDstAddr(ProtoAddress& addr) const
{
    // flow_key:  dlen [+ dst + dmask] + slen [+ src + smask] + class + protocol + index
    switch (GetDstLength())
    {
        case 0:
            addr.Invalidate();
            return false;
        case 4:
            addr.SetRawHostAddress(ProtoAddress::IPv4, flow_key+OFFSET_DST, 4);
            return true;
        case 16:
            addr.SetRawHostAddress(ProtoAddress::IPv6, flow_key+OFFSET_DST, 16);
            return true;
        default:
            PLOG(PL_ERROR, "ProtoFlow::Description::GetDstAddr() error: invalid address length\n");
            return false;
    }
}  // end ProtoFlow::Description::GetDstAddr()

bool ProtoFlow::Description::GetSrcAddr(ProtoAddress& addr) const
{
    // flow_key:  dlen [+ dmask + dst] + slen [+ smask + src] + class + protocol + index
    switch (GetSrcLength())
    {
        case 0:
            addr.Invalidate();
            return false;
        case 4:
            addr.SetRawHostAddress(ProtoAddress::IPv4, flow_key+Offset_Src(), 4);
            return true;
        case 16:
            addr.SetRawHostAddress(ProtoAddress::IPv6, flow_key+Offset_Src(), 16);
            return true;
        default:
            PLOG(PL_ERROR, "ProtoFlow::Description::GetSrcAddr() error: invalid address length\n");
            return false;
    }
}  // end ProtoFlow::Description::GetSrcAddr()

ProtoFlow::Table::MaskLengthList::MaskLengthList()
 : list_length(0)
{
    memset(ref_count, 0, 129*sizeof(unsigned int));
}

ProtoFlow::Table::MaskLengthList::~MaskLengthList()
{
}

void ProtoFlow::Table::MaskLengthList::Insert(UINT16 value)
{
    if (value > MASK_SIZE_MASK)
    {
        PLOG(PL_ERROR, "ProtoFlow::Table::MaskLengthList::Insert() error: invalid value\n");
        return;
    }
    if (ref_count[value] > 0)
    {
        ref_count[value] += 1;
        return;  // already in list
    }
    else
    {
        ref_count[value] = 1;
    }
    if (0 == list_length)
    {
        mask_list[0] = value;
        list_length += 1;
    }
    else if (value > mask_list[0])
    {
        memmove(mask_list+1, mask_list, list_length);
        mask_list[0] = value;
        list_length += 1;
    }
    else if (value < mask_list[list_length - 1])
    {
        mask_list[list_length] = value;
        list_length += 1;
    }
    else
    {
        // Do a hybrid binary/linear search
        UINT16 index = list_length/2;
        UINT16 delta = index;
        UINT16 x = mask_list[index];
        // Binary search portion
        while (delta > 1)
        {
            delta /= 2;
            if (value < x)
                index += delta;
            else if (value > x)
                index -= delta;
            else
                return;  // value already in list
            x = mask_list[index];
        }
        // Linear search portion
        if (value < x)
            do {x = mask_list[++index];} while (value < x);
        else if (value > x)
            do {x = mask_list[--index];} while (value > x);
        if (value < x)
        {
            memmove(mask_list+index+2, mask_list+index+1, list_length-index-1);
            mask_list[index+1] = value;
            list_length += 1;
        }
        else if (value > x)
        {
            memmove(mask_list+index+1, mask_list+index, list_length-index);
            mask_list[index] = value;
            list_length += 1;
        }
        // else value already in list
    }
}  // end ProtoFlow::Table::MaskLengthList::Insert()

void ProtoFlow::Table::MaskLengthList::Remove(UINT16 value)
{
    if ((0 == list_length) || (value > mask_list[0]) || (value < mask_list[list_length-1]))
        return;  // value out of list range

    if (ref_count[value] > 1)
    {
        ref_count[value] -= 1;
        return;  // still has non-zero reference count
    }
    else if (ref_count[value] > 0)
    {
        ref_count[value] = 0;
    }
    else
    {
        return;  // not in list
    }
    // Do a hybrid binary/linear search
    UINT16 index = list_length/2;
    UINT16 delta = index;
    UINT16 x = mask_list[index];
    // Binary search portion
    while (delta > 1)
    {
        delta /= 2;
        if (value < x)
        {
            index += delta;
        }
        else if (value > x)
        {
            index -= delta;
        }
        else
        {
            // Found it directly via binary search
            memmove(mask_list+index, mask_list+index+1, list_length-index-1);
            list_length -= 1;
            return;
        }
        x = mask_list[index];
    }
    // Linear search portion
    if (value < x)
        do {x = mask_list[++index];} while (value < x);
    else if (value > x)
        do {x = mask_list[--index];} while (value > x);
    if (value == x)
    {
        memmove(mask_list+index, mask_list+index+1, list_length-index-1);
        list_length -= 1;
    }
    // else not in list
}  // end ProtoFlow::Table::MaskLengthList::Remove()

ProtoFlow::Table::Entry::Entry(const ProtoAddress&  dst,
                               const ProtoAddress&  src,
                               UINT8                trafficClass,
                               ProtoPktIP::Protocol theProtocol,
                               unsigned int         ifaceIndex)
  : flow_description(dst, src, trafficClass, theProtocol, ifaceIndex)
{
}

ProtoFlow::Table::Entry::Entry(const Description& flowDescription, int flags)
{
    flow_description.InitFromDescription(flowDescription, flags);
}

ProtoFlow::Table::Entry::~Entry()
{
}

ProtoFlow::Table::Iterator::Iterator(BaseIterator& tableIterator, MaskLengthList& maskLengthList)
 : table_iterator(tableIterator), mask_iterator(maskLengthList)
{
    // the derived class should call Reset() in its constructor
}

ProtoFlow::Table::Iterator::~Iterator()
{
}

void ProtoFlow::Table::Iterator::Reset(const Description* description, int flags, bool bimatch)
{
    bi_match = bimatch;
    prefix_mask_size = current_mask_size = 0;
    next_mask_size = -1;
    src_addr.Invalidate();
    src_mask_size = 0;
    if (NULL != description)
    {

        const char* prefix = description->GetKey();
        unsigned int prefixSize = 0;
        bool extendPrefix = true;
        UINT8 dstLen = description->GetDstLength();
        UINT8 srcLen = description->GetSrcLength();
        if ((0 != dstLen) && (0 != (Description::FLAG_DST & flags)))
        {
            UINT8 dstMask = description->GetDstMaskLength();
            prefixSize = 8 + dstMask;  // dstLen + dstAddr
            if (dstMask == (dstLen << 3))
                prefixSize += 8; // dstMask field
            else
                extendPrefix = false;
        }
        else
        {
            extendPrefix = false;
            prefix = NULL;
        }
        if ((0 != srcLen) && (0 != (Description::FLAG_SRC & flags)))
        {
            src_mask_size = description->GetSrcMaskLength();
            if (extendPrefix)
            {
                prefixSize += 8 + src_mask_size; // srcLen + srcAddr fields
                if (src_mask_size == (srcLen << 3))
                    prefixSize += 8;  // srcMask field
                else
                    extendPrefix = false;
            }
            else
            {
                // Need to match src address separately
                description->GetSrcAddr(src_addr);
            }
        }
        else if (extendPrefix)
        {
            extendPrefix = false;
        }
        traffic_class = (0 != (Description::FLAG_CLASS & flags)) ? description->GetTrafficClass() : 0x03;
        if (extendPrefix)
        {
            if (0x03 != traffic_class)
                prefixSize += 8;  // trafficClass field
            else
                extendPrefix = false;
        }
        protocol = (0 != (Description::FLAG_PROTO & flags)) ? description->GetProtocol() : ProtoPktIP::RESERVED;
        if (extendPrefix)
        {
            if (ProtoPktIP::RESERVED != protocol)
                prefixSize += 8;  // protocol field
            else
                extendPrefix = false;
        }
        iface_index = (0 != (Description::FLAG_INDEX & flags)) ? description->GetInterfaceIndex() : 0;
        if (extendPrefix && (0 != iface_index))
            prefixSize += sizeof(unsigned int) << 3;
        prefix_mask_size = prefixSize;
        // Save prefix key for iterator resets as needed.
        UINT8 prefixBytes = (UINT8)((prefixSize + 0x07) >> 3);
        memcpy(prefix_mask, description->GetKey(), prefixBytes);
        mask_iterator.Reset();
        current_mask_size = mask_iterator.GetNextMaskSize();
        if (current_mask_size < 0) current_mask_size = 0;
        next_mask_size = current_mask_size;
        while (next_mask_size >= prefix_mask_size)
        {
            current_mask_size = next_mask_size;
            next_mask_size = mask_iterator.GetNextMaskSize();
        }
        if (prefix_mask_size < current_mask_size)
            current_mask_size = prefix_mask_size;
        table_iterator.Reset(false, prefix, prefixSize);
    }
    else
    {
        // Iterate over everything in table
        table_iterator.Reset();
        src_addr.Invalidate();
        src_mask_size = 0;
        traffic_class = 0x03;
        protocol = ProtoPktIP::RESERVED;
        iface_index = 0;
    }
}  // end ProtoFlow::Table::Iterator::Reset()

ProtoFlow::Table::Entry* ProtoFlow::Table::Iterator::GetNextEntry()
{
    Entry* entry = nullptr;
    while (current_mask_size >= 0)
    {
        while (NULL != (entry = table_iterator.GetNextEntry()))
        {
            if (0 != prefix_mask_size)
            {
                // This conditional makes sure the longest matching prefix matches are
                // returned first by the iterator.  The "current_mask_size" can be
                // queried by the user of the iterator to know when the returned matches
                // descend to a short matching prefix length.  The second part of the
                // conditional here ensures matching entries are returned only once
                // upon the secondary, etc. iterations at different prefix mask lengths
                // according to the "mask_list" of table being iterated.

                if ((entry->GetPrefixSize() < current_mask_size) ||
                    (((entry->GetPrefixSize() > current_mask_size) || !bi_match) && (current_mask_size < prefix_mask_size)))
                {
                    continue;  // not a match (yet)
                }
            }
            if (src_addr.IsValid())
            {
                ProtoAddress src;
                entry->GetSrcAddr(src);
                if ((src.IsValid() || !bi_match) && !src.PrefixIsEqual(src_addr, src_mask_size))
                        continue; // not a match
            }
            if (0x03 != traffic_class)
            {
                UINT8 t = entry->GetTrafficClass();
                if (((0x03 != t) || !bi_match) && (t != traffic_class))
                    continue; // not a match
            }
            if (ProtoPktIP::RESERVED != protocol)
            {
                ProtoPktIP::Protocol p = entry->GetProtocol();
                if (((ProtoPktIP::RESERVED != p) || !bi_match) && (p != protocol))
                    continue;  // not a match
            }
            break;
        }
        if (NULL == entry)
        {
            current_mask_size = next_mask_size;
            if (next_mask_size >= 0)
            {
                // Adjust prefix iteration length (adding 8 bits for dstLen field)
                table_iterator.Reset(false, prefix_mask, next_mask_size);
                next_mask_size = mask_iterator.GetNextMaskSize();
            }
        }
        else
        {
            break;
        }
    }
    return entry;
}  // end ProtoFlow::Table::Iterator::GetNextEntry()

ProtoFlow::Table::Entry* ProtoFlow::Table::Iterator::FindBestMatch(const Description& flowDescription, bool deepSearch)
{
    // IMPORTANT: This function assumes the iterator has already been Reset() with the given "flowDescription"
    // This uses prefix iteration over our ProtoFlow::Description tuple to find the best match
    // (The EntryIterator returns longest destination prefix matches first and
    //  thus we can end the search early when the iterator "current mask len"
    //  decrements below where a candidate match has already been found.  The
    // "deepSearch" option is provided to bypass this early break if desired
    //  E.g., when best match to source address is desired when there are
    //  entries of different destination mask lengths).
    // Note that the "trafficClass" and "protocol" elements have low weighting
    // as compared to address/prefix matching. And "trafficClass" trumps "protocol".
    UINT8 trafficClass = flowDescription.GetTrafficClass();
    ProtoPktIP::Protocol protocol = flowDescription.GetProtocol();
    int lastMaskLen = GetCurrentMaskLength();
    Entry* bestMatch = NULL;
    unsigned int bestWeight = 0;
    Entry* entry;
    while (NULL != (entry = GetNextEntry()))
    {
        int currentMaskLen = GetCurrentMaskLength();
        if (NULL == bestMatch)
        {
            lastMaskLen = currentMaskLen;
            unsigned int weight = entry->GetDstMaskLength() + entry->GetSrcMaskLength();  // dst and src match
            if (entry->GetTrafficClass() == 0x03)
                weight += 1;
            else if (entry->GetTrafficClass() == trafficClass)
                weight += 2;
            else
                continue;  // not a match (this should never happen if iterator is doing its job)
            if (entry->GetProtocol() == ProtoPktIP::RESERVED)
                weight += 1;
            else if (entry->GetProtocol() == protocol)
                weight += 2;
            else
                continue;  // not a match (this should never happen if iterator is doing its job)
            bestMatch = entry;
            bestWeight = weight;
            continue;
        }
        else if ((currentMaskLen < lastMaskLen) && !deepSearch)
        {
            // Don't match against shorter dst masks if a match has been found
            break;
        }
        lastMaskLen = currentMaskLen;
        unsigned int weight = entry->GetDstMaskLength() + entry->GetSrcMaskLength();  // dst and src match
        if (entry->GetTrafficClass() == 0x03)
            weight += 1;
        else if (entry->GetTrafficClass() == trafficClass)
            weight += 2;
        else
            continue;  // not a match (this should never happen if iterator is doing its job)
        if (entry->GetProtocol() == ProtoPktIP::RESERVED)
            weight += 1;
        else if  (entry->GetProtocol() == protocol)
            weight += 2;
        else
            continue;  // not a match (this should never happen if iterator is doing its job)
        if (weight > bestWeight)
        {
            bestMatch = entry;
            bestWeight = weight;
            continue;
        }
        else if (weight == bestWeight)
        {
            // Break tie (trafficClass match trumps protocol match)
            if (entry->GetTrafficClass() == trafficClass)
                bestMatch = entry;
        }
    }  // end while (entry = GetNextEntry())
    return bestMatch;
}  // end ProtoFlow::Table::Iterator::FindBestMatch()
