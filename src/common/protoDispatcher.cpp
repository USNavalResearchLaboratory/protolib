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
 :  type(theType) 
#ifdef WIN32
    ,index(-1), outdex(-1)
#endif // WIN32
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

ProtoDispatcher::TimerStream::TimerStream()
 : Stream(TIMER), descriptor(INVALID_DESCRIPTOR)
{
}

ProtoDispatcher::TimerStream::~TimerStream()
{
#ifdef UNIX
    if (INVALID_DESCRIPTOR != descriptor)
    {
        close(descriptor);    
        descriptor = INVALID_DESCRIPTOR;
    }
#endif // UNIX
}

ProtoDispatcher::EventStream::EventStream(ProtoEvent& theEvent)
 : Stream(EVENT), event(&theEvent)
{
}

ProtoDispatcher::EventStream::~EventStream()
{
}

ProtoDispatcher::GenericStream::GenericStream(Descriptor theDescriptor)
 : Stream(GENERIC), myself(this), descriptor(theDescriptor), callback(NULL), client_data(NULL)
{
}
            
ProtoDispatcher::ProtoDispatcher()
    : run(false), wait_status(-1), exit_code(0), timer_delay(-1), precise_timing(false),
      thread_id((ThreadId)(NULL)), external_thread(false), priority_boost(false), 
      thread_started(false), thread_signaled(false), thread_master((ThreadId)(NULL)), 
      suspend_count(0), signal_count(0), controller(NULL), 
      prompt_set(false), prompt_callback(NULL), prompt_client_data(NULL),
      break_stream(break_event)
#ifdef WIN32
      ,stream_handles_array(NULL), stream_ptrs_array(NULL),
      stream_array_size(0), stream_count(0), msg_window(NULL),
	  actual_thread_handle(NULL) 
#ifdef USE_WAITABLE_TIMER
	  ,timer_active(false)
#endif // USE_WAITABLE_TIMER
#else // UNIX
#ifdef USE_TIMERFD
      ,timer_fd(-1)
#endif  // USE_TIMERFD
#if defined(USE_SELECT)
      // no special initialization required
#elif defined(USE_KQUEUE)
      ,kevent_queue(-1)
#elif defined(USE_EPOLL)
      ,epoll_fd(-1)
#else
#error "undefined async i/o mechanism"  // to make sure we implement something
#endif  // if/else USE_SELECT / USE_KQUEUE / USE_EPOLL
#endif // if/else WIN32/UNIX
{    
}

ProtoDispatcher::~ProtoDispatcher()
{
    Destroy();
}

