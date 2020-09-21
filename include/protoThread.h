#ifndef _PROTO_THREAD
#define _PROTO_THREAD

#ifdef WIN32
#include <processthreadsapi.h>
#include <synchapi.h>
#else
#include <pthread.h>
#endif // if/else WIN32/UNIX

class ProtoMutex
{
    public:
        ProtoMutex(bool init = true);
        ~ProtoMutex();
        
        void Init();
        void Destroy();
        void Lock();
        void Unlock();
        // TBD - add TryLock() call?
        
    private:
#ifdef WIN32
        CRITICAL_SECTION mutex;
#else
        pthread_mutex_t  mutex;
#endif // if/else WIN32/UNIX
};  // end class ProtoMutex

class ProtoThread
{
    public:
#ifdef WIN32
        typedef DWORD ThreadId;
        static ThreadId GetCurrentThread() 
            {return ::GetCurrentThreadId();}        
#else        
        typedef pthread_t ThreadId;
         static ThreadId GetCurrentThread()
            {return pthread_self();}   
#endif  // if/else WIN32       
            
        virtual ~ProtoThread();
        // Can use externally created thread but must be within that thread when calling StartThread() with non-NULL threadId
        bool StartThread(ThreadId threadId = (ThreadId)NULL);  // creates thread if needed and enters its "Run()" method
        virtual int RunThread() = 0;                  // required override, must return "exit_code"
        virtual void StopThread(int exitCode = 0)     // probably will need to override to break any RunThread() loop
        {
            CloseThread(exitCode);  // any overridden StopThread() method must call CloseThread() to wrap things up properly
        }
        
        bool IsStarted() const
            {return ((ThreadId)NULL != thread_id);}
        
        bool IsRunning() const
            {return thread_running;}
           
        bool IsMyself() 
            {return (GetCurrentThread() == thread_id);}     
        
        int GetExitCode() const
            {return exit_code;}
         
    protected:
        ProtoThread();     
        void CloseThread(int exitCode);   // subclasses should override this and call this method to clean up       
            
    private:
#ifdef WIN32
        typedef DWORD ExitStatus;
        ExitStatus GetExitStatus()  // pthread uses a _pointer_ to the status value location
             {return &exit_status;} 
        static unsigned int __stdcall DoThreadStart(void* lpParameter);
        static void DoThreadExit(ExitStatus exitStatus) 
            {ExitThread(exitStatus);}      
#else
        typedef void* ExitStatus;        
         ExitStatus GetExitStatus()  // pthread uses a _pointer_ to the status value location
             {return &exit_status;}   
         static void* DoThreadStart(void* arg);
         static void DoThreadExit(ExitStatus exitStatus)
             {pthread_exit(exitStatus);}
#endif  // if/else WIN32    
            
        ThreadId    thread_id;
        bool        external_thread;
        bool        thread_running;
        int         exit_code;
        int         exit_status;
            
};  // end class ProtoThread

#endif // !_PROTO_THREAD
