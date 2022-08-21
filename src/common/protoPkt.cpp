/**
* @file protoPkt.cpp
* 
* @brief This is a base class that maintains a 32-bit aligned buffer for "packet" (or message) building and parsing 
*/
#include "protoDebug.h"
#include "protoPkt.h"

ProtoPkt::ProtoPkt(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
 : buffer_ptr((UINT32*)bufferPtr), buffer_allocated(freeOnDestruct ? (UINT32*)bufferPtr : NULL),
   buffer_bytes(numBytes), pkt_length(0)
{    
    
}

ProtoPkt::~ProtoPkt()
{
    if (NULL != buffer_allocated)
    {
        buffer_ptr = NULL;
        delete[] buffer_allocated;
        buffer_allocated = NULL;
        buffer_bytes = 0;
    }   
}

UINT8 
ProtoPkt::GetUINT8Bits(unsigned int byteOffset, unsigned int bitOffset, unsigned int bitLength) const
{
    if(bitOffset>7)
    {
        byteOffset += bitOffset/8;
        bitOffset = bitOffset % 8;
    }
    UINT8 ans = 0;
    if(bitOffset==0)
    {
        ans = ((UINT8*)buffer_ptr)[byteOffset];
    } else {
        UINT8 byte1 = ((UINT8*)buffer_ptr)[byteOffset];
        UINT8 byte2 = ((UINT8*)buffer_ptr)[byteOffset+1];
        ans = (byte1 << bitOffset) | (byte2>>(8-bitOffset));
    }
    ans = ans >> (8-bitLength);
    return ans;
}
        
void
ProtoPkt::SetUINT8Bits(unsigned int byteOffset,unsigned int bitOffset,UINT8 value, unsigned int bitLength)
{
    if(bitOffset>7)
    {
        byteOffset += bitOffset/8;
        bitOffset = bitOffset % 8;
    }
    value = value << (8-bitLength); //left align the bits
    if(bitOffset==0)
    {
        UINT8 mask = 0xff >> (bitLength);
        ((UINT8*)buffer_ptr)[byteOffset] = (((UINT8*)buffer_ptr)[byteOffset] & mask) | value;
    }
    else
    {
        UINT8 byte1 = ((UINT8*)buffer_ptr)[byteOffset];
        UINT8 mask1 = 0xff << (8-bitOffset);        
        mask1 = mask1 | (0xff >> bitOffset >> bitLength);
        byte1 = (byte1 & mask1) | ((value >> bitOffset) & ~mask1);
        ((UINT8*)buffer_ptr)[byteOffset] = byte1;
        if(8-(int)bitOffset-(int)bitLength < 0) //need to set the second byte as well
        { 
            UINT8 byte2 = ((UINT8*)buffer_ptr)[byteOffset+1];
            UINT8 mask2 = 0xff >> (bitOffset+bitLength-8);
            UINT8 value2 = value << (8-bitOffset);
            byte2 = (byte2 & mask2) | (value2 & ~mask2);
            ((UINT8*)buffer_ptr)[byteOffset+1] = byte2;
        }
    }    
}

UINT16 
ProtoPkt::GetUINT16Bits(unsigned int byteOffset, unsigned int bitOffset, unsigned int bitLength) const
{
    if(bitLength<=8){
        return (UINT16)GetUINT8Bits(byteOffset,bitOffset,bitLength);
    } else {
        return (((UINT16)GetUINT8Bits(byteOffset,bitOffset,8)) << (bitLength-8)) |
                ((UINT16)GetUINT8Bits(byteOffset+1,bitOffset,bitLength-8));
    }
}
void
ProtoPkt::SetUINT16Bits(unsigned int byteOffset,unsigned int bitOffset,UINT16 value, unsigned int bitLength)
{
    if(bitLength<=8){
        SetUINT8Bits(byteOffset,bitOffset,(UINT8)value,bitLength);
    } else {
        SetUINT8Bits(byteOffset,bitOffset,(UINT8)(value>>(bitLength-8)),8);
        UINT8 mask = 0xff >> (16-bitLength);
        SetUINT8Bits(byteOffset+1,bitOffset,(UINT8)(value) & mask,bitLength-8);
    }
}
UINT32 
ProtoPkt::GetUINT32Bits(unsigned int byteOffset, unsigned int bitOffset, unsigned int bitLength) const
{
    if(bitLength <= 16)
    {
        return (UINT32)GetUINT16Bits(byteOffset,bitOffset,bitLength);
    } else {
        return (((UINT32)GetUINT16Bits(byteOffset,bitOffset,16)) << (bitLength-16)) |
                ((UINT32)GetUINT16Bits(byteOffset+2,bitOffset,bitLength-16));
    }
}
void
ProtoPkt::SetUINT32Bits(unsigned int byteOffset,unsigned int bitOffset,UINT32 value, unsigned int bitLength)
{
    if(bitLength<=16){
        SetUINT16Bits(byteOffset,bitOffset,(UINT16)value,bitLength);
    } else {
        SetUINT16Bits(byteOffset,bitOffset,(UINT16)(value>>(bitLength-16)),16);
        UINT16 mask = 0xffff >> (32-bitLength);
        SetUINT16Bits(byteOffset+2,bitOffset,(UINT16)(value) & mask,bitLength-16);
    }
}