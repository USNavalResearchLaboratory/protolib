#include "protoZMQ.h"
#include "protoDebug.h"

#ifdef WIN32
#include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#include <unistd.h>  // for getpid()
#endif

// Static member initializations
ProtoZmq::PollerThread* ProtoZmq::Socket::default_poller_thread = NULL;
ProtoMutex  ProtoZmq::Socket::default_poller_mutex;

ProtoZmq::Socket::Socket()
  : state(CLOSED), zmq_ctx(NULL), zmq_sock(NULL), poll_flags(ZMQ_POLLIN)
{
}

ProtoZmq::Socket::~Socket()
{
    Close();
}

bool ProtoZmq::Socket::Open(int socketType, void* zmqSocket, void* zmqContext)
{
    if (IsOpen()) Close();
    ext_ctx = false;
    if (NULL == zmqContext)
        zmqContext = zmq_ctx_new();
    else
        ext_ctx = true;
    if (NULL == zmqContext)
    {
        PLOG(PL_ERROR, " ProtoZmq::Socket::Open() zmq_ctx_new() error: %s\n", GetErrorString());
        return false;
    }
    zmq_ctx = zmqContext;
    ext_sock = false;
    if (NULL == zmqSocket)
        zmqSocket = zmq_socket(zmq_ctx, socketType);
    else
        ext_sock = true;
    if (NULL == zmqSocket)
    {
        PLOG(PL_ERROR, " ProtoZmq::Socket::Open() zmq_socket() error: %s\n", GetErrorString());
        Close();
        return false;
    }
    zmq_sock = zmqSocket;
    if (!ProtoEvent::Open())
    {
        
        PLOG(PL_ERROR, " ProtoZmq::Socket::Open() ProtoEvent::Open() error: %s\n", GetErrorString());
        Close();
        return false;
    }
    state = IDLE;
    return true;
}  // emd ProtoZmq::Socket::Open()

void ProtoZmq::Socket::Close()
{
    state = CLOSED;
    UpdateNotification();
    if (ProtoEvent::IsOpen()) 
        ProtoEvent::Close();
    if (NULL != zmq_sock)
    {
        if (!ext_sock)
            zmq_close(zmq_sock);
        zmq_sock = NULL;
        ext_sock = false;
    }
    if (NULL != zmq_ctx)
    {  
        if (!ext_ctx)
           zmq_ctx_term(zmq_ctx);
        zmq_ctx = NULL;
        ext_ctx = false;
    }
    
}  // end ProtoZmq::Socket::Close()

bool ProtoZmq::Socket::StartInputNotification()
{
    if (0 == (ZMQ_POLLIN & poll_flags))
    {
        poll_flags |= ZMQ_POLLIN;
        if (!UpdateNotification())
        {
            PLOG(PL_ERROR, "ProtoZmq::Socket::StartInputNotification() error: unable to update notifications\n");
            poll_flags &= ~ZMQ_POLLIN;
            return false;
        }
    }
    return true;
}  // end ProtoZmq::Socket::StartInputNotification()

bool ProtoZmq::Socket::StopInputNotification()
{
    if (0 != (ZMQ_POLLIN & poll_flags))
    {
        poll_flags &= ~ZMQ_POLLIN;
        if (!UpdateNotification())
        {
            PLOG(PL_ERROR, "ProtoZmq::Socket::StopInputNotification() error: unable to update notifications\n");
            poll_flags |= ~ZMQ_POLLIN;
            return false;
        }
    }
    return true;
}  // end ProtoZmq::Socket::StartInputNotification()

bool ProtoZmq::Socket::StartOutputNotification()
{
    if (0 == (ZMQ_POLLOUT & poll_flags))
    {
        poll_flags |= ZMQ_POLLOUT;
        if (!UpdateNotification())
        {
            PLOG(PL_ERROR, "ProtoZmq::Socket::StartOutputNotification() error: unable to update notifications\n");
            poll_flags &= ~ZMQ_POLLOUT;
            return false;
        }
    }
    return true;
}  // end ProtoZmq::Socket::StopInputNotification()

