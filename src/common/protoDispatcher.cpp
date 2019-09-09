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
 /**
* @file protoDispatcher.cpp
* 
* @brief This class provides a core around which Unix and Win32 applications using Protolib can be implemented 
*/


#include "protoDispatcher.h"

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <TCHAR.h>
#ifndef _WIN32_WCE
#include <process.h>  // for _beginthreadex() and _endthreadex()
#endif // _WIN32_WCE
const ProtoDispatcher::Descriptor ProtoDispatcher::INVALID_DESCRIPTOR = INVALID_HANDLE_VALUE;
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#ifdef HAVE_SCHED
#include <sched.h>
#else
#include <sys/resource.h>
#endif // HAVE_SCHED
const ProtoDispatcher::Descriptor ProtoDispatcher::INVALID_DESCRIPTOR = -1;
#endif  // if/else WIN32/UNIX


ProtoDispatcher::Stream::Stream(Type theType)
 :  type(theType), flags(0), 
#ifdef WIN32
    index(-1), outdex(-1),
#endif // WIN32
    prev(NULL), next(NULL)
{
}

ProtoDispatcher::SocketStream::SocketStream(ProtoSocket& theSocket)
 : Stream(SOCKET), socket(&theSocket)
{
}

ProtoDispatcher::ChannelStream::ChannelStream(ProtoChannel& theChannel)
 : Stream(CHANNEL), channel(&theChannel)
{
}

ProtoDispatcher::GenericStream::GenericStream(Descriptor theDescriptor)
 : Stream(GENERIC), descriptor(theDescriptor), callback(NULL), client_data(NULL)
{
}
            
ProtoDispatcher::ProtoDispatcher()
    : socket_stream_pool(NULL), socket_stream_list(NULL),
      channel_stream_pool(NULL), channel_stream_list(NULL),
      generic_stream_pool(NULL), generic_stream_list(NULL),  
      run(false), wait_status(-1), exit_code(0), timer_delay(-1), precise_timing(false),
      thread_id((ThreadId)(NULL)), priority_boost(false), thread_started(false),
      thread_master((ThreadId)(NULL)), suspend_count(0), signal_count(0),
      controller(NULL), 
      prompt_set(false), prompt_callback(NULL), prompt_client_data(NULL)
#ifdef USE_TIMERFD
      ,timer_fd(8)
#endif
#ifdef WIN32
      ,stream_array_size(DEFAULT_ITEM_ARRAY_SIZE), 
      stream_handles_array(stream_handles_default), 
      stream_ptrs_array(stream_ptrs_default), 
      stream_count(0), msg_window(NULL),
      break_event(NULL), break_event_stream(NULL),
      socket_io_pending(false)
{
#else
{    
    break_pipe_fd[0] = break_pipe_fd[1] = INVALID_DESCRIPTOR;
#endif // if/else WIN32/UNIX
}

ProtoDispatcher::~ProtoDispatcher()
{
    Destroy();
}

void ProtoDispatcher::Destroy()
{
    Stop();
    while (channel_stream_list) 
    {
        channel_stream_list->GetChannel().SetNotifier(NULL);
        //ReleaseSocketStream(socket_stream_list);
    }
    while (channel_stream_pool)
    {
        ChannelStream* channelStream = channel_stream_pool;
        channel_stream_pool = (ChannelStream*)channelStream->GetNext();
        delete channelStream;   
    }   
    while (socket_stream_list) 
    {
        socket_stream_list->GetSocket().SetNotifier(NULL);
    }
    while (socket_stream_pool)
    {
        SocketStream* socketStream = socket_stream_pool;
        socket_stream_pool = (SocketStream*)socketStream->GetNext();
        delete socketStream;   
    }   
    while (generic_stream_list) RemoveGenericStream(generic_stream_list);    
    while (generic_stream_pool)
    {
        GenericStream* stream = (GenericStream*)generic_stream_pool;
        generic_stream_pool = (GenericStream*)stream->GetNext();
        delete stream;   
    }
#ifdef WIN32
    if (DEFAULT_ITEM_ARRAY_SIZE != stream_array_size)
    {
        delete[] stream_handles_array;
        stream_handles_array = stream_handles_default;
        delete[] stream_ptrs_array;
        stream_ptrs_array = stream_ptrs_default;
        stream_array_size = DEFAULT_ITEM_ARRAY_SIZE;
    }
    stream_count = 0;
    Win32Cleanup();
#endif // WIN32
}  // end ProtoDispatcher::Destroy()

bool ProtoDispatcher::UpdateChannelNotification(ProtoChannel&   theChannel,
                                                int             notifyFlags)
{
    SignalThread();
    // Find stream in our "wait" list, or add it to the list ...
    ChannelStream* channelStream = GetChannelStream(theChannel);
    if (channelStream)
    {
        if (0 != notifyFlags)
        {
#ifdef WIN32
            // Deal with input aspect
            int index = channelStream->GetIndex();
            if (index < 0)
            {
                // Not yet installed for input notification, so install if needed
                if (0 != (notifyFlags & ProtoChannel::NOTIFY_INPUT))
                {
                    if ((index = Win32AddStream(*channelStream, theChannel.GetInputHandle())) < 0)
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error adding input stream\n");
                        if (channelStream->GetOutdex() < 0)
                            ReleaseChannelStream(channelStream);
                        UnsignalThread();
                        return false;
                    }
                    channelStream->SetIndex(index);
                }
            }
            else if (0 == (notifyFlags & ProtoChannel::NOTIFY_INPUT))
            {
                Win32RemoveStream(index);
                channelStream->SetIndex(-1);
            }
            else
            {
                // Update handle in case it has changed
                stream_handles_array[index] = theChannel.GetInputHandle();
            }
            // Deal with output aspect
            int outdex = channelStream->GetOutdex();
            if (outdex < 0)
            {
                if (0 != (notifyFlags & ProtoChannel::NOTIFY_OUTPUT))
                {
                    if ((outdex = Win32AddStream(*channelStream, theChannel.GetOutputHandle())) < 0)
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error adding output stream\n");
                        if (channelStream->GetIndex() < 0)
                            ReleaseChannelStream(channelStream);
                        UnsignalThread();
                        return false;
                    }
                    channelStream->SetOutdex(outdex);
                }
            }
            else if (0 == (notifyFlags & ProtoChannel::NOTIFY_OUTPUT))
            {
                Win32RemoveStream(outdex);
                channelStream->SetOutdex(-1);
            }
            else
            {
                // Update handle in case it has changed
                stream_handles_array[outdex] = theChannel.GetOutputHandle();
            }
#endif // WIN32
            channelStream->SetFlags(notifyFlags); 
            UnsignalThread();    
            return true;  
        }
        else
        {
            ReleaseChannelStream(channelStream);  // remove channel from our "wait" list
            UnsignalThread();
            return true;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() new ChannelStream error: %s\n",
                GetErrorString());
        UnsignalThread();
        return false;   
    }   
}  // end ProtoDispatcher::UpdateChannelNotification()


