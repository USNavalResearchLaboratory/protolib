#ifndef _PROTO_PKT_IP
#define _PROTO_PKT_IP

#include "protoPkt.h"
#include "protoAddress.h"
#include "protoDebug.h"

/**
 * @class ProtoPktIP
 *
 * @brief  These classes provide representations of IP packet formats
 * for IPv4 and IPv6 packets:
 * 
 *    ProtoPktIP   - base class with access to/control of "version" field
 *    ProtoPktIPv4 - class for parsing/building IPv4 packets into buffer
 *    ProtoPktIPv6 - class for parsing/building IPv6 packets into buffer
 *    ProtoPktUDP  - class for parsing/building UDP datagrams
 *                   (hey, we needed to test ProtoPktIP with something)
 */
class ProtoPktIP : public ProtoPkt
{
    public:
        ProtoPktIP(UINT32*        bufferPtr = NULL, 
                   unsigned int   bufferBytes = 0, 
                   bool           freeOnDestruct = false);
        ~ProtoPktIP();
        
        
        bool InitFromBuffer(UINT16         pktLength,
                            UINT32*        bufferPtr = NULL, 
                            unsigned int   bufferBytes = 0,
                            bool           freeOnDestruct = false)
        {
            bool result = ProtoPkt::InitFromBuffer(pktLength, bufferPtr, bufferBytes, freeOnDestruct);
            result &= (pktLength > OFFSET_VERSION);
            return result;
        }
        
        UINT8 GetVersion() const
            {return ((pkt_length > OFFSET_VERSION) ? 
                        (((UINT8*)buffer_ptr)[OFFSET_VERSION] >> 4) : 0);}
        
        void SetVersion(UINT8 version)
        {
            ((UINT8*)buffer_ptr)[OFFSET_VERSION] &= 0x0f;
            ((UINT8*)buffer_ptr)[OFFSET_VERSION] |= (version << 4);
        }
        
        bool SetDstAddr(ProtoAddress& dst); 
        bool GetDstAddr(ProtoAddress& dst);
        bool SetSrcAddr(ProtoAddress& src); 
        bool GetSrcAddr(ProtoAddress& src);
        
        // This is a partial list at the moment
        enum Protocol
        {
            HOPOPT   =   0,  // IPv6 hop-by-hop option                    
            ICMP     =   1,  // Internet Control Message Protocol         
            IGMP     =   2,  // Internet Group Management Protocol             
            IPIP     =   4,  // IPv4 in IPv4 encapsulation                    
            TCP      =   6,  // Transmission Control Protocol             
            UDP      =  17,  // User Datagram Protocol  
            IPV6     =  41,  // Used for tunneling IPv6 packets over IPv4 or IPv6
            RTG      =  43,  // IPv6 routing header
            FRAG     =  44,  // IPv6 fragment header
            GRE      =  47,  // Generic Router Encapsulation
            ESP      =  50,  // Encapsulation security payload header
            AUTH     =  51,  // authentication/ESP header  
            MOBILE   =  55,  // IP Moobility (Min Encap)                
            ICMPv6   =  58,  // ICMP for IPv6 
            MLD      =  58,  // IPv6 Multicast Listener Discovery  
            NONE     =  59,  // IPPROTO_NONE
            DSTOPT   =  60,  // IPv6 destination options header 
            OSPF     =  89,  // OSPF routing protocol   
            MOBILITY = 135,  // IPv6 mobility extension header                        
            EXP1     = 253,  // for experimental use
            EXP2     = 254,  // for experimental use     
            RESERVED = 255        
        };
        
        static bool IsExtension(Protocol p)
        {
            switch (p)
            {
                case HOPOPT:  // hop-by-hop options
                case DSTOPT:  // destination options
                case RTG:     // routing header
                case FRAG:    // fragment header
                case AUTH:    // IPSec AH header
                    return true;
                default:
                    return false;
            }
        }
        
        /**
         * @class OptionBase
         *
         * @brief Base class for IPv4 and IPv6 Option subclasses
         */
        class OptionBase
        {
            public:
                ~OptionBase();
                
                void AttachBuffer(char*        bufferPtr = NULL, 
                                  unsigned int numBytes = 0, 
                                  bool         freeOnDestruct = false)
                {
                    if (NULL != buffer_allocated) delete[] buffer_allocated;
                    buffer_ptr = bufferPtr;
                    buffer_allocated = freeOnDestruct ? bufferPtr : NULL;
                    buffer_bytes = numBytes;
                }
                char* DetachBuffer()
                {
                    char* theBuffer = buffer_ptr;
                    buffer_ptr = buffer_allocated = NULL;
                    buffer_bytes = 0;
                    return theBuffer;   
                }                
                
                const char* GetBuffer() const 
                    {return buffer_ptr;}
                char* AccessBuffer() const 
                    {return buffer_ptr;}
                unsigned int GetBufferLength() const 
                    {return buffer_bytes;}
                
            protected:
                OptionBase(char*        bufferPtr = NULL, 
                           unsigned int numBytes = 0, 
                           bool         freeOnDestruct = false);
            
                char*           buffer_ptr;
                char*           buffer_allocated;
                unsigned int    buffer_bytes;
                    
        };  // end class ProtoPkt::OptionBase
        
    protected:
        enum {OFFSET_VERSION = 0};   // 1/2 byte, most sig nybble
    
};  // end class ProtoPktIP

/**
 * @class ProtoPktIPv4
 *
 * @brief Parses IPv4 Packets
 */

class ProtoPktIPv4 : public ProtoPktIP
{
    public:
        ProtoPktIPv4(UINT32*        bufferPtr = NULL, 
                     unsigned int   numBytes = 0, 
                     bool           initFromBuffer = false,
                     bool           freeOnDestruct = false); 
        ProtoPktIPv4(ProtoPktIP& ipPkt);
        ~ProtoPktIPv4();
        
        enum {ADDR_LEN = 4};
        
        enum Flag
        {
            FLAG_NONE     = 0x00,
            FLAG_RESERVED = 0x80,  // reserved bit
            FLAG_DF       = 0x40,  // 0 = may fragment,  1 = don't fragment 
            FLAG_MF       = 0x20   // 0 = last fragment, 1 = more fragments
        };  
            
