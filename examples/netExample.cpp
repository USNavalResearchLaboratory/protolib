
#include "protoApp.h"
#include "protoNet.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

/**
 * @class NetExample
 *
 * @brief An example using a ProtoNet class instance to 
  *       monitor network device status
 */
class NetExample : public ProtoApp
{
     public:
         NetExample();
        ~NetExample();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv) 
            {return true;}
        void OnShutdown();

    private:
        void MonitorEventHandler(ProtoChannel&               theChannel,
                                 ProtoChannel::Notification  theNotification);
            
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static const char* const CMD_LIST[];
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        ProtoNet::Monitor*  monitor;
        
}; // end class NetExample

const char* const NetExample::CMD_LIST[] =
{
    NULL
};

// This macro creates our ProtoApp derived application instance 
PROTO_INSTANTIATE_APP(NetExample) 
        
NetExample::NetExample()
 : monitor(NULL)
   
{       
   
}

NetExample::~NetExample()
{
}

void NetExample::Usage()
{
    fprintf(stderr, "netExample\n");
}  // end NetExample::Usage()



bool NetExample::OnStartup(int argc, const char*const* argv)
{

    // Create our cap_rcvr instance and initialize ...
    if (!(monitor = ProtoNet::Monitor::Create()))
    {
        PLOG(PL_ERROR, "detourExample::OnStartup() new ProtoNet::Monitor error: %s\n", GetErrorString());
        return false;
    }  
    
    monitor->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    monitor->SetListener(this, &NetExample::MonitorEventHandler);  
    
    if (!monitor->Open())
    {
        PLOG(PL_ERROR, "detourExample::OnStartup() ProtoNet::Monitor::Open() error\n");
    } 
    
    return true;
}  // end NetExample::OnStartup()

void NetExample::OnShutdown()
{

   if (NULL != monitor)
   {
       monitor->Close();
       delete monitor;
       monitor = NULL;
   }
   PLOG(PL_ERROR, "netExample: Done.\n"); 
   CloseDebugLog();
}  // end NetExample::OnShutdown()

void NetExample::MonitorEventHandler(ProtoChannel&               theChannel, 
                                     ProtoChannel::Notification  theNotification)
{
    if (ProtoChannel::NOTIFY_INPUT == theNotification)
    {
        // Read all available (until NULL_EVENT) events
        while (1)
        {
            ProtoNet::Monitor::Event theEvent;
            if (!monitor->GetNextEvent(theEvent))
            {
				PLOG(PL_ERROR, "NetExample::MonitorEventHandler() error: failure getting network events\n");
                 return;
            }       
            if (ProtoNet::Monitor::Event::NULL_EVENT == theEvent.GetType()) break;
			char ifName[256];
            ifName[255] = '\0';
            if (ProtoNet::Monitor::Event::UNKNOWN_EVENT != theEvent.GetType())
            {
                if (!ProtoNet::GetInterfaceName(theEvent.GetInterfaceIndex(), ifName, 255))
                {

					ifName[0] = '\0';
#ifndef WIN32
					PLOG(PL_ERROR, "NetExample::MonitorEventHandler(event:%d) error: unable to get name for iface index %d\n",
						theEvent.GetType(),theEvent.GetInterfaceIndex());
                    continue; // we want to report "down" interfaces on win32 that we won't have interface names for
#endif // WIN32
				}
            }               
            switch(theEvent.GetType())
            {
                case ProtoNet::Monitor::Event::IFACE_UP:
                    PLOG(PL_ALWAYS, "netExample: interface \"%s\" index %d is up ...\n", ifName,theEvent.GetInterfaceIndex());
                    break;
                case ProtoNet::Monitor::Event::IFACE_DOWN:
                    PLOG(PL_ALWAYS, "netExample: interface \"%s\" index %d is down ...\n", ifName, theEvent.GetInterfaceIndex());
                    break;
                case ProtoNet::Monitor::Event::IFACE_ADDR_NEW:
                    PLOG(PL_ALWAYS, "netExample: interface \"%s\" index %d new address %s ...\n", 
                            ifName, theEvent.GetInterfaceIndex(),theEvent.GetAddress().GetHostString());
                    break;
                case ProtoNet::Monitor::Event::IFACE_ADDR_DELETE:
                    PLOG(PL_ALWAYS, "netExample: interface \"%s\" index %d deleted address %s ...\n", 
                            ifName, theEvent.GetInterfaceIndex(), theEvent.GetAddress().GetHostString());
                    break;
                case ProtoNet::Monitor::Event::IFACE_STATE:
					PLOG(PL_ALWAYS, "netExample: interface \"%s\" index %d state change ", ifName, theEvent.GetInterfaceIndex());
					if (theEvent.GetAddress().GetType() != ProtoAddress::INVALID)
						PLOG(PL_ALWAYS, "address %s ...\n", theEvent.GetAddress().GetHostString());
					else 
						PLOG(PL_ALWAYS,"... \n");
 
                    break;
                default:
                    break;
            }
        }
    }

}  // end NetExample::MonitorEventHandler()
