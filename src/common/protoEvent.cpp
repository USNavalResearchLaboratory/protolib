
#include "protoEvent.h"
#include "protoDebug.h"

#ifdef WIN32

#else
#include <string.h>   // for memset()
#include <unistd.h>
#include <fcntl.h>
#ifdef LINUX
#include <sys/eventfd.h>
#endif // LINUX
#endif  // if/else WIN32/UNIX

ProtoEvent::ProtoEvent(bool autoReset)
 : auto_reset(autoReset), notifier(NULL), listener(NULL)
{
#ifdef WIN32
    event_handle = INVALID_HANDLE_VALUE;
#else  // UNIX
#if defined(USE_EVENTFD) || defined(USE_KEVENT)
    event_fd = -1;
#else 
    event_pipe_fd[0] = event_pipe_fd[1] = -1; 
#endif // if/else (USE_EVENTFD | USE_KEVENT) / pipe                  
#endif // if/else WIN32/UNIX
}

ProtoEvent::~ProtoEvent()
{
    Close();
}

bool ProtoEvent::UpdateNotification()
{
    if (NULL != notifier)
    {
        if (IsOpen())
        {
            if (!notifier->UpdateEventNotification(*this, NOTIFY_INPUT))
            {
                PLOG(PL_FATAL, "ProtoEvent::UpdateNotification() error: unable to update notifier\n");
                return false;
            }
        }
        else
        {
            notifier->UpdateEventNotification(*this, NOTIFY_NONE);
        }
    }
    return true;
}  // end ProtoEvent::UpdateNotification()

bool ProtoEvent::SetNotifier(ProtoEvent::Notifier* theNotifier)
{
    if (notifier != theNotifier)
    {
        if (IsOpen())
        {
            // 1) Detach old notifier, if any
            if (NULL != notifier)
                notifier->UpdateEventNotification(*this, NOTIFY_NONE);
            // 2) Set and update new notifier (if applicable)
            notifier = theNotifier;
            if (!UpdateNotification())
            {
                notifier = NULL;  
                return false;
            } 
        }
        else
        {
            notifier = theNotifier;
        }
    }
    return true;
}  // end ProtoEvent::SetNotifier()

void ProtoEvent::OnNotify()
{
    if (NULL != listener) 
        listener->on_event(*this);
}  // end ProtoEvent::OnNotify()