void ProtoDispatcher::Destroy()
{
    Stop();
    
    // Iterate through stream_table and disable notification
    // (TBD - could we skip this and just call stream_list.Destroy()?)
    Stream* stream;
    StreamTable::Iterator iterator(stream_table);
    while (NULL != (stream = iterator.GetNextItem()))
    {
        switch (stream->GetType())
        {
            case Stream::CHANNEL:
                static_cast<ChannelStream*>(stream)->GetChannel().SetNotifier(NULL);
                break;
            case Stream::SOCKET:
                static_cast<SocketStream*>(stream)->GetSocket().SetNotifier(NULL);
                break;
            case Stream::GENERIC:
                ReleaseGenericStream(static_cast<GenericStream&>(*stream));
                break;
            case Stream::TIMER:
                // No timer streams are put in the stream_table (yet)
               
                break;
            case Stream::EVENT:
                ReleaseEventStream(static_cast<EventStream&>(*stream));
                break;
        }
    }
    ASSERT(stream_table.IsEmpty());
    channel_stream_pool.Destroy();
    socket_stream_pool.Destroy();
    generic_stream_pool.Destroy();
    
#ifdef WIN32
    if (NULL != stream_handles_array)
    {
        delete[] stream_handles_array;
        delete[] stream_ptrs_array;
        stream_array_size = 0;
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

    if (NULL != channelStream)
    {
        
        if ((channelStream->GetNotifyFlags() == notifyFlags) && (0 != notifyFlags))
        {
            // no notification change needed, but maybe input/output ready change below
            ASSERT(0 != notifyFlags); 
        }
        else if (0 != notifyFlags)
        {
            // Determine what notification changes are needed for this stream and make them
            if (0 != (notifyFlags & ProtoChannel::NOTIFY_INPUT)) 
            {
                if (!channelStream->IsInput())  // Enable system-specific  input notification 
                {
                    if (!UpdateStreamNotification(*channelStream, ENABLE_INPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error: unable to ENABLE_INPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }   
            else
            {
                if (channelStream->IsInput())  // Disable system-specific  input notification 
                {
                    if (!UpdateStreamNotification(*channelStream, DISABLE_INPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error: unable to DISABLE_INPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }         
            if (0 != (notifyFlags & ProtoChannel::NOTIFY_OUTPUT)) 
            {
                if (!channelStream->IsOutput())  // Enable system-specific  input notification 
                {
                    if (!UpdateStreamNotification(*channelStream, ENABLE_OUTPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error: unable to ENABLE_OUTPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }   
            else
            {
                if (channelStream->IsOutput())  // Disable system-specific  output notification 
                {
                    if (!UpdateStreamNotification(*channelStream, DISABLE_OUTPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error: unable to DISABLE_OUTPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }  
            // TBD - support exception notification ???   
        }
        else  // 0 == notifyFlags
        {
            if (channelStream->HasNotifyFlags())
            {
                if (!UpdateStreamNotification(*channelStream, DISABLE_ALL))
                {
                    PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error: unable to DISABLE_ALL!\n");
                    UnsignalThread();
                    return false;
                }
            }
            ReleaseChannelStream(*channelStream);
            UnsignalThread();
            return true;
        }  // end if/else 0 != notifyFlags
        
#ifdef WIN32
        // Add or remove channelStream from "ready_stream_list" as appropriate
        // Note input/output readiness may have changed even if notification did not
        if ((theChannel.IsInputReady() && channelStream->IsInput()) ||
            (theChannel.IsOutputReady() && channelStream->IsOutput()))   
        {
            // Make sure our "ready" socket is in the "ready_stream_list"
            if (!ready_stream_list.Contains(*channelStream))
            {
                if (!ready_stream_list.Append(*channelStream))
                {
                    ReleaseChannelStream(*channelStream);
                    PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() error: unable to append ready channel!\n");
                    UnsignalThread();
                    return false;

                }
            }
        }
        else if (ready_stream_list.Contains(*channelStream))
        {
            // Remove from "ready_socket_list"
            ready_stream_list.Remove(*channelStream);
        }
#endif // WIN32
        
        UnsignalThread();
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::UpdateChannelNotification() new ChannelStream error: %s\n",
                GetErrorString());
        UnsignalThread();
        return false;   
    }  
}  // end ProtoDispatcher::UpdateChannelNotification()

bool ProtoDispatcher::UpdateEventNotification(ProtoEvent& theEvent, 
                                              int         notifyFlags)
{
    SignalThread();   // TBD- check result?
    EventStream* eventStream = GetEventStream(theEvent);
    if (NULL != eventStream)
    {
        if (eventStream->GetNotifyFlags() == notifyFlags)
        {
            if (0 == notifyFlags)
            {
                ReleaseEventStream(*eventStream);
                UnsignalThread();
                return true;
            }
        }
        else if (0 != notifyFlags)
        {
            // ProtoEvents only have NOTIFY_INPUT 
            ASSERT(!eventStream->NotifyFlagIsSet(EventStream::NOTIFY_INPUT));
            if (!UpdateStreamNotification(*eventStream, ENABLE_INPUT))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateEventNotification() error: unable to ENABLE_INPUT!\n");
                UnsignalThread();
                return false;
            }
        }
        else // 0 == notifyFlags
        {
            if (!UpdateStreamNotification(*eventStream, DISABLE_INPUT))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateEventNotification() error: unable to DISABLE_INPUT!\n");
                UnsignalThread();
                return false;
            }
            ASSERT(0 == eventStream->GetNotifyFlags());
            ReleaseEventStream(*eventStream);
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::UpdateEventNotification() new EventStream error: %s\n",
                GetErrorString());
        UnsignalThread();
        return false;   
    }  // end if/else (NULL != eventStream)
    UnsignalThread();
    return true; 
}  // end ProtoDispatcher::UpdateEventNotification()

bool ProtoDispatcher::UpdateSocketNotification(ProtoSocket& theSocket, 
                                               int          notifyFlags)
{
    SignalThread();  // TBD - check result?
    SocketStream* socketStream = GetSocketStream(theSocket);
    if (NULL != socketStream)
    {
        if (socketStream->GetNotifyFlags() == notifyFlags)
        {
            // nothing to do up here, notification unchanged  
            // (but for WIN32, socket "readiness" may have changed)
            // _unless_ it's a new (closed) socket or idle tcp socket
            // TBD - should prob not be calling "UpdateSocketNotification()" in this case
            if (0 == notifyFlags)  
            {
                
                ReleaseSocketStream(*socketStream);
                UnsignalThread();
                return true;
            }
        }
        else if (0 != notifyFlags)
        {
#ifdef WIN32
            // WIN32 Sockets are handled differently
			int index = socketStream->GetIndex();
            if (index < 0)
            {
                if ((index = Win32AddStream(*socketStream, theSocket.GetInputEventHandle())) < 0)
                {
                    PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error adding handle\n");
                    ReleaseSocketStream(*socketStream);
                    UnsignalThread();
                    return false;     
                }
                socketStream->SetIndex(index);
            }
            long eventMask = 0;
            if (0 != (notifyFlags & ProtoSocket::NOTIFY_INPUT))
                eventMask |= (FD_READ | FD_ACCEPT | FD_CLOSE);
            if (0 != (notifyFlags & ProtoSocket::NOTIFY_OUTPUT))
			{
				eventMask |= (FD_WRITE | FD_CONNECT | FD_CLOSE);
			}
            if (0 != (notifyFlags & ProtoSocket::NOTIFY_EXCEPTION))
                eventMask |= FD_ADDRESS_LIST_CHANGE;
            // Note for IPv4/IPv6 sockets, the "input event handle" is used for both input & output
            if (0 != WSAEventSelect(theSocket.GetHandle(), theSocket.GetInputEventHandle(), eventMask))
            {
                ReleaseSocketStream(*socketStream);
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() WSAEventSelect(0x%x) error: %s\n",
                        eventMask, ProtoSocket::GetErrorString());
                UnsignalThread();
                return false;
            }
			socketStream->SetNotifyFlags(notifyFlags);
#else // UNIX    
            // Determine what notification changes are needed for this stream and make them
            if (0 != (notifyFlags & ProtoSocket::NOTIFY_INPUT)) 
            {
                if (!socketStream->IsInput())  // Enable system-specific  input notification 
                {
                    if (!UpdateStreamNotification(*socketStream, ENABLE_INPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error: unable to ENABLE_INPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }   
            else
            {
                if (socketStream->IsInput())  // Disable system-specific  input notification 
                {
                    if (!UpdateStreamNotification(*socketStream, DISABLE_INPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error: unable to DISABLE_INPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }         
            if (0 != (notifyFlags & ProtoSocket::NOTIFY_OUTPUT)) 
            {
                if (!socketStream->IsOutput())  // Enable system-specific  input notification 
                {
                    if (!UpdateStreamNotification(*socketStream, ENABLE_OUTPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error: unable to ENABLE_OUTPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            }   
            else
            {
                if (socketStream->IsOutput())  // Disable system-specific  output notification 
                {
                    if (!UpdateStreamNotification(*socketStream, DISABLE_OUTPUT))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error: unable to DISABLE_OUTPUT!\n");
                        UnsignalThread();
                        return false;
                    }
                }
            } 
            // TBD - support exception notification ??? 
#endif  // if/else WIN32/UNIX
        }
        else  // 0 == notifyFlags
        {
            ASSERT(socketStream->HasNotifyFlags());
#ifdef WIN32
            if (0 != WSAEventSelect(theSocket.GetHandle(), theSocket.GetInputEventHandle(), 0))
                 PLOG(PL_WARN, "ProtoDispatcher::UpdateSocketNotification() WSAEventSelect(0) warning: %s\n",
                          ProtoSocket::GetErrorString());
#endif // WIN32  
            if (!UpdateStreamNotification(*socketStream, DISABLE_ALL))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error: unable to DISABLE_ALL!\n");
                UnsignalThread();
                return false;
            }
            ReleaseSocketStream(*socketStream);
            UnsignalThread();
            return true;
        }  // end if/else 0 != notifyFlags
#ifdef WIN32
        // Add or remove socketStream from "ready_socket_list" as appropriate
        // Note input/output readiness may have changed even if notification did not
        if ((theSocket.IsInputReady() && socketStream->IsInput()) ||
            (theSocket.IsOutputReady() && socketStream->IsOutput()))   
        {
            // Make sure our "ready" socket is in the "ready_stream_list"
            if (!ready_stream_list.Contains(*socketStream))
            {
				if (!ready_stream_list.Append(*socketStream))
                {
                    ReleaseSocketStream(*socketStream);
                    PLOG(PL_ERROR, "ProtoDispatcher::UpdateSocketNotification() error: unable to append ready socket!\n");
                    UnsignalThread();
                    return false;

                }
            }
        }
        else if (ready_stream_list.Contains(*socketStream))
        {
			// Remove from "ready_stream_list"
            ready_stream_list.Remove(*socketStream);
        }
#endif // WIN32
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
    ProtoSocket* socketPtr = &theSocket;
    SocketStream* socketStream = 
        static_cast<SocketStream*>(stream_table.Find((const char*)&socketPtr, sizeof(ProtoSocket*) << 3));
    if (NULL == socketStream)
    {
        // Get one from the pool or create a new one
        socketStream = socket_stream_pool.Get();
        if (NULL != socketStream)
        {
            socketStream->ClearNotifyFlags();
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
        // Insert into "active" stream list
        stream_table.Insert(*socketStream);
    }
    return socketStream;
}  // end ProtoDispatcher::GetSocketStream()

void ProtoDispatcher::ReleaseSocketStream(SocketStream& socketStream)
{  
#ifdef WIN32
    if (ready_stream_list.Contains(socketStream)) 
        ready_stream_list.Remove(socketStream);
    // Makes sure the channel input/output event HANDLE is removed from array
    // (This removes it from list passed to MsgWaitForMultipleObjectsEx() call)
    if (socketStream.GetIndex() >= 0)
    {
        Win32RemoveStream(socketStream.GetIndex());
        socketStream.SetIndex(-1);
    }
    if (socketStream.GetOutdex() >= 0)
    {
        Win32RemoveStream(socketStream.GetOutdex());
        socketStream.SetOutdex(-1);
    }
#endif // WIN32
    if (socketStream.HasNotifyFlags())
    {
        if (!UpdateStreamNotification(socketStream, DISABLE_ALL))
            PLOG(PL_ERROR, "ProtoDispatcher::ReleaseSocketStream() error: UpdateStreamNotification(DISABLE_ALL) failure!\n");
        socketStream.ClearNotifyFlags();
    }
    stream_table.Remove(socketStream);
	socket_stream_pool.Put(socketStream);
}  // end ProtoDispatcher::ReleaseSocketStream()


ProtoDispatcher::ChannelStream* ProtoDispatcher::GetChannelStream(ProtoChannel& theChannel)
{

    // First, search our list of active channels
    ProtoChannel* channelPtr = &theChannel;
    ChannelStream* channelStream = 
        static_cast<ChannelStream*>(stream_table.Find((const char*)&channelPtr, sizeof(ProtoChannel*) << 3));

    if (NULL == channelStream)
    {
        // Get one from the pool or create a new one
        channelStream = channel_stream_pool.Get();
        if (NULL != channelStream)
        {
            channelStream->ClearNotifyFlags();
            channelStream->SetChannel(theChannel);
        }
        else
        {
            if (NULL == (channelStream = new ChannelStream(theChannel)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::GetChannelStream() new ChannelStream error: %s\n", GetErrorString());
                return NULL;   
            }
        }
        // Insert into "active"  stream table
        stream_table.Insert(*channelStream);
    }
    return channelStream;
}  // end ProtoDispatcher::GetChannelStream()

void ProtoDispatcher::ReleaseChannelStream(ChannelStream& channelStream)
{
#ifdef WIN32
    if (ready_stream_list.Contains(channelStream)) 
        ready_stream_list.Remove(channelStream);
    // Makes sure the channel input/output event HANDLE is removed from array
    // (This removes it from list passed to MsgWaitForMultipleObjectsEx() call)
    if (channelStream.GetIndex() >= 0)
    {
        Win32RemoveStream(channelStream.GetIndex());
        channelStream.SetIndex(-1);
    }
    if (channelStream.GetOutdex() >= 0)
    {
        Win32RemoveStream(channelStream.GetOutdex());
        channelStream.SetOutdex(-1);
    }
#endif // WIN32
    stream_table.Remove(channelStream);
    channelStream.ClearNotifyFlags();
    channel_stream_pool.Put(channelStream);
}  // end ProtoDispatcher::ReleaseChannelStream()

ProtoDispatcher::EventStream* ProtoDispatcher::GetEventStream(ProtoEvent& theEvent)
{
    // First, search our list of active events
    ProtoEvent* eventPtr = &theEvent;
    EventStream* eventStream = 
        static_cast<EventStream*>(stream_table.Find((const char*)&eventPtr, sizeof(ProtoEvent*) << 3));
    if (NULL == eventStream)
    {
        // Get one from the pool or create a new one
        eventStream = event_stream_pool.Get();
        if (NULL != eventStream)
        {
            eventStream->ClearNotifyFlags();
            eventStream->SetEvent(theEvent);
        }
        else
        {
            if (NULL == (eventStream = new EventStream(theEvent)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::GetEventStream() new EventStream error: %s\n", GetErrorString());
                return NULL;   
            }
        }
        // Insert into "active"  stream table
        stream_table.Insert(*eventStream);
    }
    return eventStream;
}  // end ProtoDispatcher::GetEventStream()

void ProtoDispatcher::ReleaseEventStream(EventStream& eventStream)
{
/*
#ifdef WIN32
    if (ready_stream_list.Contains(eventStream)) 
        ready_stream_list.Remove(eventStream);
    // Makes sure the event input/output event HANDLE is removed from array
    // (This removes it from list passed to MsgWaitForMultipleObjectsEx() call)
    if (eventStream.GetIndex() >= 0)
    {
        Win32RemoveStream(eventStream.GetIndex());
        eventStream.SetIndex(-1);
    }
    if (eventStream.GetOutdex() >= 0)
    {
        Win32RemoveStream(eventStream.GetOutdex());
        eventStream.SetOutdex(-1);
    }
#endif // WIN32
*/
    stream_table.Remove(eventStream);
    eventStream.ClearNotifyFlags();
    event_stream_pool.Put(eventStream);
}  // end ProtoDispatcher::ReleaseEventStream()


bool ProtoDispatcher::InstallGenericInput(ProtoDispatcher::Descriptor descriptor, 
                                          ProtoDispatcher::Callback*  callback, 
                                          const void*                 clientData)
{
    SignalThread();
    GenericStream* stream = GetGenericStream(descriptor);
    if (NULL != stream)
    {
        if (!stream->IsInput())  // Enable system-specific input notification 
        {
            if (!UpdateStreamNotification(*stream, ENABLE_INPUT))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::InstallGenericInput() error: unable to ENABLE_INPUT!\n");
                if (!stream->HasNotifyFlags()) ReleaseGenericStream(*stream);
                UnsignalThread();
                return false;
            }
        }
    }
    stream->SetNotifyFlag(Stream::NOTIFY_INPUT);
    stream->SetCallback(callback, clientData);
    UnsignalThread();
    return true;;  
}  // end ProtoDispatcher::InstallGenericInput()

void ProtoDispatcher::RemoveGenericInput(Descriptor descriptor)
{
    SignalThread();
    GenericStream* stream = FindGenericStream(descriptor);
    if (NULL != stream)
    {
        if (stream->IsInput())
        {
            if (!UpdateStreamNotification(*stream, DISABLE_INPUT))
                PLOG(PL_ERROR, "ProtoDispatcher::RemoveGenericInput() error: UpdateStreamNotification(DISABLE_INPUT) failure!\n"); 
            stream->UnsetNotifyFlag(Stream::NOTIFY_INPUT);
        }
        if (!stream->HasNotifyFlags()) ReleaseGenericStream(*stream);
    }   
    UnsignalThread(); 
}  // end ProtoDispatcher::RemoveGenericInput()

bool ProtoDispatcher::InstallGenericOutput(ProtoDispatcher::Descriptor descriptor, 
                                           ProtoDispatcher::Callback*  callback, 
                                           const void*                 clientData)
{
    SignalThread();
    GenericStream* stream = FindGenericStream(descriptor);
    if (NULL != stream)
    {
        if (!stream->IsOutput())  // Enable system-specific output notification 
        {
            if (!UpdateStreamNotification(*stream, ENABLE_OUTPUT))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::InstallGenericOutput() error: unable to ENABLE_OUTPUT!\n");
                if (!stream->HasNotifyFlags()) ReleaseGenericStream(*stream);
                UnsignalThread();
                return false;
            }
        }
    }
    stream->SetNotifyFlag(Stream::NOTIFY_OUTPUT);
    stream->SetCallback(callback, clientData);
    UnsignalThread();
    return true;  
}  // end ProtoDispatcher::InstallGenericOutput()

void ProtoDispatcher::RemoveGenericOutput(Descriptor descriptor)
{
    SignalThread();
    GenericStream* stream = FindGenericStream(descriptor);
    if (NULL != stream)
    {
        if (stream->IsOutput())
        {
            if (!UpdateStreamNotification(*stream, DISABLE_OUTPUT))
                PLOG(PL_ERROR, "ProtoDispatcher::RemoveGenericOutput() error: UpdateStreamNotification(DISABLE_OUTPUT) failure!\n"); 
            stream->UnsetNotifyFlag(Stream::NOTIFY_OUTPUT);
        }
        if (!stream->HasNotifyFlags()) ReleaseGenericStream(*stream);
    }   
    UnsignalThread();
}  // end ProtoDispatcher::RemoveGenericOutput()

ProtoDispatcher::GenericStream* ProtoDispatcher::GetGenericStream(Descriptor descriptor)
{
    // First, search our list of active generic streams
    GenericStream* genericStream = generic_stream_table.FindByDescriptor(descriptor);
    if (NULL == genericStream)
    {
        // Get one from the pool or create a new one
        genericStream = generic_stream_pool.Get();
        if (NULL != genericStream)
        {
            genericStream->ClearNotifyFlags();
            genericStream->SetDescriptor(descriptor);
        }
        else
        {
            if (NULL == (genericStream = new GenericStream(descriptor)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::GetGenericStream() new GenericStream error: %s\n", GetErrorString());
                return NULL;   
            }
        }
        // Insert into "active"  stream table
        stream_table.Insert(*genericStream);
        // Insert into generic_stream_table (indexed by descriptor)
        if (!generic_stream_table.Insert(*genericStream))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::GetGenericStream() error: unable to add to table: %s\n", GetErrorString());
            ReleaseGenericStream(*genericStream);
            return NULL;
        }
    }
    return genericStream;
}  // end ProtoDispatcher::GetGenericStream()

void ProtoDispatcher::ReleaseGenericStream(GenericStream& genericStream)
{
    // First, disable system-specific notifications for stream
    if (genericStream.IsInput()) 
    {
        if (!UpdateStreamNotification(genericStream, DISABLE_INPUT))
                PLOG(PL_ERROR, "ProtoDispatcher::ReleaseGenericStream() error: UpdateStreamNotification(DISABLE_INPUT) failure!\n"); 
    }
    if (genericStream.IsOutput()) 
    {
        if (!UpdateStreamNotification(genericStream, DISABLE_OUTPUT))
                PLOG(PL_ERROR, "ProtoDispatcher::ReleaseGenericStream() error: UpdateStreamNotification(DISABLE_OUTPUT) failure!\n"); 
    }
    genericStream.ClearNotifyFlags();
    stream_table.Remove(genericStream);
    generic_stream_table.Remove(genericStream);
    generic_stream_pool.Put(genericStream);
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
    if (0 != sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (0 != sched_setscheduler(0, SCHED_OTHER, &schp))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() error: sched_setscheduler() error: %s\n", GetErrorString());
            return false;
        }   
        else
        {
            PLOG(PL_WARN, "ProtoDispatcher::BoostPriority() warning: unable to set SCHED_FIFO boost, SCHED_OTHER set.\n"
                          "                  (run as root or sudofor full SCHED_FIFO priority boost)\n");
        }
    }
#else
    // (TBD) Do something differently if "pthread sched param"?
    if (0 != setpriority(PRIO_PROCESS, getpid(), -20))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::BoostPriority() error: setpriority() error: %s\n", GetErrorString());
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
    
#ifdef USE_TIMERFD  // LINUX-only
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0)
    {
        PLOG(PL_ERROR, "ProtoDispatcher::Run() timerfd_create() error: %s\n", GetErrorString());
        return -1;  // TBD - is there a more specific exitCode we should use instead???
    }
#ifdef USE_EPOLL   
    if (!EpollChange(tfd, EPOLLIN, EPOLL_CTL_ADD, &timer_stream))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::Run() error EpolLChange(timer_fd) failed!\n");
        close(tfd);
        return -1;  // TBD - is there a more specific exitCode we should use instead???
    }
#endif    
    timer_stream.SetDescriptor(tfd);
#endif // USE_TIMERFD  

#ifdef USE_WAITABLE_TIMER  // WIN32-only
	HANDLE wtm = CreateWaitableTimer(NULL, TRUE, NULL);
	if (INVALID_HANDLE_VALUE == wtm)
	{
		PLOG(PL_ERROR, "ProtoDispatcher::Run() CreateWaitableTimer() error: %s\n", GetErrorString());
		return -1;
	}
	timer_stream.SetDescriptor(wtm);
	timer_stream.SetIndex(-1);  // the timer_stream will added when activated
	timer_active = false;
#endif // USE_WAITABLE_TIMER   
      
    // TBD - should we keep "run" true while in the do loop so "IsRunning()" 
    //       can be interpreted properly (or just deprecate IsRunning() ???
    run = oneShot ? false : true;
    do
    { 
        if (IsPending())
        {
            // Here we "latch" the next timeout _before_
            // we open the suspend window to be safe
            timer_delay = ProtoTimerMgr::GetTimeRemaining();
            if (IsThreaded())
            {
                Lock(signal_mutex);
                Unlock(suspend_mutex);
                Wait();
                Unlock(signal_mutex);

                // <-- this is where a dispatcher rests after SignalThread() call
                
                // what if another thread calls SignalThread() at this exact moment?
                // and gets the suspend and signal mutexes ...
                // a) WasSignaled() will be false on Unix (break_pipe fd not set)
                // b) then Dispatch() will be called ... but only valid descriptors, etc are checked
                //    as Wait resets/builds the FD_SET
                // c) next time WasSignaled() will be true, but no big deal
                
                Lock(suspend_mutex);
                
                if (prompt_set)
                {
                
                    if (NULL != prompt_callback)
                         prompt_callback(prompt_client_data);
                    prompt_set = false;
                }
                /*
                if (WasSignaled())  // true" if was signaled via ProtoDispatcher::SignalThread()
                {
                    // We "continue" here in case i/o or timeout status 
                    // was changed during signal suspend
                    continue;
                }
                else */
                if (NULL != controller)
                {
                    // Relinquish to controller thread
                    // Note this assumes the controller thread is the _only_
                    // other thread vying for control of this dispatcher thread
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
    // Note if USE_EPOLL, closing the "timer_fd" automatically deletes the event
    close(timer_stream.GetDescriptor());
    timer_stream.SetDescriptor(INVALID_DESCRIPTOR);
#endif // USE_TIMERFD
#ifdef USE_WAITABLE_TIMER
	if (timer_active)
	{
		CancelWaitableTimer(timer_stream.GetDescriptor());
		timer_active = false;
	}
	if (timer_stream.GetIndex() >= 0)
	{
		Win32RemoveStream(timer_stream.GetIndex());
		timer_stream.SetIndex(-1);
	}
	CloseHandle(timer_stream.GetDescriptor());
	timer_stream.SetDescriptor(INVALID_DESCRIPTOR);
#endif // USE_WAITABLE_TIMER
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
                                  ProtoDispatcher::Controller* theController,
                                  ThreadId                     threadId)
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
    
    if ((ThreadId)NULL == threadId)
    {
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
            Unlock(suspend_mutex);  // will be relocked by DoThreadStart()
            thread_id = (ThreadId)NULL;
            controller = NULL;
            return false;
        }
        external_thread = false;
        Unlock(suspend_mutex);
    }
    else
    {
        // Starting within externally created thread
        ASSERT(threadId == GetCurrentThread());
        thread_id = threadId;  // externally created thread
        external_thread = true;
        // So must exec DoThreadStart() steps here
        thread_started = true;
        exit_status = Run();
        Unlock(suspend_mutex);
    }
    return true;
}  // end ProtoDispatcher::StartThread()

/**
 * This wakes up the thread, makes a call to the installed prompt_callback
 * and the thread goes back to waiting.  This is the way to have a ProtoDispatcher
 * thread perform some "work" function.  It currently does not have a means to
 * signal when the work is completed other than this call (due to SuspendThread() call)
 * will block until the work is done.  TBD - provide a method to access/monitor/wait on
 * the "suspend_mutex" that will be unlocked when the thread goes back to "sleep"
 */
bool ProtoDispatcher::PromptThread()
{
    // Make sure thread is asleep before setting prompt
    if (SuspendThread())
    {
        prompt_set = true;  // indicate prompt_callback should be called
        // Now explicitly wake the thread that the prompt is set
        if (!SignalThread()) 
        {
            ResumeThread();
            return false; // suspend/signal thread
        }
        UnsignalThread();
        ResumeThread();
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoDispatcher::PromptThread()


/**
 * This brings the thread to a suspended state _outside_ 
 * of _its_ ProtoDispatcher::Wait() state.  A call to
 * SignalThread() followed by UnsignalThread() will make
 * the thread break from waiting and dispatch timers, pending I/O, etc
 *  The caller can safely do things in between signal/unsignal like
 *  add/remove descriptors, etc and this is used by internal ProtoDispatcher
 * code to do this safely for threaded dispatcher instances.  Any call to SignalThread()
 * MUST be mirrored with an UnsignalThread() call and it is OK for it to be re-entrantly
 * called (that is what the "signal_count" is for).
 */
bool ProtoDispatcher::SignalThread()
{
    SuspendThread();
    ThreadId currentThread = GetCurrentThread();    
    if (IsThreaded() && (currentThread != thread_id))
    {
        if (signal_count > 0)
        {
            signal_count++;
            return true;
        }
        else
        {
            if (!SetBreak())
            {
                PLOG(PL_ERROR, "ProtoDispatcher::SignalThread() error: SetBreak() failed!\n");
                ResumeThread();
                return false;
            } 
            // Attempting to lock the "signal_mutex" blocks until the thread
            // is "outside" its "ProtoDispatcher::Wait()" stage
            // (thread can't reenter wait stage until "UnsignalThread()" is 
            //  called and the "signal_mutex" is unlocked)
            Lock(signal_mutex);
            signal_count = 1;
            thread_signaled = true;
        }
    }
    return true;   
}  // end ProtoDispatcher::SignalThread()

void ProtoDispatcher::UnsignalThread()
{
    ThreadId currentThread = GetCurrentThread();
    if (IsThreaded() && (currentThread != thread_id) && (thread_master == currentThread))
    {
        ASSERT(0 != signal_count);
        signal_count--;
        if (0 == signal_count) 
            Unlock(signal_mutex);  
    }
    ResumeThread(); 
}  // end ProtoDispatcher::UnsignalThread()

/*
 * This makes sure the thread is asleep (i.e. waiting) or stopped
 * until ResumeThread() is called (unlocks the suspend_mutex so
 * thread can proceed)  Note that SuspendThread() will block
 * until the thread gets to the waiting state.  Any call to SuspendThread()
 * MUST be mirrored with an ResumeThread() call and it is OK for it to 
 * be re-entrantly called (that is what the "suspend_count" is for).
 * 
 */
bool ProtoDispatcher::SuspendThread()
{
    ThreadId currentThread = GetCurrentThread();
    if (IsThreaded() && (currentThread != thread_id))
    {
        if (currentThread == thread_master)
        {
            suspend_count++;
            return true;   
        }
        // We use while() here to make sure the thread started
        // (TBD) use a spin_count to limit iterations as safeguard
        while (!thread_started);
        Lock(suspend_mutex);  // TBD - check result of "Lock()" 
        thread_master = currentThread;
        suspend_count = 1;
    }
    return true;
}  // end ProtoDispatcher::SuspendThread()

void ProtoDispatcher::ResumeThread()
{
    ThreadId currentThread = GetCurrentThread();
    if (IsThreaded() && (currentThread != thread_id))
    {
        if (currentThread == thread_master)
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
        if (!external_thread)
        {
#ifdef WIN32
            if (!IsMyself()) WaitForSingleObject(actual_thread_handle, INFINITE);
#ifdef _WIN32_WCE
            CloseHandle(actual_thread_handle);
            actual_thread_handle = NULL;
#endif // _WIN32_WCE
#else
            if (!IsMyself()) pthread_join(thread_id, NULL);
#endif // if/else WIN32/UNIX
        }
        thread_started = false;
        thread_id = (ThreadId)NULL;
        external_thread = false;
        RemoveBreak();
        Destroy(suspend_mutex);
        Destroy(signal_mutex);
    }
}  // end ProtoDispatcher::DestroyThread()

/////////////////////////////////////////////////////////////////////////
// Unix-specific methods and implementation
#ifdef UNIX



#if defined(USE_SELECT)
// USE_SELECT implementation of ProtoDispatcher::UpdateStreamNotification()
bool ProtoDispatcher::UpdateStreamNotification(Stream& stream, NotificationCommand cmd)
{
    // ProtoDispatcher currently builds its "struct fdsets" by iterating through
    // the "stream_list" each time ProtoDispatcher::Wait() is called
    // TBD - we should use FD_CLR here to make sure our current input_set and
    // output_set are in the proper state for when a stream's notification
    // is disabled (e.g. due to deletion or other)
    
    switch (cmd)
    {
        case ENABLE_INPUT:
            stream.SetNotifyFlag(Stream::NOTIFY_INPUT);
            break;
        case DISABLE_INPUT:
            stream.UnsetNotifyFlag(Stream::NOTIFY_INPUT);
            if (INVALID_DESCRIPTOR != stream.GetInputHandle())
                FD_CLR(stream.GetInputHandle(), &input_set);
            break;
        case ENABLE_OUTPUT:
            stream.SetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;
        case DISABLE_OUTPUT:
            stream.UnsetNotifyFlag(Stream::NOTIFY_OUTPUT);
            if (INVALID_DESCRIPTOR != stream.GetOutputHandle())
                FD_CLR(stream.GetOutputHandle(), &output_set);
            break;
        case DISABLE_ALL:
            stream.ClearNotifyFlags();
            if (INVALID_DESCRIPTOR != stream.GetInputHandle())
                FD_CLR(stream.GetInputHandle(), &input_set);
            if (INVALID_DESCRIPTOR != stream.GetOutputHandle())
                FD_CLR(stream.GetOutputHandle(), &output_set);
            break;
    }
    
    return true;  
}  // end ProtoDispatcher::UpdateStreamNotification() [USE_SELECT]

#elif defined(USE_KQUEUE)

// USE_KQUEUE implementation of ProtoDispatcher::UpdateStreamNotification()
bool ProtoDispatcher::UpdateStreamNotification(Stream& stream, NotificationCommand cmd)
{
    switch (cmd)
    {
        case ENABLE_INPUT:
            if (!KeventChange(stream.GetInputHandle(), EVFILT_READ, EV_ADD | EV_ENABLE, &stream))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(ENABLE_INPUT) KeventChange() error!\n");
                return false;
            }
            stream.SetNotifyFlag(Stream::NOTIFY_INPUT);
            break;  
                
        case DISABLE_INPUT:
            if (!KeventChange(stream.GetInputHandle(), EVFILT_READ, EV_DISABLE, &stream))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_INPUT) KeventChange() error!\n");
                return false;
            }
            stream.UnsetNotifyFlag(Stream::NOTIFY_INPUT);
            break;
            
        case ENABLE_OUTPUT:
            if (!KeventChange(stream.GetOutputHandle(), EVFILT_WRITE, EV_ADD | EV_ENABLE, &stream))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_INPUT) KeventChange() error!\n");
                return false;
            }
            stream.SetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;
            
        case DISABLE_OUTPUT:
            if (!KeventChange(stream.GetOutputHandle(), EVFILT_WRITE, EV_DISABLE, &stream))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_INPUT) KeventChange() error!\n");
                return false;
            }
            stream.UnsetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;
            
        case DISABLE_ALL:
            if (stream.IsInput() && !KeventChange(stream.GetInputHandle(), EVFILT_READ, EV_DISABLE, &stream))
            {
                PLOG(PL_WARN, "ProtoDispatcher::UpdateStreamNotification(DISABLE_ALL) KeventChange(EVFILT_READ) error!\n");
                //return false;
            }
            if (stream.IsOutput() && !KeventChange(stream.GetOutputHandle(), EVFILT_WRITE, EV_DISABLE, &stream))
            {
                PLOG(PL_WARN, "ProtoDispatcher::UpdateStreamNotification(DISABLE_ALL) KeventChange(EVFILT_WRITE) error!\n");
                //return false;
            }
            stream.ClearNotifyFlags();
            // TBD - support exceptions ??
            break;
    }
    return true;
}  // end ProtoDispatcher::UpdateStreamNotification() [USE_KQUEUE]

bool ProtoDispatcher::KeventChange(uintptr_t ident, int16_t filter, uint16_t flags, void* udata)
{
    // This simply sets a new "struct kevent" in our "kevent_array"
    if (-1 == kevent_queue)
    {
        if (-1 == (kevent_queue = kqueue()))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::KeventChange() kqueue() error: %s\n", GetErrorString());
            return false;
        }
    }
    struct kevent kvt;
    kvt.ident = ident;
    kvt.filter = filter;
    kvt.flags = flags;
    kvt.fflags = 0;
    kvt.data = 0;
    kvt.udata = udata;
    
    // On EV_DISABLE, go through the current kevent_array and make 
    // sure this "ident::filter" is nullified to avoid undesired
    // notification attempts (i.e. disable current, pending notification, if any)
    //  (i.e., set the "filter" to harmless EVFILT_USER value)
    if (0 != (flags & EV_DISABLE))
    {
        struct kevent* kep = kevent_array;
        for (int i = 0; i < wait_status; i++)
        {
            if ((ident == kep->ident) && (filter == kep->filter))
            {
                kep->ident = 0;  // different than our "break" EVFILT_USER if ever needed
                kep->filter = EVFILT_USER;
            }
        }
    }
    // This call enables/disables future notifications as desired
    if (kevent(kevent_queue, &kvt, 1, NULL, 0, NULL) < 0)
    {
        PLOG(PL_ERROR, "ProtoDispatcher::KeventChange() kevent() error: %s\n", GetErrorString());
        return false;
    }
    else
    {
        return true;
    }
}  // end ProtoDispatcher::KeventChange()

#elif USE_EPOLL

// USE_EPOLL implementation of ProtoDispatcher::UpdateStreamNotification()
bool ProtoDispatcher::UpdateStreamNotification(Stream& stream, NotificationCommand cmd)
{
    // Note for ProtoChannel it is possible that separate input and output descriptors may
    // be used and so the code below checks for this condition
    switch (cmd)
    {
        case ENABLE_INPUT:
            ASSERT(!stream.IsInput());
            if (!((stream.IsOutput() && (stream.GetInputHandle() == stream.GetOutputHandle())) ?
                    EpollChange(stream.GetInputHandle(), EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, &stream) :
                    EpollChange(stream.GetInputHandle(), EPOLLIN, EPOLL_CTL_ADD, &stream)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(ENABLE_INPUT) error: EpollChange() failed!\n");
                return false;
            }
            stream.SetNotifyFlag(Stream::NOTIFY_INPUT);
            break;
                
        case DISABLE_INPUT:
            ASSERT(stream.IsInput());
            if (!((stream.IsOutput() && (stream.GetInputHandle() == stream.GetOutputHandle())) ?
                    EpollChange(stream.GetInputHandle(), EPOLLOUT, EPOLL_CTL_MOD, &stream) :
                    EpollChange(stream.GetInputHandle(), 0, EPOLL_CTL_DEL, NULL)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_INPUT) error: EpollChange() failed!\n");
                return false;
            }
            stream.UnsetNotifyFlag(Stream::NOTIFY_INPUT);
            break;
            
        case ENABLE_OUTPUT:
            ASSERT(!stream.IsOutput());
            if (!((stream.IsInput() && (stream.GetInputHandle() == stream.GetOutputHandle())) ?
                    EpollChange(stream.GetOutputHandle(), EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, &stream) :
                    EpollChange(stream.GetOutputHandle(), EPOLLOUT, EPOLL_CTL_ADD, &stream)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(ENABLE_OUTPUT) error: EpollChange() failed!\n");
                return false;
            }
            stream.SetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;
            
        case DISABLE_OUTPUT:
            ASSERT(stream.IsOutput());
            if (!((stream.IsInput() && (stream.GetInputHandle() == stream.GetOutputHandle())) ?
                    EpollChange(stream.GetOutputHandle(), EPOLLIN, EPOLL_CTL_MOD, &stream) :
                    EpollChange(stream.GetOutputHandle(), 0, EPOLL_CTL_DEL, NULL)))
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_OUTPUT) error: EpollChange() failed!\n");
                return false;
            }
            stream.UnsetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;
            
        case DISABLE_ALL:
            ASSERT(stream.IsInput() || stream.IsOutput());
            if (stream.GetInputHandle() == stream.GetOutputHandle())
            {
                if (!EpollChange(stream.GetInputHandle(), 0, EPOLL_CTL_DEL, NULL))
                {
                    PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_ALL) error: EpollChange() failed!\n");
                    return false;
                }
            }
            else
            {
                if (stream.IsInput())
                {
                    if (!EpollChange(stream.GetInputHandle(), 0, EPOLL_CTL_DEL, NULL))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_ALL) error: EpollChange(input) failed!\n");
                        return false;
                    }
                }
                if (stream.IsOutput())
                {
                    if (!EpollChange(stream.GetOutputHandle(), 0, EPOLL_CTL_DEL, NULL))
                    {
                        PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification(DISABLE_ALL) error: EpollChange(output) failed!\n");
                        return false;
                    }
                }
            }
            stream.ClearNotifyFlags();
            // TBD - support exceptions ??
            break;
    }
    return true;
}  // end ProtoDispatcher::UpdateStreamNotification() [USE_EPOLL]

bool ProtoDispatcher::EpollChange(int fd, int events, int op, void* udata)
{
    if (-1 == epoll_fd)
    {
        if (-1 == (epoll_fd = epoll_create1(0)))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification() epoll_create() error: %s\n",
                           GetErrorString());
            return false;
        }
    }
    // Go through the current "epoll_event_array" and trim down or nullify
    // applicable current pending events for this "fd"
    // (So we don't get undesired notification dispatches)
    switch (op)
    {
        case EPOLL_CTL_MOD:
        {
            struct epoll_event* evp = epoll_event_array;
            for (int i = 0; i < wait_status; i++)
            {
                if (evp->data.ptr == udata)
                    evp->events &= events;  // mask out events no longer wanted
                evp++;
            }
            break;
        }
        case EPOLL_CTL_DEL:
        {
            struct epoll_event* evp = epoll_event_array;
            for (int i = 0; i < wait_status; i++)
            {
                if (evp->data.ptr == udata)
                {
                    evp->events = 0;        // no events wanted
                    evp->data.ptr = NULL;  // make sure no dereference is attempted
                }
                evp++;
            }
            break;
        }
        default:
            break;
    }  // end switch(op)
    // Set or disable future events for this descriptor
    struct epoll_event event;
    event.events = events;
    event.data.ptr = udata;
    if (-1 == epoll_ctl(epoll_fd, op, fd, &event))
    {
#ifdef USE_TIMERFD
        PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification() epoll_ctl() error: %s (timer_fd:%d epoll_fd:%d)\n",
                           GetErrorString(), timer_stream.GetDescriptor(), epoll_fd);
#else
        PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification() epoll_ctl() error: %s (epoll_fd:%d)\n",
                           GetErrorString(), epoll_fd);
#endif
        return false;
    }
    return true;
}  // end ProtoDispatcher::EpollChange()

#else
#error "undefined async i/o mechanism"  // to make sure we implement something
#endif // !USE_SELECT && !USE_KQUEUE

bool ProtoDispatcher::InstallBreak()
{ 
#ifndef USE_KQUEUE
    // ProtoEvent uses eventfd(), kevent(), or pipe under the hood for us
    if (!break_event.Open())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() break_event.Open() error: %s\n", GetErrorString());
        return false;
    }
#ifdef USE_EPOLL
    if (!EpollChange(break_event.GetDescriptor(), EPOLLIN, EPOLL_CTL_ADD, &break_stream))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() error: EpollChange() failed!\n");
        break_event.Close();
        return false;
    }
#endif // USE_EPOLL    
#else   
    // TBD - use ProtoEvent here, too
    if (!KeventChange(1, EVFILT_USER, EV_ADD | EV_CLEAR, NULL))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() KeventChange(EVFILT_USER) error: %s\n", GetErrorString());
        return false;
    }
#endif // if/else !KQUEUE
    return true;
}  // end ProtoDispatcher::InstallBreak()

bool ProtoDispatcher::SetBreak()
{      
#ifndef USE_KQUEUE
    if (!break_event.Set())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::SetBreak() break_event.Set() error: %s\n", GetErrorString());
        return false;
    }
#else
    // TBD - use ProtoEvent here, too
    struct kevent kev;
    EV_SET(&kev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    if (-1 == kevent(kevent_queue, &kev, 1, NULL, 0, NULL))
    {
        PLOG(PL_ERROR, "ProtoDispatcher::SetBreak() kevent() error: %s\n", GetErrorString());
        return false;
    }
#endif // if/else !KQUEUE
    return true;
}  // end ProtoDispatcher::SetBreak()

void ProtoDispatcher::RemoveBreak()
{
#ifndef USE_KQUEUE
    if (INVALID_DESCRIPTOR != break_stream.GetDescriptor())
    {
#ifdef USE_EPOLL
        if (!EpollChange(break_stream.GetDescriptor(), EPOLLIN, EPOLL_CTL_DEL, &break_stream))  
        {
            PLOG(PL_ERROR, "ProtoDispatcher::RemoveBreak() error: EpollChange() failed!\n");
        }
#endif // USE_EPOLL
        // Close down the break_stream ProtoEvent
        break_event.Close();
    }
#else // USE_KQUEUE
    struct kevent kev;
    EV_SET(&kev, 1, EVFILT_USER, EV_DELETE, 0, 0, NULL);
    if (-1 == kevent(kevent_queue, &kev, 1, NULL, 0, NULL))
        PLOG(PL_ERROR, "ProtoDispatcher::RemoveBreak() kevent() error: %s\n", GetErrorString());
#endif // if/else !KQUEUE
}  // end ProtoDispatcher::RemoveBreak()


/**
 * Warning! This may block indefinitely if !IsPending() ...
 */
void ProtoDispatcher::Wait()
{
    // (TBD) We could put some code here to protect this from
    // being called by the wrong thread?
    
#if defined(USE_KQUEUE) || defined(HAVE_PSELECT) || defined(USE_TIMERFD)
#define USE_TIMESPEC 1 // so we can use the "struct timespec" created here
#endif  // USE_KQUEUE || HAVE_PSELECT || USE_TIMERFD    
    
    
#ifdef USE_SELECT
    int maxDescriptor = -1;   
    FD_ZERO(&input_set);
    FD_ZERO(&output_set); 
#endif // USE_SELECT
    
#ifdef USE_TIMESPEC
    struct timespec timeout;
    struct timespec* timeoutPtr = NULL;
#else
    struct timeval timeout;
    struct timeval* timeoutPtr = NULL;
#endif // if/else USE_TIMESPEC
    //double timerDelay = ProtoTimerMgr::GetTimeRemaining();
    double timerDelay = timer_delay;
    if (timerDelay < 0.0)
    {
#ifdef USE_TIMERFD
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;
#endif //USE_TIMERFD

    }
    else
    {
// We have observed some different thresholds on "precise timing" for
// for different systems and APIs
#if defined(MACOSX)
#define PRECISE_THRESHOLD 1.0e-05  // about 10 microseconds for MacOS whether select(), kevent(), etc
#elif defined(LINUX)
#ifdef USE_TIMERFD
#define PRECISE_THRESHOLD 2.0e-05  // about 20 microseconds for Linux timerfd usage
#else
#define PRECISE_THRESHOLD 2.0e-03  // about 2 msec for Linux select()
#endif  // if/else USE_TIMERFD
#else
#define PRECISE_THRESHOLD 2.0e-03  // assume 2 msec for other Unix
#endif  // if/else MACOSX / LINUX / OTHER           
                      
        // If (true == precise_timing) essentially force polling for small delays
        // (Note this will consume CPU resources)
        if (precise_timing && (timerDelay < PRECISE_THRESHOLD)) timerDelay = 0.0;
        timeout.tv_sec = (unsigned long)timerDelay;
#ifdef USE_TIMESPEC
        timeout.tv_nsec = 
            (unsigned long)(1.0e+09 * (timerDelay - (double)timeout.tv_sec));
#else
        timeout.tv_usec = 
            (suseconds_t)(1.0e+06 * (timerDelay - (double)timeout.tv_sec));
#endif // if/else USE_TIMESPEC
        timeoutPtr = &timeout;
#ifdef USE_TIMERFD
        if ((0 != timeout.tv_nsec) || (0 != timeout.tv_sec))
        {
            // Install the timerfd descriptor to our "input_set"
            // configured with an appropriate one-shot timeout
            struct itimerspec timerSpec;
            timerSpec.it_interval.tv_sec =  timerSpec.it_interval.tv_nsec = 0;  // non-repeating timeout?
            timerSpec.it_value = timeout;
            if (0 == timerfd_settime(timer_stream.GetDescriptor(), 0, &timerSpec, 0))
            {
#ifdef USE_SELECT
            FD_SET(timer_stream.GetDescriptor(), &input_set);
            if (timer_stream.GetDescriptor() > maxDescriptor) 
                maxDescriptor = timer_stream.GetDescriptor();
#endif  // USE_SELECT
            timeoutPtr = NULL;  // select() (or pselect()) will be using "timer_fd" instead
            }
            else
            {
                PLOG(PL_ERROR, "ProtoDispatcher::Wait() timerfd_settime() error: %s\n", GetErrorString());

            }
        }
#endif // USE_TIMERFD
    }
    
#if defined(USE_SELECT)  
    // Monitor "break_pipe" if we are a threaded dispatcher
    if (IsThreaded())
    {
        FD_SET(break_event.GetDescriptor(), &input_set);  
        if (break_event.GetDescriptor() > maxDescriptor)
            maxDescriptor = break_event.GetDescriptor();
    }
    
    // Iterate through and add streams to our FD_SETs
    StreamTable::Iterator iterator(stream_table);
    Stream* stream;
    while (NULL != (stream = iterator.GetNextItem()))
    {
        if (stream->IsInput()) 
        {
            Descriptor descriptor = stream->GetInputHandle();
            FD_SET(descriptor, &input_set);
            if (descriptor > maxDescriptor) maxDescriptor = descriptor;
        }
        if (stream->IsOutput()) 
        {
            Descriptor descriptor = stream->GetOutputHandle();
            FD_SET(descriptor, &output_set);
            if (descriptor > maxDescriptor) maxDescriptor = descriptor;
        }
        // TBD - handle exception notification ???
    }
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
                         (timeval*)timeoutPtr);
#endif  // if/else HAVE_PSELECT
    
#elif defined(USE_EPOLL)
    
    // If no sockets were installed yet, create epoll_fd early
    if (-1 == epoll_fd)
    {
        if (-1 == (epoll_fd = epoll_create1(0)))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::Wait() epoll_create() error: %s\n",
                           GetErrorString());
            return;
        }
    }

    // Note if (NULL == timeoutPtr), then the timer_fd has been set up with the proper timeout value
    // (otherwise we convert "timerDelay" to milliseconds)
  	wait_status = epoll_wait(epoll_fd, epoll_event_array, EPOLL_ARRAY_SIZE, (NULL != timeoutPtr) ? (int)(timerDelay*1000.0) : -1);
     