bool ProtoDispatcher::UpdateSocketNotification(ProtoSocket& theSocket, 
                                               int          notifyFlags)
{
	SignalThread();
    SocketStream* socketStream = GetSocketStream(theSocket);
    if (socketStream)
    {
        if (0 != notifyFlags)
        {
#ifdef WIN32
            if (ProtoSocket::LOCAL != theSocket.GetDomain())
            {
                int index = socketStream->GetIndex();
                if (index < 0)
                {
                    if ((index = Win32AddStream(*socketStream, theSocket.GetInputEventHandle())) < 0)
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error adding handle\n");
                        ReleaseSocketStream(socketStream);
                        UnsignalThread();
                        return false;     
                    }
                    socketStream->SetIndex(index);
                }
                long eventMask = 0;
                if (0 != (notifyFlags & ProtoSocket::NOTIFY_INPUT))
                    eventMask |= (FD_READ | FD_ACCEPT | FD_CLOSE);
                if (0 != (notifyFlags & ProtoSocket::NOTIFY_OUTPUT))
                    eventMask |= (FD_WRITE | FD_CONNECT | FD_CLOSE);
                if (0 != (notifyFlags & ProtoSocket::NOTIFY_EXCEPTION))
                    eventMask |= FD_ADDRESS_LIST_CHANGE;
                // Note for IPv4/IPv6 sockets, the "input event handle" is used for both input & output
                if (0 != WSAEventSelect(theSocket.GetHandle(), theSocket.GetInputEventHandle(), eventMask))
                {
                    ReleaseSocketStream(socketStream);
                    PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() WSAEventSelect(0x%x) error: %s\n",
                            eventMask, ProtoSocket::GetErrorString());
                    UnsignalThread();
                    return false;
                }
            }
            else
            {
                // For "LOCAL" Win32 ProtoSockets (i.e. ProtoPipes) we manage separate input/output
                // event handles for asynchronous I/O
                int index = socketStream->GetIndex();
                if (index < 0)
                {
                    if (0 != (notifyFlags & ProtoSocket::NOTIFY_INPUT))
                    {
                        if ((index = Win32AddStream(*socketStream, theSocket.GetInputEventHandle())) < 0)
                        {
                            PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error adding input stream\n");
                            ReleaseSocketStream(socketStream);
                            UnsignalThread();
                            return false;
                        }
                        socketStream->SetIndex(index);
                    }
                }
                else if (0 == (notifyFlags & ProtoSocket::NOTIFY_INPUT))
                {
                    Win32RemoveStream(index);
                    socketStream->SetIndex(-1);
                }
                else
                {
                    stream_handles_array[index] = theSocket.GetInputEventHandle();
                }
                int outdex = socketStream->GetOutdex();
                if (outdex < 0)
                {
                    if (0 != (notifyFlags & ProtoSocket::NOTIFY_OUTPUT))
                    {
                        if ((outdex = Win32AddStream(*socketStream, theSocket.GetOutputEventHandle())) < 0)
                        {
                            PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error adding output stream\n");
                            ReleaseSocketStream(socketStream);
                            UnsignalThread();
                            return false;
                        }
                        socketStream->SetOutdex(index);
                    }
                }
                else if (0 == (notifyFlags & ProtoSocket::NOTIFY_OUTPUT))
                {
                    Win32RemoveStream(outdex);
                    socketStream->SetOutdex(-1);
                }
                else
                {
                    stream_handles_array[outdex] = theSocket.GetOutputEventHandle();
                }
            }
#endif // WIN32
            socketStream->SetFlags(notifyFlags);
        }
        else
        {
#ifdef WIN32
            if (socketStream->HasFlags() && ProtoSocket::LOCAL != theSocket.GetDomain())
            {
                if (0 != WSAEventSelect(theSocket.GetHandle(), theSocket.GetInputEventHandle(), 0))
                     PLOG(PL_WARN, "ProtoDispatcher::UpdateSocketNotification() WSAEventSelect(0) warning: %s\n",
                              ProtoSocket::GetErrorString());
            }
#endif // WIN32  
            ReleaseSocketStream(socketStream);
        }
        UnsignalThread();
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() new SocketStream error: %s\n",
                GetErrorString());
        UnsignalThread();
        return false;   
    }
}  // end ProtoDispatcher::UpdateSocketNotification()

