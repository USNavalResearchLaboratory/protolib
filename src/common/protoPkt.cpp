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