        /**
         * @class Option
		 * @brief ProtoPktIPv4 Option Base class
         */
        class Option : public ProtoPktIP::OptionBase
        {
            public:
                enum Type
                {
                    EOOL   =      0, // End of Options List    [RFC791,JBP]                            
                    NOP    =      1, // No Operation           [RFC791,JBP]                            
                    SEC    =    130, // Security                  [RFC1108]                            
                    LSR    =    131, // Loose Source Route     [RFC791,JBP]                            
                    TS     =     68, // Time Stamp             [RFC791,JBP]                            
                    ESEC   =    133, // Extended Security         [RFC1108]                            
                    CIPSO  =    134, // Commercial Security           [???]                            
                    RR     =      7, // Record Route           [RFC791,JBP]                            
                    SID    =    136, // Stream ID              [RFC791,JBP]                            
                    SSR    =    137, // Strict Source Route    [RFC791,JBP]                            
                    ZSU    =     10, // Experimental Measurement      [ZSu]                            
                    MTUP   =     11, // MTU Probe                 [RFC1191]*                           
                    MTUR   =     12, // MTU Reply                 [RFC1191]*                           
                    FINN   =    205, // Experimental Flow Control    [Finn]                            
                    VISA   =    142, // Expermental Access Control [Estrin]                            
                    ENCODE =     15, // ???                      [VerSteeg]                            
                    IMITD  =    144, // IMI Traffic Descriptor        [Lee]                            
                    EIP    =    145, // Extended Internet Protocol[RFC1385]                            
                    TR     =     82, // Traceroute        [RFC1393]                                    
                    ADDEXT =    147, // Address Extension    [Ullmann IPv7]                            
                    RTRALT =    148, // Router Alert              [RFC2113]                            
                    SDB    =    149, // Selective Directed Broadcast[Graff]                            
                    XXX    =    150, // Unassigned (Released 18 October 2005)                          
                    DPS    =    151, // Dynamic Packet State        [Malis]                            
                    UMP    =    152, // Upstream Multicast Pkt. [Farinacci]                            
                    QS     =     25, // Quick//Start           [RFC4782]                               
                    EXP1   =     30, // RFC3692//style Experiment (**) [RFC4727]                       
                    EXP2   =     94, // RFC3692//style Experiment (**) [RFC4727]                       
                    EXP3   =    158, // RFC3692//style Experiment (**) [RFC4727]                       
                    EXP4   =    222  // RFC3692//style Experiment (**) [RFC4727]                       
                };    
                    
                Option(char*        bufferPtr = NULL, 
                       unsigned int numBytes = 0, 
                       bool         initFromBuffer = true, 
                       bool         freeOnDestruct = false);
                ~Option();
                
                // Use these to build an option
                bool InitIntoBuffer(Type         type,
                                    char*        bufferPtr = NULL, 
                                    unsigned int numBytes = 0, 
                                    bool         freeOnDestruct = false);
                
                bool SetData(const char* data, unsigned int length);
                
                
                // Use these to parse
                bool InitFromBuffer(char*        bufferPtr = NULL, 
                                    unsigned int numBytes = 0, 
                                    bool         freeOnDestruct = false);
                
                Type GetType() const
                    {return (Type)((UINT8)buffer_ptr[OFFSET_TYPE]);}
                
                unsigned int GetLength() const
                    {return opt_length;}
                
                const char* GetData() const
                    {return (buffer_ptr + OffsetData());}
                
                unsigned int GetDataLength() const
                    {return (opt_length - ((OffsetData() != OFFSET_LENGTH) ? 2 : 1));}
                
                // Per RFC4302 spec that cites related specs
                bool IsMutable() const
                    {return IsMutable(GetType());}
                
                static bool IsMutable(Type type);
                
                /**
                 *
                 * @class Iterator
				 * @brief Iterator class
                 */
                class Iterator
                {
                    public:
                        Iterator(const ProtoPktIPv4& ip4Pkt);
                        ~Iterator();
                        void Reset() {offset = 20;}
                        bool GetNextOption(Option& option);

                    private:
                        const char*  pkt_buffer;
                        unsigned int offset;
                        unsigned int offset_end;   
                };  // end class ProtoPktIPv4::Option::Iterator
                
            private:
                static int GetLengthByType(Type type);    
                    
                enum
                {
                    LENGTH_VARIABLE = 0,
                    LENGTH_UNKNOWN  = -1
                };
                enum
                {
                    OFFSET_TYPE    = 0,                // (UINT8 offset
                    OFFSET_LENGTH  = (OFFSET_TYPE + 1) // UINT8 offset      
                };
                     
                unsigned int opt_length; 
                
                unsigned int OffsetData() const
                    {return ((LENGTH_VARIABLE == GetLengthByType(GetType())) ? 2 : 1);}
        
        };  // end class ProtoPktIPv4::Option   
            
                
        /// Use these to parse a packet
        bool InitFromBuffer(UINT32* bufferPtr       = NULL, 
                            unsigned int numBytes   = 0, 
                            bool freeOnDestruct     = false);
        