ProtoDispatcher::SocketStream* ProtoDispatcher::GetSocketStream(ProtoSocket& theSocket)
{
    // First, search our list of active sockets
    SocketStream* socketStream = socket_stream_list;
    while (NULL != socketStream)
    {
        if (&theSocket == &socketStream->GetSocket())
            break;
        else
            socketStream = (SocketStream*)socketStream->GetNext();  
    }
    if (NULL == socketStream)
    {
        // Get one from the pool or create a new one
        socketStream = socket_stream_pool;
        if (NULL != socketStream)
        {
            socket_stream_pool = (SocketStream*)socketStream->GetNext();
            socketStream->ClearFlags();
            socketStream->SetSocket(theSocket);
        }
        else
        {
            if (!(socketStream = new SocketStream(theSocket)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::GetSocketStream() new SocketStream error: %s\n", GetErrorString());
                return NULL;   
            }
        }
        // Prepend to "active" socket stream list
        socketStream->SetPrev(NULL);
        socketStream->SetNext(socket_stream_list);
        if (socket_stream_list) 
            socket_stream_list->SetPrev(socketStream);
        socket_stream_list = socketStream;
    }
    return socketStream;
}  // end ProtoDispatcher::GetSocketStream()

void ProtoDispatcher::ReleaseSocketStream(SocketStream* socketStream)
{
    socketStream->ClearFlags();
    SocketStream* prevStream = (SocketStream*)socketStream->GetPrev();
    SocketStream* nextStream = (SocketStream*)socketStream->GetNext();
    if (prevStream)
        prevStream->SetNext(nextStream);
    else
        socket_stream_list = nextStream;
    if (nextStream) nextStream->SetPrev(prevStream);
    socketStream->SetNext(socket_stream_pool);
    socket_stream_pool = socketStream;
#ifdef WIN32
    if (socketStream->GetIndex() >= 0)
    {
        Win32RemoveStream(socketStream->GetIndex());
        socketStream->SetIndex(-1);
    }
    if (socketStream->GetOutdex() >= 0)
    {
        Win32RemoveStream(socketStream->GetOutdex());
        socketStream->SetOutdex(-1);
    }
#endif // WIN32
}  // end ProtoDispatcher::ReleaseSocketStream()


ProtoDispatcher::ChannelStream* ProtoDispatcher::GetChannelStream(ProtoChannel& theChannel)
{
    // First, search our list of active channels
    ChannelStream* channelStream = channel_stream_list;
    while (channelStream)
    {
        if (&theChannel == &channelStream->GetChannel())
            break;
        else
            channelStream = (ChannelStream*)channelStream->GetNext();  
    }
    if (!channelStream)
    {
        // Get one from the pool or create a new one
        channelStream = channel_stream_pool;
        if (channelStream)
        {
            channel_stream_pool = (ChannelStream*)channelStream->GetNext();
            channelStream->ClearFlags();
            channelStream->SetChannel(theChannel);
        }
        else
        {
            if (!(channelStream = new ChannelStream(theChannel)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::GetChannelStream() new ChannelStream error: %s\n", GetErrorString());
                return NULL;   
            }
        }
        // Prepend to "active" channel stream list
        channelStream->SetPrev(NULL);
        channelStream->SetNext(channel_stream_list);
        if (channel_stream_list) 
            channel_stream_list->SetPrev(channelStream);
        channel_stream_list = channelStream;
    }
    return channelStream;
}  // end ProtoDispatcher::GetChannelStream()

void ProtoDispatcher::ReleaseChannelStream(ChannelStream* channelStream)
{
    channelStream->ClearFlags();
    ChannelStream* prevStream = (ChannelStream*)channelStream->GetPrev();
    ChannelStream* nextStream = (ChannelStream*)channelStream->GetNext();
    if (prevStream)
        prevStream->SetNext(nextStream);
    else
        channel_stream_list = nextStream;
    if (nextStream) nextStream->SetPrev(prevStream);
    channelStream->SetNext(channel_stream_pool);
    channel_stream_pool = channelStream;
#ifdef WIN32
    if (channelStream->GetIndex() >= 0)
    {
        Win32RemoveStream(channelStream->GetIndex());
        channelStream->SetIndex(-1);
    }
    if (channelStream->GetOutdex() >= 0)
    {
        Win32RemoveStream(channelStream->GetOutdex());
        channelStream->SetOutdex(-1);
    }
#endif // WIN32
}  // end ProtoDispatcher::ReleaseChannelStream()

bool ProtoDispatcher::InstallGenericStream(ProtoDispatcher::Descriptor descriptor, 
                                           Callback*                   callback, 
                                           const void*                 userData,
                                           Stream::Flag                flag)
{   
    GenericStream* stream = GetGenericStream(descriptor);
    if (stream)
    {
#ifdef WIN32
        int index = stream->GetIndex();
        if (index < 0)
        {
            if ((index = Win32AddStream(*stream, descriptor)) < 0)
            {
                PLOG(PL_ERROR, "ProtoDispatcher::InstallGenericStream() error adding stream\n");
                ReleaseGenericStream(stream);
                return false;   
            }   
            stream->SetIndex(index);
        }
        else
        {
            stream_handles_array[index] = descriptor;
        }
#endif // WIN32
        stream->SetCallback(callback, userData);
        stream->SetFlag(flag);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallGenericStream() error getting GenericStream\n");
        return false;
    }
}  // end ProtoDispatcher::InstallGenericStream()

ProtoDispatcher::GenericStream* ProtoDispatcher::GetGenericStream(Descriptor descriptor)
{
    // First, search our list of active generic streams
    GenericStream* stream = generic_stream_list;
    while (stream)
    {
        if (descriptor == stream->GetDescriptor())
            break;
        else
            stream = (GenericStream*)stream->GetNext();   
    }
    if (!stream)
    {
        // Get one from the pool or create a new one
        stream = generic_stream_pool;
        if (stream)
        {
            generic_stream_pool = (GenericStream*)stream->GetNext();
            stream->ClearFlags();
            stream->SetDescriptor(descriptor);
        }
        else
        {
            if (!(stream = new GenericStream(descriptor)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::GetGenericStream() new GenericStream error: %s\n", GetErrorString());
                return NULL;   
            }
        }
        // Add to "active" generic stream list
        stream->SetPrev(NULL);
        stream->SetNext(generic_stream_list);
        if (generic_stream_list) generic_stream_list->SetPrev(stream);
        generic_stream_list = stream;
    }
    return stream;
}  // end ProtoDispatcher::GetGenericStream()

ProtoDispatcher::GenericStream* ProtoDispatcher::FindGenericStream(Descriptor descriptor) const
{
    GenericStream* next = generic_stream_list;
    while (next)
    {
        if (next->GetDescriptor() == descriptor)
            return next;
        else
            next = (GenericStream*)next->GetNext();
    }
    return NULL;
}  // end ProtoDispatcher::FindGenericStream()

void ProtoDispatcher::ReleaseGenericStream(GenericStream* stream)
{
    stream->ClearFlags();
    // Move from "active" generic stream list to generic stream "pool"
    GenericStream* prevStream = (GenericStream*)stream->GetPrev();
    GenericStream* nextStream = (GenericStream*)stream->GetNext();
    if (prevStream)
        prevStream->SetNext(nextStream);
    else
        generic_stream_list = nextStream;
    if (nextStream) nextStream->SetPrev(prevStream);
    stream->SetNext(generic_stream_pool);
    generic_stream_pool = stream;
#ifdef WIN32
    if (stream->GetIndex() >= 0)
    {
        Win32RemoveStream(stream->GetIndex());
        stream->SetIndex(-1);
    }
#endif // WIN32
}  // end ProtoDispatcher::ReleaseGenericStream()


/**
 * @brief Boost process priority for real-time operation
 */
bool ProtoDispatcher::BoostPriority()
{
#ifdef WINCE
    PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() not supported on WinCE");
    return false;
#elif defined(WIN32)
    ThreadId threadId = IsThreaded() ? thread_id : ::GetCurrentThreadId();
    HANDLE threadHandle = OpenThread(THREAD_SET_INFORMATION, FALSE, threadId);
    if (NULL == threadHandle)
    {
        PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() error: OpenThread() error: %s\n", GetErrorString());
        return false;
    }
    if (!SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL))
    {
	    PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() error: SetThreadPriority() error: %s\n", GetErrorString());
        CloseHandle(threadHandle);
        return false;
    }
    CloseHandle(threadHandle);
#else
    //int who = IsThreaded() ? thread_id : 0;
#ifdef HAVE_SCHED
    // (This _may_ work on Linux-only at this point)
    // (TBD) We should probably look into pthread-setschedparam() instead
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority =  sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &schp))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() error: sched_setscheduler() error: %s\n", GetErrorString());
            return false;
        }   
    }
