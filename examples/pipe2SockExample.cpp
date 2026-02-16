 
#include "protokit.h"
#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

#define PIPE_TYPE ProtoPipe::MESSAGE

/**
 * @class Pipe2Sock
 *
 * @brief This example program illustrates/tests the use of the ProtoPipe class
 * for local domain interprocess communications
 */
class Pipe2Sock : public ProtoApp
{
    public:
        Pipe2Sock();
        ~Pipe2Sock();

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

        ProtoPipe    listen_pipe;
        char         listen_pipe_name[8191];
        
        ProtoSocket  socket;
        ProtoAddress dst_addr;
        int          dst_port;
        int          snd_port;
        
       
        char         msg_buffer[8191];
        unsigned int msg_len;
};  // end class Pipe2Sock

// Instantiate our application instance 
PROTO_INSTANTIATE_APP(Pipe2Sock) 
        
Pipe2Sock::Pipe2Sock()
 : listen_pipe(PIPE_TYPE), socket(ProtoSocket::UDP),
   msg_len(8191)
{
    memset(msg_buffer,0,8191);
    listen_pipe.SetNotifier(&GetSocketNotifier());
    listen_pipe.SetListener(this, &Pipe2Sock::OnListenEvent);
    snd_port = 11151;//doesn't really matter
    dst_port = 11151;//this should be set to the default port sdt listens too
    dst_addr.ResolveFromString("127.0.0.1"); //just send it to the loopback

    sprintf(listen_pipe_name,"sdt");
}

Pipe2Sock::~Pipe2Sock()
{
}

void Pipe2Sock::Usage()
{
    fprintf(stderr, "pipe2Sock [listen <listenName>][send <ipaddress>/<dst_port>][port <snd_port>]\n");
}  // end Pipe2Sock::Usage()
  
bool Pipe2Sock::OnStartup(int argc, const char*const* argv)
{
    if (argc == 1)
    {
        TRACE("No args!  Are you sure you want to be running without any args?\n");
        Usage();
    }
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "pipe2Sock::OnStartup() error processing command line\n");
        Usage();
        return false;
    }
    //set the destination address/port info
    dst_addr.SetPort(dst_port);
    TRACE("Destination address set to %s/%d\n",dst_addr.GetHostString(),dst_port);

    //set the socket sending/binding port info
    if(!(socket.Bind(snd_port))){
        PLOG(PL_ERROR,"pipe2Sock::OnStartup() error binding to port %d\n",snd_port);
        return false;
    }
    if(!(socket.Open(snd_port,ProtoAddress::IPv4,false)))
    {
        PLOG(PL_ERROR,"pipe2Sock::OnStartup() error opening the socket on port  %d\n",snd_port);
        return false;
    }
    if(dst_addr.IsMulticast())
    {
        if(!socket.JoinGroup(dst_addr))
        {
            PLOG(PL_ERROR,"pipe2Sock::OnStartup() error joining multicast group %s\n",dst_addr.GetHostString());
            socket.Close();
            return false;
        }
        TRACE("Joined multicast group %s\n",dst_addr.GetHostString());
    }
    TRACE("Sending port set to %d\n",snd_port);

    //open up a listening pipe 
    if (!listen_pipe.Listen(listen_pipe_name))
    {
        socket.Close();
        PLOG(PL_ERROR, "Pipe2Sock::OnCommand() listen_pipe.Listen() error opening pipe with name %s\n",listen_pipe_name);
        return false;   
    }
    TRACE("pipe2Sock: listen \"%s\" listening ...\n", listen_pipe_name);

    return true;
}  // end Pipe2Sock::OnStartup()

void Pipe2Sock::OnShutdown()
{
    if (listen_pipe.IsOpen()) listen_pipe.Close();
    if (socket.IsOpen())
    { 
        if(dst_addr.IsMulticast())
        {
            socket.LeaveGroup(dst_addr);
        }
        socket.Close();
    }
    PLOG(PL_ERROR, "pipe2Sock: Done.\n");
}  // end Pipe2Sock::OnShutdown()

void Pipe2Sock::OnListenEvent(ProtoSocket&       /*theSocket*/, 
                                ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("pipe2Sock: listen RECV event ..\n");
            unsigned int len = 8191;
            if (listen_pipe.Recv(msg_buffer, len))
            {
                if (len){
                    SendMessage();
                    TRACE("pipe2Sock: recvd \"%s\"\n", msg_buffer);
                }
            }
            else
            {
                PLOG(PL_ERROR, "Pipe2Sock::OnListenEvent() listen_pipe.Recv() error\n");
            }            
            break;
        }
        case ProtoSocket::SEND:
            TRACE("pipe2Sock: listen SEND event ..\n");
            break;
        case ProtoSocket::ACCEPT:
            TRACE("pipe2Sock: listen ACCEPT event ..\n");
            if (!listen_pipe.Accept())
                PLOG(PL_ERROR, "Pipe2Sock::OnListenEvent() listen_pipe.Accept() error\n");
            break;
        case ProtoSocket::DISCONNECT:
            TRACE("pipe2Sock: listen DISCONNECT event ..\n");
            char pipeName[PATH_MAX];
            strncpy(pipeName, listen_pipe.GetName(), PATH_MAX);
            listen_pipe.Close();
            if (!listen_pipe.Listen(pipeName))
                PLOG(PL_ERROR, "pipe2Sock: error restarting listen pipe ...\n");
            break;
        default:
            TRACE("Pipe2Sock::OnListenEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end Pipe2Sock::OnListenEvent()

void Pipe2Sock::SendMessage()
{
    if(socket.IsOpen())
    {
        TRACE("Sending message %s to %s/%d\n",msg_buffer,dst_addr.GetHostString(),dst_addr.GetPort());
        unsigned int len = strlen(msg_buffer);
        socket.SendTo(msg_buffer,len,dst_addr);
        memset(msg_buffer,0,msg_len); //clear out the message
    }
}

bool Pipe2Sock::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        if (!strcmp("listen", argv[i]))
        {
            i++;
            strcpy(listen_pipe_name,argv[i]);
        }
        else if (!strcmp("send", argv[i]))
        {
            i++;
            const char* index = strchr(argv[i],'/');
            if(index!=NULL)
            {
                char charAddr[255];
                memset(charAddr,0,255);
                strncpy(charAddr,argv[i],index-argv[i]);
                if(!dst_addr.ResolveFromString(charAddr))
                {
                    PLOG(PL_ERROR, "Pipe2Sock::OnCommand() send error: setting dst address to %s\n",charAddr);
                    return false;
                }
                dst_port = atoi(index+1);
            }
            else
            {
                PLOG(PL_ERROR,"Pipe2Sock::OnCommand() missing \"/\" for send command\n");
                return false;
            }
        }
        else if (!strcmp("port", argv[i]))
        {
            i++;
            snd_port = atoi(argv[i]);
        }
        else
        {
            PLOG(PL_ERROR, "Pipe2Sock::OnCommand() unknown command error?\n");
            return false;
        }
        i++;
    }
    return true;
}  // end Pipe2Sock::ProcessCommands()