        UINT8 GetHeaderLength() const  // in bytes
            {return ((((UINT8*)buffer_ptr)[OFFSET_HDR_LEN] & 0x0f) << 2); }
        UINT8 GetTOS() const
            {return (((UINT8*)buffer_ptr)[OFFSET_TOS]);}
        UINT16 GetTotalLength() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_LEN]);}
        UINT16 GetID() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_ID]);}
        bool FlagIsSet(Flag flag) const
            {return (0 != (flag & (((UINT8*)buffer_ptr)[OFFSET_FLAGS])));}
        UINT16 GetFragmentOffset() const
            {return (0x1fff & ntohs(((UINT16*)buffer_ptr)[OFFSET_FRAGMENT]));}
        UINT8 GetTTL() const
            {return (((UINT8*)buffer_ptr)[OFFSET_TTL]);}
        Protocol GetProtocol() const
            {return (Protocol)(((UINT8*)buffer_ptr)[OFFSET_PROTOCOL]);}
        UINT16 GetChecksum() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_CHECKSUM]);} // (TBD) ??? is ntohs() needed ???   
        void GetSrcAddr(ProtoAddress& addr) const
            {addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)(buffer_ptr+OFFSET_SRC_ADDR), 4);}
        void GetDstAddr(ProtoAddress& addr) const
            {addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)(buffer_ptr+OFFSET_DST_ADDR), 4);}
        
        /// Helper method to get pointer to ID portion of IPv4 header
        const char* GetIDPtr() const
            {return ((char*)buffer_ptr + (OFFSET_ID*2));}
        /// Helper methods for UDP checksum calculation, etc
        const UINT32* GetSrcAddrPtr() const {return (buffer_ptr + OFFSET_SRC_ADDR);}
        const UINT32* GetDstAddrPtr() const {return (buffer_ptr + OFFSET_DST_ADDR);}
        
        // (TBD) provide methods to get any header extensions
        
        UINT16 GetPayloadLength() const {return (GetTotalLength() - GetHeaderLength());}
        const char* GetPayload() const 
            {return ((const char*)buffer_ptr + GetHeaderLength());}
        UINT32* AccessPayload() 
            {return (buffer_ptr + (((UINT8*)buffer_ptr)[OFFSET_HDR_LEN] & 0x0f));}
        
        /// Use these to build a packet
        bool InitIntoBuffer(UINT32*         bufferPtr = NULL, 
                            unsigned int    bufferBytes = 0, 
                            bool            freeOnDestruct = false);
        /// (TBD) modify "Set" methods to optionally update checksum
        void SetTOS(UINT8 tos, bool updateChecksum = false);
        void SetID(UINT16 id, bool updateChecksum = false) ;
        void SetFlag(Flag flag, bool updateChecksum = false);
        void ClearFlag(Flag flag, bool updateChecksum = false);
        void SetFragmentOffset(UINT16 fragmentOffset, bool updateChecksum = false);
        void SetTTL(UINT8 ttl, bool updateChecksum = false) ;
        void SetProtocol(Protocol protocol, bool updateChecksum = false);
        void SetChecksum(UINT16 checksum)
            {((UINT16*)buffer_ptr)[OFFSET_CHECKSUM] = htons(checksum);}  
        void SetSrcAddr(const ProtoAddress& addr, bool calculateChecksum = false);
        void SetDstAddr(const ProtoAddress& addr, bool calculateChecksum = false);
        /// (TBD) support header extensions for IPv4
        void SetPayloadLength(UINT16 numBytes, bool calculateChecksum = true);
        void SetPayload(const char* payload, UINT16 numBytes, bool calculateChecksum = true)
        {
            memcpy(AccessPayload(), payload, numBytes);
            SetPayloadLength(numBytes, calculateChecksum);   
        }
        
        /** updates IPv4 header checksum when an aligned
        * (16-bit portion is changed (e.g. packet id)
        */
		void UpdateChecksum(UINT16 oldVal, UINT16 newVal)
        {
            UINT16 oldSum = GetChecksum();
            UINT32 sum = oldSum + oldVal + (~newVal & 0xffff);
            UINT16 newSum = (UINT16)(sum + (sum >> 16));
            SetChecksum(newSum);
        }
        
        /// updates IPv4 header checksum when a byte is changed.
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
        /// Helper methods for  common packet manipulation
        UINT8 DecrementTTL()
        {
            UINT8 ttl = GetTTL();
            if (ttl > 0)
            {
                SetTTL(ttl-1);
                UINT16 oldSum = GetChecksum();
                UINT32 sum = oldSum + 0x0100;
                UINT16 newSum = (UINT16)(sum + (sum >> 16));
                SetChecksum(newSum);
            }
            return ttl;
        }
        
        // Return checksum in host byte order
        UINT16 CalculateChecksum(bool set = true);
        UINT16 FinalizeChecksum()
            {return CalculateChecksum(true);}
           
    private:
        void SetHeaderLength(UINT8 hdrBytes) 
        {  
            ((UINT8*)buffer_ptr)[OFFSET_HDR_LEN] &= 0xf0;
            ((UINT8*)buffer_ptr)[OFFSET_HDR_LEN] |= (hdrBytes >> 2);
            ProtoPkt::SetLength(hdrBytes);
        }
        void SetTotalLength(UINT16 numBytes) 
        {
            ((UINT16*)buffer_ptr)[OFFSET_LEN] = htons(numBytes);
            pkt_length = numBytes;
        }
        
        enum
        {
            OFFSET_HDR_LEN  = OFFSET_VERSION,            // 0.5 bytes (masked)            
            OFFSET_TOS      = OFFSET_HDR_LEN + 1,        // 1 bytes                     
            OFFSET_LEN      = (OFFSET_TOS+1)/2,          // 1 UINT16 (2 bytes)          
            OFFSET_ID       = OFFSET_LEN+1,              // 2 UINT16 (4 bytes)          
            OFFSET_FLAGS    = (OFFSET_ID+1)*2,           // 3 bits            
            OFFSET_FRAGMENT = OFFSET_FLAGS/2,            // 13 bits (masked) 
            OFFSET_TTL      = (OFFSET_FRAGMENT+1)*2,     // 8 bytes                     
            OFFSET_PROTOCOL = OFFSET_TTL+1,              // 9 bytes
            OFFSET_CHECKSUM = (OFFSET_PROTOCOL+1)/2,     // 5 UINT16 (10 bytes)
            OFFSET_SRC_ADDR = ((OFFSET_CHECKSUM+1)*2)/4, // 3 UINT32 (12 bytes)
            OFFSET_DST_ADDR = OFFSET_SRC_ADDR+1,         // 4 UINT32 (16 bytes)
            OFFSET_OPTIONS  = (OFFSET_DST_ADDR+1)*4      // 20 bytes
        };  
};  // end class ProtoPktIPv4

/**
 * @class ProtoPktIPv6
 *
 * @brief Parses IPv6 packets
 */

class ProtoPktIPv6 : public ProtoPktIP
{
    public:
        ProtoPktIPv6(UINT32*        bufferPtr = 0, 
                     unsigned int   numBytes = 0, 
                     bool           initFromBuffer = false,
                     bool           freeOnDestruct = false);
        ProtoPktIPv6(ProtoPktIP& ipPkt);
        ~ProtoPktIPv6();
        
        enum {ADDR_LEN = 16};
        
        class Extension;      
        /**
         * @class Option
         *
		 * @brief IPv6 Option Base class
         */

