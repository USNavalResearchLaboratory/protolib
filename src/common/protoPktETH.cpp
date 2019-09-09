/**
* @file protoPketETH.cpp
* 
* @brief This is a class that provides access to and control of ETHernet header fields for the associated buffer space. The ProtoPkt "buffer" is presumed to contain a valid ETHernet frame
*/
#include "protoPktETH.h"

ProtoPktETH::ProtoPktETH(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{    
}

ProtoPktETH::~ProtoPktETH()
{
}
