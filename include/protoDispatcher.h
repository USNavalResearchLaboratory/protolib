/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson and Joe Macker of the Naval Research 
 *       Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/

#ifndef _PROTO_DISPATCHER
#define _PROTO_DISPATCHER

#include "protoTimer.h"
#include "protoSocket.h"
#include "protoChannel.h"
#include "protoEvent.h"
#include "protoQueue.h"    // for keeping track of our "streams" (channels, sockets, generic)

#ifdef WIN32

#ifndef _WIN32_WCE
#include <process.h>  // for _beginthreadex/_endthreadex
#endif // !_WIN32_WCE

#define USE_WAITABLE_TIMER 1

#else  // UNIX
#include <pthread.h>
#include <unistd.h>  // for read(), etc

// There are some compile-time options here for
// the async i/o mechanism ProtoDispatcher uses
// (e.g., "select()", "kqueue()", "epoll", etc)
#if defined(MACOSX)
// Makefile must pick either USE_KQUEUE or USE_SELECT
// or we default to USE_SELECT (TBD)
#if (!defined(USE_KQUEUE) && !defined(USE_SELECT))
#warning "Neither USE_SELECT or USE_KQUEUE defined, setting USE_SELECT by default"
#define USE_SELECT 1
#endif // MACOSX

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#elif defined(LINUX)
// Makefile must pick USE_SELECT or USE_EPOLL or we
// default to USE_SELECT (TBD)
#if (!defined(USE_SELECT) && !defined(USE_EPOLL))
#warning "Neither USE_SELECT or USE_EPOLL defined, setting USE_SELECT"
#define USE_SELECT 1
#endif // !defined(USE_SELECT) & !defined(USE_EPOLL)
// Makefile must pick either USE_EPOLL or USE_SELECT
#ifdef USE_EPOLL
#include <sys/epoll.h>
#ifdef USE_SELECT
#error "Must define either USE_EPOLL or USE_SELECT, not both!"
#endif // USE_SELECT
#endif // USE_EPOLL
#ifdef USE_TIMERFD
#include <sys/timerfd.h>
#endif // USE_TIMERFD
#ifdef USE_EVENTFD
#include <sys/eventfd.h>
#endif // USE_EVENTFD

#else  // "other" UNIX
#define USE_SELECT  1  // default Unix async i/o and timing mechanism
#endif // if/else MACOSX / LINUX / OTHER

#endif // if/else WIN32/UNIX

#ifndef WIN32
// This solves a conflict with pcap.h 
#ifdef SOCKET
#undef SOCKET    
#endif
#endif // !WIN32

