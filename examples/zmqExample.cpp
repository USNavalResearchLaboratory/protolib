 
#include "protoZMQ.h"
#include "protoTimer.h"
#include "protoApp.h"

#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

/*
 * @class ZmqExample
 *
 * @brief This example program illustrates/tests the use of the ProtoZmq::Socket class
 */
class ZmqExample : public ProtoApp
{
    public:
        ZmqExample();
        ~ZmqExample();

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
        
        bool OnTxTimeout(ProtoTimer& theTimer);
        void OnSocketEvent(ProtoEvent& theEvent);
        
        ProtoTimer          tx_timer;
        ProtoZmq::Socket    zmq_socket;
        //int                 socket_type;  // ZMQ_PUB, ZMQ_SUB, ZMQ_RADIO, ZMQ_DISH, etc
        
        char*        msg_buffer;
        unsigned int msg_len;
        unsigned int msg_index;
        int          msg_repeat;
        int          msg_repeat_count;
                      
};  // end class ZmqExample

// Instantiate our application instance 
PROTO_INSTANTIATE_APP(ZmqExample) 
        
ZmqExample::ZmqExample()
  : msg_buffer(NULL), msg_len(0), msg_index(0),
    msg_repeat(0), msg_repeat_count(0)
{
    tx_timer.SetListener(this, &ZmqExample::OnTxTimeout);
    tx_timer.SetInterval(1.0);
    tx_timer.SetRepeat(-1);
    zmq_socket.SetNotifier(&dispatcher);
    zmq_socket.SetListener(this, &ZmqExample::OnSocketEvent);
}

ZmqExample::~ZmqExample()
{
}

const char* const ZmqExample::CMD_LIST[] =
{
    "+publish",     // Publish 'send' messages using ZMQ_PUB socket
    "+subscribe",   // Subscribe with ZMQ_SUB socket
    "+radio",       // Publish 'send' messages with given ZMQ_RADIO socket
    "+dish",        // Receive with given ZMQ_DISH socket
    "+repeat",     // repeat message multiple times
    "+send",       // Send given string as message
    NULL
};
    
void ZmqExample::Usage()
{
    fprintf(stderr, "zmqExample [publish <endpoint>][subscribe <endpoint>]\n"
                    "            [send <message>][repeat <repeatCount>]\n");
}  // end ZmqExample::Usage()
  
bool ZmqExample::OnStartup(int argc, const char*const* argv)
{
    if (argc < 2)
	{
		Usage();
		return false;
	}
	if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "ZmqExample::OnStartup() error processing command line\n");
		Usage();
        return false;
    }
    return true;
}  // end ZmqExample::OnStartup()

void ZmqExample::OnShutdown()
{
    if (tx_timer.IsActive()) tx_timer.Deactivate();
    if (zmq_socket.IsOpen()) zmq_socket.Close();
    if (zmq_socket.IsOpen()) zmq_socket.Close();
    PLOG(PL_ERROR, "zmqExample: Done.\n");
}  // end ZmqExample::OnShutdown()