        class Option : public ProtoPktIP::OptionBase
        {
            public:
                Option(char*        bufferPtr = NULL, 
                       unsigned int numBytes = 0, 
                       bool         initFromBuffer = true,
                       bool         freeOnDestruct = false);
                ~Option();
                
                bool InitFromBuffer(char* bufferPtr         = NULL, 
                                    unsigned int numBytes   = 0, 
                                    bool freeOnDestruct     = false);
                
                enum UnknownPolicy
                {
                    SKIP           = 0x00, /// skip and keep processing packet
                    DISCARD        = 0x01, /// silently discard the packet
                    REPORT_ALL     = 0x02, /// discard and ICMP to source for any packet
                    REPORT_UNICAST = 0x03  /// discard and ICMP to source for non-multicast packets
                };
                    
                enum Type
                {
                    PAD1    = 0,   
                    PADN    = 1,
                    SMF_DPD = 2  /// SMF duplicate packet detection option (temp value assignment)
                };
                   
                 
                UnknownPolicy GetUnknownPolicy() const
                    {return (UnknownPolicy)(((UINT8)buffer_ptr[OFFSET_TYPE]) >> 6);}
                bool IsMutable() const
                    {return (0 != (FLAG_MUTABLE & buffer_ptr[OFFSET_TYPE]));}
                Type GetType() const
                    {return (Type)(((UINT8)buffer_ptr[OFFSET_TYPE]) & 0x1f);}
                UINT8 GetDataLength() const
                    {return ((PAD1 == GetType()) ? 0 : ((UINT8)buffer_ptr[OFFSET_DATA_LENGTH]));}
                bool HasData() {return (GetDataLength() > 0);}
                const char* GetData() const
                    {return (buffer_ptr + OFFSET_DATA);}
                unsigned int GetLength() const
                {
                    return ((0 == buffer_bytes) ? 
                                0 : ((PAD1 == GetType()) ? 
                                        1 : 2 + GetDataLength()));  
                }
                
                bool InitIntoBuffer(Type         type,
                                    char*        bufferPtr = NULL, 
                                    unsigned int numBytes = 0, 
                                    bool         freeOnDestruct = false);
                void SetUnknownPolicy(UnknownPolicy policy)
                {
                    buffer_ptr[OFFSET_TYPE] &= ~((char)(0x03 << 6));
                    buffer_ptr[OFFSET_TYPE] |= ((char)policy) << 6;    
                }    
                void SetMutable(bool state)
                {
                    buffer_ptr[OFFSET_TYPE] = state ? 
                                            (buffer_ptr[OFFSET_TYPE] | FLAG_MUTABLE) :
                                            (buffer_ptr[OFFSET_TYPE] & ~FLAG_MUTABLE);
                }
                void SetType(Type type)
                {
                    buffer_ptr[OFFSET_TYPE] &= ~((char)0x1f);
                    buffer_ptr[OFFSET_TYPE] |= (char)(type & 0x1f);
                }
                bool SetData(char* dataPtr, UINT8 dataLen);
                bool MakePad(UINT8 numBytes);
                
                /**
                 * @class Iterator
                 *
				 * @brief Iterator class
                 */

                class Iterator
                {
                    public:
                        Iterator(const Extension& extension);
                        ~Iterator();
                        void Reset() {offset = 2;}
                        bool GetNextOption(Option& option);
                        
                    private:
                        const Extension& hdr_extension;
                        unsigned int     offset;
                                
                };  // end class ProtoPktIPv6::Option::Iterator
                
            protected:
                void SetDataLength(UINT8 dataLen)
                    {buffer_ptr[OFFSET_DATA_LENGTH] = (char)dataLen;}
                
                enum {FLAG_MUTABLE = 0x20};
                enum
                {
                    OFFSET_TYPE        = 0,
                    OFFSET_DATA_LENGTH = OFFSET_TYPE + 1,
                    OFFSET_DATA        = OFFSET_DATA_LENGTH + 1   
                };
                    
        };  // end class ProtoPktIPv6::Option
        
        /**
         * @class Extension
		 * @brief IPv6 Extension class
         */
        class Extension : public ProtoPkt
        {
            public:
                Extension(Protocol      extType = ProtoPktIP::NONE,
                          UINT32*       bufferPtr = NULL, 
                          unsigned int  numBytes = 0, 
                          bool          initFromBuffer = true,
                          bool          freeOnDestruct = false);
                ~Extension();
                
                // Must have a buffer attached or allocated before 
                // calling this one.  
                bool Copy(const Extension& ext);
                
                bool InitIntoBuffer(Protocol      extensionType,
                                    UINT32*       bufferPtr = NULL, 
                                    unsigned int  numBytes = 0, 
                                    bool          freeOnDestruct = false);
                void SetType(Protocol extensionType)
                    {ext_type = extensionType;}
                void SetNextHeader(Protocol protocol)
                {
                    ASSERT(buffer_bytes > OFFSET_NEXT_HDR);
                    ((UINT8*)buffer_ptr)[OFFSET_NEXT_HDR] = (UINT8)protocol;
                }
                void SetExtensionLength(UINT16 numBytes);  
                
                Option* AddOption(Option::Type optType);  // not for all extension types!
                bool ReplaceOption(Option& oldOpt, Option& newOpt);
                
                
                bool Pack();
                
                bool InitFromBuffer(Protocol extType, UINT32* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);
                Protocol GetType() const 
                    {return ext_type;}
                Protocol GetNextHeader() const
                    {return (Protocol)(((UINT8*)buffer_ptr)[OFFSET_NEXT_HDR]);}
                /** 
				* This gets the extension's length embedded in the assumed "payload length" field
                * (Note the FRAGMENT extension doesn't have this field and is of fixed length)
				*/
                 UINT16 GetExtensionLength() const; // _total_ length of extension header in bytes
                
                bool IsOptionHeader()
                {
                    switch(ext_type)
                    {
                        case HOPOPT:
                        case DSTOPT:
                            return true;
                        default:
                            return false;
                    }
                }
                
                /**
                 * @class Iterator
                 *
				 * @brief Extension iterator class
                 */
                class Iterator
                {
                    public:
                        Iterator(const ProtoPktIPv6& pkt);
                        ~Iterator();
                        void Reset()
                        {
                            next_hdr = ipv6_pkt.GetNextHeader();
                            offset = 40;  // look for first extension after IPv6 base header
                        }
                        bool GetNextExtension(Extension& extension);
                        
