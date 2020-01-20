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
 //       packet format specification ... although we can do that with UINT32* and implement
 //       some logic for downgraded (e.g., UINT16*, etc) pointer types???
 
 // We can make this code safe regardless of alignment by:
 // 1) Use void* arguments where "bufferPtr" values are passed in (and cast to UINT32* under the hood)
 // 2) Add alignment check conditional behavior to all value set/get/access methods
 
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
        
        void SetLength(unsigned int bytes) 
            {pkt_length = bytes;}   
        
        unsigned int GetBufferLength() const 
            {return buffer_bytes;}
        
        unsigned int GetLength() const 
            {return pkt_length;} 
        
        // These methods get/set fields by byte offsets and 
        // do alignment checks to guarantee safety regardless
        // of alignment.
        
        const char* GetBuffer() const 
            {return (char*)buffer_ptr;} 
        const char* GetBuffer(unsigned int byteOffset) const
            {return GetBuffer() + byteOffset;}
        char* AccessBuffer() 
            {return (char*)buffer_ptr;}  
        char* AccessBuffer(unsigned int offset)
            {return (((char*)buffer_ptr) + offset);}
        
        UINT8 GetUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        UINT8& AccessUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        void SetUINT8(unsigned int byteOffset, UINT8 value)
            {((UINT8*)buffer_ptr)[byteOffset] = value;}  
        
        // Pointer alignment checks to verify safe access
        static inline bool IsAligned16(const void* pointer)
            {return (0 == ((uintptr_t)pointer & 1));}
        
        static inline bool IsAligned32(const void* pointer)
            {return (0 == ((uintptr_t)pointer & 3));}
        
        UINT16 GetUINT16(unsigned int byteOffset) const
        {
            const char* ptr = ((const char*)buffer_ptr) + byteOffset;
            if (IsAligned16(ptr))
            {   
                return ntohs(*((UINT16*)((void*)ptr)));
            }
            else
            {
                UINT16 value;
                memcpy(&value, ptr, 2);
                return ntohs(value);
            }
        }
        void SetUINT16(unsigned int byteOffset, UINT16 value)
        {
            char* ptr = ((char*)buffer_ptr) + byteOffset;
            if (IsAligned16(ptr))
            {   
                *((UINT16*)((void*)ptr)) = htons(value);
            }
            else
            {
                value = htons(value);
                memcpy(ptr, &value, 2);
            }
        }
        UINT32 GetUINT32(unsigned int byteOffset) const
        {
            const char* ptr = ((const char*)buffer_ptr) + byteOffset;
            if (IsAligned32(ptr))
            {   
                return ntohl(*((UINT32*)((void*)ptr)));
            }
            else
            {
                UINT32 value;
                memcpy(&value, ptr, 4);
                return ntohl(value);
            }
        }
        void SetUINT32(unsigned int byteOffset, UINT32 value)
        {
            char* ptr = ((char*)buffer_ptr) + byteOffset;
            if (IsAligned32(ptr))
            {   
                *((UINT32*)((void*)ptr)) = htonl(value);
            }
            else
            {
                value = htonl(value);
                memcpy(ptr, &value, 4);
            }
        }
        
        // These MUST only be called by subclasses that are absolutely
        // sure that UINT32 alignment is guaranteed for the 'buffer_ptr'
        const UINT16* GetBuffer16() const 
            {return (UINT16*)buffer_ptr;}
        const UINT16* GetBuffer16(unsigned int wordOffset) const
            {return GetBuffer16() + wordOffset;}
        const UINT32* GetBuffer32() const 
            {return buffer_ptr;}
        const UINT32* GetBuffer32(unsigned int wordOffset) const
            {return GetBuffer32() + wordOffset;}
        
        UINT16* AccessBuffer16()
            {return (UINT16*)buffer_ptr;}
        UINT16* AccessBuffer16(unsigned int wordOffset)
            {return AccessBuffer16() + wordOffset;}
        UINT32* AccessBuffer32()
            {return buffer_ptr;}
        UINT32* AccessBuffer32(unsigned int wordOffset)
            {return AccessBuffer32() + wordOffset;}
            
        // These methods get/set fields by pointer directly
        static UINT16 GetUINT16(const UINT16* ptr) 
            {return ntohs(*ptr);}
        static UINT32 GetUINT32(const UINT32* ptr) 
            {return ntohl(*ptr);}
        static void SetUINT16(UINT16* ptr, UINT16 value) 
            {*ptr = htons(value);}
        static void SetUINT32(UINT32* ptr, UINT32 value)
            {*ptr = htonl(value);}    
        
        // These methods get/set field by aligned word offsets
        UINT16 GetWord16(unsigned int wordOffset) const
            {return GetUINT16(GetBuffer16(wordOffset));}
        UINT16& AccessWord16(unsigned int wordOffset)
            {return AccessBuffer16(wordOffset)[0];}
        void SetWord16(unsigned int wordOffset, UINT16 value) 
            {SetUINT16(AccessBuffer16(wordOffset), value);}
        UINT32 GetWord32(unsigned int wordOffset) const
            {return GetUINT32(GetBuffer32(wordOffset));}
        void SetWord32(unsigned int wordOffset, UINT32 value) 
            {SetUINT32(AccessBuffer32(wordOffset), value);}
        
        bool FreeOnDestruct() const
            {return (NULL != buffer_allocated);}
        
    protected:
        UINT32*         buffer_ptr;
        UINT32*         buffer_allocated;
        unsigned int    buffer_bytes;
        unsigned int    pkt_length;
        
};  // end class ProtoPkt

#endif // _PROTO_PKT
