
#include <protoApp.h>
#include <protoSocket.h>
#include <protoPipe.h>
#include <protoPkt.h>
#include <protoQueue.h>
#include <protoNet.h>

#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>
#include <stdlib.h>  // for rand(), etc


// IMPORTANT NOTE:  Although this works, it is still a work in progress.  It currently does 
// a basic ping/time check (ting) over a UDP sockett to a multicast and/or unicast destinations.
// The results it reports are currently simple.  One measurement is based on a handshake and
// observation of RTT (similar to NTP) while for multicast it also can use a form of 
// Reference Broadcast Synchronization (RBS) through shared observations of other hosts'
// broadcasts.  RTT is also opportunistically measure during the multicast rounds.  
// There is more work to do here including:
//
// 1) Optional use of PCAP socket (via ProtoCap class) to access the more accurate timestamp
//    for received packets.  This helps RBS and the RTT measurement by removing uncertainty
//    due to the application space UDP socket getting serviced.  RBS benefits more here since the
//    UDP tx socket queuing/sending doesn't matter for it.
//
// 2) Allowing for multiple rounds in multicast sessions, or even ongoing signaling with different
//    hosts initiating the "ting" chatter"  This would also mean maintaining some observation state
//    from one round to the next, pruning old observations, etc. Also need to do more sophisticated
//    processing of multicast feedback to compute offsets for hosts that share common observations/
//    with _other_ receivers but not ourself, but where a "chain" of observations is available to
//    deduce the offsets to those hosts.  This can be done iteratively
// 
// 3) Observe outbound PCAP timestamps and send "corrections" to sent_time for previous feedback
//    reports.  This would tighten round trip measurements a little more.
//
// 4) More sophisticated filtering of observations.  For example, lower RTT is a good indicator of 
//    a better measurement.
//
// 5) Related to #4, account for clock skew among hosts
//
// 6) Add option to actually perform iterative time synchronization, making this into something
//    more useful than a time check.
//
// 7) Add sequence number to packets to measure packet loss.
//
// 8) Use ProtoPipe coordination from new ting initiator(s) to already running ting instance
//    instead of the "reuse port" hack that we now use.  Any initiated commands would be
//    relayed to running instance and relevant feedback would be collected before initiator exit
//
// 9) Add (optional?) timestamp to log output lines so we can track time offset overs time
//



// ting ("time ping") is an application that can either idle, listening for ting requests
// or send ting requests to initiate a time check to another ting listener
//
//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |version|  type |      flags    |   backoff     |    round_id   |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                        sent_time_sec                          |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                        sent_time_usec                         |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                                                               |
//      +                                                               +
//      |                       Feedback Reports                        |
//      +                                                               +
//      |                             ...                               |


//                          Feedback report format 
//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |     length    |    round_id   |          src_port             |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                                                               |
//      +                           src_addr                            +
//      |                           ...                                 |
//
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                       reference_time_sec                      |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                       reference_time_usec                     |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                      observe_time_sec                         |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                      observe_time_usec                        |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                           rtt_usec                            |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     

// Pick a random number from 0..max
inline double UniformRand(double max)
    {return (max * ((double)rand() / (double)RAND_MAX));}
         
// Pick a random number from 0..max
// (truncated exponential dist. lambda = log(groupSize) + 1)
inline double ExponentialRand(double max, double groupSize)
{
    double lambda = log(groupSize) + 1;
    double x = UniformRand(lambda/max)+lambda/(max*(exp(lambda)-1));
    return ((max/lambda)*log(x*(exp(lambda)-1)*(max/lambda)));   
}     
     
enum {TING_MSG_MAX = 1400};

class TingMessage : public ProtoPkt
{
    public:
        enum {VERSION = 1};
    
        TingMessage(UINT32*        bufferPtr = 0, 
                    unsigned int   numBytes = 0, 
                    bool           initFromBuffer = false,
                    bool           freeOnDestruct = false);
        ~TingMessage();
        
        // TBD - should we define different formats for each message type?
        enum Type
        {
            INVALID = 0,
            HAIL    = 1,  // a "hail" is sent first to make sure ARP, etc is all good
            REQUEST = 2,  // requests have valid timestamps
            REPLY   = 3        
        };
            
        enum Flag
        {
            UNICAST = 0x01
        };
            
        class Report;
            
        bool IsValid() const
        {
            return ((buffer_bytes >= GetMinimumLength()) && 
                    (VERSION == GetVersion()) && 
                    (INVALID != GetType()));
        }
        
        // Message parsing methods
        UINT8 GetVersion() const
            {return (GetUINT8(OFFSET_VERSION) >> 4);}
        Type GetType() const
            {return (Type)(GetUINT8(OFFSET_TYPE) & 0x0f);}
        bool FlagIsSet(Flag flag) const
            {return (0 != (flag & GetUINT8(OFFSET_FLAGS)));}
        bool IsUnicast() const
            {return FlagIsSet(UNICAST);}
        UINT8 GetBackoffQuantized() const 
            {return GetUINT8(OFFSET_BACKOFF);}
        double GetBackoff() const
            {return UnquantizeBackoff(GetBackoffQuantized());}
        UINT8 GetRoundId() const
            {return GetUINT8(OFFSET_ROUND_ID);}
        void GetSentTime(struct timeval& theTime) const
        {
            theTime.tv_sec = GetUINT32(buffer_ptr+OFFSET_SENT_SEC);
            theTime.tv_usec = GetUINT32(buffer_ptr+OFFSET_SENT_USEC);
        }
        
        // Start with a null buffer report to reset iteration
        bool GetNextReport(Report& report) const
        {
            UINT32* nextBuffer = (UINT32*)report.AccessBuffer();
            nextBuffer = (NULL != nextBuffer) ? (nextBuffer + report.GetReportWords()) : 
                                                (buffer_ptr + OFFSET_REPORT);
            unsigned int nextOffset = (nextBuffer - buffer_ptr) << 2;
            unsigned int space = (nextOffset < GetLength()) ? (GetLength() - nextOffset) : 0;
            report.AttachBuffer(nextBuffer, space);
            return report.InitFromBuffer();
        }
        
        // Message creation methods 
        // (These assume the buffer size is sufficient!)
        void Init()
        {
            memset(buffer_ptr, 0, GetMinimumLength());
            SetVersion(VERSION);
        }        
        void SetVersion(UINT8 version)
        {
            SetUINT8(OFFSET_VERSION, (GetUINT8(OFFSET_VERSION) & 0x0f) | (version << 4));
            SetLength(GetMinimumLength());
        }
        void SetType(Type type)
            {SetUINT8(OFFSET_TYPE, (GetUINT8(OFFSET_TYPE) & 0xf0) | type);}
        void SetFlag(Flag flag)
            {SetUINT8(OFFSET_FLAGS, flag | GetUINT8(OFFSET_FLAGS));}
        void ClearFlag(Flag flag)
            {SetUINT8(OFFSET_FLAGS, ~flag & GetUINT8(OFFSET_FLAGS));}
        void SetBackoff(double seconds)
            {SetBackoffQuantized(QuantizeBackoff(seconds));}
        void SetBackoffQuantized(UINT8 quant)
            {SetUINT8(OFFSET_BACKOFF, quant);}
        void SetRoundId(UINT8 roundId)
            {SetUINT8(OFFSET_ROUND_ID, roundId);}
        void SetSentTime(struct timeval theTime)
        {
            SetUINT32(buffer_ptr+OFFSET_SENT_SEC, theTime.tv_sec);
            SetUINT32(buffer_ptr+OFFSET_SENT_USEC, theTime.tv_usec);
        }
        bool AppendReport(const ProtoAddress&   srcAddr, 
                          UINT8                 roundId,
                          const struct timeval& refTime, 
                          const struct timeval& obsTime, 
                          double                rtt);
        
        
        class Report : public ProtoPkt
        {
            public:
                Report(UINT32*        bufferPtr = NULL, 
                       unsigned int   numBytes = 0, 
                       bool           initFromBuffer = false,
                       bool           freeOnDestruct = false);
                ~Report();
                
                
                // Report parsing
                bool InitFromBuffer(UINT32* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);
                UINT8 GetReportWords() const
                    {return GetUINT8(OFFSET_LENGTH);}
                unsigned int GetReportLength() const
                    {return ((unsigned int)GetReportWords() << 2);}
                UINT8 GetRoundId() const
                    {return GetUINT8(OFFSET_ROUND_ID);}
                bool GetSrcAddress(ProtoAddress& srcAddr) const;
                
                void GetReferenceTime(struct timeval& theTime) const
                {
                    theTime.tv_sec = GetUINT32(buffer_ptr + OffsetReferenceSec());
                    theTime.tv_usec = GetUINT32(buffer_ptr + OffsetReferenceUsec());
                }
                void GetObservedTime(struct timeval& theTime) const
                {
                    theTime.tv_sec = GetUINT32(buffer_ptr + OffsetObservedSec());
                    theTime.tv_usec = GetUINT32(buffer_ptr + OffsetObservedUsec());
                }
                double GetRtt() const
                {
                    UINT32 value = GetUINT32(buffer_ptr + OffsetRttUsec());
                    return ((0xffffffff != value) ? (double)value : -1.0);
                }
                
