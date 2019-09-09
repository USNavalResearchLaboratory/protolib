#ifndef _PROTO_NOTIFY
#define _PROTO_NOTIFY

// This defines a base class used for Protolib classes
// that support or use asynchronous I/O notifications
class ProtoNotify
{
    public:
        ProtoNotify() : notify_flags(0) {}
    
        enum NotifyFlag 
        {
            NOTIFY_NONE        = 0x00, 
            NOTIFY_INPUT       = 0x01, 
            NOTIFY_OUTPUT      = 0x02, 
            NOTIFY_EXCEPTION   = 0x04,
            NOTIFY_ERROR       = 0x08
        };
        bool NotifyFlagIsSet(NotifyFlag theFlag) const 
            {return (0 != (notify_flags & theFlag));}
        void SetNotifyFlag(NotifyFlag theFlag) 
            {notify_flags |= theFlag;}
        void UnsetNotifyFlag(NotifyFlag theFlag) 
            {notify_flags &= ~theFlag;}
        void SetNotifyFlags(int theFlags) 
            {notify_flags = theFlags;}
        bool HasNotifyFlags() const 
            {return (0 != notify_flags);}
        int GetNotifyFlags() const 
            {return notify_flags;}
        void ClearNotifyFlags() 
            {notify_flags = 0;}   
        
    private:
        int notify_flags;
                       
};  // end class ProtoNotify


#endif // _PROTO_NOTIFY