bool ProtoZmq::Socket::StopOutputNotification()
{
    if (0 != (ZMQ_POLLOUT & poll_flags))
    {
        poll_flags &= ~ZMQ_POLLOUT;
        if (!UpdateNotification())
        {
            PLOG(PL_ERROR, "ProtoZmq::Socket::StopOutputNotification() error: unable to update notifications\n");
            poll_flags |= ~ZMQ_POLLOUT;
            return false;
        }
    }
    return true;
}  // end ProtoZmq::Socket::StopOutputNotification()

bool ProtoZmq::Socket::SetNotifier(ProtoEvent::Notifier* theNotifier, PollerThread* pollerThread)
{
    if (NULL != theNotifier)
    {
        if (theNotifier == GetNotifier()) return true;  // already set
        
        if (HasNotifier())
        {
            if ((NULL != poller_thread) && poller_active)
            {
                poller_thread->RemoveSocket(*this);
                poller_active = false;
            }
            ProtoEvent::SetNotifier(NULL);
        }    
        if (NULL == pollerThread)
        {
            // Use default poller thread
            default_poller_mutex.Lock();
            if (NULL == default_poller_thread)
            {
                if (NULL == (default_poller_thread = new PollerThread()))
                {
                    PLOG(PL_ERROR, "ProtoZmq::Socket::SetNotifier() new default_poller_thread error: %s\n", GetErrorString());
                    default_poller_mutex.Unlock();
                    return false;
                }
            }
            poller_thread = default_poller_thread;
            default_poller_mutex.Unlock();
        } 
        else
        {
            poller_thread = pollerThread;
        }
    }
    else if (HasNotifier())
    {
        if ((NULL != poller_thread) && poller_active)
        {
            poller_thread->RemoveSocket(*this);
            poller_active = false;
        }
        poller_thread = NULL;
    }
    ProtoEvent::SetNotifier(theNotifier);
    return UpdateNotification();  // will install socket to poller_thread as needed
}  // end ProtoZmq::Socket::SetNotifier()

bool ProtoZmq::Socket::UpdateNotification()
{
    if (CONNECTED == state)
    {
        if (HasNotifier())
        {
            ASSERT(NULL != poller_thread); 
            if (poller_active)
            {
                if (0 != poll_flags)
                {
                    if (!poller_thread->ModSocket(*this))
                    {
                        PLOG(PL_ERROR, "ProtoZmq::Socket::UpdateNotification() error: unable to modify socket on poller_thread!\n");
                        return false;
                    }
                }
                else
                {
                    poller_thread->RemoveSocket(*this);
                    poller_active = false;
                }
            }
            else
            {
                 
                if (!poller_thread->AddSocket(*this))
                {
                    PLOG(PL_ERROR, "ProtoZmq::Socket::UpdateNotification() error: unable to add socket to poller_thread!\n");
                    return false;
                }
                poller_active = true;
            }
        }
    }
    else if ((NULL != poller_thread) && poller_active)
    {
        poller_thread->RemoveSocket(*this);
        poller_active = false;
    }
    return true;
}  // end ProtoZmq::Socket::UpdateNotification()

bool ProtoZmq::Socket::Bind(const char* endpoint)
{
    if (!IsOpen())
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Bind() error: socket not open!\n");
        return false;
    }
    if (0 != zmq_bind(zmq_sock, endpoint))
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Bind() zmq_bind() error: %s\n", GetErrorString());
        return false;
    }
    state = CONNECTED;  // ZMQ sockets are immediately "connected" since ZMQ state routines will be used
    if (!UpdateNotification())
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Bind() error: unable to update notification status\n");
        Close();
        return false;
    }
    return true;
}  // end ProtoZmq::Socket::Bind()

