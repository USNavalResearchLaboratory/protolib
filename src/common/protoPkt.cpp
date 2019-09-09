/**
* @file protoPkt.cpp
* 
* @brief This is a base class that maintains a 32-bit aligned buffer for "packet" (or message) building and parsing 
*/
#include "protoPkt.h"

ProtoPkt::ProtoPkt(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
 : buffer_ptr(bufferPtr), buffer_allocated(freeOnDestruct ? bufferPtr : NULL),
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