#elif defined(USE_KQUEUE)
    if (-1 == kevent_queue)
    {
        // Need to create our kqueue instance
        if (-1 == (kevent_queue = kqueue()))
        {
            PLOG(PL_ERROR, "ProtoDispatcher::Wait() kqueue() error: %s\n", GetErrorString());
            // TBD - should we set "run" to false here???
            wait_status = -1;
            return;
        }
    }
    wait_status = kevent(kevent_queue, NULL, 0, kevent_array, KEVENT_ARRAY_SIZE, timeoutPtr);
    // TBD - should we print a message here on error? (Dispatch() does this for us)
#else
#error "undefined async i/o mechanism"  // to make sure we implement something    
#endif // !USE_SELECT && !USE_KQUEUE
    
}  // end ProtoDispatcher::Wait()

void ProtoDispatcher::Dispatch()
{
#if defined(USE_SELECT)
    // Here the "wait_status" is the return value from the select() call
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
            // Iterate through and check/dispatch streams 
            StreamTable::Iterator iterator(stream_table);
            Stream* stream;
            while (NULL != (stream = iterator.GetNextItem()))
            {
                switch (stream->GetType())
                {
                    case Stream::CHANNEL:
                    {
                        ProtoChannel& theChannel = static_cast<ChannelStream*>(stream)->GetChannel();
                        // A channel _might_ be implemented with separate input/output descriptors
                        if (stream->IsInput())
                        {
                            if (FD_ISSET(theChannel.GetInputEventHandle(), &input_set))
                                theChannel.OnNotify(ProtoChannel::NOTIFY_INPUT);
                        }
                        // Note that if the input notification handling caused the
                        // channel to even be deleted, because the stream is "pooled"
                        // and its flags zero, we're safe. If the channel is removed
                        // and the stream repurposed, we could throw a false notification
                        // here, but no big deal (same is true for socket handling
                        // immediately below). TBD - use FD_CLR in "UpdateChannelNotification()" 
                        // to avoid this possibility
                        if (stream->IsOutput())
                        {
                            if (FD_ISSET(theChannel.GetOutputEventHandle(), &output_set))
                                theChannel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                        }
                        break;
                    }
                    case Stream::SOCKET:
                    {
                        // A socket has a single input/output descriptor
                        ProtoSocket& theSocket = static_cast<SocketStream*>(stream)->GetSocket();
                        Descriptor descriptor = theSocket.GetHandle();
                        if (stream->IsInput() && FD_ISSET(descriptor, &input_set))
                            theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                        // TBD - what if stream and/or theSocket was deleted?
                        if (stream->IsOutput() && FD_ISSET(descriptor, &output_set))
                            theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                        break;
                    }
                    case Stream::GENERIC:
                    {
                        // A generic stream has a single input/output descriptor
                        Descriptor descriptor = static_cast<GenericStream*>(stream)->GetDescriptor();
                        if (stream->IsInput() && FD_ISSET(descriptor, &input_set))
                            static_cast<GenericStream*>(stream)->OnEvent(EVENT_INPUT);
                        if (stream->IsOutput() && FD_ISSET(descriptor, &output_set))
                            static_cast<GenericStream*>(stream)->OnEvent(EVENT_OUTPUT);
                        break;
                    }
                    case Stream::TIMER:
                        break;
                    case Stream::EVENT:
                    {
                        EventStream* eventStream = static_cast<EventStream*>(stream);
                        if (FD_ISSET(eventStream->GetDescriptor(), &input_set))
                        {
                            ProtoEvent& theEvent = eventStream->GetEvent();
                            if (theEvent.GetAutoReset()) theEvent.Reset();
                            theEvent.OnNotify(); 
                        }
                        break;
                    }
                }  // end switch stream->GetType())
            }
            if ((INVALID_DESCRIPTOR != break_stream.GetDescriptor()) && 
                 FD_ISSET(break_stream.GetDescriptor(), &input_set))
            {
                // We were signaled so reset our break_event
                break_event.Reset();  // break_event is auto reset
            }
#ifdef USE_TIMERFD
            // TBD - should we put this code up at the top as the first check
            // instead of checking all descriptors, all the time (maybe "epoll()" will help?)
            if (timer_stream.GetDescriptor() != INVALID_DESCRIPTOR &&
                FD_ISSET(timer_stream.GetDescriptor(), &input_set))
            {
                uint64_t expirations = 0;
                if (read(timer_stream.GetDescriptor(), &expirations, sizeof(expirations)) < 0)
                    PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() read(timer_fd) error: %s\n", GetErrorString());
            }
#endif // USE_TIMERFD
            OnSystemTimeout();
            break;
    }  // end switch(wait_status) [USE_SELECT]
    