#else
    // (TBD) Do something differently if "pthread sched param"?
    if (0 != setpriority(PRIO_PROCESS, getpid(), -20))
    {
        PLOG(PL_ERROR, "PrototDispatcher::BoostPriority() error: setpriority() error: %s\n", GetErrorString());
        return false;
    }
#endif // HAVE_SCHED
#endif // if/else WIN32/UNIX
    return true;
}  // end ProtoDispatcher::BoostPriority()


/**
 * A ProtoDispatcher "main loop"
 */
int ProtoDispatcher::Run(bool oneShot)
{
    exit_code = 0;
    wait_status = -1;  
    if (priority_boost) BoostPriority();
    
#ifdef USE_TIMERFD
    timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd < 0)
    {
        PLOG(PL_ERROR, "ProtoDispatcher::Run() timerfd_create() error: %s\n", GetErrorString());
        return -1;  // TBD - is there a more specific exitCode we should use instead???
    }
#endif // USE_TIMERFD    
    // TBD - should we keep "run" true while in the do loop so "IsRunning()" 
    //       can be interpreted properly (or just deprecate IsRunning() ???
    run = oneShot ? false : true;
    do
    { 
        if (IsPending())
        {
            // Here we "latch" the next timeout _before_
            // we open the suspend window to be safe
            timer_delay =  ProtoTimerMgr::GetTimeRemaining();
            if (IsThreaded())
            {
                Lock(signal_mutex);
                Unlock(suspend_mutex);
                Wait();
                Unlock(signal_mutex);
                Lock(suspend_mutex);
                
                if (prompt_set)
                {
                
                    if (NULL != prompt_callback)
                         prompt_callback(prompt_client_data);
                    prompt_set = false;
                }
                
                if (WasSignaled())
                {
                    continue;
                }
                else if (controller)
                {
                    // Relinquish to controller thread
                    Unlock(suspend_mutex);
                    controller->DoDispatch();
                    Lock(suspend_mutex);   
                }
                else
                {
                    // Do our own event dispatching
                    Dispatch();
                }
            }
            else
            {
                Wait();
                Dispatch();   
            }
        }
        else
        {
            PLOG(PL_DEBUG, "ProtoDispatcher::Run() would be stuck with infinite timeout & no inputs!\n");
            break;
        }
    }  while (run);
#ifdef USE_TIMERFD
    close(timer_fd);
    timer_fd = INVALID_DESCRIPTOR;
#endif // USE_TIMERFD
    return exit_code;
}  // end ProtoDispatcher::Run()

void ProtoDispatcher::Stop(int exitCode)
{
    if (controller)
    {
        controller->OnThreadStop();
        controller = NULL;
    }
    SignalThread();
    exit_code = run ? exitCode : exit_code;
    run = false;
#ifdef WIN32
    if (msg_window)
        PostMessage(msg_window, WM_DESTROY, NULL, NULL);
#endif // WIN32
    UnsignalThread();
    DestroyThread();
}  // end ProtoDispatcher::Stop();


#ifdef WIN32
#ifdef _WIN32_WCE
DWORD WINAPI ProtoDispatcher::DoThreadStart(LPVOID param)
#else
unsigned int __stdcall ProtoDispatcher::DoThreadStart(void* param)
#endif // if/else _WIN32_WCE
#else
void* ProtoDispatcher::DoThreadStart(void* param)
#endif // if/else WIN32/UNIX
{
    ProtoDispatcher* dp = reinterpret_cast<ProtoDispatcher*>(param);
    ASSERT(NULL != dp);
    if (NULL != dp->controller) Lock(dp->controller->lock_b);
    Lock(dp->suspend_mutex);
    dp->thread_started = true;
    dp->exit_status = dp->Run();  // TBD - should have Run() set exit_status internally instead???
    Unlock(dp->suspend_mutex);
    DoThreadExit(dp->GetExitStatus());
    return (dp->GetExitStatus());
}  // end ProtoDispatcher::DoThreadStart()


bool ProtoDispatcher::StartThread(bool                         priorityBoost,
                                  ProtoDispatcher::Controller* theController)
{
    if (IsThreaded())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::StartThread() error: thread already started\n");
        return false;
    }
    priority_boost = priorityBoost;
    if (!InstallBreak())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::StartThread() error: InstallBreak() failed\n");
        return false;
    }
    controller = theController;
    Init(suspend_mutex);
    Init(signal_mutex);
    Lock(suspend_mutex);
#ifdef WIN32
    // Note: For WinCE, we need to use CreateThread() instead of _beginthreadex()
#ifdef _WIN32_WCE
    if (!(actual_thread_handle = CreateThread(NULL, 0, DoThreadStart, this, 0, &thread_id)))
#else
    if (!(actual_thread_handle = (HANDLE)_beginthreadex(NULL, 0, DoThreadStart, this, 0, (unsigned*)&thread_id)))
#endif // if/else _WIN32_WCE
#else
    if (0 != pthread_create(&thread_id, NULL, DoThreadStart, this))    
#endif // if/else WIN32/UNIX
    {
        PLOG(PL_ERROR, "ProtoDispatcher::StartThread() create thread error: %s\n", GetErrorString());
        RemoveBreak();
        Unlock(suspend_mutex);
        thread_id = (ThreadId)NULL;
        controller = NULL;
        return false;
    }
    Unlock(suspend_mutex);
    return true;
}  // end ProtoDispatcher::StartThread()

/**
 * This brings the thread to a suspended state _outside_ 
 * of _its_ ProtoDispatcher::Wait() state
 */
bool ProtoDispatcher::SignalThread()
{
    SuspendThread();
    if (IsThreaded() && !IsMyself())
    {
        if (signal_count > 0)
        {
            signal_count++;
            return true;
        }
        else
        {
#ifdef WIN32
            if (0 == SetEvent(break_event))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::SignalThread() SetEvent(break_event) error\n");
                ResumeThread();
                return false;
            }
#else
            while (1)
            {
                char byte;
                int result = write(break_pipe_fd[1], &byte, 1);
                if (1 == result)
                {
                    break;
                }
                else if (0 == result)
                {
                    PLOG(PL_ERROR, "ProtoDispatcher::SignalThread() warning: write() returned zero\n");
                    continue;
                }
                else
                {
                    if (EINTR == errno) continue;  // (TBD) also for EAGAIN ???
                    PLOG(PL_ERROR, "ProtoDispatcher::SignalThread() write() error: %s\n",
                            GetErrorString());
                    ResumeThread();
                    return false;
                }
            }
#endif // if/else WIN32/UNIX
            Lock(signal_mutex);
            signal_count = 1;
        }
    }
    return true;   
}  // end ProtoDispatcher::SignalThread()

