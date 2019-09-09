#ifndef _PROTO_PKT_RTP
#define _PROTO_PKT_RTP

#include "protoPkt.h"
#include "protoDebug.h"
/**
 * @class ProtoPktRTP
 *
 * @brief Useful for building/parsing Real-Time Protocol (RTP), RFC3550, messages.
 */ 

class ProtoPktRTP : public ProtoPkt
{
    public:
        ProtoPktRTP(UINT32*        bufferPtr = NULL, 
                    unsigned int   numBytes = 0,  
                    unsigned int   pktLength = 0,  // inits from buffer if non-zero
                    bool           freeOnDestruct = false); 
        virtual ~ProtoPktRTP();
        
        enum {VERSION = 2};
        
		static const UINT16 SEQUENCE_MAX;         // 16 bits, unsigned 
		static const unsigned int BASE_HDR_LEN;   // (12 bytes) base header size, as of RFC 3550
		static const unsigned int CSRC_COUNT_MAX; // (15)
		
        enum PayloadType
        {
            PCMU    =   0,
            CELP    =   1,
            G721    =   2,
            GSM     =   3,
			G723    =   4,  // patented, not in public domain?
            DVI48K  =   5,  //  8k sample rate
            DVI416K =   6,  // 16k sample rate
            LPC     =   7,
            PCMA    =   8,
            G722    =   9,
            L16S    =  10,  // stereo
            L16M    =  11,  // monaural       
			QCELP	=  12,
			CN		=  13,
			MPA		=  14,
			G728	=  15,

            // ... (more to add later)
			// note that dynamic payload types (96-127) are also allowed but must be defined by the application
			// see RFC 3551, section 3
			PT_MAX  = 128  // ensures that enough size is set aside for dynamic PT's
        };
        
        /**
         * @class Extension
         *
         * @brief Extension class for ProtoPkt
         *
         * (TBD) Perhaps not create a ProtoPktRTP::Extension class since an RTP packet
         *       can have only one extension anyway?  Although this might be useful
         *       to subclass from to create C++ wrappers for different extension types?
         */
        class Extension : public ProtoPkt
        {
            friend class ProtoPktRTP;
            
            public:
                Extension(UINT32*       bufferPtr = NULL, 
                          unsigned int  numBytes = 0, 
                          bool          initFromBuffer = true,
                          bool          freeOnDestruct = false);
                virtual ~Extension();
                
