 
#include "protokit.h"
#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

// 1) Uncomment this for example using Protolib async i/o
#define ASYNC_IO_EXAMPLE

// 2) Uncomment _one_ of these ProtoPipe "types" for demonstration
#define PIPE_TYPE ProtoPipe::MESSAGE
//#define PIPE_TYPE ProtoPipe::STREAM

#ifdef ASYNC_IO_EXAMPLE
/**
 * @class PipeExample
 *
 * @brief This example program illustrates/tests the use of the ProtoPipe class
 * for local domain interprocess communications
 */
class PipeExample : public ProtoApp
{
    public:
        PipeExample();
        ~PipeExample();

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
        
        bool OnSendTimeout(ProtoTimer& theTimer);
        void OnServerEvent(ProtoSocket&       theSocket, 
                           ProtoSocket::Event theEvent);
        void OnClientEvent(ProtoSocket&       theSocket, 
                           ProtoSocket::Event theEvent);
        
        ProtoTimer   send_timer;
        ProtoPipe    server_pipe;
        ProtoPipe    client_pipe;
        char*        msg_buffer;
        unsigned int msg_len;
        unsigned int msg_index;
        int          msg_repeat;
        int          msg_repeat_count;
            
            
};  // end class PipeExample

// Instantiate our application instance 
PROTO_INSTANTIATE_APP(PipeExample) 
        
PipeExample::PipeExample()
 : server_pipe(PIPE_TYPE), client_pipe(PIPE_TYPE),
   msg_buffer(NULL), msg_len(0), msg_index(0),
   msg_repeat(0), msg_repeat_count(0)
{
    send_timer.SetListener(this, &PipeExample::OnSendTimeout);
    send_timer.SetInterval(1.0);
    send_timer.SetRepeat(-1);
    server_pipe.SetNotifier(&GetSocketNotifier());
    server_pipe.SetListener(this, &PipeExample::OnServerEvent);
    client_pipe.SetNotifier(&GetSocketNotifier());
    client_pipe.SetListener(this, &PipeExample::OnClientEvent);    
}

PipeExample::~PipeExample()
{
    if (NULL != msg_buffer)
    {
        delete[] msg_buffer;
        msg_buffer = NULL;   
    }
}

const char* const PipeExample::CMD_LIST[] =
{
    "+listen",     // Listen for TCP connections on <port>
    "+connect",    // TCP connect and send to destination host/port
    "+repeat",     // repeat message multiple times
    "+send",       // Send UDP packets to destination host/port
    NULL
};
    
void PipeExample::Usage()
{
    fprintf(stderr, "pipeExample [listen <serverName>][connect <serverName>]\n"
                    "            [send <message>][repeat <repeatCount>]\n");
}  // end PipeExample::Usage()
  
bool PipeExample::OnStartup(int argc, const char*const* argv)
{
    if (argc < 2)
	{
		Usage();
		return false;
	}
	if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "PipeExample::OnStartup() error processing command line\n");
		Usage();
        return false;
    }
    return true;
}  // end PipeExample::OnStartup()

void PipeExample::OnShutdown()
{
    if (send_timer.IsActive()) send_timer.Deactivate();
    if (server_pipe.IsOpen()) server_pipe.Close();
    if (client_pipe.IsOpen()) client_pipe.Close();
    PLOG(PL_ERROR, "pipeExample: Done.\n");
}  // end PipeExample::OnShutdown()

bool PipeExample::OnCommand(const char* cmd, const char* val)
{
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "pipeExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("listen", cmd, len))
    {
        if (server_pipe.IsOpen()) server_pipe.Close();
        if (!server_pipe.Listen(val))
        {
            PLOG(PL_ERROR, "PipeExample::OnCommand() server_pipe.Listen() error\n");
            return false;   
        }
        TRACE("pipeExample: server \"%s\" listening ...\n", val);
    }
    else if (!strncmp("connect", cmd, len))
    {
        if (client_pipe.IsOpen()) client_pipe.Close();
        if (!client_pipe.Connect(val))
        {
            PLOG(PL_ERROR, "PipeExample::OnCommand() client_pipe.Connect() error: %s\n",
                    GetErrorString());
            return false;   
        }
    }
    else if (!strncmp("repeat", cmd, len))
    {
        msg_repeat = atoi(val);
    }
    else if (!strncmp("send", cmd, len))
    {
        if (msg_buffer) delete[] msg_buffer;
        msg_len = strlen(val) + 1;
        if (!(msg_buffer = new char[msg_len]))
        {
            PLOG(PL_ERROR, "pipeExample: new msg_buffer error: %s\n", GetErrorString());
            msg_len = 0;
            return false;   
        }
        memcpy(msg_buffer, val, msg_len);
        msg_index = 0;
        msg_repeat_count = msg_repeat;
        if (ProtoPipe::MESSAGE == client_pipe.GetType())
        {
            if (send_timer.IsActive()) send_timer.Deactivate();
            if (msg_repeat_count) ActivateTimer(send_timer);
            OnSendTimeout(send_timer);  // go ahead and send msg immediately
        }
        else
        {
            if (!client_pipe.StartOutputNotification())
            {
                PLOG(PL_ERROR, "PipeExample::OnCommand() StartOutputNotification() error\n");
                return false;
            }
        }
    }
    else
    {
        PLOG(PL_ERROR, "PipeExample::OnCommand() unknown command error?\n");
        return false;
    }
    return true;
}  // end PipeExample::OnCommand()
    
