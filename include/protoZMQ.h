

#ifndef _PROTO_ZMQ
#define _PROTO_ZMQ

#include "protoSocket.h"
#include "zmq.h"

class ProtoZmq
{
    public:
        // Use the ProtoSocket notification framework for a ZMQ Socket
        class Socket : private ProtoSocket
        {
            public:
                Socket();
                virtual ~Socket();
                
                // "socketType" is ZMQ socket type.  E.g., ZMQ_PUB, ZMQ_SUB, etc
                bool Open(int socketType, void* zmqSocket = NULL, void* zmqContext = NULL);
                void Close();
                
                bool Bind(const char* endpoint);
                bool Connect(const char* endpoint);
       
                // TBD - support multi-part messages, etc
                bool Send(char* buffer, unsigned int& numBytes);
                bool Recv(char* buffer, unsigned int& numBytes);
                
                void* GetContext()
                    {return zmq_ctx;}
                void* GetSocket() 
                    {return zmq_sock;}
                
                bool IsOpen() const
                        {return (NULL != zmq_sock);}
                
                bool SetNotifier(ProtoSocket::Notifier* theNotifier)
                    {return ProtoSocket::SetNotifier(theNotifier);}
                
                template <class listenerType>
                bool SetListener(listenerType* theListener, void(listenerType::*eventHandler)(ProtoSocket&, Event))
                {
                    bool doUpdate = ((NULL != theListener) || (NULL != listener));
                    if (NULL != listener) delete listener;
                    listener = (NULL != theListener) ? new LISTENER_TYPE<listenerType>(theListener, eventHandler) : NULL;
                    bool result = (NULL != theListener) ? (NULL != listener) : true;
                    return result ? (doUpdate ? UpdateNotification() : true) : false;
                }
                
            private:
                bool RetrieveDescriptor();       
                void* zmq_ctx;  // true if zmq_ctx was supplied externally
                bool  ext_ctx;
                void* zmq_sock;
                bool  ext_sock; // true if zmq_sock was supplied externally
                
        };  // end ProtoZmq::Socket
    
    
};  // end class ProtoZmq


#endif // !_PROTO_ZMQ
