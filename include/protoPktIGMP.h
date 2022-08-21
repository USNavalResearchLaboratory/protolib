#ifndef _PROTO_PKT_IGMP
#define _PROTO_PKT_IGMP

#include "protoPkt.h"
#include "protoAddress.h"

class ProtoPktIGMP : public ProtoPkt
{
    public:
        ProtoPktIGMP(void*          bufferPtr = NULL, 
                     unsigned int   numBytes = 0, 
                     bool           initFromBuffer = false,
                     bool           freeOnDestruct = false); 
        ~ProtoPktIGMP();
        
        enum Type
        {
            QUERY       = 0x11,
            REPORT_V1   = 0x12,
            REPORT_V2   = 0x16,
            REPORT_V3   = 0x22,
            LEAVE       = 0x17 
        };
            
        static const UINT8 DEFAULT_QRV;         // 2 per RFC 3376
        static const double DEFAULT_QQIC;       // 125 seconds per RFC 3376
        static const double DEFAULT_MAX_RESP;   // 10 seconds per RFC 3376
        
        // IGMPv3 REPORT Group Record
        class GroupRecord : public ProtoPkt
        {
            public:
                GroupRecord(void*          bufferPtr = NULL, 
                            unsigned int   numBytes = 0, 
                            bool           initFromBuffer = false,
                            bool           freeOnDestruct = false); 
                ~GroupRecord(); 
                
                enum Type
                {
                    INVALID_TYPE            = 0,
                    MODE_IS_INCLUDE         = 1,
                    MODE_IS_EXCLUDE         = 2,
                    CHANGE_TO_INCLUDE_MODE  = 3,   // include none == ASM leave, otherwise SSM join src list
                    CHANGE_TO_EXCLUDE_MODE  = 4,   // exclude none == ASM join group, otherwise SSM leave src list
                    ALLOW_NEW_SOURCES       = 5,   // source-specific join essentially?
                    BLOCK_OLD_SOURCES       = 6    // source-specific leave essentially?
                };
                    
                // Use these to parse Group Records
                bool InitFromBuffer(void*   bufferPtr       = NULL, 
                                    unsigned int bufferBytes = 0, 
                                    bool freeOnDestruct     = false);    
                    
                Type GetType() const
                    {return (Type)GetUINT8(OFFSET_TYPE);}
                unsigned int GetAuxDataLen() const
                {
                    unsigned int len = GetUINT8(OFFSET_AUX_LEN);
                    return (len << 2);  // returns length in bytes
                }
                UINT16 GetNumSources() const
                    {return GetUINT16(OFFSET_NUM_SRC);}
                bool GetGroupAddress(ProtoAddress& groupAddr)
                {
                    groupAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer(OFFSET_GROUP), 4);
                    return groupAddr.IsMulticast();
                }
                bool GetSourceAddress(UINT16 index, ProtoAddress& srcAddr) const;
                const void* GetAuxData() const
                    {return GetBuffer(OffsetAuxData());}
                
                // Use these to create group records
                bool InitIntoBuffer(void*        bufferPtr = NULL, 
                                    unsigned int numBytes = 0, 
                                    bool         freeOnDestruct = false);
                void SetType(Type type)
                    {SetUINT8(OFFSET_TYPE, (UINT8)type);}
                void SetGroupAddress(const ProtoAddress* groupAddr = NULL);
                bool AppendSourceAddress(const ProtoAddress& srcAddr);
                bool AppendAuxiliaryData(const char* data, UINT16 len);
                
