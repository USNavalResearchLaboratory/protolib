#include "protoZMQ.h"
#include "protoDebug.h"

ProtoZmq::Socket::Socket()
  : ProtoSocket(ZMQ), zmq_ctx(NULL), zmq_sock(NULL)
{
}

ProtoZmq::Socket::~Socket()
{
    Close();
}

bool ProtoZmq::Socket::RetrieveDescriptor()
{
    size_t len = sizeof(ProtoSocket::Handle);
    if (0 != zmq_getsockopt(zmq_sock, ZMQ_FD, &handle, &len))
    {
        PLOG(PL_ERROR, "ProtoZmq::Socket::RetrieveDescriptor() zmq_getsockopt() error: %s\n", GetErrorString());
        return false;
    }
    TRACE("ProtoZmq::Socket::RetrieveDescriptor() retrieved handle %d\n", handle);
    return true;
}  // end ProtoZmq::Socket::RetrieveDescriptor()

bool ProtoZmq::Socket::Open(int socketType, void* zmqSocket, void* zmqContext)
{
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
    
    RetrieveDescriptor();
    
#ifdef WIN32
    // Create ProtoSocket::input_event_handle needed for WIN32 event notification
    if (NULL == (input_event_handle = WSACreateEvent()))
    {
        PLOG(PL_ERROR, "ProtoSocket::Open() WSACreateEvent() error: %s\n", GetErrorString());
        Close();
        return false;
    } 
	input_ready = false;
	output_ready = true;
#endif  // WIN32
    state = IDLE;
    return UpdateNotification();
    
}  // emd ProtoZmq::Socket::Open()

void ProtoZmq::Socket::Close()
{
    if (IsOpen()) 
    {
        state = CLOSED;
        UpdateNotification();   
#ifdef WIN32
        if (NULL != input_event_handle)
        {
            WSACloseEvent(input_event_handle);
            input_event_handle = NULL;
        }
#endif // if WIN32
    }
    if (NULL != zmq_sock)
    {
        zmq_close(zmq_sock);
        zmq_sock = NULL;
    }
    handle = INVALID_HANDLE;   
    if (!ext_ctx)
    {
        zmq_ctx_destroy(zmq_ctx);
        zmq_ctx = NULL;
    }    
}  // end ProtoZmq::Socket::Close()

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
    RetrieveDescriptor();  // just in case
    return UpdateNotification();
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
    RetrieveDescriptor();  // just in case
    return UpdateNotification();
}  // end ProtoZmq::Socket::Connect()


bool ProtoZmq::Socket::Send(char* buffer, unsigned int& numBytes)
{
    // TBD - support both blocking and non-blocking operation as well as multi-part messages
    int result = zmq_send(zmq_sock, buffer, numBytes, ZMQ_DONTWAIT);
    if (result < 0)
    {
        switch (errno)
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
    if (result < 0)
    {
        switch (errno)
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