bool ProtoZmq::Socket::Connect(const char* endpoint)
{
    if (!IsOpen())
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Connect() error: socket not open!\n");
        return false;
    }
    if (0 != zmq_connect(zmq_sock, endpoint))
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Connect() zmq_bind() error: %s\n", GetErrorString());
        return false;
    }
    state = CONNECTED;  // ZMQ sockets are immediately "connected" since ZMQ state routines will be used
    if (!UpdateNotification())
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Connect() error: unable to update notification status\n");
        Close();
        return false;
    }
    return true;
}  // end ProtoZmq::Socket::Connect()

 
bool ProtoZmq::Socket::Subscribe(const char* prefix, unsigned int length)
{
    if (NULL == zmq_sock)
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Subscribe() error: socket not open\n");
        return false;   
    }
    if (NULL == prefix)
        length = 0;
    else if (0 == length)
        length = (unsigned int)strlen(prefix);
    if (0 != zmq_setsockopt(zmq_sock, ZMQ_SUBSCRIBE, prefix, length))
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Subscribe() zmq_setsockopt(ZMQ_SUBSCRIBE) error: %s\n", zmq_strerror(zmq_errno()));
        return false;   
    }
    return true;
}  // end ProtoZmq::Socket::Subscribe()

// for ZMQ_DISH sockets
bool ProtoZmq::Socket::Join(const char* group)
{
    if (NULL == zmq_sock)
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Join() error: socket not open\n");
        return false;   
    }
    if (0 != zmq_join(zmq_sock, group))
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::Join() zmq_join() error: %s\n", zmq_strerror(zmq_errno()));
        return false;   
    }
    return true;
}  // end ProtoZmq::Socket::Join()

bool ProtoZmq::Socket::Send(char* buffer, unsigned int& numBytes)
{
    // TBD - support both blocking and non-blocking operation as well as multi-part messages
    int result = zmq_send(zmq_sock, buffer, numBytes, ZMQ_DONTWAIT);
    if (poller_active && (0 != (ZMQ_POLLOUT & poll_flags)))
        UpdateNotification();  // resets poll_flags and PollerThread notification
    if (result < 0)
    {
        switch (zmq_errno())
        {
            case EAGAIN:
            case EINTR:   // do we need to have a loop here to retry on EINTR?
                numBytes = 0;
                break;
            default:
                PLOG(PL_ERROR, "ProtoZmq::Socket::Send() zmq_send() error: %s\n", GetErrorString());
                numBytes = 0;
                return false;    
        }
    }
    else
    {
        // TBD - do we need to handle 0 == result differently
        numBytes = result;
    }
    return true;
}  // end ProtoZmq::Socket::Send()

bool ProtoZmq::Socket::Recv(char* buffer, unsigned int& numBytes)
{
    int result = zmq_recv(zmq_sock, buffer, numBytes, ZMQ_DONTWAIT);
    if (poller_active && (0 != (ZMQ_POLLIN & poll_flags)))
        UpdateNotification();  // resets poll_flags and PollerThread notification
    if (result < 0)
    {
        switch (zmq_errno())
        {
            case EAGAIN:
            case EINTR:   // do we need to have a loop here to retry on EINTR?
                numBytes = 0;
                break;
            default:
                PLOG(PL_ERROR, "ProtoZmq::Socket::Recv() zmq_recv() error: %s\n", GetErrorString());
                numBytes = 0;
                return false;    
        }
    }
    else
    {
        // TBD - do we need to handle 0 == result differently
        numBytes = result;
    }
    return true;
}  // end ProtoZmq::Socket::Recv()

bool ProtoZmq::Socket::RecvMsg(zmq_msg_t* zmqMsg)
{
    int result = zmq_msg_recv(zmqMsg, zmq_sock, ZMQ_DONTWAIT);
    if (poller_active && (0 != (ZMQ_POLLIN & poll_flags)))
        UpdateNotification();  // resets poll_flags and PollerThread notification
    if (result < 0)
    {
        switch (zmq_errno())
        {
            case EAGAIN:
            case EINTR:   // do we need to have a loop here to retry on EINTR?
                break;
            default:
                PLOG(PL_ERROR, "ProtoZmq::Socket::RecvMsg() zmq_recv() error: %s\n", GetErrorString());
                return false;    
        }
    }
    return true;
}  // end ProtoZmq::Socket::Recv()

ProtoZmq::PollerThread::PollerThread()
  : zmq_ctx(NULL), ext_ctx(false), zmq_poller(NULL), event_array(NULL), 
    event_array_length(0), socket_count(0)
{
}