void ProtoDispatcher::UnsignalThread()
{
    if (IsThreaded() && !IsMyself() && (thread_master == GetCurrentThread()))
    {
        ASSERT(0 != signal_count);
        signal_count--;
        if (0 == signal_count) 
            Unlock(signal_mutex);  
    }
    ResumeThread(); 
}  // end ProtoDispatcher::UnsignalThread()

bool ProtoDispatcher::SuspendThread()
{
    if (IsThreaded() && !IsMyself())
    {
        if (GetCurrentThread() == thread_master)
        {
            suspend_count++;
            return true;   
        }
        // We use while() here to make sure the thread started
        // (TBD) use a spin_count to limit iterations as safeguard
        while (!thread_started);
        Lock(suspend_mutex);  // TBD - check result of "Lock()" 
        thread_master = GetCurrentThread();
        suspend_count = 1;
    }
    return true;
}  // end ProtoDispatcher::SuspendThread()

void ProtoDispatcher::ResumeThread()
{
    if (IsThreaded() && !IsMyself())
    {
        if (GetCurrentThread() == thread_master)
        {
            if (suspend_count > 1)
            {
                suspend_count--;
            }
            else
            {
                thread_master = (ThreadId)NULL;
                suspend_count = 0;
                Unlock(suspend_mutex);
            }
        }
    }
}  // end ProtoDispatcher::ResumeThread()

void ProtoDispatcher::DestroyThread()
{
    if (IsThreaded())
    { 
        controller = NULL;
#ifdef WIN32
        if (!IsMyself()) WaitForSingleObject(actual_thread_handle, INFINITE);
        thread_started = false;
#ifdef _WIN32_WCE
        CloseHandle(actual_thread_handle);
        actual_thread_handle = NULL;
#endif // _WIN32_WCE
#else
        if (!IsMyself()) pthread_join(thread_id, NULL);
#endif // if/else WIN32/UNIX
        thread_id = (ThreadId)NULL;
        RemoveBreak();
        Destroy(suspend_mutex);
        Destroy(signal_mutex);
    }
}  // end ProtoDispatcher::DestroyThread()

/////////////////////////////////////////////////////////////////////////
// Unix-specific methods and implementation
#ifdef UNIX

/** 
 * (TBD) It would be nice to use something like SIGUSR or something to break
 * out of the select() (or pselect()) call in ProtoDispatcher::Wait()
 * instead of a pipe() ... but to support threaded operation and do
 * it right, we would need something like pthread-sigsetjmp() to deal
 * with the "edge-triggered" nature issue of signals, and/or we
 * might wish to consider kevent() for systems like BSD, etc
 */
bool ProtoDispatcher::InstallBreak()
{    
    if (0 != pipe(break_pipe_fd))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() pipe() error: %s\n",
                GetErrorString());
        return false;
    }
    // make reading non-blocking
    if(-1 == fcntl(break_pipe_fd[0], F_SETFL, fcntl(break_pipe_fd[0], F_GETFL, 0)  | O_NONBLOCK))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end ProtoDispatcher::InstallBreak()

void ProtoDispatcher::RemoveBreak()
{
    if (INVALID_DESCRIPTOR != break_pipe_fd[0])
    {
        close(break_pipe_fd[0]);
        close(break_pipe_fd[1]);
        break_pipe_fd[0] = INVALID_DESCRIPTOR;
    }
}  // end ProtoDispatcher::RemoveBreak()

/**
 * Warning! This may block indefinitely iF !IsPending() ...
 */
void ProtoDispatcher::Wait()
{
    // (TBD) We could put some code here to protect this from
    // being called by the wrong thread?
    
#ifdef USE_TIMERFD
#define HAVE_PSELECT // so we can use the "struct timespec" created here
#endif  // USE_TIMERFD    
    
#ifdef HAVE_PSELECT
    struct timespec timeout;
    struct timespec* timeoutPtr;
#else
    struct timeval timeout;
    struct timeval* timeoutPtr;
#endif // if/else HAVE_PSELECT
    //double timerDelay = ProtoTimerMgr::GetTimeRemaining();
    double timerDelay = timer_delay;
    if (timerDelay < 0.0)
    {
        timeoutPtr = NULL; 
#ifdef USE_TIMERFD
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;
#endif // USE_TIMERFD     
    }
    else
    {
        // If (true == precise_timing) essentially force polling for small delays
        // (Note this will consume CPU resources)
        if (precise_timing && (timerDelay < 0.010)) timerDelay = 0.0;
        timeout.tv_sec = (unsigned long)timerDelay;
#ifdef HAVE_PSELECT
        timeout.tv_nsec = 
            (unsigned long)(1.0e+09 * (timerDelay - (double)timeout.tv_sec));
#else
        timeout.tv_usec = 
            (unsigned long)(1.0e+06 * (timerDelay - (double)timeout.tv_sec));
#endif // if/else HAVE_PSELECT
        timeoutPtr = &timeout;
    }
    
    FD_ZERO(&input_set);
    FD_ZERO(&output_set);
    int maxDescriptor = -1;
    
    
#ifdef USE_TIMERFD
    if ((0 != timeout.tv_nsec) || (0 != timeout.tv_sec))
    {
        // Install the timerfd descriptor to our "input_set"
        // configured with an appropriate one-shot timeout
        struct itimerspec timerSpec;
        timerSpec.it_interval.tv_sec =  timerSpec.it_interval.tv_nsec = 0;  // non-repeating timeout?
        timerSpec.it_value = timeout;
        if (0 == timerfd_settime(timer_fd, 0, &timerSpec, 0))
        {
            //TRACE("putting timer_fd %d input input_set with sec:%d nsec:%d\n", 
            //        timer_fd, timerSpec.it_value.tv_sec, timerSpec.it_value.tv_nsec);
            FD_SET(timer_fd, &input_set);
            maxDescriptor = timer_fd;
            timeoutPtr = NULL;  // select() will be using "timer_fd" instead
        }
        else
        {
            PLOG(PL_ERROR, "ProtoDispatcher::Wait() timerfd_settime() error: %s\n", GetErrorString());
        }
    }
#endif // USE_TIMERFD
    
    
    // Monitor "break_pipe" if we are a threaded dispatcher
    // TBD - change this to use "eventfd()" on Linux
    if (IsThreaded())
    {
        FD_SET(break_pipe_fd[0], &input_set);  
        maxDescriptor = break_pipe_fd[0];
    }
    // Monitor socket streams ...
    SocketStream* nextSocket = socket_stream_list;
    while (nextSocket)
    {
        Descriptor descriptor = nextSocket->GetSocket().GetHandle();
        if (nextSocket->IsInput()) FD_SET(descriptor, &input_set);
        if (nextSocket->IsOutput()) FD_SET(descriptor, &output_set);
        if (descriptor > maxDescriptor) maxDescriptor = descriptor;
        nextSocket = (SocketStream*)nextSocket->GetNext();    
    }
    // Monitor channel streams ...
    ChannelStream* nextChannel = channel_stream_list;
    while (nextChannel)
    {
        Descriptor descriptor = nextChannel->GetChannel().GetHandle();
        if (nextChannel->IsInput()) FD_SET(descriptor, &input_set);
        if (nextChannel->IsOutput()) FD_SET(descriptor, &output_set);
        if (descriptor > maxDescriptor) maxDescriptor = descriptor;
        nextChannel = (ChannelStream*)nextChannel->GetNext();    
    }
    // Monitor generic streams ...
    GenericStream* nextStream = generic_stream_list;
    while (nextStream)
    {
        Descriptor descriptor = nextStream->GetDescriptor();
        if (nextStream->IsInput()) FD_SET(descriptor, &input_set);
        if (nextStream->IsOutput()) FD_SET(descriptor, &output_set);
        if (descriptor > maxDescriptor) maxDescriptor = descriptor;
        nextStream = (GenericStream*)nextStream->GetNext();    
    }
    
    // (TBD) It might be nice to use "pselect()" here someday
#ifdef HAVE_PSELECT
    wait_status = pselect(maxDescriptor+1, 
                         (fd_set*)&input_set, 
                         (fd_set*)&output_set, 
                         (fd_set*) NULL, 
                         timeoutPtr,
                         (sigset_t*)NULL);
#else    
    wait_status = select(maxDescriptor+1, 
                         (fd_set*)&input_set, 
                         (fd_set*)&output_set, 
                         (fd_set*) NULL, 
                         timeoutPtr);
#endif  // if/else HAVE_PSELECT
}  // end ProtoDispatcher::Wait()

