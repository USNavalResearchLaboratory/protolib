#ifndef _PROTO_PIPE
#define _PROTO_PIPE

#include "protoSocket.h"
#include "protoDefs.h"
/**
 * @class ProtoPipe 
 *
 * @brief Provides a cross-platform interprocess communication
 * mechanism for Protolib using Unix-domain sockets (UNIX) or 
 * named pipe/mailslot mechanisms (WIN32).  This class 
 * extends the "ProtoSocket" class to support "LOCAL" 
 * domain interprocess communications.
 */
class ProtoPipe : public ProtoSocket
{
    public:
        enum Type {MESSAGE, STREAM};

		ProtoPipe(Type theType = MESSAGE);
        ~ProtoPipe();
        
        Type GetType() {return ((UDP == GetProtocol()) ? MESSAGE : STREAM);}
        const char* GetName() {return path;}
        bool Connect(const char* serverName);
        bool Listen(const char* theName);
        bool Accept(ProtoPipe* thePipe = NULL);
        void Close();

        bool Send(const char* buffer, unsigned int& numBytes)
            {return ProtoSocket::Send(buffer, numBytes);}
        bool Recv(char* buffer, unsigned int& numBytes)
            {return ProtoSocket::Recv(buffer, numBytes);}
        
    private:
        bool Open(const char* theName);
#ifndef WIN32
        void Unlink(const char* theName);
        bool unlink_tried;
#else
#ifdef _NEVER_
        virtual bool SetBlocking(bool blocking);
        HANDLE       pipe_handle;
		bool         is_mailslot;  
        // These member facilitate Win32 overlapped I/O
        // which we use for async I/O
        enum {BUFFER_MAX = 8192};
        char         read_buffer[BUFFER_MAX];
        unsigned int read_count;
        unsigned int read_index;
        char         write_buffer[BUFFER_MAX];
        OVERLAPPED   read_overlapped;
        OVERLAPPED   write_overlapped;
#endif  
        HANDLE       named_event_handle;
#endif // if/else !WIN32
        char        path[PATH_MAX];
};

#endif // !_PROTO_PIPE