                    private:
                        const ProtoPktIPv6&   ipv6_pkt;
                        Protocol              next_hdr;
                        UINT16                offset;   // current byte offset into IPv6 packet buffer
                };  // end class ProtoPktIPv6::Extension::Iterator
                    
            protected:
                void PackOption();      // pack pending option, if needed
                bool PadOptionHeader(); // pack and pad as needed
                
                enum
                {
                    OFFSET_NEXT_HDR = 0,
                    OFFSET_LENGTH   = OFFSET_NEXT_HDR+1  // only applies to some extension types
                };
                    
                Protocol    ext_type;
                Option      opt_temp;
                bool        opt_pending;
                bool        opt_packed;  // has been padded/packed
        };  // end class ProtoPktIPv6::Extension
        
        
        // Use these to build a packet
        bool InitIntoBuffer(UINT32*       bufferPtr = NULL, 
                            unsigned int  numBytes = 0, 
                            bool          freeOnDestruct = false);
        
        void SetTrafficClass(UINT8 trafficClass) 
        {
            ((UINT8*)buffer_ptr)[OFFSET_CLASS_MSN] &= 0xf0;
            ((UINT8*)buffer_ptr)[OFFSET_CLASS_MSN] |= (trafficClass >> 4);
            ((UINT8*)buffer_ptr)[OFFSET_CLASS_LSN] &= 0x0f;
            ((UINT8*)buffer_ptr)[OFFSET_CLASS_LSN] |= (trafficClass << 4);
        }
        void SetFlowLabel(UINT32 flowLabel) 
        {
            flowLabel = (0xfc00 & ntohl(buffer_ptr[OFFSET_LABEL])) | (flowLabel & 0x03ff);
            buffer_ptr[OFFSET_LABEL] = htonl(flowLabel);
        }
        void SetPayloadLength(UINT16 numBytes) 
        {
            ((UINT16*)buffer_ptr)[OFFSET_LENGTH] = htons(numBytes);
            pkt_length = 40 + numBytes;    
        }
        void SetNextHeader(Protocol protocol) 
            {((UINT8*)buffer_ptr)[OFFSET_NEXT_HDR] = (UINT8)protocol;}
        void SetHopLimit(UINT8 hopLimit) 
            {((UINT8*)buffer_ptr)[OFFSET_HOP_LIMIT] = hopLimit;}
        void SetSrcAddr(ProtoAddress& addr) 
            {memcpy((char*)(buffer_ptr+OFFSET_SRC_ADDR), addr.GetRawHostAddress(), 16);}
        void SetDstAddr(ProtoAddress& addr) 
            {memcpy((char*)(buffer_ptr+OFFSET_DST_ADDR), addr.GetRawHostAddress(), 16);}
        
        
        // Map extension to end of current IPv6 packet header
        Extension* AddExtension(Protocol extType);
        bool PackHeader(Protocol nextHeader = NONE); // finalize pending extension, if applicable
        
        // Copy extension to beginning of packet's set of header extensions (payload is moved if present)
        bool PrependExtension(Extension& ext);
        // Copy extension to end of current IPv6 header (payload is moved if present)
        bool AppendExtension(Extension& ext);
        bool ReplaceExtension(Extension& oldExt, Extension& newExt);
        
        bool SetPayload(Protocol payloadType, const char* dataPtr, UINT16 dataLen);
                
        // Use these to parse a packet
        bool InitFromBuffer(UINT32* bufferPtr       = NULL, 
                            unsigned int numBytes   = 0, 
                            bool freeOnDestruct     = false);
        UINT8 GetTrafficClass() const
        {
            return (((((UINT8*)buffer_ptr)[OFFSET_CLASS_MSN] & 0x0f) << 4) |
                    ((((UINT8*)buffer_ptr)[OFFSET_CLASS_LSN] & 0xf0) >> 4));
        }
        UINT32 GetFlowLabel() const
            {return (ntohl(buffer_ptr[OFFSET_LABEL]) & 0x03ff);}
        Protocol GetNextHeader() const
            {return (Protocol)((UINT8*)buffer_ptr)[OFFSET_NEXT_HDR];}
        Protocol GetLastHeader() const; // returns type of final extension/transport in packet
        UINT8 GetHopLimit() const
            {return ((UINT8*)buffer_ptr)[OFFSET_HOP_LIMIT];}
        void GetSrcAddr(ProtoAddress& addr) const
            {addr.SetRawHostAddress(ProtoAddress::IPv6, (char*)(buffer_ptr+OFFSET_SRC_ADDR), 16);}
        const UINT32* GetSrcAddrPtr() const
            {return (buffer_ptr+OFFSET_SRC_ADDR);}
        void GetDstAddr(ProtoAddress& addr) const
                {addr.SetRawHostAddress(ProtoAddress::IPv6, (char*)(buffer_ptr+OFFSET_DST_ADDR), 16);}
        const UINT32* GetDstAddrPtr() const
            {return (buffer_ptr+OFFSET_DST_ADDR);}
                
        bool HasExtendedHeader() const 
            {return IsExtension(GetNextHeader());}
        
        const UINT32* GetPayload() const 
            {return (buffer_ptr + BASE_HDR_LENGTH/4);}
        UINT32* AccessPayload() 
            {return (buffer_ptr + BASE_HDR_LENGTH/4);}
        UINT16 GetPayloadLength() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_LENGTH]);}
        
    private:
        enum
        {
            OFFSET_CLASS_MSN    = OFFSET_VERSION,     // 1/2 byte traffic class, most sig nybble
            OFFSET_CLASS_LSN    = OFFSET_CLASS_MSN+1, // 1/2 byte traffic class, least sig nybble
            OFFSET_LABEL        = OFFSET_CLASS_LSN/4, // 24-bit flow label at word offset 0
            OFFSET_LENGTH       = (OFFSET_LABEL+1)*2, // 2 payload length bytes
            OFFSET_NEXT_HDR     = (OFFSET_LENGTH+1)*2, // 1 byte next header type
            OFFSET_HOP_LIMIT    = OFFSET_NEXT_HDR+1,   // 1 byte hop limit value
            OFFSET_SRC_ADDR     = (OFFSET_HOP_LIMIT+1)/4,  // 16 bytes of IPv6 address
            OFFSET_DST_ADDR     = OFFSET_SRC_ADDR+4,
            BASE_HDR_LENGTH     = (OFFSET_DST_ADDR+4)*4   // 40 bytes
        };
            
        Extension   ext_temp;
        bool        ext_pending;

};  // end class ProtoPktIPv6

