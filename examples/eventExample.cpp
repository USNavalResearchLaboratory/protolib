#include "protokit.h"

#ifdef UNIX
#include <unistd.h>  // for sleep() function
#endif // UNIX

/**
 * @class SimpleThread
 *
 * @brief This example illustrates the use of the ProtoEvent class threaded ProtoDispatcher.
 *
 *  A "SimpleThread" class that uses a threaded ProtoDispatcher is defined that contains
 *  a ProtoTimer that can be started that fires with a one-second interval and also 
 *  contains a ProtoEvent that can be signaled.  The main() routine also has a 
 *  ProtEvent and uses its "Wait()" method to be signaled by the ProtoDispatcher.  So,
 *  this illustrates "procedural, "stand-alone" use of the ProtoEvent along with the
 *  asynchronous I/O, notification-based use within the SimpleThread()
 *
 *  When using a threaded ProtoDispatcher, make sure to call SuspendThread()
 *  before manipulating dispatcher objects.  (See SimpleThread::StartTimer() and
 *  SimpleThread::StopTimer() as an example)
 */
class SimpleThread
{
    public:
        SimpleThread();
        ~SimpleThread();
        
        bool Start(ProtoEvent* parentEvent);
        void Stop();
        
        bool StartTimer()
        {
            bool result = dispatcher.SuspendThread();
            if (result)
            {
                if (!timer.IsActive()) dispatcher.ActivateTimer(timer); 
                dispatcher.ResumeThread();
            }
            return result;
        }
        bool StopTimer()
        {
            bool result = dispatcher.SuspendThread();
            {
                if (timer.IsActive()) timer.Deactivate();
                dispatcher.ResumeThread();
            }
            return result;
        }
        bool TimerIsActive() 
        {
            bool result = dispatcher.SuspendThread();
            if (result)
            {
                result = timer.IsActive();
                dispatcher.ResumeThread();
            }
            return result;  
        }
        
        bool SetEvent()
        {
            bool result = dispatcher.SuspendThread();
            if (result)
            {
                result = event.Set();
                dispatcher.ResumeThread();
            }
            return result;  
        }
          
    private:
        
        void OnTimeout(ProtoTimer& theTimer);
        void OnEvent(ProtoEvent& theEvent);
        
        ProtoDispatcher dispatcher;
        ProtoTimer      timer;   
        ProtoEvent      event;
        ProtoEvent*     parent_event;
        unsigned int    timeout_count;  // how many timeouts before we signal parent_event
};  // end class SimpleThread

SimpleThread::SimpleThread()
 : event(true), parent_event(NULL), timeout_count(15)
{
    timer.SetListener(this, &SimpleThread::OnTimeout);
    timer.SetInterval(1.0);
    timer.SetRepeat(-1);   
    
    event.SetNotifier(&dispatcher);
    event.SetListener(this, &SimpleThread::OnEvent);
}

SimpleThread::~SimpleThread()
{
    Stop();   
}

bool SimpleThread::Start(ProtoEvent* parentEvent)
{
    if (!event.Open())
    {
        PLOG(PL_ERROR, "eventExample: SimpleThread::Start() event open failure ...\n");
        return false;
    }
    dispatcher.ActivateTimer(timer);
    if (dispatcher.StartThread())
    {
        parent_event = parentEvent;
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "eventExample: SimpleThread::Start() thread start failure ...\n");
        timer.Deactivate();
        event.Close();
        return false;
    }      
}  // end  SimpleThread::Start()

void SimpleThread::Stop()
{
    dispatcher.Stop();
    if (timer.IsActive()) timer.Deactivate();
    event.Close();
}  // end SimpleThread::Stop()

/**
 * IMPORTANT NOTE @note Remember this timeout handler
 * is being called by the dispatcher child thread, _not_
 * the main thread since we're using ProtoDispatcher::StartThread()
 * to run the dispatcher
 */
void SimpleThread::OnTimeout(ProtoTimer& theTimer)
{
    TRACE("eventExample: child thread SimpleThread::OnTimeout() timeout_count:%d ...\n", timeout_count);
    if ((1 == timeout_count) && (NULL != parent_event))
    {
        TRACE("eventExample: child thread signaling main thread ...\n");
        parent_event->Set();
    }
    if (0 != timeout_count) 
        timeout_count--;
}  // end SimpleThread::OnTimeout()

void SimpleThread::OnEvent(ProtoEvent& theEvent)
{
    TRACE("eventExample: child thread SimpleThread::OnEvent() ...\n");
}  // end SimpleThread::OnTimeout()

int main(int argc, char* argv[])
{
    SimpleThread simpleThread;
    ProtoEvent myEvent(false);
   
    if (!myEvent.Open())
    {
        PLOG(PL_ERROR, "eventExample:myEvent.Open() error:%s\n", GetErrorString());
        return -1;   
    }
    
    // This "starts" the  simpleThread thread 
    if (!simpleThread.Start(&myEvent))
    {
        PLOG(PL_ERROR, "eventExample: SimpleThread::Start() error\n");
        return -1;   
    }
    
    int count = 2;
    while (count--)
    {
#ifdef WIN32
        Sleep(3000);
#else
        sleep(3);
#endif  // if/else WIN32
        TRACE("eventExample: main thread countdown = %d\n",count);   
        if (0 == count)
        {
            TRACE("eventExample: main thread waiting for event signal from child ...\n");
            myEvent.Wait();  
            TRACE("eventExample: main thread got event signal from child ...\n");
            simpleThread.Stop();
        }
        if (1 == count)
        {
            TRACE("eventExample: main thread signaling child thread ...\n");
            simpleThread.SetEvent();
        }
    }
    TRACE("eventExample: main thread simpleThread stopped.\n");
    TRACE("eventExample: main thread Done.\n");
    return 0;
    
}  // end main()