void ProtoDispatcher::Dispatch()
{
    
    switch (wait_status)
    {
        case -1:
            if (EINTR != errno)
                PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() select() error: %s\n", GetErrorString());
            //OnSystemTimeout();
            break;

        case 0:
            // timeout only ?
            OnSystemTimeout();
            break;

        default:
            // (TBD) make this safer (we need a safe iterator for these)...
            // Check socket streams ...
            SocketStream* nextSocketStream = socket_stream_list;
            while (nextSocketStream)
            {
                SocketStream* savedNext = (SocketStream*)nextSocketStream->GetNext();
                ProtoSocket& theSocket = nextSocketStream->GetSocket();
                Descriptor descriptor = theSocket.GetHandle();
                if (nextSocketStream->IsInput() && FD_ISSET(descriptor, &input_set))
                {
                    theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                    //break;
                }
                if (nextSocketStream->IsOutput() && FD_ISSET(descriptor, &output_set))
                {
                    theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                    //break;
                }
                nextSocketStream = savedNext;
            }
            // Check channel streams ...
            ChannelStream* nextChannelStream = channel_stream_list;
            while (nextChannelStream)
            {
                ChannelStream* savedNext = (ChannelStream*)nextChannelStream->GetNext();
                ProtoChannel& theChannel = nextChannelStream->GetChannel();
                Descriptor descriptor = theChannel.GetHandle();
                if (nextChannelStream->IsInput() && FD_ISSET(descriptor, &input_set))
                {
                    theChannel.OnNotify(ProtoChannel::NOTIFY_INPUT);
                    //break;
                }
                if (nextChannelStream->IsOutput() && FD_ISSET(descriptor, &output_set))
                {
                    theChannel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                    //break;
                }
                nextChannelStream = savedNext;
            }
            // Check generic streams ...
            GenericStream* nextStream = generic_stream_list;
            while (nextStream)
            {
                GenericStream* savedNext = (GenericStream*)nextStream->GetNext(); 
                Descriptor descriptor = nextStream->GetDescriptor();
                if (nextStream->IsInput() && FD_ISSET(descriptor, &input_set))
                {
                    nextStream->OnEvent(EVENT_INPUT);
                    //break;
                }
                if (nextStream->IsOutput() && FD_ISSET(descriptor, &output_set))
                {
                    nextStream->OnEvent(EVENT_OUTPUT);
                    //break;
                }
                nextStream = savedNext; 
            }
#ifdef USE_TIMERFD
            // TBD - should we put this code up at the top as the first check
            // intead of checking all descriptors, all the time (maybe "epoll()" will help?)
            if (FD_ISSET(timer_fd, &input_set))
            {
                //TRACE("timer_fd was set\n");
                // clear the timer_fd status by reading from it
                uint64_t expirations = 0;
                if (read(timer_fd, &expirations, sizeof(expirations)) < 0)
                    PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() read(timer_fd) error: %s\n", GetErrorString());
            }
#endif // USE_TIMERFD
            OnSystemTimeout();
            break;
    }  // end switch(status)
}  // end ProtoDispatcher::Dispatch()

#endif // UNIX


#ifdef WIN32

bool ProtoDispatcher::InstallBreak()
{
    // The break_event used to break the MsgWaitForMultipleObjectsEx() call
    // This needs to be manual reset?
    if (!(break_event = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() CreateEvent() error\n");
        return false;
    }
    if (InstallGenericStream(break_event, NULL, NULL, Stream::INPUT))
    {
        break_event_stream = GetGenericStream(break_event);   
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() CreateEvent() error\n");
        CloseHandle(break_event);
        break_event = NULL;
        return false;
    }
    return true;
}  // end ProtoDispatcher::InstallBreak()

void ProtoDispatcher::RemoveBreak()
{
    if (NULL != break_event)
    {
        RemoveGenericStream(break_event_stream);
        break_event_stream = NULL;
        CloseHandle(break_event);
        break_event = NULL;
    }
}  // end ProtoDispatcher::RemoveBreak()

bool ProtoDispatcher::Win32Init()
{
    HINSTANCE theInstance = GetModuleHandle(NULL);    
    // Register our msg_window class
    WNDCLASS cl;
    cl.style = CS_HREDRAW | CS_VREDRAW;
    cl.lpfnWndProc = MessageHandler;
    cl.cbClsExtra = 0;
    cl.cbWndExtra = 0;
    cl.hInstance = theInstance;
    cl.hIcon = NULL;
    cl.hCursor = NULL;
    cl.hbrBackground = NULL;
    cl.lpszMenuName = NULL;
    cl.lpszClassName = _T("ProtoDispatcher");
    if (!RegisterClass(&cl))
    {
        LPVOID lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                      FORMAT_MESSAGE_FROM_SYSTEM | 
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, GetLastError(),
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                      (LPTSTR) &lpMsgBuf, 0, NULL );
        // Display the string.
        MessageBox( NULL, (LPCTSTR)lpMsgBuf, (LPCTSTR)"Error", MB_OK | MB_ICONINFORMATION );
        // Free the buffer.
        LocalFree(lpMsgBuf);
        PLOG(PL_ERROR, "ProtoDispatcher::Win32Init() Error registering message window class!\n");
        return false;
    }

    // Create msg_window to receive event messages
    HWND parent = NULL;
#ifdef HWND_MESSAGE
    parent = HWND_MESSAGE;    
#endif // HWND_MESSAGE
    msg_window = CreateWindowEx(0, _T("ProtoDispatcher"),  // registered class name
                              _T("ProtoDispatcher"),  // window name
                              WS_OVERLAPPED,               // window style
                              CW_USEDEFAULT,               // horizontal position of window
                              CW_USEDEFAULT,               // vertical position of window
                              0,              // window width
                              0,             // window height
                              parent,  // handle to parent or owner window
                              NULL,          // menu handle or child identifier
                              theInstance,  // handle to application instance
                              this);        // window-creation data
    if (msg_window)
    {
        ShowWindow(msg_window, SW_HIDE);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::Win32Init() Error creating message window: %s (%d)\n", 
                GetErrorString(), GetLastError());
        return false;
    }
}  // end ProtoDispatcher::Win32Init()

