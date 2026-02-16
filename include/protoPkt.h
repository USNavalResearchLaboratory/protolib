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
 * @brief This is a base class that maintains a uffer for "packet"
 * (or message) building and parsing.
 *
 * Generally, classes will be derived
 * from this base class to create classes for 
 * protocol-specific packet/message
 * building and parsing (For examples, see 
 * ProtoPktIP, ProtoPktRTP, etc)
 */

class ProtoPkt
{
    public:
        ProtoPkt(void* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);
        virtual ~ProtoPkt();
        
        bool AllocateBuffer(unsigned int numBytes)
        {
            unsigned int len = numBytes / sizeof(UINT32);
            len += (0 == (len % sizeof(UINT32))) ? 0 : 1;
            if (NULL != buffer_allocated) delete[] buffer_allocated;
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
            buffer_allocated =  freeOnDestruct ? (UINT32*)bufferPtr : NULL;
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
        
        unsigned int GetBufferLength() const 
            {return buffer_bytes;}
        
        void SetLength(unsigned int bytes) 
            {pkt_length = bytes;}   
        unsigned int GetLength() const 
            {return pkt_length;} 
        
        // These methods get/set fields by byte offsets, using 
        // alignment check and different access methods to 
        // guarantee safety regardless of alignment.
        
        const void* GetBuffer() const 
            {return (void*)buffer_ptr;} 
        const void* GetBuffer(unsigned int byteOffset) const
            {return (char*)buffer_ptr + byteOffset;}
        void* AccessBuffer() 
            {return (void*)buffer_ptr;}  
        void* AccessBuffer(unsigned int offset)
            {return (char*)buffer_ptr + offset;}
        
        // These methods get/set fields by _byte_ offsets
        UINT8 GetUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        UINT8 GetUINT8Bits(unsigned int byteOffset, unsigned int bitOffset, unsigned int bitLength = 8) const;
        UINT8& AccessUINT8(unsigned int byteOffset) const
            {return ((UINT8*)buffer_ptr)[byteOffset];}
        void SetUINT8(unsigned int byteOffset, UINT8 value)
            {((UINT8*)buffer_ptr)[byteOffset] = value;}
        void SetUINT8Bits(unsigned int byteOffset, unsigned int bitOffset, UINT8 value, unsigned int bitLength = 8);
        
        UINT16 GetUINT16(unsigned int byteOffset) const
            {return GetUINT16(GetBuffer(byteOffset));}
        UINT16 GetUINT16Bits(unsigned int byteOffset, unsigned int bitOffset, unsigned int bitLength = 16) const;
        void SetUINT16(unsigned int byteOffset, UINT16 value)
            {SetUINT16(AccessBuffer(byteOffset), value);}
        void SetUINT16Bits(unsigned int byteOffset, unsigned int bitsOffset, UINT16 value, unsigned int bitLength = 16);
        
        UINT32 GetUINT32(unsigned int byteOffset) const
            {return GetUINT32(GetBuffer(byteOffset));}
        UINT32 GetUINT32Bits(unsigned int byteOffset, unsigned int bitOffset, unsigned int bitLength = 32) const;
        void SetUINT32(unsigned int byteOffset, UINT32 value)
            {SetUINT32(AccessBuffer(byteOffset), value);}
        void SetUINT32Bits(unsigned int byteOffset, unsigned int bitOffset, UINT32 value, unsigned int bitLength = 32);
        
        // These methods get/set fields by _word_ offsets
        UINT16 GetWord16(unsigned int wordOffset) const
            {return GetUINT16((UINT16*)buffer_ptr + wordOffset);}
        void SetWord16(unsigned int wordOffset, UINT16 value) 
            {SetUINT16((UINT16*)buffer_ptr + wordOffset, value);}
        UINT32 GetWord32(unsigned int wordOffset) const
            {return GetUINT32((UINT32*)buffer_ptr + wordOffset);}
        void SetWord32(unsigned int wordOffset, UINT32 value) 
            {SetUINT32((UINT32*)buffer_ptr + wordOffset, value);}
       
        const void* GetBuffer16(unsigned int wordOffset) const
            {return (void*)((const UINT16*)buffer_ptr + wordOffset);}
        void* AccessBuffer16(unsigned int wordOffset)
            {return (void*)((const UINT16*)buffer_ptr + wordOffset);}         
            
        const void* GetBuffer32(unsigned int wordOffset) const
            {return (void*)((const UINT32*)buffer_ptr + wordOffset);}
        void* AccessBuffer32(unsigned int wordOffset)
            {return (void*)((const UINT32*)buffer_ptr + wordOffset);}
                
        // Pointer alignment checks to determine safe access strategy
        static inline bool IsAligned16(const void* pointer)
            {return (0 == ((uintptr_t)pointer & 1));}
        
        static inline bool IsAligned32(const void* pointer)
            {return (0 == ((uintptr_t)pointer & 3));}
        
        // These methods get/set fields by pointer directly
        // TBD - benchmark this code that does its own
        // alignment checking versus just calling memcpy() always.
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
        // Note if externally allocated, these pointers may
        // _not_ be 32-bit aligned, but access routines above
        // safely allow for this.
        UINT32*         buffer_ptr;
        UINT32*         buffer_allocated;
        unsigned int    buffer_bytes;
        unsigned int    pkt_length;
        
};  // end class ProtoPkt

#endif // _PROTO_PKT
