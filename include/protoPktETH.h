#ifndef _PROTO_PKT_ETH
#define _PROTO_PKT_ETH

#include "protoPkt.h"
#include "protoAddress.h"

/**
 * @class ProtoPktETH
 *
 * @brief This is a class that provides access to and control of ETHernet
 * header fields for the associated buffer space. The ProtoPkt "buffer"
 * is presumed to contain a valid ETHernet frame.
 * 
 * NOTE: Since ETHernet headers are 14 bytes and often encapsulate
 *       IP packets (for most Protolib purposes), this creates a
 *       problem with respect to 32-bit alignment of the buffer
 *       space if a ProtoPktIP is subsequently instantiated from the same 
 *       buffer space.  My recommendation here is to actually cheat and 
 *       offset into the buffer space by 2 bytes for the ProtoPktETH 
 *       instance so that the ProtoPktIP ends up being properly aligned.
 *       (You can get away with this since accesses to the buffer
 *       for ProtoPktETH  purposes are 16-bit aligned worst case
 *       anyway.  On some systems this will produce a compiler warning
 *       when you the following, but it should work OK:
 *
 *            UINT32 buffer[MAX_LEN/4];
 *            ProtoPktETHer  etherFrame((UINT32*)(((UINT16*)buffer)+1), MAX_LEN-2);
 *
 *            UINT16 numBytes = ReadETHernetFrame(((char*)buffer)+2, MAX_LEN-2);
 *            etherFrame.SetLength(numBytes);  // "numBytes" include ETHernet header
 *
 *            ASSERT(ProtoPktETHer::IP == etherFrame.GetPayloadType());
 *            ProtoPktIp ipPkt(buffer+4, MAX_LEN-16);  // Notice IP content is 32-bit aligned
 *            ipPkt.SetLength(etherFrame.GetPayloadLength());
 *
 *            (The resultant "ipPkt" instance can be safely manipulated with 32-bit alignment)
 */
         
// Ethernet Packet Format:
//        
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     destination MAC address                   |
// +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               |                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               + 
// |                        source MAC address                     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           Ethertype           |                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
// |                            data ...                           |
// +                                                               +
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         CRC checksum                          |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   
      

class ProtoPktETH : public ProtoPkt
{
    public:
        ProtoPktETH(UINT32*         bufferPtr = NULL, 
                    unsigned int    numBytes = 0,
                    bool            freeOnDestruct = false); 
        ~ProtoPktETH();
        
        enum Type
        {
            IP   = 0x0800,
            ARP  = 0x0806,
            IPv6 = 0x86dd    
        };
            
        enum 
        {
            ADDR_LEN = 6,
            HDR_LEN  = 2*ADDR_LEN + 2
        };  
        
        bool InitFromBuffer(unsigned int    frameLength,
                            UINT32*         bufferPtr = NULL,
                            unsigned int    numBytes = 0,
                            bool            freeOnDestruct = false)
            {return ProtoPkt::InitFromBuffer(frameLength, bufferPtr, numBytes, freeOnDestruct);}
        
        void GetSrcAddr(ProtoAddress& addr)
            {addr.SetRawHostAddress(ProtoAddress::ETH, ((char*)buffer_ptr)+OFFSET_SRC, ADDR_LEN);}
        void GetDstAddr(ProtoAddress& addr)
            {addr.SetRawHostAddress(ProtoAddress::ETH, ((char*)buffer_ptr)+OFFSET_DST, ADDR_LEN);}
        Type GetType()
            {return((Type)ntohs(*(((UINT16*)buffer_ptr)+OFFSET_TYPE)));}
        unsigned int GetPayloadLength() {return (GetLength() - HDR_LEN);}
        const char* GetPayload() {return (((const char*)buffer_ptr)+OFFSET_PAYLOAD);}
        char* AccessPayload() {return (((char*)buffer_ptr)+OFFSET_PAYLOAD);}
        
        bool InitIntoBuffer(UINT32*         bufferPtr = NULL, 
                            unsigned int    bufferBytes = 0, 
                            bool            freeOnDestruct = false);
        void SetSrcAddr(ProtoAddress srcAddr)
            {memcpy(((char*)buffer_ptr)+OFFSET_SRC, srcAddr.GetRawHostAddress(), ADDR_LEN);}
        void SetDstAddr(ProtoAddress dstAddr)
            {memcpy(((char*)buffer_ptr)+OFFSET_DST, dstAddr.GetRawHostAddress(), ADDR_LEN);}  
        void SetType(Type type)
            {*(((UINT16*)buffer_ptr)+OFFSET_TYPE) = htons((UINT16)type);}
        void SetPayload(const char* payload, unsigned int numBytes)
        {
            memcpy(((char*)buffer_ptr)+OFFSET_PAYLOAD, payload, numBytes);
            SetPayloadLength(numBytes);
        }
        void SetPayloadLength(unsigned int numBytes)
            {SetLength(numBytes + HDR_LEN);}
    
    private:
        enum
        {
            OFFSET_DST     = 0,                         //  6 bytes, zero offset
            OFFSET_SRC     = OFFSET_DST + ADDR_LEN,     //  6 bytes, UINT8 offset
            OFFSET_TYPE    = (OFFSET_SRC + ADDR_LEN)/2, //  2 bytes, UINT16 offset
            OFFSET_PAYLOAD = 2*(OFFSET_TYPE+1)          //  UINT8 offset
        };
};  // end class ProtoPktETH


#endif // _PROTO_PKT_ETH
