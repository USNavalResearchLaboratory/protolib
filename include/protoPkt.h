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
 * from this base class to create classes for 
 * protocol-specific packet/message
 * building and parsing (For examples, see 
 * ProtoPktIP, ProtoPktRTP, etc)
 */
 
// TBD - remove all final vestiges of any specific pointer types,
//       using void* instead.  Make member variables private and
//       force use of methods. Note this will ripple down to 
//       existing ProtoPkt subclasses ...

class ProtoPkt
{
    public:
        ProtoPkt(void* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);
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
        void AttachBuffer(void* bufferPtr, unsigned int numBytes, bool freeOnDestruct = false)
        {
            buffer_ptr = (0 != numBytes) ? (UINT32*)bufferPtr : NULL;
            buffer_bytes = (NULL != bufferPtr) ? numBytes : 0;
            pkt_length = 0;
            if (NULL != buffer_allocated) delete[] buffer_allocated;
            if (freeOnDestruct) buffer_allocated = (UINT32*)bufferPtr;
        }
        UINT32* DetachBuffer()
        {
            UINT32* theBuffer = buffer_ptr;
            buffer_allocated = buffer_ptr = NULL; 
            pkt_length = buffer_bytes = 0;
            return theBuffer;  
        }
        bool InitFromBuffer(unsigned int    packetLength,
                            void*           bufferPtr = NULL,
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
        
        // These methods get/set fields by byte offsets, using 
        // alignment check and different access methods to 
        // guarantee safety regardless of alignment.
        
        const char* GetBuffer() const 
            {return (char*)buffer_ptr;} 
        const char* GetBuffer(unsigned int byteOffset) const
            {return GetBuffer() + byteOffset;}
        char* AccessBuffer() 
            {return (char*)buffer_ptr;}  
        char* AccessBuffer(unsigned int offset)
            {return (((char*)buffer_ptr) + offset);}
        
        // These methods get/set fields by byte offsets
        UINT8 GetUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        UINT8& AccessUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        void SetUINT8(unsigned int byteOffset, UINT8 value)
            {((UINT8*)buffer_ptr)[byteOffset] = value;}  
        
        UINT16 GetUINT16(unsigned int byteOffset) const
            {return GetUINT16(GetBuffer(byteOffset));}
        void SetUINT16(unsigned int byteOffset, UINT16 value)
            {SetUINT16(AccessBuffer(byteOffset), value);}
        UINT32 GetUINT32(unsigned int byteOffset) const
            {return GetUINT32(GetBuffer(byteOffset));}
        void SetUINT32(unsigned int byteOffset, UINT32 value)
            {SetUINT32(AccessBuffer(byteOffset), value);}
        
        // These methods get/set fields by aligned word offsets
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
        
        // Note the pointers returned by these are only properly
        // aligned pointers if the ProtoPkt was initialized with
        // a properly aligned pointer
        // TBD - make these return void* to be more explicit???
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
            
        // Pointer alignment checks to verify safe access
        static inline bool IsAligned16(const void* pointer)
            {return (0 == ((uintptr_t)pointer & 1));}
        
        static inline bool IsAligned32(const void* pointer)
            {return (0 == ((uintptr_t)pointer & 3));}
        
        // These methods get/set fields by pointer directly
        static inline UINT16 GetUINT16(const void* ptr) 
        {
            if (IsAligned16(ptr))
            {   
                return ntohs(*((const UINT16*)ptr));
            }
            else
            {
                UINT16 value;
                memcpy(&value, ptr, 2);
                return ntohs(value);
            }
        }
        static inline void SetUINT16(void* ptr, UINT16 value) 
        {
            if (IsAligned16(ptr))
            {   
                *((UINT16*)ptr) = htons(value);
            }
            else
            {
                value = htons(value);
                memcpy(ptr, &value, 2);
            }
        }
        static inline UINT32 GetUINT32(const void* ptr)
        {
            if (IsAligned32(ptr))
            {   
                return ntohl(*((const UINT32*)ptr));
            }
            else
            {
                UINT32 value;
                memcpy(&value, ptr, 4);
                return ntohl(value);
            }
        }
        static inline void SetUINT32(void* ptr, UINT32 value)
        {
            if (IsAligned32(ptr))
            {   
                *((UINT32*)ptr) = htonl(value);
            }
            else
            {
                value = htonl(value);
                memcpy(ptr, &value, 4);
            }
        }
        
        bool FreeOnDestruct() const
            {return (NULL != buffer_allocated);}
        
    protected:
        // TBD - make these "void*" instead of UINT32*
        UINT32*         buffer_ptr;
        UINT32*         buffer_allocated;
        unsigned int    buffer_bytes;
        unsigned int    pkt_length;
        
};  // end class ProtoPkt

#endif // _PROTO_PKT
