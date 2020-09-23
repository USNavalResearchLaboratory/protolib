

#ifndef _PROTO_ZMQ
#define _PROTO_ZMQ

#define ZMQ_BUILD_DRAFT_API

#include "protoThread.h"
#include "protoEvent.h"
#include "zmq.h"

class ProtoZmq
{
    protected:
        class PollerThread;  // forward declaration
    
    public:
        // Use the ProtoEvent notification framework for a ZMQ Socket
        // A PollerThread is used to run a zmq_poll() loop to monitor active sockets
        class Socket : private ProtoEvent
        {
            public:
                Socket();
                virtual ~Socket();
                
                enum State
                {
                    CLOSED = 0,
                    IDLE,
                    CONNECTED
                };
                
                // "socketType" is ZMQ socket type.  E.g., ZMQ_PUB, ZMQ_SUB, etc
                bool Open(int socketType, void* zmqSocket = NULL, void* zmqContext = NULL);
                void Close();
                
                bool Bind(const char* endpoint);
                bool Connect(const char* endpoint);
                
                // for ZMQ_SUB sockets
                bool Subscribe(const char* prefix, unsigned int length = 0);  // treats prefix as string if non-NULL and length is zero
                
                // for ZMQ_DISH sockets
                bool Join(const char* group);
       
                // TBD - support multi-part messages, etc
                bool Send(char* buffer, unsigned int& numBytes);
                bool Recv(char* buffer, unsigned int& numBytes);
                bool RecvMsg(zmq_msg_t* zmqMsg);
                bool StartInputNotification();
                bool StopInputNotification();
                bool StartOutputNotification();
                bool StopOutputNotification();
                
                int GetPollFlags() const
                    {return poll_flags;}
                
                int GetPollStatus() const
                    {return poll_status;}
                bool IsInputReady() const
                    {return (0 != ((ZMQ_POLLIN & poll_status)));}
                bool IsOutputReady() const
                    {return (0 != ((ZMQ_POLLOUT & poll_status)));}
                
                void* GetContext()
                    {return zmq_ctx;}
                void* GetSocket() 
                    {return zmq_sock;}
                
                bool IsOpen() const
                    {return (NULL != zmq_sock);}
                
                bool UsingExternalContext() const
                    {return ext_ctx;}
                
                // If a non-NULL pollerThread is used, you should disassociate your sockets from the pollerThread
                // before deleting it! (The "default" NULL pollerThread takes care of itself)
                bool SetNotifier(ProtoEvent::Notifier* theNotifier, class PollerThread* pollerThread = NULL);
                
                template <class listenerType>
                bool SetListener(listenerType* theListener, void(listenerType::*eventHandler)(ProtoEvent&))
                    {return ProtoEvent::SetListener(theListener, eventHandler);}
                
            private:
                friend class PollerThread;
                void SetEvent()
                    {ProtoEvent::Set();}
                void SetPollStatus(int status)
                    {poll_status = status;}
            
                bool UpdateNotification();
            
                static PollerThread* default_poller_thread; 
                static ProtoMutex    default_poller_mutex;  // to guarantee thread-safe instantiation of default_poller_thread    
                    
                State               state;
                void*               zmq_ctx;  // true if zmq_ctx was supplied externally
                bool                ext_ctx;
                void*               zmq_sock;
                bool                ext_sock; // true if zmq_sock was supplied externally
                int                 poll_flags;
                int                 poll_status;
                
                PollerThread*       poller_thread;
                bool                poller_active;
                
        };  // end ProtoZmq::Socket
        
    protected:
        // This class is used to "wire up" thread-safe ZMQ sockets to ProtoDispatcher notifications
        class PollerThread : private ProtoThread
        {
            public:
                PollerThread();
                ~PollerThread();
                
                bool Open(void* zmqContext = NULL, bool retainLock = false);
                void Close();
                
                bool AddSocket(Socket& zmqSock);
                bool ModSocket(Socket& zmqSock);
                bool RemoveSocket(Socket& zmqSock);
                
            private:
                int RunThread();    
                bool SetBreak()
                {
                    char dummy;
                    unsigned oneByte = 1;
                    return break_client.Send(&dummy, oneByte);
                }
                void ClosePrivate();
                bool AddSocketPrivate(Socket& zmqSock);
                bool Signal();
                void Unsignal(); 
            
                void*               zmq_ctx;
                bool                ext_ctx;
                void*               zmq_poller;
                volatile bool       poller_running;
                zmq_poller_event_t* event_array;
                unsigned int        event_array_length;
                unsigned int        socket_count;
                Socket              break_server;  // listens for 'break' messages to interrupt poller
                Socket              break_client;  // used to send 'break' messages to the break_server
                ProtoMutex          suspend_mutex;
                ProtoMutex          signal_mutex;
                
        }; // end class ProtoZmq::PollerThread
    
};  // end class ProtoZmq


#endif // !_PROTO_ZMQ