void ProtoDispatcher::Win32Cleanup()
{
    if (msg_window)
    {
        DestroyWindow(msg_window);
        msg_window = NULL;
    }
}  // end Win32Cleanup()

int ProtoDispatcher::Win32AddStream(Stream& stream, HANDLE handle)
{
    int index = stream_count;
    if (stream_count >= stream_array_size)
    {
        if (!Win32IncreaseStreamArraySize())
        {
            PLOG(PL_ERROR, "ProtoDispatcher::AddStream() error increasing array size\n");
            return -1;
        }   
    }
    stream_handles_array[index] = handle;
    stream_ptrs_array[index] = &stream;
    stream_count++;        
    return index;
}  // end ProtoDispatcher::Win32AddStream()

void ProtoDispatcher::Win32RemoveStream(int index)
{ 
    ASSERT((index >= 0) && (index < (int)stream_count));
    unsigned int moveCount = stream_count - index - 1;
    memmove(stream_handles_array+index, stream_handles_array+index+1, 
            moveCount*sizeof(HANDLE));
    memmove(stream_ptrs_array+index, stream_ptrs_array+index+1,
            moveCount*sizeof(Stream*));
    stream_count--;
    for (; index < (int)stream_count; index++)
        stream_ptrs_array[index]->SetIndex(index);
}  // end ProtoDispatcher::Win32RemoveStream

bool ProtoDispatcher::Win32IncreaseStreamArraySize()
{
    unsigned int newSize = 2 * stream_array_size;
    HANDLE* hPtr = new HANDLE[newSize];
    Stream** iPtr = new Stream*[newSize];
    if (hPtr && iPtr)
    {
        memcpy(hPtr, stream_handles_array, stream_count*sizeof(HANDLE));
        memcpy(iPtr, stream_ptrs_array, stream_count*sizeof(Stream*));
        if (stream_handles_default != stream_handles_array)
            delete[] stream_handles_array;
        if (stream_ptrs_default != stream_ptrs_array)
            delete[] stream_ptrs_array;
        stream_handles_array = hPtr;
        stream_ptrs_array = iPtr;
        return true;
    }
    else
    {
        if (hPtr) delete[] hPtr;
        if (iPtr) delete[] iPtr;
        return false;    
    }        
}  // end ProtoDispatcher::Win32IncreaseStreamArraySize()

void ProtoDispatcher::Wait()
{
    double timerDelay = timer_delay;
    DWORD msec = (timerDelay < 0.0) ? INFINITE : ((DWORD)(1000.0 * timerDelay));

    // If (false == precise_timing) enforce minimum delay of 1 msec
    // (Note "precise_timing" will consume additional CPU resources)
    if ((timerDelay > 0.0) && (0 == msec) && !precise_timing) msec = 1;

    // Don't wait if any sockets are still "OutputReady()"
    // Monitor socket streams ...
    SocketStream* nextSocket = socket_stream_list;
    while (nextSocket)
    {
        ProtoSocket& theSocket = nextSocket->GetSocket();
        if (theSocket.IsInputReady() ||
           (theSocket.NotifyOutput() && theSocket.IsOutputReady()))
        {
            msec = 0;
            socket_io_pending = true;
            break;
        }
        nextSocket = (SocketStream*)nextSocket->GetNext();    
    }
    // Set some waitFlags
    DWORD waitFlags = 0;
    // on WinNT 4.0, getaddrinfo() doesn't work, so we check the OS version
    // to decide what to do.  Try "gethostbyaddr()" if it's an old OS (e.g. NT 4.0 or earlier)
    OSVERSIONINFO vinfo;
    vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&vinfo);
    if ((VER_PLATFORM_WIN32_NT == vinfo.dwPlatformId) &&
        ((vinfo.dwMajorVersion > 4) ||
            ((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion > 0))))
                waitFlags |= MWMO_INPUTAVAILABLE;  // it's a modern OS where MWMO_INPUTAVAILABLE should work!
    // (TBD) should MWMO_INPUTAVAILABLE be set fo Win98 type OS's???
#ifndef _WIN32_WCE
    waitFlags |= MWMO_ALERTABLE;
#endif // !_WIN32_WCE
    wait_status =  MsgWaitForMultipleObjectsEx(stream_count,   // number of handles in array
                                               stream_handles_array, // object-handle array
                                               msec,           // time-out interval
                                               QS_ALLINPUT,   // input-event type
                                               waitFlags);
}  // end ProtoDispatcher::Wait()
      