ProtoZmq::PollerThread::~PollerThread()
{
    // This code guarantees safe destruction of the default poller thread
    bool isDefault = (this == ProtoZmq::Socket::default_poller_thread);
    if (isDefault)
    {
        ProtoZmq::Socket::default_poller_mutex.Lock();
        if (NULL == ProtoZmq::Socket::default_poller_thread)
        {
            ProtoZmq::Socket::default_poller_mutex.Unlock();
            return;  // another thread already cleaned it up.
        }
    }
    Close();
    if (isDefault) 
    {
        ProtoZmq::Socket::default_poller_thread = NULL;
        ProtoZmq::Socket::default_poller_mutex.Unlock();
    }
}

bool ProtoZmq::PollerThread::Open(void* zmqContext, bool retainLock)
{
    suspend_mutex.Lock();
    if (NULL != zmq_poller)
    {
        // This makes sure we don't conflict with another thread who was already opening the thread
        if (!retainLock) suspend_mutex.Unlock();
        return true;
    }
    if (NULL == (zmq_poller = zmq_poller_new()))
    {
        PLOG(PL_ERROR, "ProtoZmq::PollerThread::Open() zmoq_poller_new() error %s\n", zmq_strerror(zmq_errno()));
        suspend_mutex.Unlock();
        return false;
    }
    
    ext_ctx = false;
    if (NULL == zmqContext)
        zmqContext = zmq_ctx_new();
    else
        ext_ctx = true;
    if (NULL == zmqContext)
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() zmq_ctx_new() error: %s\n", zmq_strerror(zmq_errno()));
        ClosePrivate();
        suspend_mutex.Unlock();
        return false;
    }
    zmq_ctx = zmqContext;
    
    // We use ZMQ_SERVER and ZMQ_CLIENT for the break_server/break_client because they are thread safe
    if (!break_server.Open(ZMQ_SERVER, NULL, zmq_ctx))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() error: unable to open break_server\n");
        ClosePrivate();
        suspend_mutex.Unlock();
        return false;
    }
    // Create a ZMQ "inproc" socket name unique to this process/pollerThread
    char endpoint[256];
    endpoint[255] = '\0';
#ifdef WIN32
    snprintf(endpoint, 255, "inproc://ProtoZmq-%d-%p", GetCurrentProcessId(), this);
#else
    snprintf(endpoint, 255, "inproc://ProtoZmq-%d-%p",getpid(), this);
#endif // WIN32/UNIX   
    if (!break_server.Bind(endpoint))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() error: unable to bind break_server\n");
        ClosePrivate();
        suspend_mutex.Unlock();
        return false;
    }
    if (!AddSocketPrivate(break_server))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() error: unable to add break_server socket to poller\n");
        ClosePrivate();
        suspend_mutex.Unlock();
        return false;
    }
    if (!break_client.Open(ZMQ_CLIENT, NULL, zmq_ctx))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() error: unable to open break_client\n");
        ClosePrivate();
        suspend_mutex.Unlock();
        return false;
    }
    if (!break_client.Connect(endpoint))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() error: unable to connect break_client\n");
        ClosePrivate();
        suspend_mutex.Unlock();
        return false;
    }
    if (!StartThread())
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::Open() error: unable to start poller thread!\n");
        ClosePrivate();
        suspend_mutex.Unlock();
    }    
    while(!poller_running);// TRACE("waiting for thread to start\n");  // makes sure the ProtoThread::RunThread() has been entered
    if (!retainLock) suspend_mutex.Unlock();
    return true;
}  // end ProtoZmq::PollerThread::Open()
        
void ProtoZmq::PollerThread::Close()
{
    suspend_mutex.Lock();
    ClosePrivate();
    suspend_mutex.Unlock();
    if (this == ProtoZmq::Socket::default_poller_thread)
    {
        delete ProtoZmq::Socket::default_poller_thread;  // its destructor NULLs default_poller_thread pointer
    }
}  // end ProtoZmq::PollerThread::Close()