#elif defined(USE_EPOLL)   
    switch(wait_status)
    {
        case -1:
            if (EINTR != errno)
                PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() epoll_wait() error: %s\n", GetErrorString());
            break;
            
        case 0: 
            // timeout only
            OnSystemTimeout(); 
            break;
            
        default:
            struct epoll_event* evp = epoll_event_array;
            for (int i = 0; i < wait_status; i++)
            {
                // First check for an error condition
                /*if (0 != (EPOLLERR & evp->events))
                {
                    // We don't check for EPOLLHUP since ProtoSocket handles that
                    PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() epoll event error: %s\n", GetErrorString());
                }
                else */
                if (NULL != evp->data.ptr)
                {
                    // TBD - process EPOLLHUP events?
                    Stream* stream = (Stream*)evp->data.ptr;
                    switch (stream->GetType())
                    {
                        case Stream::CHANNEL:
                        {
                            ProtoChannel& channel = static_cast<ChannelStream*>(stream)->GetChannel();
                            if (0 != (EPOLLIN & evp->events))
                            {
                                if (stream->IsInput())
                                    channel.OnNotify(ProtoChannel::NOTIFY_INPUT);
                            }
                            if (0 != (EPOLLOUT & evp->events))
                            {
                                if (stream->IsOutput())
                                    channel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                            }
                            if (0 != (EPOLLERR & evp->events))
                            {
                                PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() ProtoChannel epoll event error: %s\n", GetErrorString());
                                // Throw a notification so error will be detected by app???
                                if (stream->IsInput())
                                    channel.OnNotify(ProtoChannel::NOTIFY_INPUT);
                                else if (stream->IsOutput())
                                    channel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                            }
                            break;
                        }
                        case Stream::SOCKET:
                        {
                            ProtoSocket& socket = static_cast<SocketStream*>(stream)->GetSocket();
                            if (0 != (EPOLLIN & evp->events))
                            {
                                if (stream->IsInput())
                                    socket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                            }
                            if (0 != (EPOLLOUT & evp->events))
                            {
                                if (stream->IsOutput())
                                    socket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                            }
                            if (0 != (EPOLLERR & evp->events))
                            {
                                socket.OnNotify(ProtoSocket::NOTIFY_ERROR);
                                // Alternatively throw an input or output notification so app gets notified?
                                //if (stream->IsInput())
                                //    socket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                                //else if (stream->IsOutput())
                                //    socket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                            }
                            break;
                        }
                        case Stream::GENERIC:
                        {
                            if (0 != (EPOLLIN & evp->events))
                            {
                                if (stream->IsInput())
                                    static_cast<GenericStream*>(stream)->OnEvent(EVENT_INPUT);
                            }
                            if (0 != (EPOLLOUT & evp->events))
                            {
                                if (stream->IsOutput())
                                    static_cast<GenericStream*>(stream)->OnEvent(EVENT_OUTPUT);
                            }
                            if (0 != (EPOLLERR & evp->events))
                            {
                                // Throw an input or output notification so app gets notified?
                                if (stream->IsInput())
                                    static_cast<GenericStream*>(stream)->OnEvent(EVENT_INPUT);
                                else if (stream->IsOutput())
                                    static_cast<GenericStream*>(stream)->OnEvent(EVENT_OUTPUT);
                            }
                            break;
                        }
                        case Stream::TIMER:
                        {
#ifdef USE_TIMERFD
                            // This _should_ only be our ProtoDispatcher::timer_stream
                            // Clear the timerfd with a read() call (timeout dispatched below)
                            uint64_t expirations = 0;
                            if (read(static_cast<TimerStream*>(stream)->GetDescriptor(), &expirations, sizeof(expirations)) < 0)
                                PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() read(timer_fd) error: %s\n", GetErrorString());
#endif // USE_TIMERFD
                            break;
                        }
                        case Stream::EVENT:
                        {
                            if (0 != (EPOLLIN & evp->events))
                            {
                                EventStream* eventStream = static_cast<EventStream*>(stream);
                                ProtoEvent& event = eventStream->GetEvent();
                                if (event.GetAutoReset()) event.Reset();
                                event.OnNotify();  
                            }
                            break; 
                        }
                    }  // end switch(stream->GetType()) [USE_EPOLL]
                }
                else
                {
                    // This must be a nullified event so do nothing
                }
                evp++;               
            }  // end for (i = 0..wait_status)
            OnSystemTimeout();
            break;
    }  // end switch(wait_status) [USE_EPOLL]   
    
#elif defined(USE_KQUEUE)
    // Here the "wait_status" is the return value from the kevent() call
    switch (wait_status)
    {
        case -1:
            if (EINTR != errno)
                PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() kevent() error: %s\n", GetErrorString());
            //OnSystemTimeout();
            break;
            
        case 0:
            // timeout only
            OnSystemTimeout();
            break;
            
        default:
        {
            // The return value is the number of events available
            // (Note "wait_status" may change on the fly, here if
            //  notification results in stream removal)
            struct kevent* kep = kevent_array;
            for (int i = 0; i < wait_status; i++)
            {
                // First check if there was an error for the event
                if (0 != (EV_ERROR & kep->flags))
                {
                    PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() kqueue event error: %s\n", GetErrorString((int)(kep->data)));
                    kep++;
                    continue;
                }
                Stream* stream = (Stream*)kep->udata;
                switch (kep->filter)
                {
                    case EVFILT_READ:
                        ASSERT(NULL != stream);
                        if (!stream->IsInput()) break;
                        switch (stream->GetType())
                        {
                            case Stream::CHANNEL:
                                static_cast<ChannelStream*>(stream)->GetChannel().OnNotify(ProtoChannel::NOTIFY_INPUT);
                                break;
                            case Stream::SOCKET:
                                static_cast<SocketStream*>(stream)->GetSocket().OnNotify(ProtoSocket::NOTIFY_INPUT);
                                break;
                            case Stream::GENERIC:
                                static_cast<GenericStream*>(stream)->OnEvent(EVENT_INPUT);
                                break;
                            case Stream::TIMER:
                                // No Stream::TIMER or Stream::EVENT is used eith EVFILT_READ for now
                                break;
                            case Stream::EVENT:
                                EventStream* eventStream = static_cast<EventStream*>(stream);
                                ProtoEvent& theEvent = eventStream->GetEvent();
                                if (theEvent.GetAutoReset()) theEvent.Reset();
                                theEvent.OnNotify();  
                                break;
                        }
                        break;
                        
                    case EVFILT_WRITE:
                        ASSERT(NULL != stream);
                        if (!stream->IsOutput()) break;
                        switch (stream->GetType())
                        {
                            case Stream::CHANNEL:
                                static_cast<ChannelStream*>(stream)->GetChannel().OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                                break;
                            case Stream::SOCKET:
                                static_cast<SocketStream*>(stream)->GetSocket().OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                                break;
                            case Stream::GENERIC:
                                static_cast<GenericStream*>(stream)->OnEvent(EVENT_OUTPUT);
                                break;
                            case Stream::TIMER:
                            case Stream::EVENT:
                                // No Stream::TIMER or Stream::EVENT is used with EVFILT_WRITE
                                break;
                        }
                        break;
                    
                    case EVFILT_USER:
                        // Nothing to be done since EVFILT_USER w/ EV_CLEAR resets itself
                        // (or was nullified event)
                        break;
                    
                    case EVFILT_TIMER:
                        // We found that "EVFILT_TIMER" was not as precise as the kevent() timeout, so we don't use it
                    default:
                        PLOG(PL_ERROR, "ProtoDispatcher::Dispatch() error: unexpected event filter type %d\n", kep->filter);
                        break;
                        
                }  // switch(kep->filter)
                kep++;
            }
            OnSystemTimeout();  // called to service timers in case one or more is ready
            break;
        }   
    }  // end switch(wait_status) [USE_KQUEUE]
#else // !USE_SELECT && !USE_KQUEUE   
#error "undefined async i/o mechanism"  // to make sure we implement something
#endif  // end if/else USE_SELECT, USE_KQUEUE, ...
    wait_status = 0;  // reset "wait_status" since we're done
}  // end ProtoDispatcher::Dispatch()