void ProtoDispatcher::Dispatch()
{ 
    switch (wait_status)
    {
        case -1:            // error status
        {
            char errorString[256];
            errorString[255] = '\0';
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL,
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          (LPTSTR) errorString, 255, NULL);
            PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() MsgWaitForMultipleObjectsEx() error: %s\n", errorString);
            break;
        }
        
        //case WAIT_TIMEOUT:  // timeout condition
        default:
            // Handle any sockets that are "io_pending"
            // (WIN32 only does "edge triggering" on sockets)
            if (socket_io_pending)
            {
                SocketStream* nextSocketStream = socket_stream_list;
                while (nextSocketStream)
                {
                    ProtoSocket& theSocket = nextSocketStream->GetSocket();
                    nextSocketStream = (SocketStream*)nextSocketStream->GetNext(); 

                    // (TBD) Make this safer (i.e. if notification destroys socket)
                    if (theSocket.IsInputReady())
                        theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                    if (theSocket.NotifyOutput() && theSocket.IsOutputReady())
                        theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                }
                socket_io_pending = false;
            }
            if (WAIT_TIMEOUT == wait_status) 
            {
                break;
            }
            else if ((WAIT_OBJECT_0 <= wait_status) && (wait_status < (WAIT_OBJECT_0 + stream_count)))
            {
                unsigned int index = wait_status - WAIT_OBJECT_0;
                Stream* stream = stream_ptrs_array[index];
                if (Stream::SOCKET == stream->GetType())
                {
                    ProtoSocket& theSocket = static_cast<SocketStream*>(stream)->GetSocket();
                    if (ProtoSocket::LOCAL != theSocket.GetDomain())
                    {
                        WSANETWORKEVENTS event;
                        if (0 == WSAEnumNetworkEvents(theSocket.GetHandle(), stream_handles_array[index], &event))
                        {
                            if (0 != (event.lNetworkEvents & (FD_READ | FD_ACCEPT)))
                                theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);

                            if (0 != (event.lNetworkEvents & FD_WRITE)) 
                                theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);

                            if (0 != (event.lNetworkEvents & FD_CLOSE)) 
                            {
                                theSocket.SetClosing(true);

                                if (0 == event.iErrorCode[FD_CLOSE_BIT])
                                    theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                                else
                                    theSocket.OnNotify(ProtoSocket::NOTIFY_ERROR);
                            }
                            if (0 != (event.lNetworkEvents & FD_CONNECT))
                            {
                                if (0 == event.iErrorCode[FD_CONNECT_BIT])
                                    theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                                else 
                                    theSocket.OnNotify(ProtoSocket::NOTIFY_ERROR);
                            }
                            if (0 != (event.lNetworkEvents & FD_ADDRESS_LIST_CHANGE))
                            {
                              if (0 == event.iErrorCode[FD_ADDRESS_LIST_CHANGE_BIT])
                                theSocket.OnNotify(ProtoSocket::NOTIFY_EXCEPTION);
                              else
                                theSocket.OnNotify(ProtoSocket::NOTIFY_ERROR);
                            }
                        }
                        else
                        {
                            PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() WSAEnumNetworkEvents() error\n");
                        }
                    }
                    else
                    {
                        // "LOCAL" domain ProtoSockets are really ProtoPipes in Win32
                        if (index == (unsigned int)stream->GetIndex())
                            theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                         else // (index == stream->GetOutdex())
                            theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                    }
                }
                else if (Stream::CHANNEL == stream->GetType())
                {
                    ProtoChannel& theChannel = static_cast<ChannelStream*>(stream)->GetChannel();
                    if (index == (unsigned int)stream->GetIndex())
                        theChannel.OnNotify(ProtoChannel::NOTIFY_INPUT);
                    else // (index == stream->GetOutdex())
                        theChannel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                }
                else
                {
                    // (TBD) Can we test the handle for input/output readiness?
                    if (stream->IsInput()) 
                        static_cast<GenericStream*>(stream)->OnEvent(EVENT_INPUT);
                    if (stream->IsOutput()) 
                        static_cast<GenericStream*>(stream)->OnEvent(EVENT_OUTPUT);
                }
            }
            else if ((WAIT_OBJECT_0 + stream_count) == wait_status)
            {
                // Windows GUI message event
                MSG msg;
                // Use PeekMessage to make sure it's not a false alarm
                if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    //if (GetMessage(&msg, NULL, 0, 0 ))
                    if (WM_QUIT != msg.message)
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                    else
                    {
                        exit_code = msg.wParam;
                        run = false;
                        break;  // we're done
                    }
                }
            }
            else
            {
                // (TBD) handle these other conditions when needed
                // WAIT_ABANDONED or WAIT_IO_COMPLETION
            } 
            break;
    }  // end switch(status)
    OnSystemTimeout();
}  // end ProtoDispatcher::Dispatch() 

LRESULT CALLBACK ProtoDispatcher::MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) 
    {
        case WM_CREATE:
        {
            CREATESTRUCT *info = (CREATESTRUCT *) lParam;
            ProtoDispatcher* dp = (ProtoDispatcher*)info->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (DWORD)dp);
            return 0;
        }    
        case WM_DESTROY:
        {
            ProtoDispatcher* dp = (ProtoDispatcher*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            dp->msg_window = NULL;
            PostQuitMessage(0);
            return 0;
        }
        case WM_QUIT:
        {
            ProtoDispatcher* dp = (ProtoDispatcher*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            dp->run = false;
            return 0;
        }
        default:
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}  // end ProtoDispatcher::MessageHandler()

#endif // WIN32

/**
 * UNIX ProtoDispatcher::Controller implementation
 */
ProtoDispatcher::Controller::Controller(ProtoDispatcher& theDispatcher)
 : dispatcher(theDispatcher), use_lock_a(true)
{
    ProtoDispatcher::Init(lock_a);
    ProtoDispatcher::Init(lock_b); 
    ProtoDispatcher::Lock(lock_a);  
    // Note: "lock_b" gets an initial Lock() by the controlled
    // thread during "ProtoDispatcher::DoThreadStart()"
}

ProtoDispatcher::Controller::~Controller()
{
    Unlock(lock_a);
    Unlock(lock_b);
    Destroy(lock_a);
    Destroy(lock_b);  
}

bool ProtoDispatcher::Controller::DoDispatch()
{
    // (TBD) make sure !IsMyself() ???
    if (use_lock_a)
        ProtoDispatcher::Unlock(lock_b);
    else
        ProtoDispatcher::Unlock(lock_a);
    if (!SignalDispatchReady())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::Controller::DoDispatch()) SignalDispatchReady() error\n");
        return false;
    }
    if (use_lock_a)
    {
        ProtoDispatcher::Lock(lock_a);
        use_lock_a = false;
    }
    else
    {
        ProtoDispatcher::Lock(lock_b);
        use_lock_a = true;
    }
    return true;
}  // end ProtoDispatcher::Controller::DoDispatch()

void ProtoDispatcher::Controller::OnDispatch()
{
    dispatcher.SuspendThread();
    dispatcher.Dispatch();
    if (use_lock_a)
    {
        ProtoDispatcher::Lock(lock_b);
        ProtoDispatcher::Unlock(lock_a);      
    }  
    else
    {
        ProtoDispatcher::Lock(lock_a);
        ProtoDispatcher::Unlock(lock_b);
    }
    dispatcher.ResumeThread();
}  // end ProtoDispatcher::Controller::OnDispatch()

