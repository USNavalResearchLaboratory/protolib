#ifndef _PROTO_PKT
#define _PROTO_PKT

#include "protoDefs.h"  // for UINT32, etc types

#ifdef WIN32
#include "winsock2.h"   // for ntohl(), htonl(), etc
#else
#include <netinet/in.h> // for ntohl(), htonl(), etc
#endif // if/else WIN32/UNIX
#include <string.h>     // for memcpy()

/**
 * @class ProtoPkt
 * 
 * @brief This is a base class that maintains a 32-bit 
 * aligned buffer for "packet"
 * (or message) building and parsing.  
 *
 * Generally, classes will be derived
 * from this base class to create classed for 
 * protocol-specific packet/message
 * building and parsing (For examples, see 
 * ProtoPktIP, ProtoPktRTP, etc)
 */
 
 // TBD - we should make this a template class so we can use different "buffer_ptr" types
 //       such as char*, UINT16*, etc depending upon the alignment requirements of the
 //       packet format specification.
 
class ProtoPkt
{
    public:
        ProtoPkt(UINT32* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);
        virtual ~ProtoPkt();
        
        bool AllocateBuffer(unsigned int numBytes)
        {
            unsigned int len = numBytes / sizeof(unsigned int);
            len += (0 == (len % sizeof(int))) ? 0 : 1;
            buffer_ptr = buffer_allocated = new UINT32[len];
            buffer_bytes = (NULL != buffer_ptr) ? numBytes : 0;
            pkt_length = 0;
            return (NULL != buffer_ptr);
        }
        void AttachBuffer(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct = false)
        {
            buffer_ptr = (0 != numBytes) ? bufferPtr : NULL;
            buffer_bytes = (NULL != bufferPtr) ? numBytes : 0;
            pkt_length = 0;
            if (NULL != buffer_allocated) delete[] buffer_allocated;
            if (freeOnDestruct) buffer_allocated = bufferPtr;
        }
        UINT32* DetachBuffer()
        {
            UINT32* theBuffer = buffer_ptr;
            buffer_allocated = buffer_ptr = NULL; 
            pkt_length = buffer_bytes = 0;
            return theBuffer;  
        }
        bool InitFromBuffer(unsigned int    packetLength,
                            UINT32*         bufferPtr = NULL,
                            unsigned int    numBytes = 0,
                            bool            freeOnDestruct = false)
        {
            if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
            bool result = (packetLength <= buffer_bytes);
            pkt_length = result ? packetLength : 0;
            return result;
        }
        
        const char* GetBuffer() const {return (char*)buffer_ptr;} 
        const UINT32* GetBuffer32() const {return buffer_ptr;}
        unsigned int GetBufferLength() const {return buffer_bytes;}
        unsigned int GetLength() const {return pkt_length;} 
        
        UINT32* AccessBuffer() {return buffer_ptr;}
        void SetLength(unsigned int bytes) {pkt_length = bytes;}
            
    protected:
        UINT8 GetUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        void SetUINT8(unsigned int byteOffset, UINT8 value)
            {((UINT8*)buffer_ptr)[byteOffset] = value;}     
        
        // These helper methods are defined for setting multi-byte protocol
        // fields in one of two ways:  1) "cast and assign", or 2) memcpy()
        // **IMPORTANT** Note the offsets are _byte_ offsets!!!
#ifdef CAST_AND_ASSIGN
        static UINT16 GetUINT16(UINT16* ptr) const
            {return ntohs(*ptr);}
        static UINT32 GetUINT32(UINT32* ptr) const
            {return ntohl(*ptr);}
        static void SetUINT16(UINT16* ptr, UINT16 value) 
            {*ptr = htons(value);}
        static void SetUINT32(UINT32* ptr, UINT32 value)
            {*ptr = htonl(value);}
        
        UINT16 GetUINT16(unsigned int byteOffset) const
            {return ntohs((UINT16*)(((char*)buffer_ptr) + byteOffset));}     
        UINT32 GetUINT32(unsigned int byteOffset) const
            {return ntohl((UINT32*)(((char*)buffer_ptr) + byteOffset));}    
        void SetUINT16(unsigned int byteOffset, UINT16 value)
            {*((UINT16*)(((char*)buffer_ptr) + byteOffset)) = htons(value);}        
        void SetUINT32(unsigned int byteOffset, UINT32 value)
            {*((UINT32*)(((char*)buffer_ptr) + byteOffset)) = htonl(value);}
#else
        static UINT16 GetUINT16(UINT16* ptr)
        {
            UINT16 value;
            memcpy(&value, ptr, sizeof(UINT16));
            return ntohs(value);
        } 
        static UINT32 GetUINT32(UINT32* ptr)
        {
            UINT32 value;
            memcpy(&value, ptr, sizeof(UINT32));
            return ntohl(value);
        }
        static void SetUINT16(UINT16* ptr, UINT16 value)
        {
            value = htons(value);
            memcpy(ptr, &value, sizeof(UINT16));
        }
        static void SetUINT32(UINT32* ptr, UINT32 value)
        {
            value = htonl(value);
            memcpy(ptr, &value, sizeof(UINT32));
        }
        
        UINT16 GetUINT16(unsigned int byteOffset) const
        {
            UINT16 value;
            memcpy(&value, (char*)buffer_ptr + byteOffset, 2);
            return ntohs(value);
        }
        UINT32 GetUINT32(unsigned int byteOffset) const
        {
            UINT32 value;
            memcpy(&value, (char*)buffer_ptr + byteOffset, 4);
            return ntohl(value);
        }
        void SetUINT16(unsigned int byteOffset, UINT16 value)
        {
            value = htons(value);
            memcpy((char*)buffer_ptr + byteOffset, &value, 2);
        }
        void SetUINT32(unsigned int byteOffset, UINT32 value)
        {
            value = htonl(value);
            memcpy((char*)buffer_ptr + byteOffset, &value, 4);
        }
#endif // if/else CAST_AND_ASSIGN
            
            
        UINT32*         buffer_ptr;
        UINT32*         buffer_allocated;
        unsigned int    buffer_bytes;
        unsigned int    pkt_length;
        
};  // end class ProtoPkt

#endif // _PROTO_PKT