#endif // UNIX


#ifdef WIN32

bool ProtoDispatcher::InstallBreak()
{
    ASSERT(INVALID_DESCRIPTOR == break_event.GetDescriptor());
    if (!break_event.Open())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() CreateEvent() error\n");
        return false;
    }
	int index = Win32AddStream(break_stream, break_event.GetDescriptor());
	if (index < 0)
	{
	    PLOG(PL_ERROR, "ProtoDispatcher::InstallBreak() error: add break_stream failed!\n");
	    break_event.Close();
	    return false;
	}
	break_stream.SetIndex(index);
    return true;
}  // end ProtoDispatcher::InstallBreak()

bool ProtoDispatcher::SetBreak()
{
    if (!break_event.Set())
    {
        PLOG(PL_ERROR, "ProtoDispatcher::SetBreak() break_event.Set() error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end ProtoDispatcher::SetBreak()

void ProtoDispatcher::RemoveBreak()
{
	if (INVALID_DESCRIPTOR != break_stream.GetDescriptor())
    {
		Win32RemoveStream(break_stream.GetIndex());
		break_stream.SetIndex(-1);
        break_event.Close();
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

    // Create hidden "msg_window" to receive event window messages
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
    if (NULL != msg_window)
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
    if (NULL != msg_window)
    {
        DestroyWindow(msg_window);
        msg_window = NULL;
    }
}  // end Win32Cleanup()


// WIN32 implementation of ProtoDispatcher::UpdateStreamNotification()
bool ProtoDispatcher::UpdateStreamNotification(Stream& stream, NotificationCommand cmd)
{
    switch (cmd)
    {
        case ENABLE_INPUT:
        {
			ASSERT(stream.GetIndex() < 0);  // input already enabled?!
			int index = Win32AddStream(stream, stream.GetInputHandle());
            if (index < 0)
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification() error adding input stream\n");
                return false;
            }
            stream.SetIndex(index);
            stream.SetNotifyFlag(Stream::NOTIFY_INPUT);
            break;          
        }
        case DISABLE_INPUT:
        {
			ASSERT(stream.GetIndex() >= 0);
            Win32RemoveStream(stream.GetIndex());
            stream.SetIndex(-1);
            stream.UnsetNotifyFlag(Stream::NOTIFY_INPUT);
            break;
        }
		case ENABLE_OUTPUT:
        {
			ASSERT(stream.GetOutdex() < 0);  // output already enabled?!
            int outdex = Win32AddStream(stream, stream.GetOutputHandle());
            if (outdex < 0)
            {
                PLOG(PL_ERROR, "ProtoDispatcher::UpdateStreamNotification() error adding output stream\n");
                return false;
            }
            stream.SetOutdex(outdex);
            stream.SetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;          
        }
        case DISABLE_OUTPUT:
        {
            ASSERT(stream.GetOutdex() >= 0);
            Win32RemoveStream(stream.GetOutdex());
            stream.SetOutdex(-1);
            stream.UnsetNotifyFlag(Stream::NOTIFY_OUTPUT);
            break;
        }
        case DISABLE_ALL:
        {       
			if (stream.GetIndex() >= 0) 
            {
                Win32RemoveStream(stream.GetIndex());
                stream.SetIndex(-1);
            }
            if (stream.GetOutdex() >= 0)
            {
                Win32RemoveStream(stream.GetOutdex());
                stream.SetIndex(-1);
            }  
            stream.ClearNotifyFlags();
            break;
        }
    }
    return true;
}  // end ProtoDispatcher::UpdateStreamNotification() [WIN32]

int ProtoDispatcher::Win32AddStream(Stream& stream, HANDLE handle)
{
    DWORD index = stream_count;
    if (index >= stream_array_size)
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
    unsigned int newSize = (0 != stream_array_size) ? (2 * stream_array_size) : DEFAULT_STREAM_ARRAY_SIZE;
    HANDLE* hPtr = new HANDLE[newSize];
    Stream** iPtr = new Stream*[newSize];
    if ((NULL != hPtr) && (NULL != iPtr))
    {
        if (0 != stream_count)
        {
            memcpy(hPtr, stream_handles_array, stream_count*sizeof(HANDLE));
            memcpy(iPtr, stream_ptrs_array, stream_count*sizeof(Stream*));
        }
        if (NULL != stream_handles_array)
        {
            delete[] stream_handles_array;
            delete[] stream_ptrs_array;
        }
        stream_handles_array = hPtr;
        stream_ptrs_array = iPtr;
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDispatcher::Win32IncreaseStreamArraySize() new stream_array error: %s\n", 
                       GetErrorString());
        if (NULL != hPtr) delete[] hPtr;
        if (NULL != iPtr) delete[] iPtr;
        return false;    
    }        
}  // end ProtoDispatcher::Win32IncreaseStreamArraySize()

void ProtoDispatcher::Wait()
{
    double timerDelay = timer_delay;
	// Don't wait if we have "ready" streams pending
	if (!ready_stream_list.IsEmpty()) timerDelay = 0.0;

#ifdef USE_WAITABLE_TIMER
	DWORD msec = INFINITE;
	if (timerDelay < 0.0)
	{
		if (timer_active)
		{
			CancelWaitableTimer(timer_stream.GetDescriptor());
			timer_active = false;
		}
		if (timer_stream.GetIndex() >= 0)
		{
			Win32RemoveStream(timer_stream.GetIndex());
			timer_stream.SetIndex(-1);
		}
	}
	else if (0.0 == timerDelay)
	{
		if (timer_active)
		{
			CancelWaitableTimer(timer_stream.GetDescriptor());
			timer_active = false;
		}
		if (timer_stream.GetIndex() >= 0)
		{
			Win32RemoveStream(timer_stream.GetIndex());
			timer_stream.SetIndex(-1);
		}
		msec = 0;
	}
	else
	{
		// The "negative" dueTime makes a "relative" waitable timer
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = (LONGLONG)(-(timerDelay * 1.0e+07));  // convert to 100 nsec ticks
		LONG period = 0;
		if (0 == SetWaitableTimer(timer_stream.GetDescriptor(), &dueTime, 0, NULL, NULL, FALSE))
		{
			PLOG(PL_ERROR, "ProtoDispatcher::Wait() SetWaitableTimer() error: %s\n", GetErrorString());
			msec = (DWORD)(1000.0 * timerDelay);
			if ((timerDelay > 0.0) && (0 == msec) && !precise_timing) msec = 1;
			timer_active = false;
		}
		else
		{
			if (timer_stream.GetIndex() < 0)
			{
				int index = Win32AddStream(timer_stream, timer_stream.GetDescriptor());
				if (index < 0)
				{
					PLOG(PL_ERROR, "ProtoDispatcher::Wait() error: unable to insert timer_stream\n");
					msec = (DWORD)(1000.0 * timerDelay);
					if ((timerDelay > 0.0) && (0 == msec) && !precise_timing) msec = 1;
					timer_active = false;
				}
				else
				{
					timer_stream.SetIndex(index);
					timer_active = true;
				}
			}
			else
			{
				timer_active = true;
			}
		}
	}
#else
    DWORD msec = (timerDelay < 0.0) ? INFINITE : ((DWORD)(1000.0 * timerDelay));
    // If (false == precise_timing) enforce minimum delay of 1 msec
    // (Note the busy-wait "precise_timing" will consume additional CPU resources)
    if ((timerDelay > 0.0) && (0 == msec) && !precise_timing) msec = 1;
#endif  // else/if USE_WAITABLE_TIMER
	
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
        default:
		{
            // Handle any "ready" sockets that are "io_pending"
            // (e.g., WIN32 only does "edge triggering" on sockets)
            // TBD - keep a separate list of io_pending socket streams?
			StreamList::Iterator sit(ready_stream_list);
			Stream* nextStream;
			while (NULL != (nextStream = sit.GetNextItem()))
			{
                switch (nextStream->GetType())
                {
					case Stream::CHANNEL:
                    {
                        ProtoChannel& theChannel = static_cast<ChannelStream*>(nextStream)->GetChannel();
                        // (TBD) Make this safer (i.e. if notification destroys channel)
                        if (theChannel.InputNotification())
                            theChannel.OnNotify(ProtoChannel::NOTIFY_INPUT);
                        if (theChannel.OutputNotification())
                            theChannel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
                        break;
                    }
                    case Stream::SOCKET:
                    {
						ProtoSocket& theSocket = static_cast<SocketStream*>(nextStream)->GetSocket();
                        // (TBD) Make this safer (i.e. if notification destroys socket)
                        if (theSocket.InputNotification())
                            theSocket.OnNotify(ProtoSocket::NOTIFY_INPUT);
                        if (theSocket.OutputNotification())
                            theSocket.OnNotify(ProtoSocket::NOTIFY_OUTPUT);
                        break;
                    }
                    default:
                    {
                        ASSERT(0);
                        break;
                    }
                }
            }
            if (WAIT_TIMEOUT == wait_status) 
            {
                break;
            }
            else if ((WAIT_OBJECT_0 <= wait_status) && (wait_status < (WAIT_OBJECT_0 + stream_count)))
            {
                unsigned int index = wait_status - WAIT_OBJECT_0;
				Stream* stream = stream_ptrs_array[index];
				switch (stream->GetType())
				{
					case Stream::SOCKET:
					{
						ProtoSocket& theSocket = static_cast<SocketStream*>(stream)->GetSocket();
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
						break;
					}  // end case Stream::SOCKET
					case Stream::CHANNEL:
					{
						ProtoChannel& theChannel = static_cast<ChannelStream*>(stream)->GetChannel();
						if (index == (unsigned int)stream->GetIndex())
							theChannel.OnNotify(ProtoChannel::NOTIFY_INPUT);
						else // (index == stream->GetOutdex())
							theChannel.OnNotify(ProtoChannel::NOTIFY_OUTPUT);
						break;
					}
					case Stream::TIMER:
					{
#ifdef USE_WAITABLE_TIMER
					    // Our one and only "timer_stream"
					    ResetEvent(timer_stream.GetDescriptor());
					    timer_active = false;
#endif // USE_WAITABLE_TIMER
					    break;
					}
					case Stream::EVENT:
					{
					    EventStream* eventStream = static_cast<EventStream*>(stream);
                        ProtoEvent& event = eventStream->GetEvent();
                        if (event.GetAutoReset()) event.Reset();
                        // Note the "break_stream" has no listener, but others might
                        if (&break_stream != eventStream) event.OnNotify();  
                        break;
					}
					case Stream::GENERIC:
					{
						// (TBD) Can we test the handle for input/output readiness?
						if (stream->IsInput()) 
							static_cast<GenericStream*>(stream)->OnEvent(EVENT_INPUT);
						if (stream->IsOutput()) 
							static_cast<GenericStream*>(stream)->OnEvent(EVENT_OUTPUT);
						break;
					}
				}  // end switch (stream->GetType())
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
		}  // end case default
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
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dp);
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
 * ProtoDispatcher::Controller implementation
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

