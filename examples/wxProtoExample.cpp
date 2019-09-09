
#include "wxProtoApp.h"

class wxProtoExampleFrame : public wxFrame
{
    private:   
        void OnClose(wxCloseEvent& event);
        DECLARE_DYNAMIC_CLASS(wxProtoExampleFrame)
        DECLARE_EVENT_TABLE()
};

IMPLEMENT_DYNAMIC_CLASS(wxProtoExampleFrame, wxFrame)

BEGIN_EVENT_TABLE(wxProtoExampleFrame, wxFrame)
  EVT_CLOSE         (wxProtoExampleFrame::OnClose)
END_EVENT_TABLE()

void wxProtoExampleFrame::OnClose(wxCloseEvent& /*event*/)
{ 
    Destroy();
}


class wxProtoExampleApp : public wxProtoApp
{
    public:
        wxProtoExampleApp();
        ~wxProtoExampleApp();
        
        // wxApp base class overrides
        bool OnInit();
        int OnExit();
                
    private:
        bool OnTxTimeout(ProtoTimer& theTimer);
        void OnClose(wxCloseEvent& event);
        void OnStart(wxCommandEvent& event);
        void OnStop(wxCommandEvent& event);
        void OnQuit(wxCommandEvent& event);
        enum
        {
            ID_QUIT = wxID_HIGHEST+1,
            ID_START,
            ID_STOP   
        };

        
    
        ProtoTimer              tx_timer;
        unsigned int            counter;
        wxProtoExampleFrame*    frame;      // main window
        wxTextCtrl*             tx_dest_edit;
        wxTextCtrl*             tx_rate_edit;
        wxTextCtrl*             rx_port_edit;
        DECLARE_EVENT_TABLE()
                
};  // end class wxProtoExampleApp


DECLARE_APP(wxProtoExampleApp)
IMPLEMENT_APP(wxProtoExampleApp)

BEGIN_EVENT_TABLE(wxProtoExampleApp, wxProtoApp)
    EVT_CLOSE(wxProtoExampleApp::OnClose) 
    EVT_MENU(wxProtoExampleApp::ID_QUIT,  wxProtoExampleApp::OnQuit) 
    EVT_BUTTON(wxProtoExampleApp::ID_START, wxProtoExampleApp::OnStart)
    EVT_BUTTON(wxProtoExampleApp::ID_STOP, wxProtoExampleApp::OnStop)
END_EVENT_TABLE()
    

        
wxProtoExampleApp::wxProtoExampleApp()
 : wxProtoApp(false), counter(0), frame(NULL)
{
    tx_timer.SetListener(this, &wxProtoExampleApp::OnTxTimeout);
}

wxProtoExampleApp::~wxProtoExampleApp()
{
    
}

void wxProtoExampleApp::OnClose(wxCloseEvent& /*event*/)
{ 
    TRACE("wxProtoExampleApp::OnClose() ...\n");
}

void wxProtoExampleApp::OnStart(wxCommandEvent& /*event*/)
{ 
    TRACE("wxProtoExampleApp::OnStart() ...\n");
    if (!tx_timer.IsActive()) ActivateTimer(tx_timer);
    wxMessageBox(_T("Hi there"));
}

void wxProtoExampleApp::OnStop(wxCommandEvent& /*event*/)
{ 
    TRACE("wxProtoExampleApp::OnStop() ...\n");
    if (tx_timer.IsActive()) tx_timer.Deactivate();
}

void wxProtoExampleApp::OnQuit(wxCommandEvent& /*event*/)
{ 
    TRACE("wxProtoExampleApp::OnQuit() ...\n");
    if (frame) 
    {
        frame->Destroy();
        frame = NULL;
    }
}