/**
 * @class ProtoPktFRAG
 *
 * @brief Builds IPv6 FRAG extension.
 */

class ProtoPktFRAG : public ProtoPktIPv6::Extension
{
    public:
        ProtoPktFRAG(UINT32*       bufferPtr = NULL, 
                     unsigned int  numBytes = 0, 
                     bool          initFromBuffer = true,
                     bool          freeOnDestruct = false);
        ~ProtoPktFRAG();
        
        // Use these to build a FRAG extension
        bool InitIntoBuffer(UINT32*       bufferPtr = NULL, 
                            unsigned int  numBytes = 0, 
                            bool          freeOnDestruct = false);
        
        void SetFragmentOffset(UINT16 offsetValue)
        {
            /// mask flags and "or" in new "offsetValue"
            UINT16 frag = ntohs(((UINT16*)buffer_ptr)[OFFSET_FRAGMENT]);
            frag = (frag & 0x0007) | (offsetValue << 3);   
            ((UINT16*)buffer_ptr)[OFFSET_FRAGMENT] = htons(frag);
        }
        
        void SetMF()
            {((UINT8*)buffer_ptr)[OFFSET_FLAGS] |= ((UINT8)FLAG_MF);}
        void ClearMF()
            {((UINT8*)buffer_ptr)[OFFSET_FLAGS] &= ~((UINT8)FLAG_MF);}
        
        void SetID(UINT32 identifier)
            {buffer_ptr[OFFSET_ID] = htonl(identifier);}
        
        /// Use these to parse a FRAG extension
        bool InitFromBuffer(UINT32*         bufferPtr = NULL, 
                            unsigned int    numBytes = 0, 
                            bool            freeOnDestruct = false)
        {
            return Extension::InitFromBuffer(ProtoPktIP::FRAG, bufferPtr, numBytes, freeOnDestruct);
        }
        
        UINT16 GetFragmentOffset() const
            {return (ntohs(((UINT16*)buffer_ptr)[OFFSET_FRAGMENT]) >> 3);}
        
        const char* GetFragmentOffsetPtr() const
            {return ((char*)buffer_ptr + (2*OFFSET_FRAGMENT));}
        
        bool GetMF() const
            {return (0 != (((UINT8*)buffer_ptr)[OFFSET_FLAGS] & ((UINT8)FLAG_MF)));}
                
        UINT32 GetID() const
            {return ntohl(buffer_ptr[OFFSET_ID]);}
        
    private:
        enum
        {
            OFFSET_RESERVED = (OFFSET_LENGTH + 1)*2,   // UINT8 offset
            OFFSET_FRAGMENT = (OFFSET_RESERVED + 1)/2, // UINT16 offset
            OFFSET_FLAGS    = (OFFSET_FRAGMENT*2) + 1,   // UINT8 offset
            OFFSET_ID       = (OFFSET_FLAGS + 1)/4     // UINT32 offset
        };
        enum Flag {FLAG_MF = 0x01};
};  // end class ProtoPktFRAG

/**
 * @class ProtoPktAUTH
 *
 * @brief IPv6 Authentication Header (AUTH) extension
 */
class ProtoPktAUTH : public ProtoPktIPv6::Extension
{
    public:
        ProtoPktAUTH(UINT32*       bufferPtr = NULL, 
                     unsigned int  numBytes = 0, 
                     bool          initFromBuffer = true,
                     bool          freeOnDestruct = false);
        ~ProtoPktAUTH();
        
        // Use these to build an AUTH extension
        // (TBD) Should "InitIntoBuffer() make sure there is room for spi & sequence fields?
        bool InitIntoBuffer(UINT32*       bufferPtr = NULL, 
                            unsigned int  numBytes = 0, 
                            bool          freeOnDestruct = false);
        void SetSPI(UINT32 spi)
            {buffer_ptr[OFFSET_SPI] = htonl(spi);}
        
        void SetSequence(UINT32 sequence)
            {buffer_ptr[OFFSET_SEQUENCE] = htonl(sequence);}
        // (TBD) add a method to set the ICV field
        
        /// Use these to parse an AUTH extension
        bool InitFromBuffer(UINT32*         bufferPtr = NULL, 
                            unsigned int    numBytes = 0, 
                            bool            freeOnDestruct = false);
        
        UINT32 GetSPI() const
            {return ntohl(buffer_ptr[OFFSET_SPI]);}
        
        UINT32* GetSPIPtr() const
            {return (buffer_ptr + OFFSET_SPI);}
        
        UINT32 GetSequence() const
            {return ntohl(buffer_ptr[OFFSET_SEQUENCE]);}
        
        const UINT32* GetSequencePtr() const
            {return (buffer_ptr + OFFSET_SEQUENCE);}
        
    private:
        enum
        {
            OFFSET_RESERVED = (OFFSET_LENGTH + 1)/2,    // UINT16 offset
            OFFSET_SPI      = ((OFFSET_RESERVED+1)*2)/4,// UINT32 offset
            OFFSET_SEQUENCE = (OFFSET_SPI + 1),         // UINT32 offset
            OFFSET_ICV      = (OFFSET_SEQUENCE + 1)     // UINT32 offset
        };
};  // end class ProtoPktAUTH

/**
 * @class ProtoPktESP
 *
 * @brief IPv6 Encapsulating Security Protocol (ESP) header
 */
class ProtoPktESP : public ProtoPkt
{
    public:
        ProtoPktESP(UINT32*       bufferPtr = NULL, 
                    unsigned int  numBytes = 0, 
                    bool          freeOnDestruct = false);
        ~ProtoPktESP();
        
        // Use these to build an ESP header
        // (TBD) Should "InitIntoBuffer() make sure there is room for spi & sequence fields?
        bool InitIntoBuffer(UINT32*       bufferPtr = NULL, 
                            unsigned int  numBytes = 0, 
                            bool          freeOnDestruct = false);
        void SetSPI(UINT32 spi)
            {buffer_ptr[OFFSET_SPI] = htonl(spi);}
        
