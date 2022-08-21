/**
* @file protoPketETH.cpp
* 
* @brief This is a class that provides access to and control of ETHernet header fields for the associated buffer space. The ProtoPkt "buffer" is presumed to contain a valid ETHernet frame
*/
#include "protoPktETH.h"

ProtoPktETH::ProtoPktETH(void*          bufferPtr, 
                         unsigned int   numBytes, 
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{    
}

ProtoPktETH::~ProtoPktETH()
{
}

bool ProtoPktETH::InitIntoBuffer(void*           bufferPtr, 
                                 unsigned int    bufferBytes, 
                                 bool            freeOnDestruct)
{
    if (NULL != bufferPtr) 
    {
        if (bufferBytes < 14)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 14) 
    {
        return false;
    }
    memset(AccessBuffer(), 0, 14);
    SetLength(14);
    return true;
}  // end ProtoPktETH::InitIntoBuffer()