bool wxProtoExampleApp::OnInit()
{

    SetDebugLevel(4);
    tx_timer.SetInterval(1.0);
    tx_timer.SetRepeat(-1);
    
    if (!(frame = new wxProtoExampleFrame()))
    {
        PLOG(PL_ERROR, "wxProtoExampleApp::OnInit() new wxFrame error: %s\n",
                GetErrorString());
        return false;   
    }
    if (!frame->Create((wxFrame*)NULL, -1, _T("wxProtoExample")))
    {
        OnExit();
        return false;
    }
    
    wxMenuBar* menuBar = new wxMenuBar();
    if (!menuBar)
    {
        fprintf(stderr, "wxProtoExampleApp::OnInit() error creating menu bar\n");
        return false;
    }
    // Create "File" menu
    wxMenu* menu = new wxMenu();
    if (!menu)
    {
        fprintf(stderr, "wxProtoExampleApp::OnInit() error creating file menu\n");
        return false;
    }
    menu->Append(ID_QUIT, _T("E&xit"));
    menuBar->Append(menu, _T("&File"));
    // Create "Help" menu
    /*menu = new wxMenu();
    if (!menu)
    {
        fprintf(stderr, "wxProtoExampleApp::OnInit() error creating help menu\n");
        return false;
    }
    menuBar->Append(menu, "&Help");*/
    frame->SetMenuBar(menuBar);
    
    wxBoxSizer* masterSizer = new wxBoxSizer(wxVERTICAL);
    
    // Row 1 - tx dest
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(new wxStaticText(frame, -1, _T("TxDest:")), 0, 
               wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    if (!(tx_dest_edit = new wxTextCtrl(frame, -1, _T("127.0.0.1/5000"))))
    {
        PLOG(PL_ERROR, "wxProtoExampleApp::OnInit() Error creating tx_dest_edit\n");
        OnExit();
        return false;
    }
    sizer->Add(tx_dest_edit, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 10);
    masterSizer->Add(sizer, 0, wxEXPAND | wxALIGN_CENTER);
    
    // Row 2 - tx rate
    sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(new wxStaticText(frame, -1, _T("TxRate:")), 0, 
               wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    if (!(tx_rate_edit = new wxTextCtrl(frame, -1, _T("1.0"))))
    {
        PLOG(PL_ERROR, "wxProtoExampleApp::OnInit() Error creating tx_dest_edit\n");
        OnExit();
        return false;
    }
    sizer->Add(tx_rate_edit, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 10);
    sizer->Add(new wxStaticText(frame, -1, _T("pkt/sec")), 0, 
               wxALIGN_CENTER_VERTICAL);
    masterSizer->Add(sizer, 0, wxEXPAND | wxALIGN_CENTER);
    
    // Row 3 - rx port
    sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(new wxStaticText(frame, -1, _T("TxRate:")), 0, 
               wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    if (!(rx_port_edit = new wxTextCtrl(frame, -1, _T("5000"))))
    {
        PLOG(PL_ERROR, "wxProtoExampleApp::OnInit() Error creating tx_dest_edit\n");
        OnExit();
        return false;
    }
    sizer->Add(rx_port_edit, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 10);
    masterSizer->Add(sizer, 0, wxEXPAND | wxALIGN_CENTER);
    
    
    // Row 4 - start/stop buttons
    sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(new wxButton(frame, ID_START, _T("Start")), 0, 
               wxALIGN_CENTER | wxALL, 10);
    sizer->Add(new wxButton(frame, ID_STOP, _T("Stop")), 0,  
               wxALIGN_CENTER | wxALL, 10);
    masterSizer->Add(sizer, 0, wxALIGN_CENTER | wxALL, 10);
    
    frame->SetSizer(masterSizer);
    frame->SetSize(300, 300);
    frame->Show(true);
    return true;
}

int wxProtoExampleApp::OnExit()
{
    TRACE("wxProtoExampleApp::OnExit() ...\n");
    if (frame)
    {
        
        //frame->Destroy();
        frame = NULL;
        tx_dest_edit = NULL;
        tx_rate_edit = NULL; 
    }
    return 0;
}  // end wxProtoExampleApp::OnExit()

bool wxProtoExampleApp::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    counter++;
    TRACE("wxPrototoExampleApp::OnTxTimeout(%u) ...\n", counter);
    return true;
}
