#include "protoApp.h"
#include "protoSerial.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>  // for "isspace()"

/**
 * @class SerialApp
 *
 * @brief Example using ProtoSerial
 *
 */
class SerialApp : public ProtoApp
{
    public:
      SerialApp();
      ~SerialApp();

      bool OnStartup(int argc, const char*const* argv);
      bool ProcessCommands(int argc, const char*const* argv);
      void OnShutdown();
      
      
      void PrintSignalStatus();

    private:
      enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
      static const char* const CMD_LIST[];
      static CmdType GetCmdType(const char* string);
      bool OnCommand(const char* cmd, const char* val);        
      void Usage();
      
      void OnSerialInput(ProtoChannel& theChannel,
                         ProtoChannel::Notification notifyType);
      
      bool OnTxTimeout(ProtoTimer&  theTimer);
              
      char          device_name[PATH_MAX+1];
      unsigned int  baud_rate;
      
      ProtoSerial*  serial;
      ProtoTimer    tx_timer;

}; // end class SerialApp

void SerialApp::Usage()
{
    fprintf(stderr, "Usage: serialExample device <name>\n");
}

const char* const SerialApp::CMD_LIST[] =
{
    "-help",        // print help info an exit
    "+device",      // serial device name
    "+baud",        // set baud rate in bits/sec
    "-send",        // send a short test message once per second
    "+debug",       // <debugLevel>
    NULL
};

/**
 * This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(SerialApp) 

SerialApp::SerialApp()
  : baud_rate(0), serial(NULL)
{
    device_name[0] = '\0';
    tx_timer.SetListener(this, &SerialApp::OnTxTimeout);
}

SerialApp::~SerialApp()
{
    if (NULL != serial)
    {
        serial->Close();
        delete serial;
        serial = NULL;
    }
}

SerialApp::CmdType SerialApp::GetCmdType(const char* cmd)
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
}  // end SerialApp::GetCmdType()


bool SerialApp::OnStartup(int argc, const char*const* argv)
{
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "SerialApp::OnStartup() error processing command line options\n");
        return false;   
    }
    
    if ('\0' == device_name[0])
    {
        PLOG(PL_ERROR, "SerialApp::OnStartup() error: no serial device specified\n");
        Usage();
        return false;
    }
    
    
    if (NULL == (serial = ProtoSerial::Create()))
    {
        PLOG(PL_ERROR, "SerialApp::OnStartup() ProtoSerial::Create() error: %s\n", GetErrorString());
        return false;
    }
    
    serial->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    serial->SetListener(this, &SerialApp::OnSerialInput);
    
    TRACE("opening port ...\n");
    
    // Open the serial device, accepting its current configuration (whatever it is)
    
    ProtoSerial::AccessMode accessMode = ProtoSerial::RDWR;//tx_timer.IsActive() ? ProtoSerial::WRONLY : ProtoSerial::RDONLY;
    
    if (!serial->Open(device_name, accessMode))
    {
        PLOG(PL_ERROR, "SerialApp::OnCommand(device) error: unable to open specified device\n");
        delete serial;
        serial = NULL;
        return false;
    }
    
    TRACE("port opened ...\n");
    
    if (0 != baud_rate)
        serial->SetBaudRate(baud_rate);
    else
        baud_rate = serial->GetBaudRate();
    
    TRACE("serialExample config: baud>%d bytes>%d parity>%s local>%s timeout>%lf sec\n",
            serial->GetBaudRate(), serial->GetByteSize(),
            serial->GetParity() ? "yes" : "no",
            serial->GetLocalControl() ? "yes" : "no",
            serial->GetReadTimeout());
    
    serial->Clear(ProtoSerial::DTR);
    serial->Clear(ProtoSerial::RTS);
    
    return true;
}  // end SerialApp::OnStartup()

void SerialApp::OnShutdown()
{
   if (NULL != serial)
   {
       serial->Close();
       delete serial;
       serial = NULL;
   }
}  // end SerialApp::OnShutdown()

bool SerialApp::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a serialExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "SerialApp::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                Usage();
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "SerialApp::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    Usage();
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "SerialApp::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    Usage();
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end SerialApp::ProcessCommands()

bool SerialApp::OnCommand(const char* cmd, const char* val)
{
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "SerialApp::ProcessCommand(%s) missing argument\n", cmd);
        Usage();
        return false;
    }
    else if (!strncmp("help", cmd, len))
    {
        Usage();
        exit(0);
    }
    else if (!strncmp("device", cmd, len))
    {
        strncpy(device_name, val, PATH_MAX);
        device_name[PATH_MAX] = '\0';
    }
    else if (!strncmp("baud", cmd, len))
    {
        baud_rate = atoi(val);
    }
    else if (!strncmp("send", cmd, len))
    {
        tx_timer.SetInterval(1.0);
        tx_timer.SetRepeat(-1);
        ActivateTimer(tx_timer);
    }
    else
    {
        PLOG(PL_ERROR, "serialExample error: invalid command\n");
        Usage();
        return false;
    }
    return true;
}  // end SerialApp::OnCommand()



void SerialApp::OnSerialInput(ProtoChannel&              theChannel,
                              ProtoChannel::Notification notifyType)
{
    if (ProtoChannel::NOTIFY_INPUT != notifyType) return;
    
    char buffer[256];
    unsigned int numBytes = 255;
    if (serial->Read(buffer, numBytes))
    {
        buffer[numBytes] = '\0';
        if (0 != numBytes)
            TRACE("%s", buffer);
        else
            TRACE("ZERO BYTES READ!\n");
        
        PrintSignalStatus();
        
    }
    else
    {
        PLOG(PL_ERROR, "SerialApp::OnSerialInput() error: unable to read from device\n");
    }
    
} // end SerialApp::OnSerialInput()

bool SerialApp::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    
    if (serial->IsSet(ProtoSerial::RTS))
        serial->Clear(ProtoSerial::RTS);
    else
        serial->Set(ProtoSerial::RTS);
    
    
    const char* buffer = "Hello, serial world.\n";
    unsigned int numBytes = strlen(buffer);
    if (serial->Write(buffer, numBytes))
    {
        if (numBytes != strlen(buffer))
            TRACE("PARTIAL SEND!\n");
        else
            TRACE("wrote %d bytes ...\n", numBytes);
        
        PrintSignalStatus();
        
    }
    else
    {
        PLOG(PL_ERROR, "SerialApp::OnSerialInput() error: unable to write to device\n");
    }
    
    return true;
}  // end SerialApp::OnTxTimeout()

void SerialApp::PrintSignalStatus()
{
    ASSERT(NULL != serial);
    if (serial->IsOpen())
    {
        int status = serial->GetStatus();
        PLOG(PL_ALWAYS, "status: DTR:%d RTS:%d CTS:%d DCD:%d DSR:%d RNG:%d\n",
                                 ProtoSerial::IsSet(status, ProtoSerial::DTR),
                                 ProtoSerial::IsSet(status, ProtoSerial::RTS),
                                 ProtoSerial::IsSet(status, ProtoSerial::CTS),
                                 ProtoSerial::IsSet(status, ProtoSerial::DCD),
                                 ProtoSerial::IsSet(status, ProtoSerial::DSR),
                                 ProtoSerial::IsSet(status, ProtoSerial::RNG));
    }
    else
    {
        PLOG(PL_ALWAYS, "status: serial port not open!\n");
    }
}  // end SerialApp::PrintSignalStatus()

