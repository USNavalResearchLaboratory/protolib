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

class ProtoChannel
{
    public:
        virtual ~ProtoChannel();
    
#ifdef WIN32
        typedef HANDLE Handle;  // WIN32 uses "HANDLE" type for descriptors
#else
        typedef int Handle;     // UNIX uses "int" type for descriptors
#endif // if/else WIN32
        static const Handle INVALID_HANDLE;
        
        // Derived classes should _end_ their own "Open()" method
        // with a call to this
        bool Open() 
        {
            StartInputNotification();  // enable input notifications by default
            return UpdateNotification();
        }
        // Derived classes should _begin_ their own "Close()" method
        // with a call to this
        void Close() 
        {
            if (IsOpen())
            {
                StopInputNotification();
                StopOutputNotification();    
            }
        }
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
        
        // Asynchronous I/O notification stuff
        enum Notification
        {
            NOTIFY_NONE     = 0x00,
            NOTIFY_INPUT    = 0x01,
            NOTIFY_OUTPUT   = 0x02
        };
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
        
        bool StartOutputNotification()
        {
            notify_flags |= (int)NOTIFY_OUTPUT;
            bool result = UpdateNotification();
#ifdef WIN32
            output_ready = result;
#endif // WIN32
            return result;   
        }
        void StopOutputNotification()
        {
            notify_flags &= ~((int)NOTIFY_OUTPUT);
            if (notifier) notifier->UpdateChannelNotification(*this, notify_flags);
        }        
        bool OutputNotification() {return (0 != (notify_flags & ((int)NOTIFY_OUTPUT)));}
        
        bool StartInputNotification()
        {
            notify_flags |= (int)NOTIFY_INPUT;
            bool result = UpdateNotification();
            return result;
        }     
        void StopInputNotification()
        {
            notify_flags &= ~((int)NOTIFY_INPUT);
            if (notifier) notifier->UpdateChannelNotification(*this, notify_flags);
        }   
        bool InputNotification() {return (0 != (notify_flags & ((int)NOTIFY_INPUT)));}        
                
        bool UpdateNotification();
        
        void OnNotify(ProtoChannel::Notification theNotification)
        {
            if (listener) listener->on_event(*this, theNotification);   
        }
        
        
#ifdef WIN32
        Handle GetInputHandle() {return input_handle;}
        Handle GetOutputHandle() {return output_handle;}
        bool IsOutputReady() {return output_ready;}
        bool IsInputReady() {return input_ready;}
        bool IsReady() {return (input_ready || output_ready);}
#else
        Handle GetInputHandle() {return descriptor;}
        Handle GetOutputHandle() {return descriptor;}
        Handle GetHandle() {return descriptor;}
#endif  // if/else WIN32/UNIX
        
        // NOTE: For VC++ 6.x Debug builds "/ZI" or "/Z7" compile options must NOT be specified
        // (or else VC++ 6.x experiences an "internal compiler error")
        template <class listenerType>
        bool SetListener(listenerType* theListener, void(listenerType::*eventHandler)(ProtoChannel&, Notification))
        {
            bool doUpdate = ((NULL == listener) && (NULL != theListener)) || 
                            ((NULL == theListener) && (NULL != listener));
            if (listener) delete listener;
            listener = theListener ? new LISTENER_TYPE<listenerType>(theListener, eventHandler) : NULL;
            bool result = theListener ? (NULL != theListener) : true;
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
                virtual void on_event(ProtoChannel& theChannel, Notification theNotification) = 0;
                virtual Listener* duplicate() = 0;
        };
        template <class listenerType>
        class LISTENER_TYPE : public Listener
        {
            public:
                LISTENER_TYPE(listenerType* theListener, 
                              void(listenerType::*eventHandler)(ProtoChannel&, Notification))
                    : listener(theListener), event_handler(eventHandler) {}
                void on_event(ProtoChannel& theChannel, Notification theNotification)
                    {(listener->*event_handler)(theChannel, theNotification);}
                Listener* duplicate()
                    {return (static_cast<Listener*>(new LISTENER_TYPE<listenerType>(listener, event_handler)));}
            private:
                listenerType* listener;
                void(listenerType::*event_handler)(ProtoChannel&, Notification);
        };
        
        Listener*               listener; 
        Notifier*               notifier;   
        int                     notify_flags; 
        
        bool                    blocking_status; 
          
    protected: 
#ifdef WIN32
		// ljt rework this!!
		void SetInputHandle(Handle theInputHandle) 
            {input_handle = theInputHandle;}
        HANDLE                  input_handle;
        bool                    input_ready;
        HANDLE                  output_handle;
        bool                    output_ready;
#else
        int                     descriptor;
#endif // WIN32
        
};  // end class ProtoChannel

#endif // PROTO_CHANNEL