        void SetSequence(UINT32 sequence)
            {buffer_ptr[OFFSET_SEQUENCE] = htonl(sequence);}
        // (TBD) add a method to set the ICV field
        
        // Use these to parse an ESP extension
        bool InitFromBuffer(UINT16          espLength,
                            UINT32*         bufferPtr = NULL, 
                            unsigned int    numBytes = 0, 
                            bool            freeOnDestruct = false);
        
        UINT32 GetSPI() const
            {return ntohl(buffer_ptr[OFFSET_SPI]);}
        
        UINT32* GetSPIPtr() const
            {return (buffer_ptr + OFFSET_SPI);}
        
        UINT32 GetSequence() const
            {return ntohl(buffer_ptr[OFFSET_SEQUENCE]);}
        
        const UINT32* GetSequencePtr() const
            {return (buffer_ptr + OFFSET_SEQUENCE);}
        
    private:
        enum
        {
            OFFSET_SPI      = 0,                        // UINT32 offset
            OFFSET_SEQUENCE = (OFFSET_SPI + 1),         // UINT32 offset
            OFFSET_PAYLOAD  = (OFFSET_SEQUENCE + 1)     // UINT32 offset
        };
};  // end class ProtoPktESP



// This represents the RFC 2004 Minimal Forwarding Header and IP Payload
class ProtoPktMobile : public ProtoPkt
{
    public:
        ProtoPktMobile(UINT32*       bufferPtr = NULL, 
                       unsigned int  numBytes = 0, 
                       bool          initFromBuffer = false,
                       bool          freeOnDestruct = false);
        ~ProtoPktMobile();
        
        enum Flag {FLAG_SRC = 0x80};
        
        // Use these to build an MOBILE Minimal Forwarding Header 
        bool InitIntoBuffer(UINT32*       bufferPtr = NULL, 
                            unsigned int  numBytes = 0, 
                            bool          freeOnDestruct = false);
        
        void SetProtocol(ProtoPktIP::Protocol protocol)
            {SetUINT8(OFFSET_PROTOCOL, (UINT8)protocol);}
        void SetFlag(Flag flag)
            {((UINT8*)buffer_ptr)[OFFSET_FLAGS] |= flag;}
        void ClearFlag(Flag flag)
            {((UINT8*)buffer_ptr)[OFFSET_FLAGS] &= ~flag;}
        void SetChecksum(UINT16 checksum)
            {((UINT16*)buffer_ptr)[OFFSET_CHECKSUM] = htons(checksum);}  
        void SetDstAddr(const ProtoAddress& addr, bool calculateChecksum = false);
        bool SetSrcAddr(const ProtoAddress& addr, bool calculateChecksum = false);
        void SetPayload(const char* payload, UINT16 numBytes)
        {
            memcpy((char*)(buffer_ptr+OffsetPayload()), payload, numBytes);
            if (FlagIsSet(FLAG_SRC))
                SetLength(12 + numBytes);
            else
                SetLength(8 + numBytes);
        }
        UINT16 CalculateChecksum(bool set = true);
        
        bool InitFromBuffer(UINT32*         bufferPtr = NULL, 
                            unsigned int    numBytes = 0, 
                            bool            freeOnDestruct = false);
        
        ProtoPktIP::Protocol GetProtocol() const
            {return (ProtoPktIP::Protocol)GetUINT8(OFFSET_PROTOCOL);}
        bool FlagIsSet(Flag flag) const
            {return (0 != (flag & (((UINT8*)buffer_ptr)[OFFSET_FLAGS])));}    
        UINT16 GetChecksum() const
            {return GetUINT16(OFFSET_CHECKSUM);}    
        void GetDstAddr(ProtoAddress& dst) const
            {dst.SetRawHostAddress(ProtoAddress::IPv4, (char*)(buffer_ptr+OFFSET_DST_ADDR), 4);}
        bool GetSrcAddr(ProtoAddress& src) const;
        
        UINT16 GetPayloadLength() const
            {return GetLength() - 4*OffsetPayload();}
        const UINT32* GetPayload() const
            {return (buffer_ptr + OffsetPayload());}
        UINT32* AccessPayload()
            {return (buffer_ptr + OffsetPayload());}
        
    private:
        enum 
        {
            OFFSET_PROTOCOL = 0,                        // UINT8 offset
            OFFSET_RESERVED = (OFFSET_PROTOCOL + 1),    // UINT8 offset
            OFFSET_FLAGS = OFFSET_RESERVED,             // UINT8 offset
            OFFSET_CHECKSUM = (OFFSET_PROTOCOL/2 + 1),  // UINT16 offset
            OFFSET_DST_ADDR = (OFFSET_CHECKSUM/2 + 1),  // UINT32 offset 
            OFFSET_SRC_ADDR = (OFFSET_DST_ADDR + 1)     // UINT32 offset   
        };
            
        unsigned int OffsetPayload() const  // (UINT32 offset
            {return (FlagIsSet(FLAG_SRC) ? OFFSET_SRC_ADDR + 1 : OFFSET_SRC_ADDR);}
                
    
};  // end class ProtoPktMobile

/**
 * @class ProtoPktDPD
 *
 * @brief IPv6 Simplified Multicast Forwarding Duplicate Packet Detection (DPD) option
 */
class ProtoPktDPD : public ProtoPktIPv6::Option
{
    public:
        ProtoPktDPD(char*        bufferPtr = NULL, 
                    unsigned int numBytes = 0, 
                    bool         initFromBuffer = true,
                    bool         freeOnDestruct = false);
        ~ProtoPktDPD();
        
        enum TaggerIdType
        {
            TID_NULL        = 0,
            TID_DEFAULT     = 1,
            TID_IPv4_ADDR   = 2,
            TID_IPv6_ADDR   = 3,
            TID_EXT         = 7  
        };
        
        // Use these to build a DPD extension (call them in order)
        bool InitIntoBuffer(char*         bufferPtr = NULL, 
                            unsigned int  numBytes = 0, 
                            bool          freeOnDestruct = false);
        
        // If SetHashAssistValue() is used, the other "set" methods do not apply
        bool SetHAV(const char* hashAssistValue, UINT8 numBytes);
        
        // After init, call one of these first to set taggerId, if applicable
        bool SetTaggerId(TaggerIdType type, const char* taggerId, UINT8 taggerIdLength);
        bool SetTaggerId(const ProtoAddress& ipAddr);
        
