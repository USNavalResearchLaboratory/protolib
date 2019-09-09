#ifndef _PROTO_CHANNEL
#define _PROTO_CHANNEL
/**
 * @class ProtoChannel 
 *
 * @brief This class serves as a base class for Protokit classes
 * which require asynchronous I/O.
 * 
 * This uses a sort of "signal/slot" design pattern
 * for managing a single listener.
 *
 * On Unix, a file descriptor serves as the "NotifyHandle"
 * On Win32, events and overlapping I/O are used
 *
 * NOTE: This is a work-in-progress !!!
 *       The intent of this class will be to serve as a _base_
 *       class for any class requiring asynchronous I/O
 *       notification and event dispatch (eventually including ProtoSockets?)
 *       This will help extend & simplify the utility of the
 *       ProtoDispatcher class
 */

#include "protoDefs.h"
#include "protoNotify.h"

class ProtoChannel : public ProtoNotify
{
    public:
        virtual ~ProtoChannel();
    
#ifdef WIN32
        typedef HANDLE Handle;  // WIN32 uses "HANDLE" type for descriptors
#else
        typedef int Handle;     // UNIX uses "int" type for descriptors
#endif // if/else WIN32
        static const Handle INVALID_HANDLE;
        
        typedef NotifyFlag Notification;  // TBD - deprecate this
        
        // Derived classes MUST _end_ their own "Open()" method
        // with a call to this
        bool Open() 
        {
            StartInputNotification();  // enable input notifications by default
            return UpdateNotification();
        }
        // Derived classes MUST _begin_ their own "Close()" method
        // with a call to this
        void Close() ;
        
        // (TBD) Should this be made virtual???
        bool IsOpen() const
        {
#ifdef WIN32
            return ((INVALID_HANDLE != input_handle) ||
                    (INVALID_HANDLE != output_handle));
#else
            return (INVALID_HANDLE != descriptor);
#endif // if/else WIN32/UNIX    
        }
        /**
         * @class Notifier
         *
         */
        class Notifier
        {
            public:
                virtual ~Notifier() {}
                // This should usually be overridden
                virtual bool UpdateChannelNotification(ProtoChannel& theChannel, 
                                                       int           notifyFlags) 
                {
                    return true;
                } 
        };
        Notifier* GetNotifier() const {return notifier;}
        bool SetNotifier(ProtoChannel::Notifier* theNotifier);
        bool SetBlocking(bool status);
        
        bool StartInputNotification();
        void StopInputNotification();
        bool InputNotification() 
            {return NotifyFlagIsSet(NOTIFY_INPUT);}      

        bool StartOutputNotification();
        void StopOutputNotification();
         bool OutputNotification() 
            {return NotifyFlagIsSet(NOTIFY_OUTPUT);} 

        bool UpdateNotification();
        
        void OnNotify(NotifyFlag theFlag)
        {
            if (listener) listener->on_event(*this, theFlag);   
        }
        
        
#ifdef WIN32
        Handle GetInputEventHandle() {return input_event_handle;}
        Handle GetOutputEventHandle() {return output_event_handle;}
        bool IsOutputReady() {return output_ready;}
        bool IsInputReady() {return input_ready;}
        bool IsReady() {return (input_ready || output_ready);}
#else
        Handle GetInputEventHandle() const {return descriptor;}
        Handle GetOutputEventHandle() const {return descriptor;}
#endif  // if/else WIN32/UNIX
        
        // NOTE: For VC++ 6.x Debug builds "/ZI" or "/Z7" compile options must NOT be specified
        // (or else VC++ 6.x experiences an "internal compiler error")
        template <class listenerType>
        bool SetListener(listenerType* theListener, void(listenerType::*eventHandler)(ProtoChannel&, NotifyFlag))
        {
            bool doUpdate = ((NULL != theListener) || (NULL != listener));
            if (NULL != listener) delete listener;
            listener = (NULL != theListener) ? new LISTENER_TYPE<listenerType>(theListener, eventHandler) : NULL;
            bool result = (NULL != theListener) ? (NULL != listener) : true;
            return result ? (doUpdate ? UpdateNotification() : true) : false;
        }        
        bool HasListener() {return (NULL != listener);}
            
    protected:
        ProtoChannel();
        
    private:
        /**
         * @class Listener
         *
         */

        class Listener
        {
            public:
                virtual ~Listener() {}
                virtual void on_event(ProtoChannel& theChannel, NotifyFlag theFlag) = 0;
                virtual Listener* duplicate() = 0;
        };
        template <class listenerType>
        class LISTENER_TYPE : public Listener
        {
            public:
                LISTENER_TYPE(listenerType* theListener, 
                              void(listenerType::*eventHandler)(ProtoChannel&, NotifyFlag))
                    : listener(theListener), event_handler(eventHandler) {}
                void on_event(ProtoChannel& theChannel, NotifyFlag theFlag)
                    {(listener->*event_handler)(theChannel, theFlag);}
                Listener* duplicate()
                    {return (static_cast<Listener*>(new LISTENER_TYPE<listenerType>(listener, event_handler)));}
            private:
                listenerType* listener;
                void(listenerType::*event_handler)(ProtoChannel&, NotifyFlag);
        };

    protected: 
#ifdef WIN32
        // On WIN32, the input/output handles may be different than the 
        // event handles (but they can be set the same by child classes
        // if applicable.  (ProtoDispatcher uses the event handles while
        // read/write calls use the regular handles
		HANDLE                  input_handle;       // handle used for ReadFile
        HANDLE                  input_event_handle; // event handle for notification
        HANDLE                  output_handle;
        HANDLE                  output_event_handle;
        bool                    input_ready;        // input_ready helps us get level-triggered nofify behaviors
        bool                    output_ready;       // output_ready helps us get level-triggered nofify behaviors
		// The followng stuff facilitates using Win32 overlapped I/O in ProtoChannel subclasses
		bool InitOverlappedIO();  // must be called before overlapped i/o routines can be used
		bool StartOverlappedRead();  // use to "kickstart" overlapped I/O aync notifications
		bool OverlappedRead(char* buffer, unsigned int& numBytes);
		bool OverlappedWrite(const char* buffer, unsigned int& numBytes);
		enum {OVERLAPPED_BUFFER_SIZE = 8192};
		OVERLAPPED				overlapped_read;
		char*					overlapped_read_buffer;
		unsigned int			overlapped_read_count;
		unsigned int			overlapped_read_index;
		OVERLAPPED				overlapped_write;
		char*					overlapped_write_buffer;
		unsigned int			overlapped_write_count;
		unsigned int			overlapped_write_index;
#else
        int                     descriptor;
#endif // WIN32        
        
    private:    
#ifdef UNIX        
        bool                    blocking_status; 
#endif // UNIX
        Listener*               listener; 
        Notifier*               notifier;   
        
};  // end class ProtoChannel

#endif // PROTO_CHANNEL