/**
 * @class ProtoDispatcher
 *
 * @brief This class provides a core around which Unix and Win32 applications
 * using Protolib can be implemented.  
 *
 * It's "Run()" method provides a "main loop" which uses 
 * the "select()" system call on Unix and the
 * similar "MsgWaitForMultipleObjectsEx()" system call on Win32.  It
 * is planned to eventually provide some built-in support for threading
 * in the future (e.g. the ProtoDispatcher::Run() method might execute
 * in a thread, dispatching events to a parent thread).
 */
 
 /***********************
    ProtoDispatcher Update Notes
    
    ProtoDispatcher is going to be updated to use epoll() and kevent()
    APIs instead of select() for supporting Linux and BSD operating systems.
    Note select() usage will still be supported, but the code is going to
    be streamlined significantly as part of this update.  Since there are
    going to be added wait/dispatch paradigms put in place, the following
    notes summarize these (i.e. "select()", "epoll()", "kevent()", and 
    "WaitForMultipleObjectsEx()"
 
 
    select() - Each call to select() requires populated file descriptor sets
               for "input", "output", and "exceptions".  These "fdsets" need
               to set prior to each call to select().  ProtoDispatcher can keep
               reference input, output, and exception "fdsets" that are modified
               with calls to Update{Channel | Socket | Generic}Notification() and
               then FD_COPY() with each call to select().
               A threaded ProtoDispatcher can be "broken" from the "select()" based
               wait state using its built-in "break pipe" (self-pipe set up for this)
               
               Summary of ProtoDispatcher state/ops needed:
                  a) Keep list of active "streams"
                  b) for time efficiency, keep reference fdsets for FD_COPY() to select()
                  c) on dispatch, iterate through active "streams" and check FD_ISSET()
               
    epoll()  - The "epoll_ctl()" function is used to set/change/unset the list of
               descriptors that epoll() monitors.  Then epoll_wait() is a blocking
               call that is used to get notification of events.  For Linux, we
               can use the timerfd, signalfd, and eventfd functions to set up
               waitable descriptors for timing, signals, and notional "events".
               We can use eventfd() instead of the "break pipe" on Linux.
               
               Summary of ProtoDispatcher state/ops needed:
                  a) Keep list of active "streams"
                  b) changes to stream status, handled directly with epoll_ctl, no extra state needed
                  c) on dispatch, dereference associated "stream" using epoll_event.data.ptr
               
    kevent() - A "changelist" is passed to each call of kevent() to optionally 
               modify the set of things monitored by a given kqueue().  The same
               "struct kevent" array can be used as the "changelist" and the
               "eventlist" to pass in changes and get pending events with one
               call.  Note that EVFILT_USER can be used to set up a sort
               of user-defined "event" that a kevent() call in another thread
               can "set" for the given kqueue descriptor. This can be used
               instead of the "break pipe".
               
               Summary of ProtoDispatcher state/ops needed:
                  a) Keep list of active "streams"
                  b) keep array of "struct kevent" to accumulate stream status changes
                  c) on dispatch, dereference associated "stream" using epoll_event.data.ptr
               
    WaitForMultipleObjectsEx() - An array of handles is passed in.  When one of
                                 handles is "ready" or "set", the function returns
                                 giving the value "index + WAIT_OBJECT0" for given
                                 the array.  ProtoDispatcher can keep a dual arrays
                                 that are set/modified/unset with calls to
                                 Update{Channel | Socket | Generic}Notification()
                                 where one array is the array of handles required 
                                 and a corresponding array has pointers to the 
                                 corresponding ProtoDispatcher::Stream object.
                                 For objects that have both input and/or output
                                 handles, separate "index" and "outdex" values are
                                 kept.
                                 
               
 
    THE PLAN:
        Step 1: Streamline current Unix/Win32 code that uses select()/WaitForMultipleObjectEx()
                so we can cleanly incorporate the alternative epoll() / kevent() implementation
                (e.g. make "thread break" consistent/modular, consolidate Channel/Socket/Generic stream
                into a single data structure that is randomly accessible and rapidly
                iterable (e.g. ProtoSortedTree). 
                
        Step 2: Extend Linux version to formally use timerfd, eventfd, and signalfd APIs with select()?
                
        Step 3: Probably tackle "kevent()" implementation next?
        
        Step 4: Finally, do "epoll()" implementation 
 
 
 ************************/
 