                // Report building (call in order, not valid until address is set)
                void InitIntoBuffer(UINT32* bufferPtr = NULL, unsigned int numBytes = 0, bool freeOnDestruct = false);
                void SetRoundId(UINT8 roundId)
                    {SetUINT8(OFFSET_ROUND_ID, roundId);}
                bool SetSrcAddress(const ProtoAddress& srcAddr);
                void SetReferenceTime(const struct timeval& theTime)
                {
                    SetUINT32(buffer_ptr + OffsetReferenceSec(), theTime.tv_sec);
                    SetUINT32(buffer_ptr + OffsetReferenceUsec(), theTime.tv_usec);
                }
                void SetObservedTime(const struct timeval& theTime)
                {
                    SetUINT32(buffer_ptr + OffsetObservedSec(), theTime.tv_sec);
                    SetUINT32(buffer_ptr + OffsetObservedUsec(), theTime.tv_usec);
                }
                void SetRtt(double rtt)
                {
                    UINT32 value = (rtt >= 0.0) ? (UINT32)(1.0e+06*rtt + 0.5) : 0xffffffff;
                    SetUINT32(buffer_ptr + OffsetRttUsec(), value);
                }
                
                
            private:
                enum
                {
                    OFFSET_LENGTH       = 0,                      // UINT8 offset (1 byte)
                    OFFSET_ROUND_ID     = OFFSET_LENGTH+1,        // UINT8 offset (1 byte)
                    OFFSET_SRC_PORT     = (OFFSET_ROUND_ID+1)/2,  // UINT16 offset 2 bytes
                    OFFSET_SRC_ADDR     = (2*(OFFSET_SRC_PORT+1))/4 // UINT32 offset
                };
                    
                void SetReportWords(UINT8 words)
                    {SetUINT8(OFFSET_LENGTH, words);}
                    
                unsigned int GetAddressWords() const
                {
                    unsigned int reportWords = GetUINT8(OFFSET_LENGTH);
                    return ((reportWords > 6) ? (reportWords - 6) : 0);
                }
                
                unsigned int GetAddressLength() const
                    {return (GetAddressWords() << 2);}
                    
                unsigned int OffsetReferenceSec() const   // UINT32 offset
                    {return (OFFSET_SRC_ADDR + GetAddressWords());}
                unsigned int OffsetReferenceUsec() const  // UINT32 offset
                    {return (OffsetReferenceSec() + 1);}
                unsigned int OffsetObservedSec() const    // UINT32 offset
                    {return (OffsetReferenceUsec() + 1);}
                unsigned int OffsetObservedUsec() const   // UINT32 offset
                    {return (OffsetObservedSec() + 1);}
                unsigned int OffsetRttUsec() const        // UINT32 offset
                    {return (OffsetObservedUsec() + 1);}
                
        }; // end class TingMessage::Report
        
        
        static const double BACKOFF_MIN;
        static const double BACKOFF_MAX; 
        static UINT8 QuantizeBackoff(double backoff);
        static double UnquantizeBackoff(UINT8 quant)
            {return BACKOFF[quant];}
        
    private:
        static const double BACKOFF[];
                
        enum
        {
            OFFSET_VERSION    = 0,                      // UINT8 offset (upper 4 bits)
            OFFSET_TYPE       = OFFSET_VERSION,         // UINT8 offset (lower 4 bits)
            OFFSET_FLAGS      = OFFSET_TYPE + 1,        // UINT8 offset (1 byte)
            OFFSET_BACKOFF    = OFFSET_FLAGS + 1,       // UINT8 offset (1 byte)
            OFFSET_ROUND_ID   = OFFSET_BACKOFF + 1,     // UINT8 offset (1 byte)
            OFFSET_SENT_SEC   = (OFFSET_ROUND_ID+1)/4,  // UINT32 offset (4 bytes)
            OFFSET_SENT_USEC  = OFFSET_SENT_SEC + 1,    // UINT32 offset (4 bytes)
            OFFSET_REPORT     = OFFSET_SENT_USEC + 1   // UINT32 offset (4 bytes)
        };
            
        static unsigned int GetMinimumLength()
            {return (OFFSET_REPORT << 2);}
        
};  // end class TingMessage

// An "AddressableItem" is indexed by is address and port number

class AddressableItem : public ProtoQueue::Item
{
    public:
        AddressableItem(const ProtoAddress& theAddr);
        virtual ~AddressableItem();
    
        const ProtoAddress& GetAddress() const
            {return item_addr;}  
        const char* GetAddressKey() const
            {return item_key;}  
        unsigned GetAddressKeysize() const
            {return ((item_addr.GetLength()+2) << 3);}   
        
    private:   
        ProtoAddress    item_addr;
        char            item_key[16+2];  // room for IPv6 addr and port number 
           
};  // end class AddressableItem

// This is currently implemented as a ProtoSortedQueue but it may be
// useful to allow for ProtoIndexedQueue support or even ProtoTree
class AddressableList : public ProtoSortedQueueTemplate<AddressableItem>
{
    public:
            
        AddressableItem* Find(const ProtoAddress& addr) const;   
        class Iterator : public ProtoSortedQueueTemplate<AddressableItem>::Iterator
        {
            public:
                Iterator(AddressableList& theList, const ProtoAddress* matchAddr = NULL);
                void Reset(const ProtoAddress* matchAddr = NULL);
                AddressableItem* GetNextItem();
                
            private:
                ProtoAddress match_addr;
                
        };  // end class AddressableList::Iterator
        
    private:
        const char* GetKey(const Item& item) const
            {return static_cast<const AddressableItem&>(item).GetAddressKey();}
        unsigned int GetKeysize(const Item& item) const
            {return static_cast<const AddressableItem&>(item).GetAddressKeysize();}  
        
};  // end class AddressableList

template<class ITEM_TYPE>
class AddressableListTemplate : public AddressableList
{
    public:
    // Insert the "item" into the tree 
        bool Insert(ITEM_TYPE& item)
            {return AddressableList::Insert(item);}
        
        // Remove the "item" from the tree
        void Remove(ITEM_TYPE& item)
            {AddressableList::Remove(item);}
        
        // Find item with exact match to "key" and "keysize" (keysize is in bits)
        ITEM_TYPE* Find(const ProtoAddress& theAddr) const
            {return static_cast<ITEM_TYPE*>(AddressableList::Find(theAddr));}
        
        class Iterator : public AddressableList::Iterator
        {
            public:
                Iterator(AddressableListTemplate& theList, const ProtoAddress* matchAddr = NULL)
                 : AddressableList::Iterator(theList, matchAddr) {}
                ~Iterator() {}
                
                void Reset(const ProtoAddress* matchAddr = NULL)
                    {AddressableList::Iterator::Reset(matchAddr);}
                
                ITEM_TYPE* GetNextItem()
                    {return static_cast<ITEM_TYPE*>(AddressableList::Iterator::GetNextItem());}
        };  // end class AddressableListTemplate::Iterator 
};  // end class AddressableListTemplate


class TingObservation : public AddressableItem
{
    public:
        TingObservation(const ProtoAddress& srcAddr);
        ~TingObservation();
        
        const ProtoAddress& GetAddress() const
            {return AddressableItem::GetAddress();}
        UINT8 GetRoundId() const
            {return round_id;}
        const struct timeval& GetReferenceTime() const
            {return ref_time;}
        const struct timeval& GetObservedTime() const
            {return obs_time;}
        double GetRtt() const
            {return rtt_measured;}
        
        void SetRoundId(UINT8 roundId) 
            {round_id = roundId;}
        void SetReferenceTime(const struct timeval refTime)
            {ref_time = refTime;}
        void SetObservedTime(const struct timeval obsTime)
            {obs_time = obsTime;}   
        void SetRtt(double rtt)
            {rtt_measured = rtt;}
         
    private:
        UINT8           round_id;
        struct timeval  ref_time;
        struct timeval  obs_time;
        double          rtt_measured;
};  // end class TingObservation

TingObservation::TingObservation(const ProtoAddress& srcAddr)
 : AddressableItem(srcAddr), rtt_measured(-1.0)
{
    ref_time.tv_sec = ref_time.tv_usec = 0;
    obs_time.tv_sec = obs_time.tv_usec = 0;
}

TingObservation::~TingObservation()
{
}

// We organize observations in two ways.  The first way is indexed by the source
// address/port of the observed message.  The second way is a simple linked list
// where the first item in list is the first observation.
class TingObservationTree : public AddressableListTemplate<TingObservation> {};
class TingObservationList : public ProtoSimpleQueueTemplate<TingObservation> {};

class TingObserver : public AddressableItem
{
    public:
        TingObserver(const ProtoAddress& addr);
        ~TingObserver();
        
        bool Insert(const ProtoAddress&   reportAddr, 
                    UINT8                 roundId, 
                    const struct timeval  refTime, 
                    const struct timeval  obsTime, 
                    double                rtt);
        
        class Iterator : public TingObservationTree::Iterator
        {
            public:
                Iterator(TingObserver& observer, const ProtoAddress* matchAddr = NULL) 
                    : TingObservationTree::Iterator(observer.observation_tree, matchAddr) {}
                TingObservation* GetNextObservation()
                    {return TingObservationTree::Iterator::GetNextItem();}
        };  // end class TingObserver::Iterator
        
    private:
        TingObservationTree observation_tree;
        TingObservationList observation_list;
};  // end class TingObserver

class TingObserverList : public AddressableListTemplate<TingObserver> {};

typedef void (TingCallback)(const void* userData);

class TingSession : public AddressableItem
{
    public:
            
