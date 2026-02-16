#include "protokit.h"

#ifdef UNIX
#include <unistd.h>  // for sleep() function
#endif // UNIX

/**
 * @class Ticker
 *
 * @brief This example illustrates the use of threaded ProtoDispatcher operation
 *
 *  A "Ticker" is started in separate thread that has a timer that fires off 
 *  once per second while the main thread goes about its business (sleeping
 *  in 10 second chunks).  The Ticker::Stop() method calls the 
 *  ProtoDispatcher::Stop() method which destroys the dispatcher thread
 *  Alternatively, the Ticker::timer could have been Deactivated without
 *  touch the dispatcher thread, but a new dispatcher thread can be 
 *  instantiated by calling ProtoDispatcher::StartThread() ... so there
 *  is flexibility here.
 *
 *  When using a threaded ProtoDispatcher, make sure to call SuspendThread()
 *  before manipulating dispatcher objects.  (See Ticker::StartTimer() and
 *  Ticker::StopTimer() as an example)
 */
class Ticker
{
    public:
        Ticker();
        ~Ticker();
        
        bool Start();
        void Stop();
        
        bool StartTimer()
        {
            bool result = dispatcher.SuspendThread();
            if (result && !timer.IsActive()) 
                dispatcher.ActivateTimer(timer); 
            dispatcher.ResumeThread();
            return result;  
        }
        bool StopTimer()
        {
            bool result = dispatcher.SuspendThread();
            if (result && timer.IsActive()) 
                timer.Deactivate(); 
            dispatcher.ResumeThread();
            return result;
        }
          
    private:
        bool OnTimeout(ProtoTimer& theTimer);
        ProtoDispatcher dispatcher;
        ProtoTimer timer;   
};  // end class Ticker

Ticker::Ticker()
{
    timer.SetListener(this, &Ticker::OnTimeout);
    timer.SetInterval(1.0);
    timer.SetRepeat(-1);   
}

Ticker::~Ticker()
{
    Stop();   
}

bool Ticker::Start()
{
    //dispatcher.ActivateTimer(timer);
    if (dispatcher.StartThread())
    {
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "threadExample: Ticker::Start() thread start failure ...\n");
        timer.Deactivate();
        return false;
    }      
}  // end  Ticker::Start()

void Ticker::Stop()
{
    dispatcher.Stop();
    if (timer.IsActive()) timer.Deactivate();
}  // end Ticker::Stop()

/**
 * IMPORTANT NOTE @note Remember this timeout handler
 * is being called by the dispatcher child thread, _not_
 * the main thread since we're using ProtoDispatcher::StartThread()
 * to run the dispatcher
 */
bool Ticker::OnTimeout(ProtoTimer& theTimer)
{
    TRACE("threadExample: child thread Ticker::OnTimeout() ...\n");
    return true;
}  // end Ticker::OnTimeout()

int main(int argc, char* argv[])
{
    Ticker ticker;
    
    // This "starts" the  ticker thread although it just sits and
    // waits until its "StartTimer()" method is called
    if (!ticker.Start())
    {
        PLOG(PL_ERROR, "threadExample: Ticker::Start() error\n");
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
        if (count)
        {
            TRACE("threadExample: main thread 3 seconds have passed (starting ticker timer)\n");        
            ticker.StartTimer();
        }
        else
        {   
            TRACE("threadExample: main thread 3 more seconds have passed (stopping ticker timer)\n");        
            ticker.Stop();
        }
        
    }
    TRACE("threadExample: main thread ticker stopped.\n");
    TRACE("threadExample: main thread Done.\n");
    return 0;
    
}  // end main()
