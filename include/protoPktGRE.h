#ifndef _PROTO_PKT_GRE
#define _PROTO_PKT_GRE

#include "protoPktETH.h"  // for ProtoPktETH::Type

class ProtoPktGRE : public ProtoPkt
{
    public:
        ProtoPktGRE(void*          bufferPtr = NULL,
                    unsigned int   numBytes = 0, 
                    bool           initFromBuffer = false,
                    bool           freeOnDestruct = false);
        ~ProtoPktGRE();
        
        enum Flag
        {
            FLAG_CHECKSUM   = 0x80,
            FLAG_ROUTING    = 0x40,
            FLAG_KEY        = 0x20,
            FLAG_SEQUENCE   = 0x10,
            FLAG_STRICT_SRC = 0x08
        };  
            
        // GRE header parsing  
        bool InitFromBuffer(UINT16         pktLength,
                            void*          bufferPtr = NULL, 
                            unsigned int   bufferBytes = 0,
                            bool           freeOnDestruct = false)
        {
            bool result = ProtoPkt::InitFromBuffer(pktLength, bufferPtr, bufferBytes, freeOnDestruct);
            result &= (pktLength >= 2*OFFSET_PROTOCOL);
            return result;
        }

        bool FlagIsSet(Flag flag) const
            {return (0 != (flag & GetUINT8(OFFSET_FLAGS)));}
        UINT8 GetRecursionControl() const
            {return (GetUINT8(OFFSET_RECURL) & 0x07);}
        UINT8 GetVersion() const
            {return (GetUINT8(OFFSET_VERSION) & 0x07);}
        ProtoPktETH::Type GetProtocol() const
            {return static_cast<ProtoPktETH::Type>(GetUINT16(OFFSET_PROTOCOL));}
        UINT16 GetChecksum() const
            {return FlagIsSet(FLAG_CHECKSUM) ? GetUINT16(OFFSET_CHECKSUM) : 0;}
        UINT32 GetKey() const
            {return FlagIsSet(FLAG_KEY) ? GetUINT32(OffsetKey()) : 0;}
        UINT32 GetSequence() const
            {return FlagIsSet(FLAG_SEQUENCE) ? GetUINT32(OffsetSequence()) : 0;}
        UINT32 GetRouting() const
            {return FlagIsSet(FLAG_ROUTING) ? GetUINT32(OffsetRouting()) : 0;}
        unsigned int GetHeaderLength() const
            {return OffsetPayload();}
        unsigned int GetPayloadLength() const
            {return (GetLength() - GetHeaderLength());}
        const void* GetPayload() const 
            {return ((void*)GetBuffer32(OffsetPayload()));}
        void* AccessPayload() 
            {return ((void*)AccessBuffer32(OffsetPayload()));}
        
        // GRE header building (must call in order to build properly)
        bool InitIntoBuffer(void*        bufferPtr = NULL, 
                            unsigned int numBytes = 0, 
                            bool         freeOnDestruct = false);
        void SetFlag(Flag flag)
            {AccessUINT8(OFFSET_FLAGS) |= flag;}
        void ClearFlag(Flag flag)
            {AccessUINT8(OFFSET_FLAGS) &= ~flag;}
        void SetRecursionControl(UINT8 value)
        {
            UINT8& byte = AccessUINT8(OFFSET_RECURL);
            byte |= (value & 0x07);
        }
        void SetVersion(UINT8 version)
        {
            UINT8& byte = AccessUINT8(OFFSET_VERSION);
            byte |= (version & 0x07);
        }
        void SetProtocol(ProtoPktETH::Type protocol)
            {SetUINT16(OFFSET_PROTOCOL, protocol);}
        void SetChecksum(UINT16 value)
        {
            SetUINT16(OFFSET_CHECKSUM, value);
            SetUINT16(OFFSET_RESERVED, 0);
            SetFlag(FLAG_CHECKSUM);
            ProtoPkt::SetLength(8);
        }
        void SetKey(UINT32 value)
        {
            SetUINT32(OffsetKey(), value);
            SetFlag(FLAG_KEY);
            ProtoPkt::SetLength(4*OffsetKey()+4);
        }
        void SetSequence(UINT32 value)
        {
            SetUINT32(OffsetSequence(), value);
            SetFlag(FLAG_SEQUENCE);
            ProtoPkt::SetLength(4*OffsetSequence()+4);
        }
        void SetRouting(UINT32 value)
        {
            SetUINT32(OffsetRouting(), value);
            SetFlag(FLAG_SEQUENCE);
            ProtoPkt::SetLength(4*OffsetRouting()+4);
        }
        void SetPayloadLength(UINT16 numBytes, bool calculateChecksum = true);
        void SetPayload(const char* payload, UINT16 numBytes, bool calculateChecksum = true)
        {
            memcpy(AccessPayload(), payload, numBytes);
            SetPayloadLength(numBytes, calculateChecksum);   
        }
        