        enum State
        {
            IDLE        = 0,    // doing nothing, waiting for request
            HAILING     = 1,    // requestor, hail initiated with initial request transmission
            CONNECTED   = 2    // reply to hail received (or request received)
        };

        TingSession(const                   ProtoAddress& addr, 
                    UINT8                   roundId, 
                    ProtoSocket&            tingSocket, 
                    ProtoTimerMgr&          timerMgr,
                    const ProtoAddressList& localAddrList);
        ~TingSession();
        
        void SetLogFile(FILE* logFile)
            {log_file = logFile;}
        
        void SetTerminateCallback(TingCallback* funcPtr, const void* userData);
        
        const ProtoAddress& GetSessionAddress() const
            {return GetAddress();}
        
        UINT8 GetRoundId() const
            {return round_id;}
        
        bool IsUnicast() const
            {return !GetSessionAddress().IsMulticast();}
        bool IsMulticast() const
            {return GetSessionAddress().IsMulticast();}
        
        bool IsActive()
            {return (IDLE != session_state);}
        
        bool Initiate(TingCallback* termCallback, const void* termData);
        void Terminate()
        {
            if (session_timer.IsActive()) session_timer.Deactivate();
            Reset(round_id);
            if (NULL != term_callback) term_callback(term_data);
        }
        
        void HandleMessage(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime);
        void HandleHail(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime);
        void HandleReply(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime);
        void HandleUnicastRequest(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime);
        void HandleMulticastRequest(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime);
        bool RecordObservation(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime);
        
        
        bool SendMessage(TingMessage::Type msgType, double backoff);
        bool OnSessionTimeout(ProtoTimer& theTimer);
        void Summarize();
        void Reset(UINT8 roundId);
        
        static const double REPLY_TIME_MAX;
        enum {REQUEST_COUNT_MAX = 5};
        enum {REQUEST_ROUND_MAX = 3};
        enum {REPORT_COUNT_MAX = 10};
                    
    private:
        ProtoSocket&            ting_socket;
        ProtoTimerMgr&          timer_mgr;
        const ProtoAddressList& local_address_list;
        ProtoTimer              session_timer;
        State                   session_state;
        UINT8                   round_id;
        UINT8                   sent_round_id;
        struct timeval          sent_msg_time;
        unsigned int            sent_request_count;  // send attempts with reply
        unsigned int            request_round_count; // number of request rounds for unicast handshake
        TingObservationTree     observation_tree;    // indexed by observation srcAddr
        TingObservationList     observation_list;    // most recent observations at tail
        TingObserverList        observer_list;       // indexed by observer srcAddr
        FILE*                   log_file;
        TingCallback*           term_callback;
        const void*             term_data;
};  // end class TingSession

class TingSessionList : public AddressableListTemplate<TingSession> {};

class TingApp : public ProtoApp
{
    public:
        TingApp();
        ~TingApp();
        
        enum {MSG_LENGTH_MAX = 1400};
        
        static const char* DEFAULT_GROUP;
        static const UINT16 DEFAULT_PORT;
        
        enum {REQ_COUNT = 1};  // number of handshakes to perform

        bool OnStartup(int argc, const char*const* argv);
        
        bool ProcessCommands(int argc, const char*const* argv)
            {return true;}

        void OnShutdown();
        

    private:
            
        void Usage();
        
        void HandleMessage(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& currentTime);

        void OnTingSocketEvent(ProtoSocket&       theSocket, 
                               ProtoSocket::Event theEvent);
        
        static void DoTerminate(const void* termData);

        ProtoSocket         ting_socket;
        UINT16              ting_port;
        ProtoAddress        group_address;
        TingSession*        group_session; // our group session
        TingSessionList     session_list;  // list of unicast sessions, initiated or received   
        ProtoAddressList    local_address_list;
        UINT8               next_round_id;
        FILE*               log_file;
        char                mcast_iface[256];
        
          
}; // end class TingApp

////// IMPLEMENTATION /////////////////////

// Our application instance 
PROTO_INSTANTIATE_APP(TingApp) 
        

AddressableItem::AddressableItem(const ProtoAddress& theAddr)
 : item_addr(theAddr)
{
    unsigned int addrLen = theAddr.GetLength();
    memcpy(item_key, theAddr.GetRawHostAddress(), addrLen);
    UINT16 port = htons(theAddr.GetPort());
    memcpy(item_key + addrLen, &port, 2);
}

AddressableItem::~AddressableItem()
{
}

AddressableItem* AddressableList::Find(const ProtoAddress& addr) const
{
    char key[16+2];
    unsigned int addrLen = addr.GetLength();
    memcpy(key, addr.GetRawHostAddress(), addrLen);
    UINT16 port = htons(addr.GetPort());
    memcpy(key+addrLen, &port, 2);
    return static_cast<AddressableItem*>(ProtoSortedQueue::Find(key, (addrLen+2) << 3));
}  // end AddressableList::Find()

AddressableList::Iterator::Iterator(AddressableList& theList, const ProtoAddress* matchAddr)
 : ProtoSortedQueueTemplate<AddressableItem>::Iterator(theList)
{
   Reset(matchAddr);
}

void AddressableList::Iterator::Reset(const ProtoAddress* matchAddr)
{
    if (NULL != matchAddr)
   {
       match_addr = *matchAddr;
       char matchKey[16+2];
       unsigned int addrLen = matchAddr->GetLength();
       memcpy(matchKey, matchAddr->GetRawHostAddress(), addrLen);
       UINT16 port = htons(matchAddr->GetPort());
       memcpy(matchKey + addrLen, &port, 2);
       ProtoSortedQueue::Iterator::Reset(false, matchKey, (addrLen+2)<<3);
   }
   else
   {
       match_addr.Invalidate();
   }
}  // end AddressableList::Iterator::Reset()

AddressableItem* AddressableList::Iterator::GetNextItem()
{
    AddressableItem* item = ProtoSortedQueueTemplate<AddressableItem>::Iterator::GetNextItem();
    if (NULL == item) return NULL;
    if (match_addr.IsValid())
        if (item->GetAddress().IsEqual(match_addr))
            return item;
        else
            return NULL; // doesn't match
    else
        return item;
}  // end AddressableList::Iterator::GetNextItem()


TingObserver::TingObserver(const ProtoAddress& addr)
 : AddressableItem(addr)
{
}

TingObserver::~TingObserver()
{
    observation_tree.Empty();
    observation_list.Destroy();
}

bool TingObserver::Insert(const ProtoAddress&   reportAddr, 
                          UINT8                 roundId, 
                          const struct timeval  refTime, 
                          const struct timeval  obsTime, 
                          double                rtt)
{
    // Is this a duplicative or adjusted former observation
    TingObservationTree::Iterator iterator(observation_tree, &reportAddr);
    TingObservation* obs;
    while (NULL != (obs = iterator.GetNextItem()))
    {
        const struct timeval& t = obs->GetReferenceTime();
        if ((t.tv_sec == refTime.tv_sec) && (t.tv_usec == refTime.tv_usec))
        {
            char observerName[256];
            observerName[255] = '\0';
            GetAddress().GetHostString(observerName, 255);
            PLOG(PL_WARN, "TingObserver::Insert() warning: duplicate observation of %s/%hu from %s/%hu\n",
                        reportAddr.GetHostString(), reportAddr.GetPort(), observerName, GetAddress().GetPort());
            return true;
        }
    }
    if (NULL == (obs = new TingObservation(reportAddr)))
    {
        PLOG(PL_ERROR, "TingObserver::Insert() new TingObservation error: %s\n", GetErrorString());
        return false;
    }
    obs->SetRoundId(roundId);
    obs->SetReferenceTime(refTime);
    obs->SetObservedTime(obsTime);
    obs->SetRtt(rtt);
    if (!observation_tree.Insert(*obs))
    {
        PLOG(PL_ERROR, "TingObserver::Insert() error: observation_tree insertion failed!\n");
        delete obs;
        return false;
    }
    if (!observation_list.Append(*obs))
    {
        PLOG(PL_ERROR, "TingObserver::Insert() error: observation_tree insertion failed!\n");
        observation_tree.Remove(*obs);
        delete obs;
        return false;
    }
    return true;
}  // end TingObserver::Insert()

const double TingMessage::BACKOFF_MIN = 1.0e-06;
const double TingMessage::BACKOFF_MAX = 1000.0; 
                
TingMessage::TingMessage(UINT32*        bufferPtr, 
                         unsigned int   numBytes, 
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer)
    {
        ProtoPkt::InitFromBuffer(numBytes);
    }
    else if (NULL != bufferPtr)
    {
        if (numBytes >= GetMinimumLength())
            Init();
        else
            memset(buffer_ptr, 0, numBytes);  // invalid version zero
    }
}

TingMessage::~TingMessage()
{
}

bool TingMessage::AppendReport(const ProtoAddress&   srcAddr, 
                               UINT8                 roundId,
                               const struct timeval& refTime, 
                               const struct timeval& obsTime, 
                               double                rtt)
{
    // Is there room for this added report
    unsigned int reportLength = srcAddr.GetLength() + 6*4;
    unsigned int bufferSpace = buffer_bytes - GetLength();
    if (reportLength > bufferSpace) return false;
    Report report(buffer_ptr + (GetLength()/4), bufferSpace);
    report.SetRoundId(roundId);
    report.SetSrcAddress(srcAddr);
    report.SetReferenceTime(refTime);
    report.SetObservedTime(obsTime);
    report.SetRtt(rtt);
    SetLength(GetLength() + reportLength);
    return true;
    
}  // end TingMessage::AppendReport()