bool ProtoEvent::Open()
{
    if (IsOpen()) return true;
#ifdef WIN32
    // Create initially non-signalled, manual reset event
    event_handle = CreateEvent(NULL, TRUE, FALSE, NULL);  
    if (NULL == event_handle)
    {
        PLOG(PL_FATAL, "ProtoEvent::Init() CreateEvent() error: %s\n", GetErrorString());
		event_handle = INVALID_HANDLE_VALUE;
        return false;
    }
#else  // UNIX
#if defined(USE_EVENTFD)
    // Create eventfd() descriptor
    if (-1 == (event_fd = eventfd(0, 0)))  // TBD - should we set the flag "EFD_NONBLOCK"???
    {
        PLOG(PL_ERROR, "ProtoEvent::Init() eventfd() error: %s\n", GetErrorString());
        return false;
    }
#elif defined(USE_KEVENT)
    // Create a kqueue() (this descriptor may be used by ProtoDispatcher, select(), etc.)
    if (-1 = (event_fd = kqueue()))
    {
        PLOG(PL_ERROR, "ProtoEvent::Init() kqueue() error: %s\n", GetErrorString());
        return false;
    }
    struct kevent kev;
    struct kevent out_kev;
    memset(&kev, 0, sizeof(kev));
    kev.ident = event_fd;
    kev.filter = EVFILT_USER;
    // Even though we set the EV_CLEAR flag here, the ProtoEvent
    // still behaves as manual reset since the ProtoEvent::Reset()
    // has to be called to clear the kqueue() descriptor.  This is
    // analogous to read() on an eventfd() or event_pipe_fd ....
    kev.flags = EV_ADD;
    if (-1 == kevent(event_fd, &kev, 1, NULL, 0, NULL))
    {
        PLOG(PL_ERROR, "ProtoEvent::Init() kqueue() error: %s\n", GetErrorString());
        close(event_fd);
        event_fd = -1;
        return false;
    }
#else 
    if (0 != pipe(event_pipe_fd))
    {
        PLOG(PL_FATAL, "ProtoEvent::Init() pipe() error: %s\n", GetErrorString());
        return false;
    }
    // make reading non-blocking so we can reset the pipe
    if(-1 == fcntl(event_pipe_fd[0], F_SETFL, fcntl(event_pipe_fd[0], F_GETFL, 0)  | O_NONBLOCK))
    {
        PLOG(PL_FATAL, "ProtoEvent::Init() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
        close(event_pipe_fd[0]);
        close(event_pipe_fd[1]);
        event_pipe_fd[0] = event_pipe_fd[1] = -1;
        return false;
    }
#endif // if/else USE_EVENTFD / USE_KEVENT) / pipe             
#endif // if/else WIN32/UNIX    
    if (NULL != notifier)
        notifier->UpdateEventNotification(*this, NOTIFY_INPUT);
    return true;
}  // end ProtoEvent::Open()

bool ProtoEvent::IsOpen() const
{
#ifdef WIN32
    return (INVALID_HANDLE_VALUE != event_handle && NULL != event_handle);
#else  // UNIX
#if defined(USE_EVENTFD) || defined(USE_KEVENT)
    return (-1 != event_fd);
#else 
    return (-1 != event_pipe_fd[0]); 
#endif // if/else USE_EVENTFD / USE_KEVENT) / pipe             
#endif // if/else WIN32/UNIX    
}  // end ProtoEvent::IsOpen()

void ProtoEvent::Close()
{
    if (!IsOpen()) return;
    if (NULL != notifier)
        notifier->UpdateEventNotification(*this, NOTIFY_NONE);
#ifdef WIN32
    CloseHandle(event_handle);
    event_handle = NULL;
#else  // UNIX
#if defined(USE_EVENTFD) || defined(USE_KEVENT)
    close(event_fd);
    event_fd = -1;
#else 
    close(event_pipe_fd[0]);
    close(event_pipe_fd[1]);
    event_pipe_fd[0] = event_pipe_fd[1] = -1; 
#endif // if/else USE_EVENTFD / USE_KEVENT) / pipe             
#endif // if/else WIN32/UNIX  
}  // end ProtoEvent::Close()


bool ProtoEvent::Set()
{
#ifdef WIN32
    if (0 == SetEvent(event_handle))
    {
        PLOG(PL_ERROR, "ProtoEvent::Set() SetEvent() error: %s\n", GetErrorString());
        return false;
    }
#else  // UNIX
#if defined(USE_EVENTFD)
    uint64_t value = 1;
    // TBD - should we make this a non-blocking call?
    if (write(event_fd, &value, sizeof(uint64_t)) < 0)
    {
        PLOG(PL_ERROR, "ProtoEvent::Set() write(eventfd) error: %s\n", GetErrorString());
        return false;
    }
#elif defined (USE_KEVENT)
    struct kevent kev;
    EV_SET(&kev, event_fd, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    if (-1 == kevent(event_fd, &kev, 1, NULL, 0, NULL))
    {
        PLOG(PL_ERROR, "ProtoEvent::Set() kevent() error: %s\n", GetErrorString());
        return false;
    }
#else 
    while (true)
    {
        char byte = 0;
        ssize_t result = write(event_pipe_fd[1], &byte, 1);
        if (1 == result)
        {
            break;
        }
        else if (0 == result)
        {
            PLOG(PL_WARN, "ProtoEvent::Set() warning: write() returned zero\n");
            continue;
        }
        else
        {
            if ((EINTR == errno) || (EAGAIN == errno)) continue;  // (TBD) also for EAGAIN ???
            PLOG(PL_ERROR, "ProtoEvent::Set() write(break_pipe) error: %s\n", GetErrorString());
            return false;
        }
    }
#endif // if/else USE_EVENTFD / USE_KEVENT) / pipe             
#endif // if/else WIN32/UNIX  
    return true;
}  // end ProtoEvent::Set()

bool ProtoEvent::Reset()
{
#ifdef WIN32
    if (0 == ResetEvent(event_handle))
    {
        PLOG(PL_ERROR, "ProtoEvent::Reset() ResetEvent() error: %s\n", GetErrorString());
        return false;
    }
#else  // UNIX
#if defined(USE_EVENTFD)
    // Reset "signal" by reading eventfd count
    uint64_t counter = 0;
    if (read(event_fd, &counter, sizeof(counter)) < 0)
    {
        PLOG(PL_ERROR, "ProtoEvent::Reset() read(event_fd) error: %s\n", GetErrorString());
        return false;
    }
#elif defined(USE_KEVENT)
    struct kevent kev;
    if (-1 == kevent(kq, NULL, 0, &kev, 1, NULL))
    {
        PLOG(PL_ERROR, "ProtoEvent::Reset() kevent() error: %s\n", GetErrorString());
        return false;
    }
    ASSERT(event_fd == kev.ident);
#else
    // Reset "signal" by emptying event_pipe
    char byte[32];
    while (read(event_pipe_fd[0], byte, 32) > 0);
#endif // if/else USE_EVENTFD / USE_KEVENT) / pipe             
#endif // if/else WIN32/UNIX  
    return true;
}  // end ProtoEvent::Reset()

bool ProtoEvent::Wait()
{
#ifdef WIN32
    if (WAIT_OBJECT_0 != WaitForSingleObject(event_handle, INFINITE))
    {
        PLOG(PL_ERROR, "ProtoEvent::Wait()  WaitForSingleObject() error: %s\n", GetErrorString());
        return false;
    }
#else
#if defined(USE_EVENTFD) || defined (USE_KEVENT)
    int eventFd = event_fd;
#else
    int eventFd = event_pipe_fd[0];
#endif
// a "select()" call is good enough here to wait on the event_fd    
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(eventFd ,&fdset);
    if (-1 == select(eventFd+1,& fdset, 0, 0, 0))
    {
        PLOG(PL_ERROR, "ProtoEvent::Wait()  select() error: %s\n", GetErrorString());
        return false;
    }
#endif  // if/else WIN32/UNIX
    if (auto_reset && !Reset())
    {
        PLOG(PL_ERROR, "ProtoEvent::Wait() error: Reset() failed!\n");
        return false;
    }
    return true;
}  // end ProtoEvent::Wait()