void ProtoZmq::PollerThread::ClosePrivate()
{
    if (poller_running)
    {
        poller_running = false;
        if (Signal()) // sends message to break out of zmq_poller->poll()
        {
            suspend_mutex.Unlock();  // allows thread to exit
            StopThread();   // sets exit_code and closes thread operation
            suspend_mutex.Lock();
            signal_mutex.Unlock();
        }
        // else someone else already stopped it
    }
    // TBD - remove all sockets from poller
    
    if (break_client.IsOpen()) 
    {
        break_client.Close();
    }
    if (break_server.IsOpen()) break_server.Close();
    
    if (NULL != zmq_ctx)
    {  
        if (!ext_ctx)
           zmq_ctx_term(zmq_ctx);
        zmq_ctx = NULL;
        ext_ctx = false;
    }
    if (NULL != zmq_poller)
    {
        zmq_poller_destroy(&zmq_poller);
        zmq_poller = NULL;
    }
}  // end ProtoZmq::PollerThread::ClosePrivate()

int ProtoZmq::PollerThread::RunThread()
{
    poller_running = true;
    suspend_mutex.Lock();
    while(poller_running)
    {
        signal_mutex.Lock();
        suspend_mutex.Unlock();    
        int result = zmq_poller_wait_all(zmq_poller, event_array, event_array_length, -1);
        signal_mutex.Unlock();
        // <-- this is where a the loop waits after PollerThread::Signal() is called
        suspend_mutex.Lock();
        if (!poller_running) break;
        if (result < 0)
        {
            switch (zmq_errno())
            {
                case ETERM:
                    PLOG(PL_ERROR, " ProtoZmq::PollerThread::RunThread() zmq_poller_wait_all() error: %s\n", zmq_strerror(ETERM));
                    poller_running = false;
                    break;
                case EINTR:
                case EAGAIN:  // this one shouldn't happen
                    break;
                default:
                    PLOG(PL_ERROR, " ProtoZmq::PollerThread::RunThread() zmq_poller_wait_all() error: %s\n", zmq_strerror(zmq_errno()));
                    poller_running = false;
                    break;    
            }
            continue;
        }
        // Dispatch events for signaled sockets
        for (int i = 0; i < result; i++)
        {
            zmq_poller_event_t* item = event_array + i;  
            if (break_server.GetSocket() == item->socket)
            {
                // It's our "break" socket, so consume any 'break' messages
                char dummy[64];
                unsigned int numBytes;
                do
                {
                    numBytes = 64;
                    if (!break_server.Recv(dummy, numBytes))
                    {
                        PLOG(PL_ERROR, "ProtoZmq::PollerThread::RunThread() error: break_server.Recv() failure!\n");
                        break;
                    }
                } while (0 != numBytes);
                continue;
            }
            Socket* zmqSocket = reinterpret_cast<Socket*>(item->user_data);
            // Filter events so we don't get redundant polling notifications.
            // (Will be reset by Socket::Recv() and/or Socket::Send() methods when invoked)
            zmqSocket->SetPollStatus(item->events);
            int tempFlags = zmqSocket->GetPollFlags() & ~item->events;
            zmq_poller_modify(zmq_poller, item->socket, tempFlags | ZMQ_POLLERR);
            zmqSocket->SetEvent();
        }   
    } 
    suspend_mutex.Unlock();
    return ProtoThread::GetExitCode();
}  // end ProtoZmq::PollerThread::RunThread()

bool ProtoZmq::PollerThread::AddSocket(Socket& zmqSocket)
{
    void* zmqContext = zmqSocket.UsingExternalContext() ? zmqSocket.GetContext() : NULL;
    // Make sure poller thread is open and we are in control
    if (!Open(zmqContext, true))
    {
        PLOG(PL_ERROR, "ProtoZmq::PollerThread::AddSocket() error: unable to open/start poller thread!\n");
        return false;
    }
    // At this point, we have an open, running poller thread and we need to signal it and add the socket
    if (!Signal())
    {
        // Another thread shut the poller down?  (This should happen after the Open(), though)
        PLOG(PL_ERROR, "ProtoZmq::PollerThread::AddSocket() error: unable to signal thread!\n");
        suspend_mutex.Unlock();
        return false;
    }   
    if (!AddSocketPrivate(zmqSocket))
    {
        PLOG(PL_ERROR, "ProtoZmq::PollerThread::AddSocket() error: unable to add socket to poller!\n");
        Unsignal();
        suspend_mutex.Unlock();
        return false;   
    }
    Unsignal();
    suspend_mutex.Unlock();
    return true;
}  // end ProtoZmq::PollerThread::AddSocket()