TingMessage::Report::Report(UINT32*        bufferPtr, 
                            unsigned int   numBytes, 
                            bool           initFromBuffer,
                            bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer)
        InitFromBuffer();
    else
        InitIntoBuffer();
}

TingMessage::Report::~Report()
{
}

void TingMessage::Report::InitIntoBuffer(UINT32*        bufferPtr, 
                                         unsigned int   numBytes, 
                                         bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (0 != buffer_bytes) SetReportWords(0);
}  // end TingMessage::Report::InitIntoBuffer()

bool TingMessage::Report::InitFromBuffer(UINT32*        bufferPtr, 
                                         unsigned int   numBytes, 
                                         bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (0 != buffer_bytes)
    {
        unsigned int reportLength = GetReportLength();
        if (reportLength <= buffer_bytes)
        {
            SetLength(reportLength);
            return true;
        }   
    }
    SetLength(0);
    return false;
}  // end TingMessage::Report::InitFromBuffer()

bool TingMessage::Report::SetSrcAddress(const ProtoAddress& srcAddr)
{
    SetUINT16(((UINT16*)buffer_ptr)+OFFSET_SRC_PORT, srcAddr.GetPort());
    switch (srcAddr.GetType())
    {
        case ProtoAddress::IPv4:
            SetReportWords(1 + 6);
            break;
        case ProtoAddress::IPv6:
            SetReportWords(4 + 6);
            break;
        default:
            PLOG(PL_ERROR, "TingMessage::Report::SetSrcAddress() error: unsupported address type\n");
            return false;
    }
    unsigned int addrLen = srcAddr.GetLength();
    unsigned int addrSpace = buffer_bytes - 6*4;
    if (addrLen <= addrSpace)
    {
        memcpy((char*)(buffer_ptr+OFFSET_SRC_ADDR), srcAddr.GetRawHostAddress(), addrLen);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "TingMessage::Report::SetSrcAddress() error: insufficient buffer space\n");
        SetReportWords(0);
        return false;
    }
}  // end TingMessage::Report::SetSrcAddress()

bool TingMessage::Report::GetSrcAddress(ProtoAddress& srcAddr) const
{
    // infer address type (IPv4 or IPv6) from address length
    switch (GetAddressLength())
    {
        case 4:
            srcAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)(buffer_ptr + OFFSET_SRC_ADDR), 4);
            break;
        case 16:
            srcAddr.SetRawHostAddress(ProtoAddress::IPv6, (char*)(buffer_ptr + OFFSET_SRC_ADDR), 16);
        default:
            PLOG(PL_ERROR, "TingMessage::Report::GetSrcAddress() error: invalid address length: %d\n", GetAddressLength());
            return false;
    }
    srcAddr.SetPort(GetUINT16(((UINT16*)buffer_ptr) + OFFSET_SRC_PORT));
    return true;
}  // end TingMessage::Report::GetSrcAddress()


const double TingSession::REPLY_TIME_MAX = 2.0;

TingSession::TingSession(const                   ProtoAddress& addr, 
                         UINT8                   roundId, 
                         ProtoSocket&            tingSocket, 
                         ProtoTimerMgr&          timerMgr,
                         const ProtoAddressList& localAddrList)
 : AddressableItem(addr), ting_socket(tingSocket), timer_mgr(timerMgr), local_address_list(localAddrList),
   session_state(IDLE), round_id(roundId), sent_round_id(0), sent_request_count(0), request_round_count(0),
   log_file(stdout), term_callback(NULL), term_data(NULL)
{
    sent_msg_time.tv_sec = sent_msg_time.tv_usec= 0;
    session_timer.SetListener(this, &TingSession::OnSessionTimeout);
}

TingSession::~TingSession()
{
    if (session_timer.IsActive()) session_timer.Deactivate();
}        


void TingSession::HandleHail(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime)
{
    // It's a unicast HAIL message, so send a REPLY
    ASSERT(IsUnicast() && msg.IsUnicast());  
    if (IDLE == session_state)
    {
        // Transition to CONNECTED state and activate session_timer
        Reset(round_id);
        session_state = CONNECTED;
        session_timer.SetInterval(REPLY_TIME_MAX);
        timer_mgr.ActivateTimer(session_timer);
        sent_request_count = REQUEST_COUNT_MAX;
    }
    else
    {
        ASSERT(session_timer.IsActive());
        session_timer.Reset();
    }
    // Note that "backoff" here doesn't really matter
    if (!SendMessage(TingMessage::REPLY, REPLY_TIME_MAX))
    {
        PLOG(PL_ERROR, "TingSession::HandleHail() error: unable to send REPLY message to host %s\n", srcAddr.GetHostString());
        session_timer.Deactivate();
        Reset(round_id);
    }
}  // end TingSession::HandleHail()

void TingSession::HandleReply(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime)
{
    ASSERT(IsUnicast() && msg.IsUnicast());  
    if (IDLE == session_state)
    {
        PLOG(PL_WARN, "TingSession::HandleReply() warning: session received unexpected REPLY from host %s/%hu\n", 
                      srcAddr.GetHostString(), srcAddr.GetPort());
        return;
    }
    if (msg.GetRoundId() != round_id)
    {
        PLOG(PL_WARN, "TingSession::HandleReply() warning: session received REPLY with non matching round_id from host %s/%hu\n",  
                      srcAddr.GetHostString(), srcAddr.GetPort());
        return;
    }
    
    RecordObservation(msg, srcAddr, recvTime);
    
    //  Send followup REQUEST if rounds remain
    if (++request_round_count <= REQUEST_ROUND_MAX)
    {
        sent_request_count = 0;
        session_timer.Reset();
        if (!SendMessage(TingMessage::REQUEST, REPLY_TIME_MAX))
        {
            PLOG(PL_ERROR, "TingSession::HandleReply() warning: unable to send REPLY to host %s/%hu\n",  
                           srcAddr.GetHostString(), srcAddr.GetPort());
            // we let the session retry / timeout on its own
        }
    }
    else
    {
        // TBD - We received the final reply, so we could finish up here instead of waiting for timeout   
        sent_request_count = REQUEST_COUNT_MAX;
        
    }
}  // end TingSession::HandleReply()

void TingSession::HandleUnicastRequest(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime)
{
    ASSERT(IsUnicast() && msg.IsUnicast());  
    if (CONNECTED != session_state)
    {
        PLOG(PL_WARN, "TingSession::HandleUnicastRequest() warning: session received unexpected REQUEST from host %s/%hu\n", 
                      srcAddr.GetHostString(), srcAddr.GetPort());
        return;
    }
    if (msg.GetRoundId() != round_id)
    {
        PLOG(PL_WARN, "TingSession::HandleUnicastRequest() warning: session received REQUEST with non matching round_id from host %s/%hu\n",  
                      srcAddr.GetHostString(), srcAddr.GetPort());
        return;
    }
    
    bool validFeedback = RecordObservation(msg, srcAddr, recvTime);
    
    // Respond accordingly (note REQUEST should always have "valid feedback")
    if (validFeedback)
    {
        if (SendMessage(TingMessage::REPLY, REPLY_TIME_MAX))
            session_timer.Reset();
        else
            PLOG(PL_WARN, "TingSession::HandleUnicastRequest() warning: unable to send  REPLY to host %s/%hu\n", 
                           srcAddr.GetHostString(), srcAddr.GetPort());
    }
    else
    {
        PLOG(PL_WARN, "TingSession::HandleUnicastRequest() warning: received REQUEST with no valid feedback from host %s/%hu\n",
                        srcAddr.GetHostString(), srcAddr.GetPort());
    }
}  // end TingSession::HandleUnicastRequest()

void TingSession::HandleMulticastRequest(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime)
{
    if (IDLE == session_state)
    {
        Reset(msg.GetRoundId());
    }
    else if (msg.GetRoundId() != round_id)
    {
        // Warn / ignore REQUEST with non-matching round id
        PLOG(PL_WARN, "TingSession::HandleMulticastRequest() warning: received REQUEST with non matching round_id from host %s/%hu\n",  
                      srcAddr.GetHostString(), srcAddr.GetPort());
        return;
    }
    
    RecordObservation(msg, srcAddr, recvTime);
    
    if (0 == sent_round_id)
    {
        // We haven't transmitted yet, so do a random backoff
        double backoff = ExponentialRand(REPLY_TIME_MAX, 10);
        session_timer.SetInterval(backoff);
        if (IDLE == session_state)
        {
            timer_mgr.ActivateTimer(session_timer);
            session_state = CONNECTED;
        }
        else
        {
            session_timer.Reset();
        }
    }   
    else 
    {
        session_timer.Reset();
    }
}  // end TingSession::HandleMulticastRequest()

