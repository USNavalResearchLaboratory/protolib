#include "wxProtoApp.h"

#define WXPROTO_USE_THREADING 1
#ifndef WXPROTO_USE_THREADING
#include "wx/apptrait.h"
#include "wx/cmdline.h"
#include "wx/evtloop.h"
#include "wx/msgout.h"
#include "wx/thread.h"
#include "wx/utils.h"
#include "wx/ptr_scpd.h"
#endif  // !WXPROTO_USE_THREADING

/*BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_EVENT_TYPE(wxPROTO_DISPATCH_EVENT, -1)
END_DECLARE_EVENT_TYPES()

DEFINE_EVENT_TYPE(wxPROTO_DISPATCH_EVENT)*/

const wxEventType wxProtoApp::wxPROTO_DISPATCH_EVENT = wxNewEventType();

// it may also be convenient to define an event table macro for this event type
#define EVT_PROTO_DISPATCH_COMMAND(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxProtoApp::wxPROTO_DISPATCH_EVENT, id, -1, \
        (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&fn, \
        (wxObject *) NULL \
    ),

DECLARE_APP(wxProtoApp)

BEGIN_EVENT_TABLE(wxProtoApp, wxApp)
    EVT_PROTO_DISPATCH_COMMAND(-1, wxProtoApp::OnDispatchEvent)
#ifndef WXPROTO_USE_THREADING
    EVT_IDLE(wxProtoApp::OnIdleEvent)
#endif // WXPROTO_USE_THREADING
END_EVENT_TABLE()


wxProtoApp::wxProtoApp(bool appDispatch)
 : ProtoDispatcher::Controller(dispatcher), app_dispatch(appDispatch)
{
   
}

wxProtoApp::~wxProtoApp()
{
    dispatcher.Stop();
}

int wxProtoApp::MainLoop()
{
#if WXPROTO_USE_THREADING
    // 1) Start ProtoDispatcher thread
    if (app_dispatch)
        dispatcher.StartThread(true, this);
    else
        dispatcher.StartThread(true);
    
    // 2) Enter standard wxWidgets event loop
    int result = wxApp::MainLoop();
    
    // Stop ProtoDispatcher thread
    dispatcher.Stop();
    
    return result;
#else
    
    TRACE("Using unstable, non-threaded wxProtoApp main loop ...\n");
   
#ifndef WIN32
    // The "guiTimer" makes sure we check for
    // wxWindows GUI events regularly
    // (TBD) asynchronously monitor for GUI events instead
    // (this already works on Win32)
    ProtoTimer guiTimer;  
    guiTimer.SetRepeat(-1);
    guiTimer.SetInterval(0.5);
    dispatcher.ActivateTimer(guiTimer);
#endif // !WIN32
    return wxApp::MainLoop();
/*    
    while (1)
    {
        TRACE("guiTimer.IsActive(): %d\n", guiTimer.IsActive());
        if (wxApp::Pending())
        {
            TRACE("dispatching ...\n");
            wxApp::Dispatch();   
        }
        else
        {
            TRACE("wx not pending ...\n");
            if (dispatcher.IsPending())
            {
                dispatcher.Run(true);  // one-shot wait/dispatch
            }
#ifdef WIN32  
            else
            {
                // (Win32 ProtoDispatcher breaks on messages)
                TRACE(" dispatcher.Wait() ...\n");
                dispatcher.Wait();
                TRACE(" dispatcher.Dispatch() ...\n");
                dispatcher.Dispatch();
            }
#endif // WIN32
        }
        if (NULL == GetTopWindow())
        { 
#ifndef WIN32
            if (guiTimer.IsActive()) guiTimer.Deactivate();
#endif // !WIN32
            if (!dispatcher.IsPending())
            {   
                dispatcher.Stop();
                break;
            }            
        }
#ifndef WIN32
        else if (!guiTimer.IsActive())
        {
             ActivateTimer(guiTimer);
        }
#endif // !WIN32
    }
    return wxApp::MainLoop();
    */
#endif
}  // end wxProtoApp::MainLoop()

bool wxProtoApp::SignalDispatchReady()
{
    wxCommandEvent dispatchEvent(wxPROTO_DISPATCH_EVENT);
    //AddPendingEvent(dispatchEvent);   
    wxPostEvent(this, dispatchEvent);
    return true;
}  // end wxProtoApp::SignalDispatchReady()


void wxProtoApp::OnDispatchEvent(wxCommandEvent& /*event*/)
{
    OnDispatch();
}

void wxProtoApp::OnIdleEvent(wxIdleEvent& event)
{
    TRACE("wxProtoApp::OnIdleEvent() ...\n");
    if (dispatcher.IsPending())
    {
        dispatcher.Run(true);   
    }
    event.RequestMore();
}