bool ProtoZmq::PollerThread::ModSocket(Socket& zmqSocket)
{
    suspend_mutex.Lock();
    ASSERT (NULL != zmq_poller);
    if (!Signal())
    {
        // Another thread shut the poller down?
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::ModSocket() error: unable to signal poller thread!\n");
        suspend_mutex.Unlock();
        return false;
    }
    if (-1 == zmq_poller_modify(zmq_poller, zmqSocket.GetSocket(), zmqSocket.GetPollFlags() | ZMQ_POLLERR))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::ModSocket() zmq_poller_modify() error: %s\n", zmq_strerror(zmq_errno()));
        Unsignal();
        suspend_mutex.Unlock();
        return false;
    }
    Unsignal();
    suspend_mutex.Unlock();
    return true;
}  // end ProtoZmq::PollerThread::ModSocket()

bool ProtoZmq::PollerThread::AddSocketPrivate(Socket& zmqSocket)
{
    if (socket_count >= event_array_length)
    {
        // Need to increase event_array size to add socket
        unsigned int length = (0 != event_array_length) ? event_array_length*2  : 2;
        zmq_poller_event_t* tempArray = new zmq_poller_event_t[length];
        if (NULL == tempArray)
        {
            PLOG(PL_ERROR, " ProtoZmq::PollerThread::AddSocketPrivate() new event_array error: %s\n", GetErrorString());
            return false;
        }
        if (NULL != event_array) delete[] event_array;  // delete old array
        event_array = tempArray;                        // point to new array
        event_array_length = length;                    // save new length
    }
    if (-1 == zmq_poller_add(zmq_poller, zmqSocket.GetSocket(), &zmqSocket, zmqSocket.GetPollFlags() | ZMQ_POLLERR))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::AddSocketPrivate() zmq_poller_add() error: %s\n", zmq_strerror(zmq_errno()));
        return false;
    }       
    socket_count += 1;
    return true;
}  // end ProtoZmq::PollerThread::AddSocketPrivate()

bool ProtoZmq::PollerThread::RemoveSocket(Socket& zmqSocket)
{
    suspend_mutex.Lock();
    if (NULL == zmq_poller)
    {
        // Already shutdown
        suspend_mutex.Unlock();
        return true;
    }
    if (!Signal())
    {
        // Another thread shut the poller down?
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::ModSocket() error: unable to signal poller thread!\n");
        suspend_mutex.Unlock();
        return false;
    }
    if (-1 == zmq_poller_remove(zmq_poller, zmqSocket.GetSocket()))
    {
        PLOG(PL_ERROR, " ProtoZmq::PollerThread::RemoveSocket() zmq_poller_remove() error: %s\n", zmq_strerror(zmq_errno()));
        Unsignal();
        suspend_mutex.Unlock();
        return false;
    }
    socket_count -= 1;    
    Unsignal();
    if (socket_count < 2)
        ClosePrivate();  // terminate poller thread when there are no active sockets
    suspend_mutex.Unlock();
    return true;
}  // end ProtoZmq::PollerThread::RemoveSocket()

bool ProtoZmq::PollerThread::Signal()
{
    // IMPORTANT - suspend_mutex MUST be locked before callin this!!!
    // stops thread in known state outside of zmq_poller->poll() call
    if (IsStarted() && !IsMyself())
    {
        char dummy = 0;
        unsigned int one;
        // The do loop makes sure that the message is sent regardless of interrupts
        do 
        {
            one = 1;
            if (!break_client.Send(&dummy, one))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::Signal() error: SetBreak() failed!\n");
                return false;
            }
        } while (0 == one);
        signal_mutex.Lock();        
    }
    if (!poller_running)
    {
        // Somebody else closed the poller before we grabbed the signal_mutex
        signal_mutex.Unlock();   
        return false;
    }
    return true;
}  // end ProtoZmq::PollerThread::Signal()

void ProtoZmq::PollerThread::Unsignal()
{
    // Resumes signaled thread in known state outside of zmq_poller->poll() call
    if (IsStarted() && !IsMyself())
    {
        signal_mutex.Unlock();
    }
}  // end ProtoZmq::PollerThread::Unsignal()