bool TingSession::RecordObservation(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime)
{
    TingMessage::Type msgType = msg.GetType();
    // Any (or none) feedback is valid for multicast signaling.
         // For unicast, feedback MUST reflect prior REQUEST / REPLY round_id and sent_time 
     bool validFeedback = IsMulticast();
     // At this point, the message is valid for our given session state, etc so we can record an observation
     // (Note we validate the feedback reports that our for ourself)        
     // Record an observation for incoming message
    TingObservation* myObservation = new TingObservation(srcAddr);
    if (NULL == myObservation)
    {
        PLOG(PL_ERROR, "TingSession::RecordObservation() new TingObservation error: %s\n", GetErrorString());
        return validFeedback;
    }
    myObservation->SetRoundId(msg.GetRoundId());
    struct timeval sentTime;
    msg.GetSentTime(sentTime);
    myObservation->SetReferenceTime(sentTime); // use message sent time as reference time
    myObservation->SetObservedTime(recvTime);
    // rtt will be set below if this message has an observation of a message I previously sent.
    if (!observation_list.Append(*myObservation))
    {
        PLOG(PL_ERROR, "TingSession::RecordObservation() error: observation_list insertion failure!\n");
        delete myObservation;
        return validFeedback;
    }
    if (!observation_tree.Insert(*myObservation))
    {
        PLOG(PL_ERROR, "TingSession::RecordObservation() error: observation_tree insertion failure!\n");
        observation_list.Remove(*myObservation);
        delete myObservation;
        return validFeedback;
    }
    // If the REQUEST or REPLY contains any reports, record them
    TingObserver* observer = observer_list.Find(srcAddr);
    TingMessage::Report report;
    while (msg.GetNextReport(report))
    {
        ProtoAddress reportAddr;
        report.GetSrcAddress(reportAddr);
        if (!reportAddr.IsValid())
        {
            PLOG(PL_ERROR, "TingSession::RecordObservation() error: received message with invalid report address!\n");
            // TBD - remove this observation?
            return validFeedback;
        }
        struct timeval refTime;
        report.GetReferenceTime(refTime);
        struct timeval obsTime;
        report.GetObservedTime(obsTime);
        double reportRtt = report.GetRtt();
        // Is the report about me?
        // If so, compute RTT for ourselves and to report
        if (local_address_list.Contains(reportAddr))
        {
            // Calculate RTT and update "myObservation"
            // rtt = recvTime - report.GetReferenceTime() - (msg.GetSentTime() - report.GetObservedTime())
            double rtt = ProtoTime(recvTime) - ProtoTime(refTime);
            double holdTime = ProtoTime(sentTime) - ProtoTime(obsTime);
            rtt -= holdTime;
            if (IsUnicast())
            {   if (report.GetRoundId() != round_id)
                {
                    PLOG(PL_WARN, "TingSession::RecordObservation() warning: received unicast %s message from %s with invalid round id: %d\n",
                        (TingMessage::REQUEST == msgType) ? "REQUEST" : "REPLY", srcAddr.GetHostString(), report.GetRoundId());
                    continue;
                }
                if ((refTime.tv_sec != sent_msg_time.tv_sec) || (refTime.tv_usec != sent_msg_time.tv_usec))
                {
                    PLOG(PL_DEBUG, "TingSession::RecordObservation() warning: received unicast %s message from %s with invalid reference time\n",
                            (TingMessage::REQUEST == msgType) ? "REQUEST" : "REPLY", srcAddr.GetHostString());
                    continue;  // in case, for some reason there is a valid feedback report included, too
                } 
            }
            // TBD - if reportRtt < 0.0, should we use this as the "reportRtt", too???
            myObservation->SetRtt(rtt);
            validFeedback = true;
        }
        else 
        {
            if (IsUnicast())
            {
                PLOG(PL_WARN, "TingSession::RecordObservation() warning: received unicast report with non-matching report address\n");
                continue;
            }
        }
        if (report.GetRoundId() != round_id)
            PLOG(PL_WARN, "TingSession::RecordObservation() warning: received report with non-matching round_id\n");

        if (NULL == observer)
        {
            if (NULL == (observer = new TingObserver(srcAddr)))
            {
                PLOG(PL_ERROR, "TingSession::RecordObservation() new TingObserver error: %s\n", GetErrorString());
                break;
            }
            if (!observer_list.Insert(*observer))
            {
                PLOG(PL_ERROR, "TingSession::RecordObservation() error: observer_list insertion error!\n");
                delete observer;
                break;
            }
        }
        // TBD - make sure observer handles duplicate observations OK?
        if (!observer->Insert(reportAddr, report.GetRoundId(), refTime, obsTime, reportRtt))
        {
            PLOG(PL_ERROR, "TingSession::RecordObservation() error:observer observation insertion failure!\n");
            break;
        }
    }
    return validFeedback;
    
}  // end TingSession::RecordObservation()
    

bool TingSession::SendMessage(TingMessage::Type msgType, double backoff)
{
    const ProtoAddress& destAddr = GetSessionAddress();
    UINT32 buffer[TING_MSG_MAX/4 + 1];
    TingMessage msg(buffer, TING_MSG_MAX);
    msg.SetType(msgType);
    if (IsUnicast()) msg.SetFlag(TingMessage::UNICAST);
    msg.SetBackoff(backoff);
    msg.SetRoundId(GetRoundId());
    if (TingMessage::HAIL != msgType)
    {
        // Append a finite number of our observations as reports
        TingObservationList::Iterator observerator(observation_list);
        TingObservation* obs;
        unsigned int reportCount = 0;
        while (NULL != (obs = observerator.GetNextItem()))
        {
            // For unicast, we only send observations relevant to that destAddr
            // TBD - should we send only our most recent observation? Or most recent first?
            if (IsUnicast() && !obs->GetAddress().IsEqual(destAddr)) continue;
            if (!msg.AppendReport(obs->GetAddress(), obs->GetRoundId(), obs->GetReferenceTime(), obs->GetObservedTime(), obs->GetRtt()))
            {
                PLOG(PL_WARN, "TingApp::SendMessage() warning: full message!\n");
                break;
            }
            if (++reportCount >= REPORT_COUNT_MAX)
                break;
        }
    }
    
    // Last, but not least, get the current time, timestamp the message and send it
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    msg.SetSentTime(currentTime);
    unsigned int numBytes = msg.GetLength();
    if (!ting_socket.SendTo((const char*)msg.GetBuffer(), numBytes, destAddr))
    {
        PLOG(PL_ERROR, "TingApp::SendMessage() error: unable to send message!\n");
        return false;
    }
    sent_msg_time = currentTime;
    sent_round_id = msg.GetRoundId();
    return true;
}  // end TingSession::SendMessage()
   

bool TingSession::OnSessionTimeout(ProtoTimer& theTimer)
{
    // Send a ting message depending on session type and state
    ASSERT(IDLE != session_state);
    
    TingMessage::Type msgType = TingMessage::INVALID;
    if (sent_request_count < REQUEST_COUNT_MAX)
    {
        if (IsUnicast())
            msgType = (HAILING == session_state) ? TingMessage::HAIL : TingMessage::REQUEST;
        else if (0 == sent_round_id)
            msgType = TingMessage::REQUEST;
    }
    
    if (TingMessage::INVALID != msgType)
    {
        if (SendMessage(msgType, REPLY_TIME_MAX))
            sent_round_id = round_id;
        else
            PLOG(PL_ERROR, "TingSession::OnSessionTimeout() error: unable to send unicast message to host %s\n", GetSessionAddress().GetHostString());
        sent_request_count++;  
        theTimer.SetInterval(REPLY_TIME_MAX);
        theTimer.Reset();
    }
    else
    {
        Summarize();
        theTimer.Deactivate();
        Reset(round_id);
    }
    
    if (NULL != term_callback)
    {
        // We're an "initiator" so shutdown after summarization
        term_callback(term_data);
    }
        
    
    return false;  // we return false because the timer is either reset or deactivated above
}  // end TingSession::OnSessionTimeout()


