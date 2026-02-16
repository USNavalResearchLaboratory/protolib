 
#include "protokit.h"
#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

#define PIPE_TYPE ProtoPipe::MESSAGE

/**
 * @class Sock2Pipe
 *
 * @brief This example program illustrates/tests the use of the ProtoPipe class
 * for local domain interprocess communications
 */
class Sock2Pipe : public ProtoApp
{
    public:
        Sock2Pipe();
        ~Sock2Pipe();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv);
        void OnShutdown();
        
        bool OnCommand(const char* cmd, const char* val);
            
    private:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static CmdType GetCmdType(const char* string);
        static const char* const CMD_LIST[];        
        void Usage();
        
        void OnListenEvent(ProtoSocket&       theSocket, 
                           ProtoSocket::Event theEvent);
        
        void SendMessage();

        ProtoPipe    send_pipe;
        char         send_pipe_name[8191];
        
        ProtoSocket  socket;
        int          port;
        ProtoAddress addr;
       
        char         msg_buffer[8191];
        unsigned int msg_len;
};  // end class Sock2Pipe

// Instantiate our application instance 
PROTO_INSTANTIATE_APP(Sock2Pipe) 
        
Sock2Pipe::Sock2Pipe()
 : send_pipe(PIPE_TYPE), socket(ProtoSocket::UDP),
   msg_len(8191)
{
    memset(msg_buffer,0,8191);
    socket.SetNotifier(&GetSocketNotifier());
    socket.SetListener(this, &Sock2Pipe::OnListenEvent);
    port = 11151;//this should be set to the default port sdt listens too

    sprintf(send_pipe_name,"sdt");
}

Sock2Pipe::~Sock2Pipe()
{
}

void Sock2Pipe::Usage()
{
    fprintf(stderr, "sock2Pipe [listen [<mcastAddr>/]<port>][send <pipename>]\n");
}  // end Sock2Pipe::Usage()
  
bool Sock2Pipe::OnStartup(int argc, const char*const* argv)
{
    if (argc == 1)
    {
        TRACE("No args!  Are you sure you want to be running without any args?\n");
        Usage();
    }
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "sock2Pipe::OnStartup() error processing command line\n");
        Usage();
        return false;
    }
    if(!(socket.Open(port,ProtoAddress::IPv4)))
    {
        PLOG(PL_ERROR,"sock2Pipe::OnStartup() error opening the socket on port  %d\n",port);
        return false;
    }
    TRACE("Listening to port %d\n",port);
    if(addr.IsMulticast()) //do multicast stuff
    {
        if(!socket.JoinGroup(addr))
        {
            PLOG(PL_ERROR,"sock2Pipe::OnStartup() error joining multicast group %s\n",addr.GetHostString());
            socket.Close();
            return false;
        }
        TRACE("Joined multicast group %s\n",addr.GetHostString());
    }
    //connect to a sending pipe 
    if (!send_pipe.Connect(send_pipe_name))
    {
        TRACE("Sock2Pipe::OnCommand() send_pipe.Connect() error connecting to pipe with name \"%s\".\nWill continue to attempt to connect.\n",send_pipe_name);
    }
    else 
    {
        TRACE("sock2Pipe: sending to pipe \"%s\"\n", send_pipe_name);
    }
    return true;
}  // end Sock2Pipe::OnStartup()

void Sock2Pipe::OnShutdown()
{
    if (send_pipe.IsOpen()) send_pipe.Close();
    if (socket.IsOpen()) 
    {
        if(addr.IsMulticast())
        {
            socket.LeaveGroup(addr);
        } 
        socket.Close();
    }
    TRACE("sock2Pipe: Done.\n");
}  // end Sock2Pipe::OnShutdown()

void Sock2Pipe::OnListenEvent(ProtoSocket&       /*theSocket*/, 
                                ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("sock2Pipe: listen RECV event ..\n");
            unsigned int len = 8191;
            if (socket.Recv(msg_buffer, len))
            {
                if (len){
                    SendMessage();
                    TRACE("sock2Pipe: recvd \"%s\"\n", msg_buffer);
                }
            }
            else
            {
                PLOG(PL_ERROR, "Sock2Pipe::OnListenEvent() listen_pipe.Recv() error\n");
            }            
            break;
        }
        case ProtoSocket::SEND:
            TRACE("sock2Pipe: listen SEND event ..\n");
            break;
        case ProtoSocket::ACCEPT:
            TRACE("sock2Pipe: listen ACCEPT event ..\n");
            break;
        case ProtoSocket::DISCONNECT:
            TRACE("sock2Pipe: listen DISCONNECT event ..\n");
            break;
        default:
            TRACE("Sock2Pipe::OnListenEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end Sock2Pipe::OnListenEvent()

void Sock2Pipe::SendMessage()
{
    if(!send_pipe.IsOpen())
    {
        //try and connect to the pipe again maybe its up now...
        send_pipe.Connect(send_pipe_name);
    }
    if(send_pipe.IsOpen())
    {
        TRACE("Sending message \"%s\" to pipe \"%s\"\n",msg_buffer,send_pipe_name);
        unsigned int len = strlen(msg_buffer);
        send_pipe.Send(msg_buffer,len);
        memset(msg_buffer,0,msg_len); //clear out the message
    } 
    else 
    {
        TRACE("Unable to send message \"%s\" to pipe \"%d\"\n",msg_buffer,send_pipe_name);
    }
    
}

bool Sock2Pipe::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        if (!strcmp("listen", argv[i]))
        {
            i++;
            const char* index = strchr(argv[i],'/');
            if(index!=NULL)
            {
                char charAddr[255];
                memset(charAddr,0,255);
                strncpy(charAddr,argv[i],index-argv[i]);
                if(!(addr.ResolveFromString(charAddr)))
                {
                    PLOG(PL_ERROR, "Sock2Pipe::OnCommand() error setting address to \"%s\"\n",charAddr);
                    return false;
                }
                port = atoi(index+1);
            }
            else 
            {
                port = atoi(argv[i]);
            }
        }
        else if (!strcmp("send", argv[i]))
        {
            i++;
            strcpy(send_pipe_name,argv[i]);
        }
        else
        {
            PLOG(PL_ERROR, "Sock2Pipe::OnCommand() unknown command error?\n");
            return false;
        }
        i++;
    }
    return true;
}  // end Sock2Pipe::ProcessCommands()