bool ZmqExample::OnCommand(const char* cmd, const char* val)
{
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "zmqExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("publish", cmd, len))
    {
        if (zmq_socket.IsOpen()) zmq_socket.Close();
        if (!zmq_socket.Open(ZMQ_PUB))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Open() error\n");
            return false;   
        }        
        if (!zmq_socket.Bind(val))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Bind() error\n");
            return false;   
        } 
        TRACE("zmqExample: publishing to %s ...\n", val);
    }
    else if (!strncmp("subscribe", cmd, len))
    {
        if (zmq_socket.IsOpen()) zmq_socket.Close();
        if (!zmq_socket.Open(ZMQ_SUB))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Open() error\n");
            return false;   
        }        
        if (!zmq_socket.Connect(val))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Connect() error\n");
            return false;   
        } 
        // subscribe to all messages
        if (!zmq_socket.Subscribe(NULL))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_setsockopt(ZMQ_SUBSCRIBE) error\n");
            //return false;   
        }
        TRACE("zmqExample: subscribed to %s ...\n", val);
    }
    else if (!strncmp("radio", cmd, len))
    {
        if (zmq_socket.IsOpen()) zmq_socket.Close();
        if (!zmq_socket.Open(ZMQ_RADIO))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Open() error\n");
            return false;   
        }        
        if (!zmq_socket.Connect(val))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Bind() error\n");
            return false;   
        } 
        TRACE("zmqExample: publishing to %s ...\n", val);
    }
    else if (!strncmp("dish", cmd, len))
    {
        if (zmq_socket.IsOpen()) zmq_socket.Close();
        if (!zmq_socket.Open(ZMQ_DISH))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Open() error\n");
            return false;   
        }        
        if (!zmq_socket.Bind(val))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_socket.Connect() error\n");
            return false;   
        } 
        // subscribe to all messages
        if (!zmq_socket.Join(""))  // "" joins "group-less" group
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_join() error: %s\n", zmq_strerror(zmq_errno()));
            return false;  
        }
        TRACE("zmqExample: subscribed to %s ...\n", val);
    }
    else if (!strncmp("repeat", cmd, len))
    {
        msg_repeat = atoi(val);
    }
    else if (!strncmp("send", cmd, len))
    {
        if (msg_buffer) delete[] msg_buffer;
        msg_len = strlen(val);
        if (!(msg_buffer = new char[msg_len + 8]))
        {
            PLOG(PL_ERROR, "zmqExample: new msg_buffer error: %s\n", GetErrorString());
            msg_len = 0;
            return false;   
        }
        strcpy(msg_buffer, val);
        msg_index = 0;
        msg_repeat_count = msg_repeat;
        if (tx_timer.IsActive()) tx_timer.Deactivate();
        if (msg_repeat_count) ActivateTimer(tx_timer);
        OnTxTimeout(tx_timer);  // go ahead and send msg immediately
    }
    else
    {
        PLOG(PL_ERROR, "ZmqExample::OnCommand() unknown command error?\n");
        return false;
    }
    return true;
}  // end ZmqExample::OnCommand()

bool ZmqExample::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    TRACE("zmqExample::OnTxTimeout() ...\n");
    // Add an index to end of msg_buffer to make it easier to track
    sprintf(msg_buffer + msg_len, " %u", (unsigned int)msg_index++);
    unsigned int numBytes = strlen(msg_buffer) + 1;
    TRACE("sending %u bytes: %s ...\n", numBytes, msg_buffer);
    zmq_socket.Send(msg_buffer, numBytes);
    msg_buffer[msg_len] = '\0';
    msg_index %= 1024;  // limit the index so the string doesn't ever get too long
    return true;
}  // end ZmqExample::OnTxTimeout()

void ZmqExample::OnSocketEvent(ProtoEvent& /*theEvent*/)
{
    //TRACE("zmqExample: zmq_socket EVENT..\n");
    int events;
    size_t len = sizeof(int);
    // Loop here to receive all available messages
    unsigned int numBytes;
    do 
    {
        // NOTE: instead of using zmq_getsockopt(), one can safely just call
        // zmq_socket.Recv() in a loop until it returns 'false' or 0 == numBytes
        // since Recv() is non-blocking.
        if (0 != zmq_getsockopt(zmq_socket.GetSocket(), ZMQ_EVENTS, &events, &len))
        {
            PLOG(PL_ERROR, "ZmqExample::OnSubEvent() zmq_getsockopt() error: %s\n", GetErrorString());
            break;
        }
        if (0 != (ZMQ_POLLIN & events))
        {
            char buffer[2048];
            buffer[2047] = '\0';
            numBytes = 2047;
            if (zmq_socket.Recv(buffer, numBytes))
            {
                buffer[numBytes] = '\0';
                TRACE("received %u bytes: %s\n", numBytes, buffer);
            }
            else
            {
                TRACE("receive error?\n");
            }
        }
    } while (0 != (ZMQ_POLLIN & events));        
    if ((0 != (ZMQ_POLLOUT & zmq_socket.GetPollFlags())) &&
        (0 != (ZMQ_POLLOUT & events)))
    {
        TRACE("   ZMQ_POLLOUT\n");  // only will occur if ProtoZmq::Socket::StartOutputNotification() was called
    }
    if (0 != (ZMQ_POLLERR & events))
    {
        TRACE("   ZMQ_POLLERR\n");
    }
}  // end ZmqExample::OnSocketEvent()


ZmqExample::CmdType ZmqExample::GetCmdType(const char* cmd)
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
}  // end ZmqExample::GetCmdType()

bool ZmqExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class ZmqExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
                PLOG(PL_ERROR, "zmqExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "zmqExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
					return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "zmqExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end ZmqExample::ProcessCommands()

