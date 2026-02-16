#ifndef _WX_PROTO_APP
#define _WX_PROTO_APP


#ifndef _WIN32_WCE
#include "protokit.h"
#endif // !_WIN32_WCE

#include <wx/wx.h>

#ifdef _WIN32_WCE
#ifndef LPGUID
typedef GUID *LPGUID;
#endif // !LPGUID
#include "protokit.h"
#endif // _WIN32_WCE

/**
* @class wxProtoApp
*
* @brief Base class that can be used to create applications using Protolib components 
* and the wxWidgets GUI toolkit. 
*
*/
class wxProtoApp : public wxApp, public ProtoDispatcher::Controller
{
    public:
        virtual ~wxProtoApp();
        
        // Some helper methods
        void ActivateTimer(ProtoTimer& theTimer)
            {dispatcher.ActivateTimer(theTimer);}
        
        void DeactivateTimer(ProtoTimer& theTimer)
            {dispatcher.DeactivateTimer(theTimer);}
        
        ProtoSocket::Notifier& GetSocketNotifier() 
            {return static_cast<ProtoSocket::Notifier&>(dispatcher);}
        
        ProtoTimerMgr& GetTimerMgr()
            {return static_cast<ProtoTimerMgr&>(dispatcher);}
        
        static const wxEventType wxPROTO_DISPATCH_EVENT;
        
    protected:
        wxProtoApp(bool appDispatch = true);
        ProtoDispatcher dispatcher;
        
    private:
        int MainLoop();
        void OnDispatchEvent(wxCommandEvent& event);
        void OnIdleEvent(wxIdleEvent& event);   
        bool SignalDispatchReady();  
        static void DoGUIEvent(ProtoDispatcher::Descriptor descriptor, 
                               ProtoDispatcher::Event      theEvent, 
                               const void*                 userData);
        bool    app_dispatch;
        
        DECLARE_EVENT_TABLE()
};  // end class wxProtoApp

#endif // !_WX_PROTO_APP