void TingSession::Summarize()
{
    // Summarize my observations
    TingObservationTree::Iterator iterator(observation_tree);
    TingObservation* obs;
    
    ProtoAddress currentAddr;
    double offsetMin = 0.0;
    double offsetMax = 0.0;
    double offsetSum = 0.0;
    unsigned int offsetCount = 0;
    double rttMin = 0.0;
    double rttMax = 0.0;
    double rttSum = 0.0;
    unsigned int rttCount = 0;
    while (NULL != (obs = iterator.GetNextItem()))
    {
        // Skip local addresses
        if (local_address_list.Contains(obs->GetAddress())) continue;
        if (!currentAddr.IsValid()) currentAddr = obs->GetAddress();
        if (!currentAddr.IsEqual(obs->GetAddress()))
        {
            fprintf(log_file, "host %s/%hu ", currentAddr.GetHostString(), currentAddr.GetPort());
            if (offsetCount > 0)
            {
                double offsetAve = offsetSum / (double)offsetCount;
                fprintf(log_file, "offset: min>%f ave>%f max>%f ", offsetMin, offsetAve, offsetMax);
            }
            else
            {
                fprintf(log_file, "offset: undetermined ");
            }
            if (rttCount > 0)
            {
                double rttAve = rttSum / (double)rttCount;
                fprintf(log_file, "rtt: min>%f ave>%f max>%f\n", rttMin, rttAve, rttMax);
            }
            else
            {
                fprintf(log_file, "rtt: undetermined\n");
            }
            rttCount = offsetCount = 0;
            currentAddr = obs->GetAddress();
        }
        
        if (currentAddr.IsEqual(obs->GetAddress()))
        {
            double rtt = obs->GetRtt();
            if (rtt >= 0.0)
            {
                // update rtt tracking
                if (rttCount > 0)
                {
                    if (rtt < rttMin) 
                        rttMin = rtt;
                    else if (rtt > rttMax)
                        rttMax = rtt;
                    rttSum += rtt;
                }
                else
                {
                    rttMin = rttMax = rttSum = obs->GetRtt();
                }
                rttCount++;
                // update offset tracking
                double offset = ProtoTime(obs->GetObservedTime()) - ProtoTime(obs->GetReferenceTime()) - (rtt / 2.0);
                if (offsetCount > 0)
                {
                    if (offset < offsetMin)
                        offsetMin = offset;
                    else if (offset > offsetMax)
                        offsetMax = offset;
                    offsetSum += offset;
                }
                else
                {
                    offsetMin = offsetMax = offsetSum = offset;
                }
                offsetCount++;
            }
        }
    }
    if (currentAddr.IsValid())
    {
        fprintf(log_file, "host %s/%hu ", currentAddr.GetHostString(), currentAddr.GetPort());
        if (offsetCount > 0)
        {
            double offsetAve = offsetSum / (double)offsetCount;
            fprintf(log_file, "offset: min>%f ave>%f max>%f ", offsetMin, offsetAve, offsetMax);
        }
        else
        {
            fprintf(log_file, "offset: undetermined ");
        }
        if (rttCount > 0)
        {
            double rttAve = rttSum / (double)rttCount;
            fprintf(log_file, "rtt: min>%f ave>%f max>%f\n", rttMin, rttAve, rttMax);
        }
        else
        {
            fprintf(log_file, "rtt: undetermined\n");
        }
    }
    
    // Summarize based on mutual observations (i.e. multicast ting)
    TingObserverList::Iterator observerator(observer_list);
    TingObserver* observer;
    while (NULL != (observer = observerator.GetNextItem()))
    {
        const ProtoAddress& observerAddr = observer->GetAddress();
        // Skip local addresses
        if (local_address_list.Contains(observerAddr)) continue;
        
        //fprintf(log_file, "Summarizing observer %s/%hu ...\n", observerAddr.GetHostString(), observerAddr.GetPort());
        offsetCount = 0;
        // Find observations of common reference(s) and compute offset
        TingObserver::Iterator it1(*observer);
        TingObservation* obs1;
        while (NULL != (obs1 = it1.GetNextObservation()))
        {
            // Do we have a common observation? (by addr and refTime)
            const struct timeval& refTime1 = obs1->GetReferenceTime();
            TingObservationTree::Iterator it2(observation_tree, &obs1->GetAddress());
            TingObservation* obs2;
            while (NULL != (obs2 = it2.GetNextItem()))
            {
                ASSERT(obs2->GetAddress().IsEqual(obs1->GetAddress()));
                const struct timeval& refTime2 = obs2->GetReferenceTime();
                if ((refTime1.tv_sec != refTime2.tv_sec) || (refTime1.tv_usec != refTime2.tv_usec))
                    continue;
                // Common observation, so compute offset time
                // (offset = my observed time minus their observed time)
                double offset = ProtoTime(obs2->GetObservedTime()) - ProtoTime(obs1->GetObservedTime());
                if (offsetCount > 0)
                {
                    if (offset < offsetMin)
                        offsetMin = offset;
                    else if (offset > offsetMax)
                        offsetMax = offset;
                    offsetSum += offset;
                }
                else
                {
                    offsetMin = offsetMax = offsetSum = offset;
                }
                offsetCount++;
                break;
            }
        }
        fprintf(log_file, "host %s/%hu ", observerAddr.GetHostString(), observerAddr.GetPort());
        if (offsetCount > 0)
        {
            double offsetAve = offsetSum / (double)offsetCount;
            fprintf(log_file, "RBS offset: min>%f ave>%f max>%f ", offsetMin, offsetAve, offsetMax);
        }
        else
        {
            fprintf(log_file, "RBS offset: undetermined ");
        }
        // Have I been able to measure RTT to this observer?
        rttCount = 0;
        iterator.Reset(&observerAddr);
        while (NULL != (obs = iterator.GetNextItem()))
        {
            double rtt = obs->GetRtt();
            if (rtt >= 0.0)
            {
                // update rtt tracking
                if (rttCount > 0)
                {
                    if (rtt < rttMin) 
                        rttMin = rtt;
                    else if (rtt > rttMax)
                        rttMax = rtt;
                    rttSum += rtt;
                }
                else
                {
                    rttMin = rttMax = rttSum = obs->GetRtt();
                }
                rttCount++;
            }
        }
        if (rttCount > 0)
        {
            double rttAve = rttSum / (double)rttCount;
            fprintf(log_file, "rtt: min>%f ave>%f max>%f\n", rttMin, rttAve, rttMax);
        }
        else
        {
            fprintf(log_file, "rtt: undetermined\n");
        }
    }
}  // end TingSession::Summarize()

void TingSession::Reset(UINT8 roundId)
{
    session_state = IDLE;
    round_id = roundId;
    sent_round_id = 0;
    sent_msg_time.tv_sec = sent_msg_time.tv_usec = 0;
    sent_request_count = 0;
    request_round_count = 0;
    // TBD - carry some historical state into future rounds?
    observation_list.Destroy();
    observer_list.Destroy();
}


bool TingSession::Initiate(TingCallback* termCallback, const void* termData)
{
    if (IDLE != session_state)
    {
        PLOG(PL_ERROR, "TingSession::Initiate() error: session %s/%hu already active\n", 
                       GetSessionAddress().GetHostString(), GetSessionAddress().GetPort());
        return false;
    }
    Reset(round_id);
    bool result;
    if (IsUnicast())
    {
        session_state = HAILING;
        result = SendMessage(TingMessage::HAIL, REPLY_TIME_MAX);
    }
    else
    {
        session_state = CONNECTED;
        result = SendMessage(TingMessage::REQUEST, REPLY_TIME_MAX);
    }
    if (result)
    {
        session_timer.SetInterval(REPLY_TIME_MAX);
        timer_mgr.ActivateTimer(session_timer);
    }
    if (!result)
        PLOG(PL_ERROR, "TingSession::Initiate() error: unable to send message to %s/%hu\n", 
                       GetSessionAddress().GetHostString(), GetSessionAddress().GetPort());
    
    term_callback = termCallback;
    term_data = termData;
    
    return result;
}  // end TingSession::Initiate()


const char* TingApp::DEFAULT_GROUP = "225.5.2.2";
const UINT16 TingApp::DEFAULT_PORT = 5522;
        
TingApp::TingApp()
: ting_socket(ProtoSocket::UDP), ting_port(DEFAULT_PORT), next_round_id(1), log_file(stdout)
{    
    ting_socket.SetNotifier(&GetSocketNotifier());
    ting_socket.SetListener(this, &TingApp::OnTingSocketEvent);
    mcast_iface[0] = '\0';
}

TingApp::~TingApp()
{
    if (ting_socket.IsOpen()) ting_socket.Close();
    if (NULL != group_session)
    {
        delete group_session;
        group_session = NULL;
    }
    session_list.Destroy();
}

void TingApp::Usage()
{
    fprintf(stderr, "ting [{<host>[/<port>] | group}][interface <ifaceName>]\n"
                    "     [listen [<groupAddr>/]<port>][log <fileName>\n");
}  // end TingApp::Usage()

