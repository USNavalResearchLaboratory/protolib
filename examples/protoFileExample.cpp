#ifdef SIMULATE
#include "nsProtoSimAgent.h"
#else
#include "protoApp.h"
#endif  // if/else SIMULATE

#include "protokit.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

class ProtoFileExample :
#ifdef SIMULATE
                     public NsProtoSimAgent
#else
                     public ProtoApp
#endif  // if/else SIMULATE
                     
{
public:
  ProtoFileExample();
  ~ProtoFileExample();
  
  /**
   * Override from ProtoApp or NsProtoSimAgent base
   */
  bool OnStartup(int argc, const char*const* argv);
  /**
   * Override from ProtoApp or NsProtoSimAgent base
   */
  bool ProcessCommands(int argc, const char*const* argv);
  /**
   * Override from ProtoApp or NsProtoSimAgent base
   */
  void OnShutdown();
  /**
   * Override from ProtoApp or NsProtoSimAgent base
   */
  virtual bool HandleMessage(unsigned int len, const char* txBuffer,const ProtoAddress& srcAddr) {return true;}

private:
  enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
  static CmdType GetCmdType(const char* string);
  bool OnCommand(const char* cmd, const char* val);        
  void Usage();
  
  bool OnTimeout(ProtoTimer& theTimer);//do nothing utility timer
  void OnFileEvent(ProtoChannel&       theChannel, 
                   ProtoChannel::Notification notifyType);
  static const char* const CMD_LIST[];
  static void SignalHandler(int sigNum);
  
  // ProtoTimer/ UDP socket demo members
  ProtoTimer          example_timer;
  
  ProtoFile           example_file;
  
  ProtoSocket::List   connection_list;
}; // end class ProtoFileExample


// (TBD) Note this #if/else code could be replaced with something like
// a PROTO_INSTANTIATE(ProtoFileExample) macro defined differently
// in "protoApp.h" and "nsProtoSimAgent.h"
#ifdef SIMULATE
#ifdef NS2
static class NsProtoFileExampleClass : public TclClass
{
	public:
		NsProtoFileExampleClass() : TclClass("Agent/ProtoFileExample") {}
	 	TclObject *create(int argc, const char*const* argv) 
			{return (new ProtoFileExample());}
} class_proto_example;	
#endif // NS2


#else

// Our application instance 
PROTO_INSTANTIATE_APP(ProtoFileExample) 

#endif  // SIMULATE

ProtoFileExample::ProtoFileExample()
{    
    example_timer.SetListener(this, &ProtoFileExample::OnTimeout);
    //these don't currently work as linux doesn't support async signaling on files (it always sends a ready signal)
    //example_file.SetNotifier(&GetChannelNotifier());
    //example_file.SetListener(this, &ProtoFileExample::OnFileEvent);
//	SetDebugLevel(PL_MAX);
}

ProtoFileExample::~ProtoFileExample()
{
    
}

void ProtoFileExample::Usage()
{
    fprintf(stderr, "protoExample [listen <file>]\n");
}  // end ProtoFileExample::Usage()


const char* const ProtoFileExample::CMD_LIST[] =
{
    "+listen",       // Open and listen to a file.
    NULL
};

ProtoFileExample::CmdType ProtoFileExample::GetCmdType(const char* cmd)
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
}  // end ProtoFileExample::GetCmdType()

bool ProtoFileExample::OnStartup(int argc, const char*const* argv)
{
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "protoFileExample: Error! bad command line\n");
        return false;
    }  
    
    return true;
}  // end ProtoFileExample::OnStartup()

void ProtoFileExample::OnShutdown()
{
    if (example_timer.IsActive()) example_timer.Deactivate();
    if (example_file.IsOpen()) example_file.Close();
    connection_list.Destroy();   
    CloseDebugLog();
}  // end ProtoFileExample::OnShutdown()

bool ProtoFileExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    if( argc <= 1 )
    {
        PLOG(PL_ERROR,"ProtoFileExample::ProcessCommands() No args given\n");
        return false;
    }

    int i = 1;
    while ( i < argc)
    {
        // Is it a class ProtoFileExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
#ifndef SIMULATE
                PLOG(PL_ERROR, "ProtoFileExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
#endif // SIMULATE
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "ProtoFileExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "ProtoFileExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end ProtoFileExample::ProcessCommands()

bool ProtoFileExample::OnCommand(const char* cmd, const char* val)
{

    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "ProtoFileExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("listen", cmd, len))
    {
        TRACE("opening file ...\n");
        if (!example_file.Open(val))
        {
            PLOG(PL_ERROR, "ProtoFileExample::ProcessCommand(listen) error opening %s\n",val);
            return false;    
        }    
        TRACE("file opened ...\n");
        example_timer.SetInterval(1.0);
        example_timer.SetRepeat(-1);
        TRACE("calling OnTimeout() ...\n");
        OnTimeout(example_timer);
        if (!example_timer.IsActive()) ActivateTimer(example_timer);
    }
    else if (!strncmp("background", cmd, len))
    {
        // do nothing (this command was scanned immediately at startup)
    }
    return true;
}  // end ProtoFileExample::OnCommand()

bool ProtoFileExample::OnTimeout(ProtoTimer& /*theTimer*/)
{
    //TRACE("ProtoFileExample::OnTimeout() ...\n");
    char buffer[1024];
    unsigned int len = 1024;
    while(example_file.Readline(buffer,len))
    {
        TRACE("Got line\"%s\" with length %d\n",buffer,len);
        len = 1024;
    } 
    if(len!=0)
    {
       TRACE("Got partial line!\n%s",buffer);
    }
    return true;
}  // end ProtoFileExample::OnTxTimeout()

void ProtoFileExample::OnFileEvent(ProtoChannel&       theChannel, 
                                   ProtoChannel::Notification notifyType)
{
//    TRACE("ProtoFileExample::OnFileEvent with notifyType value of %d\n",notifyType);
    char buffer[1024];
    unsigned int len = 1024;
    while(example_file.Readline(buffer,len))
    {
        TRACE("Got line\"%s\" with length %d\n",buffer,len);
        len = 1024;
    } 
    if(len!=0)
    {
       // TRACE("Got partial line!\n%s",buffer);
    }
}  // end ProtoFileExample::OnUdpSocketEvent()
