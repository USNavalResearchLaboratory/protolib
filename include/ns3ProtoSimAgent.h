#include "protoSimAgent.h"

#include "ns3/application.h"


/** 
* @class Ns3ProtoSimAgent
*
* @brief Ns3 ProtoSimAgent implementation.
*/
class Ns3ProtoSimAgent : public ProtoSimAgent : public Application
{
    public:
        virtual ~Ns3ProtoSimAgent();
              
        virtual bool OnStartup(int argc, const char*const* argv) = 0;
        virtual bool ProcessCommands(int argc, const char*const* argv) = 0;
        virtual void OnShutdown() = 0;    
        
        // Some helper methods
        void ActivateTimer(ProtoTimer& theTimer)
            {ProtoSimAgent::ActivateTimer(theTimer);}
        
        void DeactivateTimer(ProtoTimer& theTimer)
            {ProtoSimAgent::DeactivateTimer(theTimer);}
        
        ProtoSocket::Notifier& GetSocketNotifier() 
            {return ProtoSimAgent::GetSocketNotifier();}
        
        ProtoTimerMgr& GetTimerMgr()
            {return ProtoSimAgent::GetTimerMgr();}
        		
        // Override ProtoTimerMgr::GetSystemTime()
        virtual void GetSystemTime(struct timeval& currentTime)
        {
            double now = ns3::Simulator::Now().Seconds();
            currentTime.tv_sec = (unsigned long)(now);
            currentTime.tv_usec = (unsigned long)((now - currentTime.tv_sec) * 1.0e06);
        }
        
    private:
            
};  // end class Ns3ProtoSimAgent
