#ifndef _PROTO_EVENT
#define _PROTO_EVENT

#include "protoDefs.h"
#include "protoNotify.h"

// Manual reset event
class ProtoEvent : ProtoNotify
{
    public:
        ProtoEvent(bool autoReset=true);
        ~ProtoEvent();
        
        bool Open();
        bool Set();
        bool Reset();
        bool Wait();
        void Close();
        
        void SetAutoReset(bool state)
            {auto_reset = state;}
        bool GetAutoReset() const
            {return auto_reset;}
        
        bool IsOpen() const;
        
        class Notifier
        {
            public:
                virtual ~Notifier() {}
                virtual bool UpdateEventNotification(ProtoEvent& theEvent, 
                                                     int         notifyFlags) {return true;}
        };
        bool SetNotifier(ProtoEvent::Notifier* theNotifier);
		Notifier* GetNotifier() const {return notifier;}
                
        // Make the event handle/descriptor available for notifications
#ifdef WIN32
        typedef HANDLE Descriptor;  // WIN32 uses "HANDLE" type for descriptors, INVALID_HANDLE_VALUE is invalid; (although CreateEvent() returns NULL)
#else
        typedef int Descriptor;     // UNIX uses "int" type for descriptors, -1 is invalid descriptor
#endif // if/else WIN32
        Descriptor GetDescriptor() const
        {
#ifdef WIN32
            return event_handle;
#else
#if defined(USE_EVENTFD) || defined(USE_KEVENT)
            return event_fd;
#else
            return event_pipe_fd[0];
#endif // if/else USE_EVENTFD | USE_EVENTFD) / pipe 
#endif // if/else WIN32 / UNIX        
        }
        
        // NOTE: For VC++ 6.0 Debug builds, you _cannot_ use pre-compiled
        // headers with this template code.  Also, "/ZI" or "/Z7" compile options 
        // must NOT be specified.  (or else VC++ 6.0 experiences an "internal compiler error")
        // (Later Visual Studio versions have fixed this error)
        template <class listenerType>
        bool SetListener(listenerType* theListener, void(listenerType::*eventHandler)(ProtoEvent&))
        {
            bool doUpdate = ((NULL != theListener) || (NULL != listener));
            if (NULL != listener) delete listener;
            listener = (NULL != theListener) ? new LISTENER_TYPE<listenerType>(theListener, eventHandler) : NULL;
            bool result = (NULL != theListener) ? (NULL != listener) : true;
            return result ? (doUpdate ? UpdateNotification() : true) : false;
        }
        void OnNotify();
        bool HasNotifier() const
            {return (NULL != notifier);}
        bool HasListener() const
            {return (NULL != listener);}
        bool UpdateNotification();
    
    protected:
		/** 
		* @class Listener
		*
		* @brief Listens for event trigger and invokes ProtoEvent handler.
		*/
        class Listener
        {
            public:
                virtual ~Listener() {}
                virtual void on_event(ProtoEvent& theEvent) = 0;
                virtual Listener* duplicate() = 0;
        };

		/**
		* @class LISTENER_TYPE
		*
		* @brief Listener template
		*/
        template <class listenerType>
        class LISTENER_TYPE : public Listener
        {
            public:
                LISTENER_TYPE(listenerType* theListener, 
                              void(listenerType::*eventHandler)(ProtoEvent&))
                    : listener(theListener), event_handler(eventHandler) {}
                void on_event(ProtoEvent& theEvent) 
                    {(listener->*event_handler)(theEvent);}
                Listener* duplicate()
                    {return (static_cast<Listener*>(new LISTENER_TYPE<listenerType>(listener, event_handler)));}
            private:
                listenerType* listener;
                void(listenerType::*event_handler)(ProtoEvent&);
        };
        
    private:
        bool            auto_reset;
#ifdef WIN32
        HANDLE          event_handle;
#else  // UNIX
#if defined(USE_EVENTFD) || defined(USE_KEVENT)
        int             event_fd;
#else 
        int             event_pipe_fd[2];
#endif // if/else (USE_EVENTFD | USE_KEVENT) / pipe            
#endif // if/else WIN32/UNIX
        
        Notifier*       notifier;
        Listener*       listener;
        
};  // end class ProtoEvent

#endif // _PROTO_EVENT