        // Finally, call one of these to set the pktId (sequence number)
        bool SetPktId(const char* pktId, UINT8 pktIdLength);
        bool SetPktId(UINT8 value)
            {return SetPktId((char*)&value, 1);}
        bool SetPktId(UINT16 value)
        {
            value = htons(value);
            return SetPktId((char*)&value, 2);
        }
        bool SetPktId(UINT32 value)
        {
            value = htonl(value);
            return SetPktId((char*)&value, 4);
        }
        
        
        // Use these to parse a DPD extension
        bool InitFromBuffer(char*           bufferPtr = NULL, 
                            unsigned int    numBytes = 0, 
                            bool            freeOnDestruct = false);
        
        bool HasHAV() const
            {return (0 != (0x80 & ((UINT8*)buffer_ptr)[OFFSET_HAV]));}
        
        UINT8 GetHAVLength() const
            {return GetDataLength();}
        
        const char* GetHAV() const
            {return (HasHAV() ? (((const char*)buffer_ptr) + OFFSET_HAV) : NULL);}
            
        
        TaggerIdType GetTaggerIdType() const
            {return (HasHAV() ? TID_NULL : (TaggerIdType)((buffer_ptr[OFFSET_TID_TYPE] >> 4) & 0x0f));}
        
        UINT8 GetTaggerIdLength() const
        {
            UINT8 tidType = (HasHAV() ? TID_NULL : buffer_ptr[OFFSET_TID_TYPE]);
            return ((TID_NULL != (TaggerIdType)tidType) ? ((tidType & 0x0f) + 1) : 0);  
        }
            
        const char* GetTaggerId() const
            {return (buffer_ptr + OFFSET_TID_VALUE);}
        bool GetTaggerId(ProtoAddress& addr) const;
        
        UINT8 GetPktIdLength() const
            {return (GetDataLength() - GetTaggerIdLength() - (HasHAV() ? 0 : 1));}
        
        const char* GetPktId() const
            {return (buffer_ptr + OffsetPktId());}
        bool GetPktId(UINT8& value) const;
        bool GetPktId(UINT16& value) const;
        bool GetPktId(UINT32& value) const;
        
    private:
        enum
        {
            OFFSET_HAV          = OFFSET_DATA,         // UINT8 offset (note msbit always '1')
            OFFSET_TID_TYPE     = OFFSET_DATA,         // UINT8 offset (upper 4 bits)
            OFFSET_TID_LENGTH   = OFFSET_TID_TYPE,     // UINT8 offset (lower 4 bits)
            OFFSET_TID_VALUE    = (OFFSET_TID_TYPE + 1)// UINT8 offset
        };
        unsigned int OffsetPktId() const
            {return (OFFSET_TID_VALUE + GetTaggerIdLength() - (HasHAV() ? 1 : 0));}
};  // end class ProtoPktDPD

/**
 * @class ProtoPktUDP
 *
 * @brief Parses UDP Packets
 */
class ProtoPktUDP : public ProtoPkt
{
    public:
        ProtoPktUDP(UINT32*        bufferPtr = 0, 
                    unsigned int   numBytes = 0, 
                    bool           initFromBuffer = true,
                    bool           freeOnDestruct = false);
        ~ProtoPktUDP();
        
        // Use these to parse the datagram
        bool InitFromBuffer(UINT32* bufferPtr       = NULL, 
                            unsigned int numBytes   = 0, 
                            bool freeOnDestruct     = false);
        bool InitFromPacket(ProtoPktIP& pkt);
        UINT16 GetSrcPort() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_SRC]);}
        UINT16 GetDstPort() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_DST]);}
        UINT16 GetChecksum() const
            {return ntohs(((UINT16*)buffer_ptr)[OFFSET_CHECKSUM]);}
        UINT16 GetPayloadLength() const
            {return (ntohs(((UINT16*)buffer_ptr)[OFFSET_LENGTH]) - 8);}
        const UINT32* GetPayload() const
            {return (buffer_ptr + OFFSET_PAYLOAD);}
        UINT32* AccessPayload()
            {return (buffer_ptr + OFFSET_PAYLOAD);}
        UINT16 ComputeChecksum(ProtoPktIP& ipPkt) const;
        bool ChecksumIsValid(ProtoPktIP& ipPkt) const
            {return (GetChecksum() == ComputeChecksum(ipPkt));}
        
        // Use these to build the datagram
        bool InitIntoBuffer(UINT32*        bufferPtr = 0, 
                            unsigned int   numBytes = 0, 
                            bool           freeOnDestruct = false);
        void SetSrcPort(UINT16 port)
            {((UINT16*)buffer_ptr)[OFFSET_SRC] = htons(port);}
        void SetDstPort(UINT16 port)
            {((UINT16*)buffer_ptr)[OFFSET_DST] = htons(port);}
        void SetChecksum(UINT16 checksum)
            {((UINT16*)buffer_ptr)[OFFSET_CHECKSUM] = htons(checksum);}
        void SetPayload(const char* payload, UINT16 numBytes)
        {
            memcpy((char*)(buffer_ptr+OFFSET_PAYLOAD), payload, numBytes);
            SetPayloadLength(numBytes);
        }    
        void SetPayloadLength(UINT16 numBytes)
        {
            UINT16 len = 8 + numBytes;
            ((UINT16*)buffer_ptr)[OFFSET_LENGTH] = htons(len);
            pkt_length = len;    
        }
        void FinalizeChecksum(ProtoPktIP& ipPkt)
            {SetChecksum(ComputeChecksum(ipPkt));}
        
    private:
        enum
        {
            OFFSET_SRC      = 0,                 // source port number (UINT16 offset)
            OFFSET_DST      = OFFSET_SRC + 1,    // destination port number (UINT16 offset)
            OFFSET_LENGTH   = OFFSET_DST + 1,    // UDP datagram length (bytes) (UINT16 offset)
            OFFSET_CHECKSUM = OFFSET_LENGTH + 1,        //  (UINT16 offset)
            OFFSET_PAYLOAD  = (OFFSET_CHECKSUM + 1)/2   // (UINT32 offset0
        };
};  // end class ProtoPktUDP


#endif // _PROTO_PKT_IP