        // Checksum helpers
        
        // Calculates checksum over header and payload
        // (also sets the GRE header checksum field when 'set' is true
        UINT16 CalculateChecksum(bool set = true);
        
        // Updates GRE header checksum when an aligned
        //  (16-bit portion is changed (e.g. content portion)
		void UpdateChecksum(UINT16 oldVal, UINT16 newVal)
        {
            UINT16 oldSum = GetChecksum();
            UINT32 sum = oldSum + oldVal + (~newVal & 0xffff);
            UINT16 newSum = (UINT16)(sum + (sum >> 16));
            SetChecksum(newSum);
        }
        
        // updates GRE header checksum when a byte is changed.
        void UpdateChecksum(UINT8 oldByte, UINT8 newByte, bool oddOffset)
        {
            UINT16 oldVal = (UINT16)oldByte;
            UINT16 newVal = (UINT16)newByte;
            if (!oddOffset)
            {
                oldVal <<= 8;
                newVal <<= 8;
            }
            UINT16 oldSum = GetChecksum();
            UINT32 sum = oldSum + oldVal + (~newVal & 0xffff);
            UINT16 newSum = (UINT16)(sum + (sum >> 16));
            SetChecksum(newSum);
        }
        
    private:
        enum
        {
            OFFSET_FLAGS    = 0,                   // (UINT8 : msb 0-4)
            OFFSET_RECURL   = OFFSET_FLAGS,        // (UINT8 : msb 5-7)
            // note reserved flags between RECURL and VERSION
            OFFSET_VERSION  = OFFSET_FLAGS + 1,    // (UINT8 : msb 5-7)
            OFFSET_PROTOCOL = OFFSET_FLAGS/2 + 1,  // (UINT16)
            OFFSET_CHECKSUM = OFFSET_PROTOCOL + 1, // (UINT16 - optional)
            OFFSET_RESERVED = OFFSET_CHECKSUM + 1  // (UINT16 - optional)
            // note optional 2 bytes reserved when checksum is present
        };
        unsigned int OffsetKey() const
        {
            unsigned int offset = OFFSET_CHECKSUM/2 + 1;
            offset += FlagIsSet(FLAG_CHECKSUM) ? 1 : 0;
            return offset;
        }
        unsigned int OffsetSequence() const
        {
            unsigned int offset = OffsetKey();
            offset += FlagIsSet(FLAG_KEY) ? 1 : 0;
            return offset;
        }
        unsigned int OffsetRouting() const
        {
            unsigned int offset = OffsetSequence();
            offset += FlagIsSet(FLAG_SEQUENCE) ? 1 : 0;
            return offset;
        }
        unsigned int OffsetPayload() const
        {
            unsigned offset = OffsetRouting();
            offset += FlagIsSet(FLAG_ROUTING) ? 1 : 0;
            return (4*offset);
        }
};  // end class ProtoPktGRE


#endif // _PROTO_PKT_GRE
