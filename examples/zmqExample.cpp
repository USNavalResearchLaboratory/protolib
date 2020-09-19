 
#include "protoZMQ.h"
#include "protoTimer.h"
#include "protoApp.h"

#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

/*
 * @class ZmqExample
 *
 * @brief This example program illustrates/tests the use of the ProtoPipe class
 * for local domain interprocess communications
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
        void OnPubEvent(ProtoSocket&       theSocket, 
                       ProtoSocket::Event theEvent);
        void OnSubEvent(ProtoSocket&       theSocket, 
                       ProtoSocket::Event theEvent);
        
        ProtoTimer          tx_timer;
        ProtoZmq::Socket    sub_socket;
        ProtoZmq::Socket    pub_socket;
        
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
    sub_socket.SetNotifier(&GetSocketNotifier());
    sub_socket.SetListener(this, &ZmqExample::OnSubEvent);
    pub_socket.SetNotifier(&GetSocketNotifier());
    pub_socket.SetListener(this, &ZmqExample::OnPubEvent);
}

ZmqExample::~ZmqExample()
{
}

const char* const ZmqExample::CMD_LIST[] =
{
    "+publish",     // Publish 'send' messages with given ZMQ transport spec
    "+subscribe",  // Subscribe with given ZMQ transport spec
    "+repeat",     // repeat message multiple times
    "+send",       // Send UDP packets to destination host/port
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
    if (sub_socket.IsOpen()) sub_socket.Close();
    if (pub_socket.IsOpen()) pub_socket.Close();
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
        if (pub_socket.IsOpen()) pub_socket.Close();
        if (!pub_socket.Open(ZMQ_PUB))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() pub_socket.Open() error\n");
            return false;   
        }        
        if (!pub_socket.Bind(val))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() pub_socket.Bind() error\n");
            return false;   
        } 
        TRACE("zmqExample: publishing to %s ...\n", val);
    }
    else if (!strncmp("subscribe", cmd, len))
    {
        if (sub_socket.IsOpen()) sub_socket.Close();
        if (!sub_socket.Open(ZMQ_SUB))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() sub_socket.Open() error\n");
            return false;   
        }        
        if (!sub_socket.Connect(val))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() sub_socket.Connect() error\n");
            return false;   
        } 
        // subscribe to all messages
        if (0 != zmq_setsockopt(sub_socket.GetSocket(), ZMQ_SUBSCRIBE, NULL, 0))
        {
            PLOG(PL_ERROR, "ZmqExample::OnCommand() zmq_setsockopt(ZMQ_SUBSCRIBE) error\n");
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
        msg_len = strlen(val) + 1;
        if (!(msg_buffer = new char[msg_len]))
        {
            PLOG(PL_ERROR, "zmqExample: new msg_buffer error: %s\n", GetErrorString());
            msg_len = 0;
            return false;   
        }
        memcpy(msg_buffer, val, msg_len);
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
    TRACE("ZmqExample::OnTxTimeout() ...\n");
    return true;
}  // end ZmqExample::OnSendTimeout()

void ZmqExample::OnSubEvent(ProtoSocket&       theSocket, 
                            ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("zmqExample: sub_socket EVENT..\n");
            int events;
            size_t len = sizeof(int);
            do 
            {
                if (0 != zmq_getsockopt(sub_socket.GetSocket(), ZMQ_EVENTS, &events, &len))
                {
                    PLOG(PL_ERROR, "ZmqExample::OnSubEvent() zmq_getsockopt() error: %s\n", GetErrorString());
                    break;
                }
                if (0 != (ZMQ_POLLIN & events))
                {
                    TRACE("   ZMQ_POLLIN: ");


                    char buffer[2048];
                    buffer[2047] = '\0';
                    unsigned int numBytes = 2047;
                    if (sub_socket.Recv(buffer, numBytes))
                    {
                        buffer[numBytes] = '\0';
                        TRACE("received %u bytes: %s\n", numBytes, buffer);
                    }
                    else
                    {
                        TRACE("receive error?\n");
                    }    
                }
                if (0 != (ZMQ_POLLOUT & events))
                    TRACE("   ZMQ_POLLOUT\n");
            } while (0 != events);
            break;
        }
        default:
            TRACE("ZmqExample::OnSubEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end ZmqExample::OnSubEvent()

void ZmqExample::OnPubEvent(ProtoSocket&       theSocket, 
                            ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("zmqExample: pub_socket EVENT..\n");
            int events;
            size_t len = sizeof(int);
            if (0 != zmq_getsockopt(pub_socket.GetSocket(), ZMQ_EVENTS, &events, &len))
            {
                PLOG(PL_ERROR, "ZmqExample::OnPubEvent() zmq_getsockopt() error: %s\n", GetErrorString());
                break;
            }
            if (0 != (ZMQ_POLLIN & events))
                TRACE("   ZMQ_POLLIN\n");
            
            if (0 != (ZMQ_POLLOUT & events))
                TRACE("   ZMQ_POLLOUT\n");
            break;
        }
        default:
            TRACE("ZmqExample::OnPubEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end ZmqExample::OnPubEvent()


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
            {
                PLOG(PL_ERROR, "zmqExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            }
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