bool TingApp::OnStartup(int argc, const char*const* argv)
{   
    
    SetDebugLevel(8);
    
    // By default, listen on the default address/port
    group_address.ResolveFromString(DEFAULT_GROUP);
    group_address.SetPort(ting_port);
    
    // Note this apply to initiated sessions only
    bool mcastOnly = true;
    bool unicastOnly = true;
    
    ProtoAddressList destList;
    
    int i = 1;
    while (i < argc)
    {
        const char* cmd = argv[i++];
        size_t len = strlen(cmd);
        if (!strncmp("log", cmd, len))
        {
            const char* val = argv[i++];
            if (NULL == val)
            {
                fprintf(stderr, "ting error: missing 'log' argument !\n");
                Usage();
                return false;
            }
            if (stdout != log_file) fclose(log_file);
            if (0 == strcmp(val, "STDOUT"))
            {
                log_file = stdout;
            }
            else
            {
                if (NULL == (log_file = fopen(val, "w+")))
                {
                    fprintf(stderr, "ting log fopen(%s) error: %s\n", val, GetErrorString());
                    return false;
                }
            }
        }
        if (!strncmp("interface", cmd, len))
        {
            const char* val = argv[i++];
            if (NULL == val)
            {
                fprintf(stderr, "ting error: missing 'log' argument !\n");
                Usage();
                return false;
            }
            strncpy(mcast_iface, val, 255);
            mcast_iface[255] = '\0';
        }
        else if (!strncmp("listen", cmd, len))
        {
            // It's a listen command, so look for port number and open socket
            // (an optional group address may be specified as prefix to port)
            const char* val = argv[i++];
            if (NULL == val)
            {
                fprintf(stderr, "ting error: missing 'listen' argument !\n");
                Usage();
                return false;
            }
            char addrText[256];
            strncpy(addrText, val, 256);
            addrText[255] = '\0';
            char* addrPtr = addrText;
            char* portPtr = strchr(addrText, '/');
            if (NULL != portPtr)
            {
                *portPtr++ = '\0';
            }
            else
            {
                addrPtr = NULL;  // no group addr was provided.
                portPtr = addrText;   
            }
            
            UINT16 port;
            if (1 != sscanf(portPtr, "%hu", &port))
            {
                fprintf(stderr, "ting listen error: invalid port number \"%s\"\n", portPtr);
                Usage();
                return false;
            }
            ting_port = port;
            if (NULL != addrPtr)
            {
                ProtoAddress addr;
                if (!addr.ResolveFromString(addrPtr))
                {
                    fprintf(stderr, "ting error: invalid 'listen' address \"%s\"\n", addrPtr);
                    Usage();
                    return false;
                }
                if (!addr.IsMulticast())
                {
                    fprintf(stderr, "ting error: 'listen' address is not IP multicast!\n");
                    Usage();
                    return false;
                }
                group_address = addr;
                group_address.SetPort(port);
            }
            else
            {
                group_address.Invalidate();
            }
        }
        else if (!strncmp("group", cmd, len))
        {
            // Was a prior group address already specified?
            // (this will replace it, so remove it)
            ProtoAddressList::Iterator iterator(destList);
            ProtoAddress nextAddr;
            while (iterator.GetNextAddress(nextAddr))
            {
                if (!nextAddr.IsValid() || nextAddr.IsMulticast())
                {
                    destList.Remove(nextAddr);
                    break;
                }
            }
            if (!destList.Insert(group_address))
            {
                fprintf(stderr, "ting error: unable to add group destination!\n");
                Usage();
                return false;
            }
            unicastOnly = false;
        }
        else
        { 
            // Assume its a <host>[/<port>] to time check
            char addr[256];
            strncpy(addr, cmd, 256);
            addr[255] ='\0';
            char* portPtr = strchr(addr, '/');
            if (NULL != portPtr) *portPtr++ = '\0';
            ProtoAddress destAddr;
            if (!destAddr.ResolveFromString(addr))
            {
                PLOG(PL_ERROR, "ting error: unable to resolve destination host address \"%s\"\n", addr);
                Usage();
                return false;
            }
            destAddr.SetPort(DEFAULT_PORT);
            if (NULL != portPtr)
            {
                UINT16 port;
                if (1 != sscanf(portPtr, "%hu", &port))
                {
                    PLOG(PL_ERROR, "ting error: invalid destination port \"%s\"\n", portPtr);
                    Usage();
                    return false;
                }  
                destAddr.SetPort(port);              
            }
            if (destAddr.IsMulticast())
            {
                group_address = destAddr;
                // Was a prior group address already specified?
                // (this will replace it, so remove it)
                ProtoAddressList::Iterator iterator(destList);
                ProtoAddress nextAddr;
                while (iterator.GetNextAddress(nextAddr))
                {
                    if (!nextAddr.IsValid() || nextAddr.IsMulticast())
                    {
                        destList.Remove(nextAddr);
                        break;
                    }
                }
                unicastOnly = false;
            }
            else
            {
                mcastOnly = false;
            }
            if (!destList.Insert(destAddr))
            {
                fprintf(stderr, "ting error: unable to add destination!\n");
                Usage();
                return false;
            }
        }
    }
    
    const char* ifaceName = ('\0' != mcast_iface[0]) ? mcast_iface : NULL;
    
    // Put all local unicast addrs in list
    if (NULL != ifaceName)
    {
        if (!ProtoNet::GetInterfaceAddressList(ifaceName, ProtoAddress::IPv4, local_address_list))
            PLOG(PL_WARN, "NormSession::Open() warning: incomplete IPv4 interface address list\n");
        if (!ProtoNet::GetInterfaceAddressList(ifaceName, ProtoAddress::IPv6, local_address_list))
            PLOG(PL_WARN, "NormSession::Open() warning: incomplete IPv6 interface address list\n");   
    }
    else
    {
        // Get all interface addresses
        if (!ProtoNet::GetHostAddressList(ProtoAddress::IPv4, local_address_list))
            PLOG(PL_WARN, "NormSession::Open() warning: incomplete IPv4 host address list\n");
        if (!ProtoNet::GetHostAddressList(ProtoAddress::IPv6, local_address_list))
            PLOG(PL_WARN, "NormSession::Open() warning: incomplete IPv6 host address list\n");
    }
    
    ProtoAddressList::Iterator it(local_address_list);
    ProtoAddress localAddr;
    while (it.GetNextAddress(localAddr))
        TRACE("ting: found local address %s\n", localAddr.GetHostString());
    
    // Open socket and bind to "ting_port"
    // By default, "listen" on the default port if no command given
    if (!(ting_socket.Open(0, ProtoAddress::IPv4, false)))
    {
        PLOG(PL_ERROR, "ting error: unable to open ting_socket!\n");
        return false;
    }
    if (mcastOnly)
    {
        ting_socket.SetReuse(true); // let's use separate ting to "initiate" checks
        ting_socket.SetLoopback(true);
    }
    if (!(ting_socket.Bind(ting_port)))
    {
        // Bind to random socket since as this may be a unicast "initiator"
        if (!unicastOnly || !ting_socket.Bind(0))
        {
            PLOG(PL_ERROR, "ting error: unable to bind ting_socket!\n");
            return false;
        }
    }
    
    if (group_address.IsValid())
    {
        group_address.SetPort(ting_port);
        if (NULL == (group_session = new TingSession(group_address, next_round_id, ting_socket, GetTimerMgr(), local_address_list)))
        {
            PLOG(PL_ERROR, "TingApp::OnStartup() error: new group TingSession error: %s\n", GetErrorString());
            return false;
        }
        group_session->SetLogFile(log_file);
        
        if (!ting_socket.JoinGroup(group_address, ifaceName))
        {
            PLOG(PL_ERROR, "TingApp::OnStartup() error: unable to join group %s\n", group_address.GetHostString());
            delete group_session;
            group_session = NULL;
            return false;
        }   
        if (NULL != ifaceName) ting_socket.SetMulticastInterface(ifaceName);
        if (0 == ++next_round_id) next_round_id = 1;
        if (NULL != ifaceName)
            TRACE("ting: listening on group/port %s/%hu interface %s\n", group_address.GetHostString(), ting_port, ifaceName);
        else
            TRACE("ting: listening on group/port %s/%hu\n", group_address.GetHostString(), ting_port);    
    }
    else
    {
        TRACE("ting: listening on port %hu\n", ting_port);
    }
    
    // Iterate through the "destList" and initiate "ting" to each
    ProtoAddress destAddr;
    ProtoAddressList::Iterator iterator(destList);
    while (iterator.GetNextAddress(destAddr))
    {
        TingSession* session;
        if (destAddr.IsMulticast())
        {
            ASSERT(NULL != group_session);
            ASSERT(destAddr.IsEqual(group_session->GetAddress()));
            session = group_session;
        }
        else
        {
            session = session_list.Find(destAddr);
            if (NULL != session)
            {
                fprintf(stderr, "ting warning: duplicate destination %s/%hu\n", destAddr.GetHostString(), destAddr.GetPort());
                continue;
            }
            else if (NULL == (session = new TingSession(destAddr, next_round_id, ting_socket, GetTimerMgr(), local_address_list)))
            {
                fprintf(stderr, "ting new unicast TingSession error: %s\n", GetErrorString());
                return false;
            }
            else
            {   
                session->SetLogFile(log_file);
                if (!session_list.Insert(*session))
                {
                    fprintf(stderr, "ting error: session_list insertion failure!\n");
                    delete session;
                    return false;
                }
                if (0 == ++next_round_id) next_round_id = 1;
            }
        }
        if (!iterator.PeekNextAddress(destAddr))
            session->Initiate(DoTerminate, this);
    }
    return true;
}  // end TingApp::OnStartup()

void TingApp::OnShutdown()
{
    session_list.Destroy();
    if (NULL != group_session)
    {
        delete group_session;
        group_session = NULL;
    }
    if (ting_socket.IsOpen()) ting_socket.Close();
    TRACE("ting: Done.\n");
}  // end TingApp::OnShutdown()

void TingApp::DoTerminate(const void* termData)
{
    TingApp* theApp = (TingApp*)(termData);
    theApp->Stop();
}  // end TingApp::DoTerminate()

void TingApp::OnTingSocketEvent(ProtoSocket&       theSocket, 
                                ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            // Receive ting message and process according to type / state
            for (;;)
            {
                struct timeval currentTime;
                ProtoSystemTime(currentTime);
                ProtoAddress srcAddr;
                UINT32 buffer[MSG_LENGTH_MAX];
                unsigned int len = MSG_LENGTH_MAX;
                if (!theSocket.RecvFrom((char*)buffer, len, srcAddr))
                {
                    PLOG(PL_ERROR, "TingApp::OnUdpSocketEvent() error receiving!\n");
                    break;
                }
                if (0 == len) break;  // nothing more to read
                //TRACE("recvd %d byte message from %s/%hu\n", len, srcAddr.GetHostString(), srcAddr.GetPort());
                TingMessage msg(buffer, len, true);
                switch (msg.GetType())
                {
                    case TingMessage::HAIL:
                    case TingMessage::REQUEST:
                    case TingMessage::REPLY:
                        HandleMessage(msg, srcAddr, currentTime);
                        break;
                    default:
                        PLOG(PL_WARN, "TingApp::OnUdpSocketEvent() warning: received invalid message type!\n");
                        break;
                }
            }
            break; 
        }
        case ProtoSocket::EXCEPTION:
        case ProtoSocket::ERROR_:
        default:
            TRACE("Unhandled event\n");
            break;
    }
}  // end TingApp::OnTingSocketEvent()