bool PipeExample::OnSendTimeout(ProtoTimer& /*theTimer*/)
{
    if (msg_buffer && client_pipe.IsOpen())
    {
        unsigned int len = msg_len - msg_index;
        if (client_pipe.Send(msg_buffer+msg_index, len))
        {
            msg_index += len;
            if (msg_index >= msg_len) 
            {
                msg_index = 0;
                if (0 == msg_repeat_count)
                {
                    if (send_timer.IsActive()) send_timer.Deactivate();
                    client_pipe.Close();
                    return false;   
                }  
                if (msg_repeat_count > 0) msg_repeat_count--;
            }
            TRACE("pipeExample: client sent %d bytes ...\n", len);
        }
        else
        {
            PLOG(PL_ERROR, "PipeExample::OnSendTimeout() client_pipe.Send() error\n");  
            client_pipe.Close();
        }     
    }
    return true;
}  // end PipeExample::OnSendTimeout()

void PipeExample::OnServerEvent(ProtoSocket&       /*theSocket*/, 
                                ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("pipeExample: server RECV event ..\n");
            char buffer[8192];
            unsigned int len = 8191;
            if (server_pipe.Recv(buffer, len))
            {
                if (len)
                    PLOG(PL_ERROR, "pipeExample: recvd \"%s\"\n", buffer);
            }
            else
            {
                PLOG(PL_ERROR, "PipeExample::OnServerEvent() server_pipe.Recv() error\n");
            }            
            break;
        }
        case ProtoSocket::SEND:
            TRACE("pipeExample: server SEND event ..\n");
            break;
        case ProtoSocket::ACCEPT:
            TRACE("pipeExample: server ACCEPT event ..\n");
            if (!server_pipe.Accept())
                PLOG(PL_ERROR, "PipeExample::OnServerEvent() server_pipe.Accept() error\n");
            break;
        case ProtoSocket::DISCONNECT:
            TRACE("pipeExample: server DISCONNECT event ..\n");
            char pipeName[PATH_MAX];
            strncpy(pipeName, server_pipe.GetName(), PATH_MAX);
            server_pipe.Close();
            if (!server_pipe.Listen(pipeName))
                PLOG(PL_ERROR, "pipeExample: error restarting server pipe ...\n");
            break;
        default:
            TRACE("PipeExample::OnServerEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end PipeExample::OnServerEvent()


void PipeExample::OnClientEvent(ProtoSocket&       /*theSocket*/, 
                                ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::CONNECT:
             PLOG(PL_ERROR, "pipeExample: clientconnected to server.\n");
             break;
        case ProtoSocket::RECV:
        {
            TRACE("pipeExample: client RECV event ...\n");
            break;
        }
        case ProtoSocket::SEND:
            TRACE("pipeExample: client SEND event ...\n");
            OnSendTimeout(send_timer);
            break;
        case ProtoSocket::DISCONNECT:
            TRACE("pipeExample: client DISCONNECT event ...\n");
            client_pipe.Close();
            break;
        default:
            TRACE("pipeExample::OnClientEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end PipeExample::OnClientEvent()


PipeExample::CmdType PipeExample::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end PipeExample::GetCmdType()

bool PipeExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class PipeExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
#ifndef SIMULATE
                PLOG(PL_ERROR, "pipeExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
#endif // SIMULATE
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "pipeExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
					return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "pipeExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end PipeExample::ProcessCommands()

#else

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        // Connect to server and send command line args
        ProtoPipe client(PIPE_TYPE);
        if (!client.Connect("protoDaemon"))
        {
            PLOG(PL_ERROR, "pipeExample: error connecting to server\n");
            return -1;
        }
        for (int i = 1; i < argc; i++)
        {
            // Send args including '\0' termination char
            unsigned int len = strlen(argv[i]) + 1;
            if (!client.Send(argv[i], len))
            {
                PLOG(PL_ERROR, "pipeExample: error sending to server\n");
                return -1; 
            } 
        }
        client.Close();
    }
    else
    {
        // No args, so just open up a "protoDaemon" server
        while(1)
        {
            ProtoPipe server(PIPE_TYPE);
            ProtoPipe acceptPipe(PIPE_TYPE);
            ProtoPipe& recvPipe = (ProtoPipe::STREAM == server.GetType()) ? acceptPipe : server;
            PLOG(PL_ERROR, "pipeExample: server listening ...\n");
            if (!server.Listen("protoDaemon"))
			{
                PLOG(PL_ERROR, "pipeExample: error opening server socket\n");
                return -1;        
            }  
            if (ProtoPipe::STREAM == server.GetType())
            {  
                // "STREAM" ProtoPipes are connection-oriented
                // Note that the argument to Accept() is optional if the
                // application only wants to have one connection at a time
                if (!server.Accept(&acceptPipe))
                {
                    PLOG(PL_ERROR, "pipeExample: error accepting connection\n");
                    server.Close();
                    return -1;       
                }
            }
            char buffer[1024];
            unsigned int len = 1024;
            while (recvPipe.Recv(buffer, len))
            {
                if (0 != len)
                    PLOG(PL_ERROR, "pipeExample: server recv'd \"%s\" ...\n", buffer);
                else
                    break;
                len = 1024;   
            }
            server.Close();
            recvPipe.Close();
        }
    }  
    return 0;
}

#endif // if/else ASYNC_IO_EXAMPLE
