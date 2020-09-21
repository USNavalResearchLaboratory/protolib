#include "protoThread.h"
#include "protoDebug.h"

ProtoMutex::ProtoMutex(bool init)
{
    if (init) Init();
}

ProtoMutex::~ProtoMutex()
{
    Destroy();
}

void ProtoMutex::Init()
{
#ifdef WIN32
    InitializeCriticalSection(&mutex);
#else
    pthread_mutex_init(&mutex, NULL);
#endif
}  // end ProtoMutex::Init()

void ProtoMutex::Destroy()
{
#ifdef WIN32
    DeleteCriticalSection(&mutex);
#else
    pthread_mutex_destroy(&mutex);
#endif
}  // end ProtoMutex::Destroy()

void ProtoMutex::Lock()
{
#ifdef WIN32
    EnterCriticalSection(&mutex);
#else
    pthread_mutex_lock(&mutex);
#endif
}  // end ProtoMutex::Lock()

void ProtoMutex::Unlock()
{
#ifdef WIN32
    LeaveCriticalSection(&mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif
}  // end ProtoMutex::Unlock()

ProtoThread::ProtoThread()
 : thread_id((ThreadId)NULL), external_thread(false),
   thread_running(false), exit_code(0)
{
}

ProtoThread::~ProtoThread()
{
    
}

#ifdef WIN32
unsigned int __stdcall ProtoThread::DoThreadStart(void* param)
#else
void* ProtoThread::DoThreadStart(void* param)
#endif // if/else WIN32/UNIX
{
    ProtoThread* protoThread = reinterpret_cast<ProtoThread*>(param);
    ASSERT(NULL != protoThread);
    protoThread->thread_running = true;
    protoThread->exit_status = protoThread->RunThread();  // TBD - should have Run() set exit_status internally instead???
    DoThreadExit(protoThread->GetExitStatus());
    return (protoThread->GetExitStatus());
}  // end ProtoThread::DoThreadStart()

bool ProtoThread::StartThread(ThreadId threadId)
{
    if ((ThreadId)NULL == threadId)
    {
        external_thread = false;
#ifdef WIN32
        if (!(actual_thread_handle = (HANDLE)_beginthreadex(NULL, 0, DoThreadStart, this, 0, (unsigned*)&thread_id)))
#else
        if (0 != pthread_create(&thread_id, NULL, DoThreadStart, this))    
#endif // if/else WIN32/UNIX
        {
            PLOG(PL_ERROR, "ProtoThread::StartThread() create thread error: %s\n", GetErrorString());
            thread_id = (ThreadId)NULL;
            return false;
        }
    }
    else
    {
         // Starting within externally created thread
        ASSERT(threadId == GetCurrentThread());
        thread_id = threadId;  // externally created thread
        external_thread = true;
        // So must exec DoThreadStart() steps here
        thread_running = true;
        exit_status = RunThread();
    }
    return true;
}  // end ProtoThread::StartThread()

void ProtoThread::CloseThread(int exitCode)
{
    exit_code = exitCode;
    if ((ThreadId)NULL != thread_id)
    { 
        if (!external_thread)
        {
#ifdef WIN32
            if (!IsMyself()) WaitForSingleObject(actual_thread_handle, INFINITE);
#else
            if (!IsMyself()) pthread_join(thread_id, NULL);
#endif // if/else WIN32/UNIX
        }
        thread_running = false;
        thread_id = (ThreadId)NULL;
        external_thread = false;
    }
}  // end ProtoThread::CloseThread()