            private:
                enum
                {
                    OFFSET_TYPE     = 0,
                    OFFSET_AUX_LEN  = OFFSET_TYPE + 1,
                    OFFSET_NUM_SRC  = OFFSET_AUX_LEN + 1,
                    OFFSET_GROUP    = OFFSET_NUM_SRC + 2,
                    OFFSET_SRC_LIST = OFFSET_GROUP + 4
                };
                UINT16 OffsetAuxData() const
                {return (OFFSET_SRC_LIST + 4 * GetNumSources());}
        };  // end class ProtoPktIGMP::GroupRecord
        
        // Use these to parse the IGMP message
        bool InitFromBuffer(UINT16  pktLength,
                            void*   bufferPtr       = NULL, 
                            unsigned int buferBytes = 0, 
                            bool freeOnDestruct     = false);
        
        UINT8 GetVersion() const;
        
        Type GetType() const
            {return (Type)GetUINT8(OFFSET_TYPE);}
        double GetMaxResponseTime() const;  // return value in seconds
        UINT16 GetChecksum() const
            {return GetUINT16(OFFSET_CHECKSUM);}
        // For QUERY, REPORT_V1, REPORT_V2, LEAVE only
        bool GetGroupAddress(ProtoAddress& groupAddr) const
        {
            groupAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)GetBuffer(OFFSET_GROUP), 4);
            return groupAddr.IsMulticast();
        }
        // These 5 are for IGMPv3 QUERY messages only
        bool GetSuppress() const
            {return (GetVersion() < 3) ? false : (0 != (GetUINT8(OFFSET_S) & 0x08));}
        UINT8 GetQRV() const
            {return ((GetVersion() < 3) ? 0 : (GetUINT8(OFFSET_QRV) & 0x07));}
        double GetQQIC() const;
        UINT16 GetNumSources() const
            {return (GetVersion() < 3) ? 0 : GetUINT16(OFFSET_NUM_SRC);}
        bool GetSourceAddress(UINT16 index, ProtoAddress& srcAddr) const;
        // These 2 are for IGMPv3 REPORT messages 
        UINT16 GetNumRecords() const
            {return GetUINT16(OFFSET_NUM_REC);}
        // This uses the prev groupRecord state to fetch next.  If "groupRecord"
        // passed in has a NULL buffer_ptr _or_ "first_ is true, then the
        // first record is fetched.
        bool GetNextGroupRecord(GroupRecord& groupRecord, bool first = false);
        
        
        // Use these for building IGMP packets
        bool InitIntoBuffer(Type         type,
                            unsigned int version,
                            void*        bufferPtr = NULL, 
                            unsigned int numBytes = 0, 
                            bool         freeOnDestruct = false);
        
        // This should be set to zero for IGMPv1 queries and report messages
        void SetMaxResponseTime(double seconds, bool updateChecksum = true);
        // set groupAddr pointer to NULL for general query
        void SetGroupAddress(ProtoAddress* groupAddr = NULL, bool updateChecksum = true);  
        // These 4 are for IGMPv3 QUERY messages only
        bool SetSuppress(bool state, bool updateChecksum = true);
        bool SetQRV(UINT8 qrv, bool updateChecksum = true);
        bool SetQQIC(double seconds, bool updateChecksum = true);
        bool AppendSourceAddress(ProtoAddress& srcAddr, bool updateChecksum = true);
        // These are for IGMPv3 REPORT messages only
        bool AttachGroupRecord(GroupRecord& groupRecord);  // inits "groupRecord"
        bool AppendGroupRecord(const GroupRecord& groupRecord, bool updateChecksum = true);
        
        
        // Return computed checksum in host byte order and set by default
        UINT16 ComputeChecksum(bool set = true);
        UINT16 FinalizeChecksum()
            {return ComputeChecksum(true);}
        
             
    private:
        // shared offsets for IGMP query, report, and leave messages
        // (all UINT8 offsets used here)
        enum
        {
            OFFSET_TYPE     = 0,
            OFFSET_MAX_RESP = OFFSET_TYPE + 1,
            OFFSET_CHECKSUM = OFFSET_MAX_RESP + 1,
            OFFSET_GROUP    = OFFSET_CHECKSUM + 2,
            OFFSET_RESERVED = OFFSET_GROUP + 4,
            OFFSET_S        = OFFSET_RESERVED,
            OFFSET_QRV      = OFFSET_S,
            OFFSET_QQIC     = OFFSET_QRV + 1,
            OFFSET_NUM_SRC  = OFFSET_QQIC + 1,    // for IGMPv3 queries
            OFFSET_NUM_REC  = OFFSET_GROUP + 2,   // for IGMPv3 reports
            OFFSET_SRC_LIST = OFFSET_NUM_SRC + 2, // for IGMPv3 queries
            OFFSET_REC_LIST = OFFSET_NUM_REC + 2  // for IGMPv3 reports
        };
        // IGMPv3 group record field offsets
        
                      
};  // end class ProtoPktIGMP

// TBD - implement MLD (incl. MLDv2) ... ProtoPktMLD
    
#endif // _PROTO_PKT_IGMP