void TingApp::HandleMessage(const TingMessage& msg, const ProtoAddress& srcAddr, const struct timeval& recvTime)
{
    TRACE("TingApp::HandleMessage() from %s/%hu ...\n", srcAddr.GetHostString(), srcAddr.GetPort());
    // Make sure the message round id is valid 
    if (0 == msg.GetRoundId())
    {
        PLOG(PL_WARN, "TingApp::HandleMessage() warning: received messsage with null round id from host %s\n",
                srcAddr.GetHostString());
        return;
    }
    if (msg.IsUnicast())
    {
        // Do we have an existing session for this unicast addr/port?
        TingSession* session = session_list.Find(srcAddr);
        if (NULL == session)
        {
            // It's possibly a new session, MUST be a HAIL message
            if (TingMessage::HAIL == msg.GetType())
            {
                if (NULL == (session = new TingSession(srcAddr, msg.GetRoundId(), ting_socket, GetTimerMgr(), local_address_list)))
                {
                    PLOG(PL_ERROR, "TingApp::HandleMessage() new TingSession error: %s\n", GetErrorString());
                    return;
                }
                session->SetLogFile(log_file);
                if (!session_list.Insert(*session))
                {
                    PLOG(PL_ERROR, "TingApp::HandleMessage() error: session_list insertion failure!\n");
                    delete session;
                    return;
                }
            }
            else
            {
                PLOG(PL_WARN, "TingApp::HandleMessage() warning: received unexpected unicast %s message from host %s\n",
                            (TingMessage::REQUEST == msg.GetType()) ? "REQUEST" : "REPLY", srcAddr.GetHostString());
                return;
            }
        }
        
        switch (msg.GetType())
        {
            case TingMessage::HAIL:
                session->HandleHail(msg, srcAddr, recvTime);
                break;
            case TingMessage::REQUEST:
                session->HandleUnicastRequest(msg, srcAddr, recvTime);
                break;
            case TingMessage::REPLY:
                session->HandleReply(msg, srcAddr, recvTime);
                break;
            default:
                break;
        }       
    }
    else
    {
        if (NULL == group_session)
        {
            PLOG(PL_WARN, "TingApp::HandleMessage() warning: received unexpected multicast message from host %s/%hu\n",
                           srcAddr.GetHostString(), srcAddr.GetPort());
            return;
        }
        if (TingMessage::REQUEST == msg.GetType())
        {
            group_session->HandleMulticastRequest(msg, srcAddr, recvTime);
        }
        else
        {
            PLOG(PL_WARN, "TingApp::HandleMessage() warning: received unexpected multicast %s message from host %s/%hu\n",
                            (TingMessage::HAIL == msg.GetType()) ? "HAIL" : "REPLY", srcAddr.GetHostString(), srcAddr.GetPort());
            return;
        }
    }
}  // end TingApp::HandleMessage()  


// valid for backoff = 1.0e-06 to 1.0e+03 seconds
UINT8 TingMessage::QuantizeBackoff(double backoff)
{
    if (backoff > BACKOFF_MAX)
        backoff = BACKOFF_MAX;
    else if (backoff < BACKOFF_MIN)
        backoff = BACKOFF_MIN;
    if (backoff < 3.3e-05) 
        return ((UINT8)((backoff/BACKOFF_MIN)) - 1);
    else
        return ((UINT8)(ceil(255.0 - (13.0*log(BACKOFF_MAX/backoff)))));
}  // end NormQuantizebackoff()

////////////////////////////////////////////////
//  inline double TingMessage::UnquantizeBackoff(UINT8 qrtt)
//  {
//      return ((qrtt < 31) ? 
//              (((double)(qrtt+1))*(double)BACKOFF_MIN) :
//              (BACKOFF_MAX/exp(((double)(255-qrtt))/(double)13.0)));
//  }  // end  TingMessage::UnquantizeBackoff(

// The lookup table here was generated from the QuantizeBackoff() method
const double TingMessage::BACKOFF[256] = 
{
    1.000e-06, 2.000e-06, 3.000e-06, 4.000e-06, 
    5.000e-06, 6.000e-06, 7.000e-06, 8.000e-06, 
    9.000e-06, 1.000e-05, 1.100e-05, 1.200e-05, 
    1.300e-05, 1.400e-05, 1.500e-05, 1.600e-05, 
    1.700e-05, 1.800e-05, 1.900e-05, 2.000e-05, 
    2.100e-05, 2.200e-05, 2.300e-05, 2.400e-05, 
    2.500e-05, 2.600e-05, 2.700e-05, 2.800e-05, 
    2.900e-05, 3.000e-05, 3.100e-05, 3.287e-05, 
    3.550e-05, 3.833e-05, 4.140e-05, 4.471e-05, 
    4.828e-05, 5.215e-05, 5.631e-05, 6.082e-05, 
    6.568e-05, 7.093e-05, 7.660e-05, 8.273e-05, 
    8.934e-05, 9.649e-05, 1.042e-04, 1.125e-04, 
    1.215e-04, 1.313e-04, 1.417e-04, 1.531e-04, 
    1.653e-04, 1.785e-04, 1.928e-04, 2.082e-04, 
    2.249e-04, 2.429e-04, 2.623e-04, 2.833e-04, 
    3.059e-04, 3.304e-04, 3.568e-04, 3.853e-04, 
    4.161e-04, 4.494e-04, 4.853e-04, 5.241e-04, 
    5.660e-04, 6.113e-04, 6.602e-04, 7.130e-04, 
    7.700e-04, 8.315e-04, 8.980e-04, 9.698e-04, 
    1.047e-03, 1.131e-03, 1.222e-03, 1.319e-03, 
    1.425e-03, 1.539e-03, 1.662e-03, 1.795e-03, 
    1.938e-03, 2.093e-03, 2.260e-03, 2.441e-03, 
    2.636e-03, 2.847e-03, 3.075e-03, 3.321e-03, 
    3.586e-03, 3.873e-03, 4.182e-03, 4.517e-03, 
    4.878e-03, 5.268e-03, 5.689e-03, 6.144e-03, 
    6.635e-03, 7.166e-03, 7.739e-03, 8.358e-03, 
    9.026e-03, 9.748e-03, 1.053e-02, 1.137e-02, 
    1.228e-02, 1.326e-02, 1.432e-02, 1.547e-02, 
    1.670e-02, 1.804e-02, 1.948e-02, 2.104e-02, 
    2.272e-02, 2.454e-02, 2.650e-02, 2.862e-02, 
    3.090e-02, 3.338e-02, 3.604e-02, 3.893e-02, 
    4.204e-02, 4.540e-02, 4.903e-02, 5.295e-02, 
    5.718e-02, 6.176e-02, 6.669e-02, 7.203e-02, 
    7.779e-02, 8.401e-02, 9.072e-02, 9.798e-02, 
    1.058e-01, 1.143e-01, 1.234e-01, 1.333e-01, 
    1.439e-01, 1.554e-01, 1.679e-01, 1.813e-01, 
    1.958e-01, 2.114e-01, 2.284e-01, 2.466e-01, 
    2.663e-01, 2.876e-01, 3.106e-01, 3.355e-01, 
    3.623e-01, 3.913e-01, 4.225e-01, 4.563e-01, 
    4.928e-01, 5.322e-01, 5.748e-01, 6.207e-01, 
    6.704e-01, 7.240e-01, 7.819e-01, 8.444e-01, 
    9.119e-01, 9.848e-01, 1.064e+00, 1.149e+00, 
    1.240e+00, 1.340e+00, 1.447e+00, 1.562e+00, 
    1.687e+00, 1.822e+00, 1.968e+00, 2.125e+00, 
    2.295e+00, 2.479e+00, 2.677e+00, 2.891e+00, 
    3.122e+00, 3.372e+00, 3.641e+00, 3.933e+00, 
    4.247e+00, 4.587e+00, 4.953e+00, 5.349e+00, 
    5.777e+00, 6.239e+00, 6.738e+00, 7.277e+00, 
    7.859e+00, 8.487e+00, 9.166e+00, 9.898e+00, 
    1.069e+01, 1.154e+01, 1.247e+01, 1.346e+01, 
    1.454e+01, 1.570e+01, 1.696e+01, 1.832e+01, 
    1.978e+01, 2.136e+01, 2.307e+01, 2.491e+01, 
    2.691e+01, 2.906e+01, 3.138e+01, 3.389e+01, 
    3.660e+01, 3.953e+01, 4.269e+01, 4.610e+01, 
    4.979e+01, 5.377e+01, 5.807e+01, 6.271e+01, 
    6.772e+01, 7.314e+01, 7.899e+01, 8.530e+01, 
    9.212e+01, 9.949e+01, 1.074e+02, 1.160e+02, 
    1.253e+02, 1.353e+02, 1.462e+02, 1.578e+02, 
    1.705e+02, 1.841e+02, 1.988e+02, 2.147e+02, 
    2.319e+02, 2.504e+02, 2.704e+02, 2.921e+02, 
    3.154e+02, 3.406e+02, 3.679e+02, 3.973e+02, 
    4.291e+02, 4.634e+02, 5.004e+02, 5.404e+02, 
    5.836e+02, 6.303e+02, 6.807e+02, 7.351e+02, 
    7.939e+02, 8.574e+02, 9.260e+02, 1.000e+03
};  // TingMessage::BACKOFF[]