                // Use these to build an extension
                bool Init(UINT32*       bufferPtr      = NULL, 
                          unsigned int  bufferBytes    = 0,
                          bool          freeOnDestruct = false);
                void SetType(UINT16 type)
                    {reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_TYPE] = htons(type);}
                bool SetData(const char* dataPtr, unsigned int numBytes);
                UINT32* AccessData() 
                    {return (buffer_ptr + OFFSET_DATA);}
                void SetDataLength(UINT16 numBytes)
                {
                    ASSERT(0 == (numBytes & 0x03));
                    reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_LENGTH] = htons(numBytes >> 2);
                    SetLength(4 + numBytes);
                }
                // "Pack()" should be called for an extension that is set using ProtoPktRTP::AttachExtension()
                // (Call this after, the extension has been completely build)
                bool Pack()
                    {return ((NULL != rtp_pkt) ? rtp_pkt->PackExtension(*this) : true);}
                
                // Use these to parse an extension
                bool InitFromBuffer(UINT32* bufferPtr           = NULL, 
                                    unsigned int bufferBytes    = 0, 
                                    bool freeOnDestruct         = false);
                UINT16 GetType()
                    {return ntohs(reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_TYPE]);}
                unsigned int GetDataLength()
                    {return (ntohs(reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_LENGTH]) << 2);}
                const UINT32* GetData()
                    {return (buffer_ptr + OFFSET_DATA);}
                
                
            protected:
                void AttachRtpPacket(ProtoPktRTP* rtpPkt)
                    {rtp_pkt = rtpPkt;}
            
                enum 
                {
                    OFFSET_TYPE     = 0,                        // UINT16 offset
                    OFFSET_LENGTH   = OFFSET_TYPE+1,            // UINT16 offset
                    OFFSET_DATA     = ((OFFSET_LENGTH+1)*2 )/4  // UINT32 offset
                };
                    
                ProtoPktRTP*     rtp_pkt;
		};  // end class ProtoPktRTP::Extension
        
        // RTP packet parsing
        bool InitFromBuffer(unsigned int    pktLength,
                            UINT32*         bufferPtr       = NULL,
                            unsigned int    bufferBytes     = 0,
                            bool            freeOnDestruct  = false);

		UINT8 GetVersion() const
            {return (reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_VERSION] >> 6);}
        
        bool HasPadding() const
            { return FlagIsSet(PADDING);}
        
        bool HasExtension() const 
                {return FlagIsSet(EXTENSION);}
        
        UINT8  GetCsrcCount() const
            {return (reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_CSRC_COUNT] & 0x0f);}
        
        bool HasMarker() const
            {return FlagIsSet(MARKER);}
        
        PayloadType GetPayloadType() const
            {return (PayloadType)(reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_PAYLOAD_TYPE] & 0x7f);}

        UINT16 GetSequence() const
            {return (ntohs(reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_SEQUENCE]));}

        UINT32 GetTimestamp() const
            {return (ntohl(buffer_ptr[OFFSET_TIMESTAMP]));}

        UINT32 GetSsrc() const
            {return ntohl(buffer_ptr[OFFSET_SSRC]);}

		UINT32 GetCsrc(unsigned int index) const
            {return (index < GetCsrcCount()) ? ntohl(buffer_ptr[OFFSET_CSRC_LIST+index]) : 0;}

        bool GetExtension(Extension& extension) const;
        unsigned int GetExtensionLength() const  
		{
            return (HasExtension() ?
                    (4+ntohs(reinterpret_cast<UINT16*>(buffer_ptr)[((OFFSET_CSRC_LIST+GetCsrcCount()) << 1)+1] << 2)) :
                    0);
            //Extension extension;
            //return (GetExtension(extension) ? (4+extension.GetDataLength()) : 0);
		}
		
        unsigned int GetHeaderLength() const
            {return (BASE_HDR_LEN + (GetCsrcCount() << 2) + GetExtensionLength());}
        
        UINT8 GetPaddingLength() const  
            {return (HasPadding() ?
                        reinterpret_cast<UINT8*>(buffer_ptr)[pkt_length-1] : 0);}
        
		unsigned int GetPayloadLength() const  
            {return (pkt_length - GetPaddingLength() - BASE_HDR_LEN - 4*GetCsrcCount() - GetExtensionLength());}

        const UINT32* GetPayload() const 
            {return (buffer_ptr + GetCsrcCount() + (BASE_HDR_LEN >> 2) + (GetExtensionLength() >> 2));}

        
        // Message building 
        // if "bufferPtr == NULL", current "buffer_ptr" is used
        bool Init(UINT32* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);

		void SetVersion(UINT8 version = VERSION) 
        {
			reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_VERSION] &= 0x03f;  // preserve what is already there
			reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_VERSION] |= (version << 6);
        }

        void SetMarker()
            {SetFlag(MARKER);}
        
        void ClearMarker()
            {ClearFlag(MARKER);}
        
        void SetPayloadType(PayloadType payloadType)  // 7 bits at present
		{
			reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_PAYLOAD_TYPE] &= 0x80;  // preserves marker bit
			reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_PAYLOAD_TYPE] |= ((UINT8)payloadType & 0x7f);  
		}

        void SetSequence(UINT16 sequence)  // may want this class to maintain the sequence # rather than get it from outside later on
        {reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_SEQUENCE] = htons(sequence);}

        void SetTimestamp(UINT32 timestamp)
            {buffer_ptr[OFFSET_TIMESTAMP] = htonl(timestamp);}

        void SetSSRC(UINT32 srcId)  
            {buffer_ptr[OFFSET_SSRC] = htonl(srcId);}

        // Must append any/all CSRC's before attaching extension or payload
        bool AppendCsrc(UINT32 srcId);

        // When an extension is used, "InitExtension()" must be called _after_ 
        // all CCSRC have been appended, and _before_ payload is set
        bool AttachExtension(Extension& extension);
        bool PackExtension(Extension& extension);

        // Set payload before any needed padding - may get info from RFC 2833?
		void SetPayload(const char* dataPtr, unsigned int numBytes)
		{
            memcpy((char*)AccessPayload(), dataPtr, numBytes);
            pkt_length = GetHeaderLength() + numBytes;
		}
        UINT32* AccessPayload() 
            {return (buffer_ptr + (BASE_HDR_LEN >> 2) + GetCsrcCount() + (GetExtensionLength() >> 2));}
        void SetPayloadLength(unsigned int numBytes)
            {pkt_length = GetHeaderLength() + numBytes;}

        // Set padding, if needed, last!
        void SetPadding(UINT8 numBytes, char* paddingPtr = NULL);

    private:
        // Bit masks for various flags in the first two bytes of RTP header
        enum Flag 
        {
            EXTENSION = 0x1000,  // indicates extension is present
            PADDING   = 0x2000,  // indicates padding is present
            MARKER    = 0x0080
        };
        bool FlagIsSet(Flag flag) const
        {
            return (0 != ( (UINT16)flag & ntohs(reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_FLAGS])));
        }
        void SetFlag(Flag flag)
        {
            reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_FLAGS] |= htons((UINT16)flag);
        }
        void ClearFlag(Flag flag)
        {
            reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_FLAGS] &= htons(~(UINT16)flag);
        }
        void ClearAllFlags()
        {
            UINT16 clear = EXTENSION | PADDING | MARKER;
            reinterpret_cast<UINT16*>(buffer_ptr)[OFFSET_FLAGS] &= htons(~(UINT16)clear);
        }
        enum
        {
			OFFSET_VERSION      = 0,                        // UINT8 offset
            OFFSET_FLAGS        = OFFSET_VERSION,           // UINT16 offset (flags in first 2 bytes)
            OFFSET_CSRC_COUNT   = OFFSET_VERSION,           // UINT8 offset, mask 4 lsb's
            OFFSET_PAYLOAD_TYPE = OFFSET_VERSION+1,         // UINT8 offset, mask 7 lsb's  
            OFFSET_SEQUENCE     = (OFFSET_PAYLOAD_TYPE+1)/2,// UINT16 offset  
            OFFSET_TIMESTAMP    = ((OFFSET_SEQUENCE+1)*2)/4,// UINT32 offset
            OFFSET_SSRC         = OFFSET_TIMESTAMP + 1,     // UINT32 offset
            OFFSET_CSRC_LIST    = OFFSET_SSRC + 1           // UINT32 offset
        };
};  // end class ProtoPktRTP

#endif  // _PROTO_PKT_RTP

