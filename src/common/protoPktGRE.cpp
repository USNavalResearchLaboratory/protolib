#include "protoPktGRE.h"

ProtoPktGRE::ProtoPktGRE(void*          bufferPtr,
                         unsigned int   numBytes,
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer(numBytes);
    else
        InitIntoBuffer();
}


ProtoPktGRE::~ProtoPktGRE()
{
}

bool ProtoPktGRE::InitIntoBuffer(void*        bufferPtr,
                                 unsigned int bufferBytes,
                                 bool         freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    if (GetBufferLength() >= 2*OFFSET_PROTOCOL)
    {
        memset(AccessBuffer(), 0, 4);
        ProtoPkt::SetLength(4);
        return true;
    }
    else
    {
        return false;
    }
}   // end ProtoPktGRE::InitIntoBuffer()

UINT16 ProtoPktGRE::CalculateChecksum(bool set)
{
    UINT32 sum = 0;
    // Calculate checksum, skipping the checksum field itself
    unsigned int i;
    for (i = 0; i < OFFSET_CHECKSUM; i++)
        sum += GetWord16(i);
    unsigned int pktEndex = GetLength() / 2;
    for (i = OFFSET_CHECKSUM+1; i < pktEndex; i++)
        sum += GetWord16(i);
    while (sum >> 16)
        sum = (sum & 0x0000ffff) + (sum >> 16);
    sum = ~sum;
    if (set) SetChecksum(sum);
    return sum;
}  // ProtoPktGRE::CalculateChecksum()

void ProtoPktGRE::SetPayloadLength(UINT16 numBytes, bool calculateChecksum)
{
    SetLength(numBytes + GetHeaderLength());
    if (calculateChecksum) CalculateChecksum();
}  // end ProtoPktIPv4::SetPayloadLength()
