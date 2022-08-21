#ifndef _PROTO_PKT_TCP
#define _PROTO_PKT_TCP

#include "protoPktIP.h"

/**
 * @class ProtoPktTCP
 *
 * @brief Parses TCP Packets
 */
class ProtoPktTCP : public ProtoPkt
{
    public:
        ProtoPktTCP(void*          bufferPtr = 0, 
                    unsigned int   numBytes = 0, 
                    bool           initFromBuffer = true,
                    bool           freeOnDestruct = false);
        ~ProtoPktTCP();
        
        enum Flag
        {
            FLAG_FIN = 0x0001,  // finished, no more data
            FLAG_SYN = 0x0002,  // sync sequence numbers (first packet)
            FLAG_RST = 0x0004,  // connection reset
            FLAG_PSH = 0x0008,  // push data to receiving app
            FLAG_ACK = 0x0010,  // ack field is valid
            FLAG_URG = 0x0020,  // urgent field is valid
            FLAG_ECE = 0x0040,  // if SYN, ECN capable, else congestion indication
            FLAG_CWR = 0x0080,  // congestion window reduced
            FLAG_NS  = 0x0100   // ECN-nonce concealment protection
        };
        
        // Use these to parse the datagram
        bool InitFromBuffer(void*   bufferPtr       = NULL, 
                            unsigned int numBytes   = 0, 
                            bool freeOnDestruct     = false);
        bool InitFromPacket(ProtoPktIP& pkt);
        UINT16 GetSrcPort() const
            {return GetWord16(OFFSET_SRC);}
        UINT16 GetDstPort() const
            {return GetWord16(OFFSET_DST);}
        UINT32 GetSequence() const
            {return GetWord32(OFFSET_SEQ);}
        UINT32 GetAckNumber() const
            {return GetWord32(OFFSET_ACK);}
        UINT16 GetFlags() const
            {return (0x01ff & GetWord16(OFFSET_FLAGS));}
        bool FlagIsSet(Flag flag) const
            {return (0 != (flag & GetFlags()));}
        UINT16 GetWindowSize() const
            {return GetWord16(OFFSET_WINDOW);}
        UINT16 GetChecksum() const
            {return GetWord16(OFFSET_CHECKSUM);}
        UINT16 GetUrgentPointer() const
            {return GetWord16(OFFSET_URGENT);}
        bool HasOptions() const
            {return (OffsetPayload() > 5);}
        const void* GetOptions() const
            {return GetBuffer32(OFFSET_OPTIONS);}
        UINT16 GetPayloadLength() const
            {return (GetLength() - (OffsetPayload() << 2));}
        const void* GetPayload() const
            {return GetBuffer32(OffsetPayload());}
        void* AccessPayload()
            {return AccessBuffer32(OffsetPayload());}
        UINT16 ComputeChecksum(ProtoPktIP& ipPkt) const;
        bool ChecksumIsValid(ProtoPktIP& ipPkt) const
            {return (GetChecksum() == ComputeChecksum(ipPkt));}
        
        // Use these to build the datagram
        bool InitIntoBuffer(void*          bufferPtr = 0, 
                            unsigned int   numBytes = 0, 
                            bool           freeOnDestruct = false);
        void SetSrcPort(UINT16 port)
            {SetWord16(OFFSET_SRC, port);}
        void SetDstPort(UINT16 port)
            {SetWord16(OFFSET_DST, port);}
        void SetSequence(UINT32 seq)
            {SetWord32(OFFSET_SEQ, seq);}
        void SetAckNumber(UINT32 ackNumber)
        {
            SetFlag(FLAG_ACK);
            SetWord32(OFFSET_ACK, ackNumber);
        }
        void SetFlags(UINT16 flags)
        {
            UINT16 field = 0xfe00 & GetWord16(OFFSET_FLAGS);
            SetWord16(OFFSET_FLAGS, field | flags);
        }
        void ClearFlags()
        {
            UINT16 field = 0xfe00 & GetWord16(OFFSET_FLAGS);
            SetWord16(OFFSET_FLAGS, field);
        } 
        void SetFlag(Flag flag)
        {
            UINT16 field = GetWord16(OFFSET_FLAGS);
            SetWord16(OFFSET_FLAGS, field | (UINT16)flag);
        }       
        void ClearFlag(Flag flag)
        {
            UINT16 field = GetWord16(OFFSET_FLAGS);
            SetWord16(OFFSET_FLAGS, field & ~(UINT16)flag);
        }
        void SetWindowSize(UINT16 windowSize)
            {SetWord16(OFFSET_WINDOW, windowSize);}
        void SetChecksum(UINT16 checksum)
            {SetWord16(OFFSET_CHECKSUM, checksum);}
        void SetUrgentPointer(UINT16 value)
        {
            SetFlag(FLAG_URG);
            SetWord16(OFFSET_URGENT, value);
        }
        void SetPayload(const char* payload, UINT16 numBytes)
        {
            memcpy((char*)AccessBuffer32(OffsetPayload()), payload, numBytes);
            ProtoPkt::SetLength(numBytes + (OffsetPayload() << 2));
        }    
        // This must be called after payload is set
        void FinalizeChecksum(ProtoPktIP& ipPkt)
            {SetChecksum(ComputeChecksum(ipPkt));}
        
    private:
        enum
        {
            OFFSET_SRC      = 0,                  // source port number (UINT16 offset)
            OFFSET_DST      = OFFSET_SRC + 1,     // destination port number (UINT16 offset)
            OFFSET_SEQ      = (OFFSET_DST + 1)/2, // sequence number (UINT32 offset)
            OFFSET_ACK      = OFFSET_SEQ + 1,     // acknowledgment number (UINT32 offset)      
            OFFSET_DATA     = (OFFSET_ACK+1)*4,   // data offset (UINT8, upper 4 bits)
            OFFSET_FLAGS    = (OFFSET_ACK+1)*2,   // flags (UINT16, lower 9 bits)
            OFFSET_WINDOW   = OFFSET_FLAGS + 1,   // window size (UINT16 offset)
            OFFSET_CHECKSUM = OFFSET_WINDOW + 1,  // checksum (UINT16 offset)
            OFFSET_URGENT   = OFFSET_CHECKSUM +1, // urgent pointer (UINT16 offset)
            OFFSET_OPTIONS  = (OFFSET_URGENT+1)/2 // options (UINT32 offset)
        };
        
        unsigned int OffsetPayload() const  // UINT32 offset value
            {return ((GetUINT8(OFFSET_DATA) >> 4) & 0x0f);}
        void SetDataOffset(UINT8 offset)  // as a UINT32 offset value
        {
            // Replace upper 4 bits of the data offset/reserved/ns byte
            UINT8 field = GetUINT8(OFFSET_DATA) & 0x0f;
            SetUINT8(OFFSET_DATA, ((offset & 0x0f) << 4) | field);
       }
};  // end class ProtoPktTCP

#endif // _PROTO_PKT_TCP