/// Asynchronous event dispatcher and timer scheduler class
class ProtoDispatcher : public ProtoTimerMgr, 
                        public ProtoSocket::Notifier,
                        public ProtoChannel::Notifier,
                        public ProtoEvent::Notifier
{
    public:    
    // Construction
        ProtoDispatcher();
        virtual ~ProtoDispatcher();
        void Destroy();
        
    // ProtoTimerMgr overrides
    //  Note the underlying ProtoTimerMgr will call ProtoDispatcher::UpdateSystemTimer()
    //  as needed which will will "wake up" (via ProtoDispatcher::SignalThread()) the
    //  dispatcher instance as needed
        void ActivateTimer(ProtoTimer& theTimer)
        {
            // TBD - should we SignalThread() the thread to wake it up?
            SuspendThread();
            ProtoTimerMgr::ActivateTimer(theTimer);
            ResumeThread();
        }
        void DeactivateTimer(ProtoTimer& theTimer)
        {
            SuspendThread();
            ProtoTimerMgr::DeactivateTimer(theTimer);
            ResumeThread();
        }
        
    // Methods to manage generic input/output streams (pipes, files, devices, etc)
        typedef ProtoChannel::Handle Descriptor;  // , UNIX "int" descriptors, Win32 "HANDLE" type 
        
        static const Descriptor INVALID_DESCRIPTOR;
        enum Event {EVENT_INPUT, EVENT_OUTPUT};
        typedef void (Callback)(ProtoDispatcher::Descriptor descriptor, 
                                ProtoDispatcher::Event      theEvent, 
                                const void*                 userData);
        bool InstallGenericInput(ProtoDispatcher::Descriptor descriptor, 
                                 ProtoDispatcher::Callback*  callback, 
                                 const void*                 clientData);
        void RemoveGenericInput(Descriptor descriptor);
        bool InstallGenericOutput(ProtoDispatcher::Descriptor descriptor, 
                                  ProtoDispatcher::Callback*  callback, 
                                  const void*                 clientData);
        void RemoveGenericOutput(Descriptor descriptor);

        // Methods to set up and explicitly control ProtoDispatcher operation
        /// Are there any pending timeouts/descriptors?
        bool IsPending() 
        {
            return (IsThreaded() || 
                    ProtoTimerMgr::IsActive() ||
                    !stream_table.IsEmpty());
        }
        
        /**
        * This can safely be called on a dispatcher _before_
        * the "StartThread()" or "Run()" method is called without 
        * affecting the current process priority
         */
		void SetPriorityBoost(bool state)
            {priority_boost = state;}
        
        /**
		* This one will affect the current process priority if
        * "StartThread()" has not already been called.
        */
		bool BoostPriority();
        
        /**
		* Block until next event (timer, socket, other, occurs)
        * (Note this will block forever if !IsPending())
        */
		void Wait();
        
        /// Dispatch round of events 
        void Dispatch();
        
        /// OR this "main loop" can be used.
        int Run(bool oneShot = false);
        void Stop(int exitCode = 0);
       
        bool IsRunning() const  // TBD - deprecate IsRunning() method?
            {return run;}
        
        /**
         * Controls whether time of day is polled for ultra-precise timing
         * If "precise timing" is set to "true", the time of day is polled
         * to achieve precise timer timeouts.  However, this can consume
         * considerable CPU resources.  The default state is "false".
         */
        void SetPreciseTiming(bool state) 
            {precise_timing = state;}
        // For debugging purposes
        void SetUserData(const void* userData)
            {user_data = userData;}
        const void* GetUserData() 
            {return user_data;}
        
 
#ifdef WIN32
        typedef CRITICAL_SECTION    Mutex;
        static void Init(Mutex& m) {InitializeCriticalSection(&m);}
        static void Destroy(Mutex& m) {DeleteCriticalSection(&m);}
        static void Lock(Mutex& m) {EnterCriticalSection(&m);}
        static void Unlock(Mutex& m) {LeaveCriticalSection(&m);}
#else
        typedef pthread_mutex_t     Mutex;
        static void Init(Mutex& m) {pthread_mutex_init(&m, NULL);}
        static void Destroy(Mutex& m) {pthread_mutex_destroy(&m);}
        static void Lock(Mutex& m) {pthread_mutex_lock(&m);}
        static void Unlock(Mutex& m) {pthread_mutex_unlock(&m);}
#endif // if/else WIN32/UNIX  
        /**
         * @class Controller
         *
		 * @brief Handles dispatching for the ProtoDispatcher
		 * The ProtoDispatcher::Controller helper class allows for
         * thread-safe synchronization when using the "StartThread()"
        * mode. 
         */
        class Controller
        {
            public:
                virtual ~Controller();
            
            protected:
                void OnDispatch();  // should be called by Controller thread in response to SignalDispatchReady() cue
                Controller(ProtoDispatcher& theDispatcher);
                
            private:
                friend class ProtoDispatcher;
                bool DoDispatch();  /// only called by ProtoDispatcher
                virtual bool SignalDispatchReady() = 0;  // cue to controller to make "OnDispatch() call
                void OnThreadStop()
                {
                    if (use_lock_a)
                        ProtoDispatcher::Unlock(lock_a);
                    else
                        ProtoDispatcher::Unlock(lock_b);   
                }
                
                ProtoDispatcher&       dispatcher;
                Mutex                  lock_a;
                Mutex                  lock_b;
                bool                   use_lock_a;
        };  // end class ProtoDispatcher::Controller()  
        friend class Controller;
    
#ifdef WIN32
        typedef DWORD ThreadId;
#else
        typedef pthread_t ThreadId;
#endif // if/else WIN32
    
        // OR the dispatcher can be run in a thread n(w/ optional Controller)
        bool StartThread(bool                         priorityBoost = false,
                         ProtoDispatcher::Controller* theController = NULL,
                         ThreadId                     threadId = (ThreadId)NULL);
        bool IsThreaded() {return ((ThreadId)(NULL) != thread_id);}
        
        /**
		* (NOTE: If dispatcher instance is threaded, you MUST successfully call
        *        "SuspendThread()" before calling _any_ dispatcher methods or
        *        manipulating any objects (ProtoTimers, ProtoSockets, etc)
        *        which affect dispatcher operation!!!!
        *        (Call "ResumeThread()" to allow the dispatcher to continue)
        *        This can be used to create thread-safe protocols/applications
        *        which use a ProtoDispatcher for async timing and I/O)
        */
		bool SuspendThread();
        void ResumeThread();
        
        // The "prompting" mechanism here is a way to cause a sleeping
        // thread to wakeup and execute a bit of code as set by the
        // "PromptCallback"
        // (TBD) refine this prompting mechanism a little bit
        // for example, some sort of waiting mechanism
        // (e.g., the child could prompt its parent?  add "promptData" option to PromptThread()?
        // should be added to know when the work has been done.
        // For example, a waitable descriptor. This way a "parent"
        // ProtoApp or ProtoDispatcher could create/start a pool of
        // waiting worker threads, assign them jobs, and reap the results
        // upon completion ...
        typedef void (PromptCallback)(const void* clientData);
        /**
		* Use this to set a PromptCallback function
        * (should do this _before_ StartThread() is invoked
        */
		void SetPromptCallback(PromptCallback* theCallback, const void* clientData = NULL)
        {
            prompt_callback = theCallback;
            prompt_client_data = clientData;  
        }
           
        /// Call this to force call of  PromptCallback in thread's context
        bool PromptThread();
        

#ifdef WIN32
        /// WIN32 apps can call this to create a msg window if desired
        bool Win32Init();
        void Win32Cleanup();
#endif // WIN32 
        
    public:
        bool SignalThread();
        void UnsignalThread();
    private:
        bool WasSignaled();
        /// Associated ProtoTimerMgrs will use this as needed
        bool UpdateSystemTimer(ProtoTimer::Command /*command*/,
                               double              /* delay*/) 
        {
            // ProtoDispatcher::Dispatch() queries ProtoTimerMgr::GetTimeRemaining() instead
            // The signal/unsignal here just wakes up the thread so this happens
            SignalThread();
            UnsignalThread();
            return true;
        }
        // Associated ProtoSockets will use this as needed
        bool UpdateSocketNotification(ProtoSocket& theSocket,
                                      int          notifyFlags);
        
        // Associated ProtoChannels will use this as needed
        bool UpdateChannelNotification(ProtoChannel& theChannel,
                                       int           notifyFlags);
        
        // Associated ProtoEvents will use this as needed
        bool UpdateEventNotification(ProtoEvent& theEvent, 
                                     int         notifyFlags);
          
        // Thread/Mutex stuff (TBD - make this its own class ???)
#ifdef WIN32
        typedef DWORD WaitStatus;
        //typedef DWORD ThreadId;
        ThreadId GetCurrentThread() {return ::GetCurrentThreadId();} 
#ifdef _WIN32_WCE
        typedef DWORD ExitStatus;
        static void DoThreadExit(ExitStatus exitStatus) {ExitThread(exitStatus);}
#else
        typedef unsigned int ExitStatus;
        static void DoThreadExit(ExitStatus exitStatus) {_endthreadex(exitStatus);}
#endif // if/else _WIN32_WCE
        ExitStatus GetExitStatus()
			{return exit_status;}
#else  // Unix
        typedef int WaitStatus;
        //typedef pthread_t ThreadId;
        static ThreadId GetCurrentThread() {return pthread_self();}
        typedef void* ExitStatus;
		ExitStatus GetExitStatus()  // note pthread uses a _pointer_ to the status value location
			{return &exit_status;}
        static void DoThreadExit(ExitStatus exitStatus) {pthread_exit(exitStatus);}
#endif // if/else WIN32/UNIX
                
        bool IsMyself() {return (GetCurrentThread() == thread_id);}
        void DestroyThread();
        
        bool InstallBreak();
        bool SetBreak();
        void RemoveBreak();
        
        /**
         * @class Stream
         *
         * @brief This class helps manage notification for
         * protoSockets and generic I/O descriptors
         */
        class Stream : public ProtoNotify, public ProtoTree::Item, public ProtoQueue::Item
        {
            public:
                enum Type {GENERIC, SOCKET, CHANNEL, TIMER, EVENT};
                Type GetType() const {return type;}
                
                bool IsInput() const {return NotifyFlagIsSet(NOTIFY_INPUT);}
                bool IsOutput() const {return NotifyFlagIsSet(NOTIFY_OUTPUT);}
                
                virtual Descriptor GetInputHandle() const = 0;
                virtual Descriptor GetOutputHandle() const = 0;
                
#ifdef WIN32                
                int GetIndex() const {return index;}
                void SetIndex(int theIndex) {index = theIndex;} 
                int GetOutdex() const {return outdex;}
                void SetOutdex(int theOutdex) {outdex = theOutdex;}
#endif // WIN32
            protected:
                Stream(Type theType);
            
            private:
                // ProtoTree::Item required overrides
                virtual const char* GetKey() const = 0;
                virtual unsigned int GetKeysize() const = 0;      
                    
                Type  type;
#ifdef WIN32
                int   index;
                int   outdex;
#endif // WIN32
        };  // end class Stream
        
       
        // Declare indexed table of streams 
        // (will be indexed by ProtoChannel, ProtoSocket, etc pointer)
        class StreamTable : public ProtoTreeTemplate<Stream> {};
        
        // Simple linked list of streams (we use for "ready_stream_list"
        class StreamList : public ProtoSimpleQueueTemplate<Stream>
        {
            public:
                StreamList() : ProtoSimpleQueueTemplate<Stream>(true) {}   // use container pooling
                ~StreamList() {}
        };  // end class ProtoDispatcher::StreamList
        
        // The "UpdateStreamNotification()" method is implemented with system-specific code
        // (i.e. for the separate WIN32, USE_SELECT, USE_KQUEUE, and USE_EPOLL cases)
        enum NotificationCommand {DISABLE_ALL, ENABLE_INPUT, DISABLE_INPUT, ENABLE_OUTPUT, DISABLE_OUTPUT};
        bool UpdateStreamNotification(Stream& stream, NotificationCommand cmd);
        
        
        /**
         * @class SocketStream
         *
         * @brief This class helps manage notification for
         * ProtoSockets and generic I/O descriptors
         */
		class SocketStream : public Stream
        {
            public:
                SocketStream(ProtoSocket& theSocket);
                ProtoSocket& GetSocket() {return *socket;}
                void SetSocket(ProtoSocket& theSocket) {socket = &theSocket;}
#ifdef WIN32
                Descriptor GetInputHandle() const
                    {return socket->GetInputEventHandle();}
                Descriptor GetOutputHandle() const
                    {return socket->GetOutputEventHandle();}
#else
                Descriptor GetInputHandle() const
                    {return socket->GetHandle();}
                Descriptor GetOutputHandle() const
                    {return socket->GetHandle();}
#endif // if/else WIN32/UNIX
                
            private:
                const char* GetKey() const
                    {return (const char*)(&socket);}
                unsigned int GetKeysize() const
                    {return (sizeof(ProtoSocket*) << 3);}
                    
                ProtoSocket*    socket;
        };  // end class SocketStream
        SocketStream* GetSocketStream(ProtoSocket& theSocket);
        void ReleaseSocketStream(SocketStream& socketStream);
        
        class SocketStreamPool : public ProtoTreeTemplate<SocketStream>::ItemPool {};
        
        /**
         * @class ChannelStream
         *
         * @brief This class helps manage notification for
         * protoSockets and generic I/O descriptors
         */
        class ChannelStream : public Stream
        {
            public:
                ChannelStream(ProtoChannel& theChannel);
                ProtoChannel& GetChannel() {return *channel;}
                void SetChannel(ProtoChannel& theChannel) {channel = &theChannel;}
                
                Descriptor GetInputHandle() const
                    {return channel->GetInputEventHandle();}
                Descriptor GetOutputHandle() const
                    {return channel->GetOutputEventHandle();}
            private:
                const char* GetKey() const
                    {return (const char*)(&channel);}
                unsigned int GetKeysize() const
                    {return (sizeof(ProtoChannel*) << 3);}
                
                ProtoChannel*    channel;
        };  // end class ChannelStream
        ChannelStream* GetChannelStream(ProtoChannel& theChannel);
        void ReleaseChannelStream(ChannelStream& channelStream);
        
        class ChannelStreamPool : public ProtoTreeTemplate<ChannelStream>::ItemPool {};
        
        
        /**
         * @class TimerStream
         *
         * @brief This class lets us have a "Stream*" pointer
         * for the user data in "struct epoll_event" since we
         * use the "event.data" for that purpose.
         */
        class TimerStream : public Stream
        {
            public:
                TimerStream();
                ~TimerStream();
                
                void SetDescriptor(Descriptor theDescriptor)
                    {descriptor = theDescriptor;}
                Descriptor GetDescriptor() const
                    {return descriptor;}
                Descriptor GetInputHandle() const
                    {return descriptor;}
                Descriptor GetOutputHandle() const
                    {return descriptor;}
            private:
                const char* GetKey() const {return NULL;}
                unsigned int GetKeysize() const {return 0;}
                
            private:
                Descriptor descriptor;
        };  // end class TimerStream
        
        /**
         * @class EventStream
         *
         * @brief This class helps manage notifications
         * for ProtoEvent instances
         */
        class EventStream : public Stream
        {
            public:
                EventStream(ProtoEvent& theEvent);
                ~EventStream();
                
                void SetEvent(ProtoEvent& theEvent) {event = &theEvent;}
                ProtoEvent& GetEvent() {return *event;}
                
                Descriptor GetDescriptor() const
                    {return (NULL != event) ? event->GetDescriptor() : INVALID_DESCRIPTOR;}
                Descriptor GetInputHandle() const {return GetDescriptor();}
                Descriptor GetOutputHandle() const {return GetDescriptor();}
                
            private:
                const char* GetKey() const
                    {return (const char*)(&event);}
                unsigned int GetKeysize() const
                    {return (sizeof(ProtoEvent*) << 3);}
                
            private:
                ProtoEvent* event;
        };  // end class EventStream
        EventStream* GetEventStream(ProtoEvent& theEvent);
        void ReleaseEventStream(EventStream& eventStream);
        class EventStreamPool : public ProtoTreeTemplate<EventStream>::ItemPool {};
        
        /**
         * @class GenericStream
         *
         * @brief This class helps manage notification for
         * protoSockets and generic I/O descriptors
         */
        class GenericStream : public Stream
        {
            public:
                GenericStream(Descriptor theDescriptor);
                Descriptor GetDescriptor() {return descriptor;}
                void SetDescriptor(Descriptor theDescriptor) {descriptor = theDescriptor;}
                void SetCallback(Callback* theCallback, const void* clientData = NULL)
                {
                    callback = theCallback;
                    client_data = clientData;  
                }
                void OnEvent(Event theEvent)
                    {if (callback) callback(descriptor, theEvent, client_data);} 
                
                const char* GetDescriptorPtr() const
                    {return ((const char*)&descriptor);}
                
                Descriptor GetInputHandle() const
                    {return descriptor;}
                Descriptor GetOutputHandle() const
                    {return descriptor;}
                
            //private:
                const char* GetKey() const
                    {return (const char*)(&myself);}  
                unsigned int GetKeysize() const
                    {return (sizeof(GenericStream*) << 3);}
                
                GenericStream*  myself;        // used as StreamTable key
                Descriptor      descriptor;
                Callback*       callback;
                const void*     client_data;
        };  // end class GenericStream
        
        // This class is used to keep a separate listing of GenericStreams indexed
        // by their descriptors so we can find them quickly.
        class GenericStreamTable : public ProtoIndexedQueueTemplate<GenericStream>
        {
            public:
                GenericStream* FindByDescriptor(Descriptor descriptor) const
                    {return Find((const char*)&descriptor, sizeof(Descriptor) << 3);}
                    
            private:
                // Required overrides for ProtoIndexedQueue subclasses
                virtual const char* GetKey(const Item& item) const
                    {return static_cast<const GenericStream&>(item).GetDescriptorPtr();}
                virtual unsigned int GetKeysize(const Item& /*item*/) const
                    {return (sizeof(Descriptor) << 3);}
        };  // end ProtoDispatcher::GenericStreamTable
        
        class GenericStreamPool : public ProtoTreeTemplate<GenericStream>::ItemPool {};
        
        GenericStream* GetGenericStream(Descriptor descriptor);
        GenericStream* FindGenericStream(Descriptor descriptor) const
            {return generic_stream_table.FindByDescriptor(descriptor);}
        void ReleaseGenericStream(GenericStream& genericStream);
        
    // Members
        StreamTable                 stream_table;           // table of active streams by channel, socket, etc pointer
        GenericStreamTable          generic_stream_table;   // Used to lookup stream by "descriptor" value
        
        ChannelStreamPool           channel_stream_pool;    // land of inactive channel streams
        SocketStreamPool            socket_stream_pool;     // land of inactive socket streams
        EventStreamPool             event_stream_pool;      // land of inactive event streams
        GenericStreamPool           generic_stream_pool;    // land of inactive generic streams
                
        volatile bool            run;           
        WaitStatus               wait_status;            
        int                      exit_code;  
        double                   timer_delay;  // ( timer_delay < 0.0) means INFINITY
        bool                     precise_timing;
        ThreadId                 thread_id;
        bool                     external_thread;
        bool                     priority_boost;
        volatile bool            thread_started;
        volatile bool            thread_signaled;  
        Mutex                    suspend_mutex;
        Mutex                    signal_mutex;
        ThreadId                 thread_master;
        unsigned int             suspend_count;
        unsigned int             signal_count;
        Controller*              controller;
        
        // The "prompt" stuff here was a hack to be able to make a threaded
        // ProtoDispatcher do some "work" (prompt_callback) in its thread
        bool                     prompt_set;
        PromptCallback*          prompt_callback;
        const void*              prompt_client_data;
        ProtoEvent               break_event;         
        EventStream              break_stream;
        bool                     is_signaled;
#ifdef WIN32
        int Win32AddStream(Stream& theStream, HANDLE theHandle);
        void Win32RemoveStream(int index);
        bool Win32IncreaseStreamArraySize();
        static LRESULT CALLBACK MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#ifdef _WIN32_WCE
        static DWORD WINAPI DoThreadStart(LPVOID lpParameter);
#else
        static unsigned int  __stdcall DoThreadStart(void* lpParameter);
#endif // if/else _WIN32_WCE
		ExitStatus				exit_status;
        enum {DEFAULT_STREAM_ARRAY_SIZE = 32};
        HANDLE*                 stream_handles_array;                                 
        Stream**                stream_ptrs_array;                                   
        DWORD                   stream_array_size;                                   
        DWORD                   stream_count;                                        
        HWND                    msg_window;   
        HANDLE                  actual_thread_handle;       
	StreamList		ready_stream_list;  // list of input or output "ready" streams     
#ifdef	USE_WAITABLE_TIMER
		TimerStream				timer_stream;
		bool					timer_active;
#endif // USE_WAITABLE_TIMER
#else  // UNIX
        
#ifdef USE_TIMERFD    
        int                      timer_fd;       
#endif // USE_TIMERFD                           
        static void* DoThreadStart(void* arg);
        int                     exit_status;
        
#if defined(USE_SELECT)
        fd_set                  input_set;        
        fd_set                  output_set;  
#elif defined(USE_KQUEUE)
        enum {KEVENT_ARRAY_SIZE = 64};
        bool KeventChange(uintptr_t ident, int16_t filter, uint16_t flags, void* udata);
        struct kevent           kevent_array[KEVENT_ARRAY_SIZE];
        int                     kevent_queue;
#elif defined(USE_EPOLL)
        enum {EPOLL_ARRAY_SIZE = 64};
        bool EpollChange(int fd, int events, int op, void* udata);
        struct epoll_event      epoll_event_array[EPOLL_ARRAY_SIZE];
        int                     epoll_fd;
#else  // UNIX
#error "undefined async i/o mechanism"  // to make sure we implement something       
#endif  // !USE_SELECT && !USE_KQUEUE
#ifdef USE_TIMERFD    
        TimerStream             timer_stream;       
#endif // USE_TIMERFD                        
#endif // if/else WIN32/UNIX

        const void*             user_data;

};  // end class ProtoDispatcher

#endif // _PROTO_DISPATCHER
