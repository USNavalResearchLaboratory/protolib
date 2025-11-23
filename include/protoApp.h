#ifndef _PROTO_APP
#define _PROTO_APP

#include "protoDispatcher.h"
/**
 * @class ProtoApp
 *
 * @brief Base class for implementing protolib-based 
 * command-line applications.
 *
 * Note that "ProtoApp" and "ProtoSimAgent" are
 * designed such that subclasses can be derived
 * from either to reuse the same code in either a
 * real-world applications or as an "agent"
 * (entity) within a network simulation
 * environment (e.g. ns-2, OPNET).  A "background"
 * command is included for Win32 to launch the
 * app without a terminal window.
 */

class ProtoApp
{
    public:
        virtual ~ProtoApp();   
     
        virtual bool OnStartup(int argc, const char*const* argv) = 0;
        virtual bool ProcessCommands(int argc, const char*const* argv) = 0;
        virtual void OnShutdown() = 0;    
        
	    bool ProcessCommandString(const char* cmd);

        int Run() 
            {return dispatcher.Run();}
        bool IsRunning() const
            {return dispatcher.IsRunning();}
        void Stop(int exitCode = 0) 
            {dispatcher.Stop(exitCode);}
        
        // Some helper methods
        void ActivateTimer(ProtoTimer& theTimer)
            {dispatcher.ActivateTimer(theTimer);}
        
        void DeactivateTimer(ProtoTimer& theTimer)
            {dispatcher.DeactivateTimer(theTimer);}
        
        ProtoSocket::Notifier& GetSocketNotifier() 
            {return static_cast<ProtoSocket::Notifier&>(dispatcher);}
        
        ProtoChannel::Notifier& GetChannelNotifier()
            {return static_cast<ProtoChannel::Notifier&>(dispatcher);}
        
        ProtoTimerMgr& GetTimerMgr()
            {return static_cast<ProtoTimerMgr&>(dispatcher);}

	    ProtoDispatcher& GetDispatcher()
	        {return dispatcher;}
        static ProtoApp* GetApp()
            {return the_app;}
        
        static void SignalHandler(int sigNum);

#ifdef WIN32
        bool Win32Init() {return dispatcher.Win32Init();}
#endif // WIN32
                
    protected:
        ProtoApp();
        
        ProtoDispatcher dispatcher;
    
    private:
        static ProtoApp* the_app;
            
};  // end class ProtoApp


/*
 * The functions and macros below are used to instantiate
 * a derive ProtoApp when needed.
 */
int ProtoMain(int argc, char* argv[], bool pauseForUser = false);

#ifdef WIN32

int PASCAL Win32ProtoMain(HINSTANCE instance, 
                          HINSTANCE prevInst, 
                          LPSTR     cmdline, 
                          int       cmdshow);
int Win32ProtoMainConsole(int argc, char* argv[]);

#ifdef _CONSOLE
#define PROTO_MAIN() \
int main(int argc, char* argv[]) \
{return Win32ProtoMainConsole(argc, argv);}
#else
#ifdef _WIN32_WCE
#define PROTO_MAIN() \
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInst, LPWSTR cmdline, int cmdshow) \
{return Win32ProtoMain(instance, prevInst, (char*)cmdline, cmdshow);}
#else
#define PROTO_MAIN() \
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInst, LPSTR cmdline, int cmdshow) \
{return Win32ProtoMain(instance, prevInst, cmdline, cmdshow);}
#endif // if/else _WIN32_WCE
#endif  // if/else _CONSOLE

#else

#define PROTO_MAIN() \
int main(int argc, char* argv[]) \
{return ProtoMain(argc, argv);}

#endif // if/else WIN32/UNIX

/**
 * Use this macro to instantiate your derived ProtoApp instance
 */
#define PROTO_INSTANTIATE_APP(X) X protoApp; PROTO_MAIN()

#endif  // !_PROTO_APP
