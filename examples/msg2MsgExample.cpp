 
#include "protokit.h"
#include "protoList.h"
#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

#define PIPE_TYPE ProtoPipe::MESSAGE

/**
 * @class Msg2Msg
 *
 * @brief This example program illustrates/tests the use of the ProtoPipe class
 * for local domain interprocess communications
 */
class Msg2Msg : public ProtoApp
{
    public:
        Msg2Msg();
        ~Msg2Msg();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        void OnShutdown();
        bool ProcessCommands(int argc, const char*const* argv);
        bool AddNewRcvPipe(const char* pipeName);
        bool AddNewSndPipe(const char* pipeName);
        bool AddNewRcvSocket(const char* socketAddrStr);
        bool AddNewSndSocket(const char* socketAddrStr);
        bool AddNewSndFile(const char* fileName);
            
    private:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static CmdType GetCmdType(const char* string);
        static const char* const CMD_LIST[];        
        void Usage();
        
        void OnSocketListenEvent(ProtoSocket&       theSocket, 
                                 ProtoSocket::Event theEvent);
        void OnPipeListenEvent(ProtoSocket&       thePipe, 
                               ProtoSocket::Event theEvent);
	//void OnSdtinTimeout(); 

        void SendMessage();

        class ListableSocket : public ProtoSocket, public ProtoList::Item
        {
          public:
            ListableSocket(ProtoSocket::Protocol theprotocol) : ProtoSocket(theprotocol = ProtoSocket::UDP) {}
        };
        class ListablePipe : public ProtoPipe, public ProtoList::Item
        {
          public:
            ListablePipe(ProtoPipe::Type thetype) : ProtoPipe(thetype) {}
        };
        class ListableFile : public ProtoFile, public ProtoList::Item{};

        class SocketList : public ProtoListTemplate<ListableSocket> {};
        class PipeList : public ProtoListTemplate<ListablePipe> {};
        class FileList : public ProtoListTemplate<ListableFile> {};
        SocketList   sndSockets;
        SocketList   rcvSockets;
        PipeList     sndPipes;
        PipeList     rcvPipes;
        FileList     sndFiles;
        bool         usingstdout;

        bool         usingstdin;
	ProtoTimer   sdtinTimer; //used for checking if anything is in sdtin
	ProtoTimerMgr *timerMgrPtr;

        char         msg_buffer[8191];
        unsigned int msg_len;
};  // end class Msg2Msg

// Instantiate our application instance 
PROTO_INSTANTIATE_APP(Msg2Msg) 
        
Msg2Msg::Msg2Msg()
 : usingstdout(false), usingstdin(false), msg_len(8191)
{
    memset(msg_buffer,0,8191);
}

Msg2Msg::~Msg2Msg()
{
}

void Msg2Msg::Usage()
{
    fprintf(stderr, "msg2Msg [listen pipe <listenName>]\n");
    fprintf(stderr, "        [listen socket <mcastAddr>/]<port>\n");
    fprintf(stderr, "        [listen stdin]\n");
    fprintf(stderr, "        [send pipe <sendName>]\n");
    fprintf(stderr, "        [send socket <ipaddress>/<dst_port>]\n");
    fprintf(stderr, "        [send file <filepath>]\n");
    fprintf(stderr, "        [send sdtout>]\n");
}  // end Msg2Msg::Usage()
  
bool Msg2Msg::OnStartup(int argc, const char*const* argv)
{
    if (argc == 1)
    {
        TRACE("No args!  Are you sure you want to be running without any args?\n");
        Usage();
    }
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "msg2Msg::OnStartup() error processing command line\n");
        Usage();
        return false;
    }
    return true;
}  // end Msg2Msg::OnStartup()

void Msg2Msg::OnShutdown()
{
    //we have to close this stuff;
    ListableSocket* sndSocketPtr = NULL;
    while(!sndSockets.IsEmpty())//close out the socket
    {
        sndSocketPtr = sndSockets.RemoveHead();
        if(sndSocketPtr->IsOpen()) sndSocketPtr->Close();
        delete sndSocketPtr;
    }
    
    ListableSocket* rcvSocketPtr = NULL;
    while(!rcvSockets.IsEmpty())//close out the socket
    {
        rcvSocketPtr = rcvSockets.RemoveHead();
        if(rcvSocketPtr->IsOpen()) rcvSocketPtr->Close();
        delete rcvSocketPtr;
    }
    
    ListablePipe* sndPipePtr = NULL;
    while(!sndPipes.IsEmpty())//close out the pipe
    {
        sndPipePtr = sndPipes.RemoveHead();
        if(sndPipePtr->IsOpen()) sndPipePtr->Close();
        delete sndPipePtr;
    }
    
    ListablePipe* rcvPipePtr = NULL;
    while(!rcvPipes.IsEmpty())//close out the pipe
    {
        rcvPipePtr = rcvPipes.RemoveHead();
        if(rcvPipePtr->IsOpen()) rcvPipePtr->Close();
        delete rcvPipePtr;
    }
    
    ListableFile* sndFilePtr = NULL;
    while(!sndFiles.IsEmpty())//close out the file
    {
        sndFilePtr = sndFiles.RemoveHead();
        if(sndFilePtr->IsOpen()) sndFilePtr->Close();
        delete sndFilePtr;
    }
    
    if(usingstdin)
    {
        //do nothing
    }
    if(usingstdout)
    {
        //do nothing
    }
    TRACE("msg2Msg: Done.\n");
}  // end Msg2Msg::OnShutdown()

void Msg2Msg::OnPipeListenEvent(ProtoSocket&       theSocket, 
                                ProtoSocket::Event theEvent)
{
    ProtoPipe *thePipePtr = (ProtoPipe*)&theSocket;
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("msg2Msg: listen RECV event ..\n");
            unsigned int len = 8191;
            if(thePipePtr->Recv(msg_buffer, len))
            {
                if(len)
                {
                    SendMessage();
                }
            }
            else
            {
                DMSG(0,"Msg2Msg::OnPipeListenEvent() Error getting message\n");
            }
            break;
        }
        case ProtoSocket::SEND:
            TRACE("msg2Msg: listen SEND event ..\n");
            break;
        case ProtoSocket::ACCEPT:
            TRACE("msg2Msg: listen ACCEPT event ..\n");
            if(!thePipePtr->Accept())
            {
                DMSG(0,"Msg2Msg::OnPipeListenEvent() Error on calling Accept()\n");
            }
            break;
        case ProtoSocket::DISCONNECT:
            TRACE("msg2Msg: listen DISCONNECT event ..\n");
            char pipeName[PATH_MAX];
            strncpy(pipeName,thePipePtr->GetName(),PATH_MAX);
            thePipePtr->Close();
            if(!thePipePtr->Listen(pipeName))
            {
                DMSG(0,"Msg2Msg::OnPipeListenEvent() Error restarting listen pipe...\n");
            }
            break;
        default:
            TRACE("Msg2Msg::OnPipeListenEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end Msg2Msg::OnPipeListenEvent()

void Msg2Msg::OnSocketListenEvent(ProtoSocket&       theSocket, 
                                  ProtoSocket::Event theEvent)
{
    switch (theEvent)
    {
        case ProtoSocket::RECV:
        {
            TRACE("msg2Msg: listen RECV event ..\n");
            unsigned int len = 8191;
            if(theSocket.Recv(msg_buffer, len))
            {
                if(len)
                {
                    SendMessage();
                }
            }
            else
            {
                DMSG(0,"Msg2Msg::OnSocketListenEvent() Error getting message\n");
            }
            break;
        }
        case ProtoSocket::SEND:
            TRACE("msg2Msg: listen SEND event ..\n");
            break;
        case ProtoSocket::ACCEPT:
            TRACE("msg2Msg: listen ACCEPT event ..\n");
            break;
        case ProtoSocket::DISCONNECT:
            TRACE("msg2Msg: listen DISCONNECT event ..\n");
            break;
        default:
            TRACE("Msg2Msg::OnSocketListenEvent(%d) unhandled event type\n", theEvent);
            break;
        
    }  // end switch(theEvent)
}  // end Msg2Msg::OnSocketListenEvent()

void Msg2Msg::SendMessage()
{
    unsigned int len = strlen(msg_buffer);

    SocketList::Iterator socketIterator(sndSockets);
    ListableSocket* sndSocketPtr = NULL;
    while((sndSocketPtr = socketIterator.GetNextItem()))//close out the socket
    {
        if(sndSocketPtr->IsOpen())
        {
            sndSocketPtr->Send(msg_buffer,len);
        }
    }
    
    PipeList::Iterator pipeIterator(sndPipes);
    ListablePipe* sndPipePtr = NULL;
    while((sndPipePtr = pipeIterator.GetNextItem()))//close out the pipe
    {
        if(sndPipePtr->IsOpen())
        {
            sndPipePtr->Send(msg_buffer,len);
        }
    }
    
    FileList::Iterator fileIterator(sndFiles);
    ListableFile* sndFilePtr = NULL;
    while((sndFilePtr = fileIterator.GetNextItem()))//close out the file
    {
        if(sndFilePtr->IsOpen())
        {
            sndFilePtr->Write(msg_buffer,len);
        }
    }
    if(usingstdout)
    {
        fprintf(stdout,"%s\n",msg_buffer);
    }
    memset(msg_buffer,0,msg_len); //clear out the message
}

bool Msg2Msg::ProcessCommands(int argc, const char*const* argv)
{
    int i = 1;
    while ( i < argc)
    {
        if (!strcmp("listen", argv[i]))
        {
            i++;
            if(!strcmp("pipe",argv[i]))
            {
                i++;
                AddNewRcvPipe(argv[i]);
            }
            else if (!strcmp("socket",argv[i]))
            {
                i++;
                AddNewRcvSocket(argv[i]);
            }
            else if (!strcmp("sdtin",argv[i]))
            {
                usingstdin = true;
                //install timer loop
                //bunny fix this to actually open stdin
            }
            else
            {
                DMSG(0,"Msg2Msg::ProcessCommands() Error setting listen to type %s\n",argv[i]);
            }
        }
        else if (!strcmp("send", argv[i]))
        {
            i++;
            if(!strcmp("pipe",argv[i]))
            {
                i++;
                AddNewSndPipe(argv[i]);    
            }
            else if (!strcmp("socket",argv[i]))
            {
                i++;
                AddNewSndSocket(argv[i]);
            }
            else if (!strcmp("file",argv[i]))
            {
                i++;
                AddNewSndFile(argv[i]);
            }
            else if (!strcmp("sdtout",argv[i]))
            {
                usingstdin = true;
            }
            else
            {
                DMSG(0,"Msg2Msg::ProcessCommands() Error setting send to type %s\n",argv[i]);
            }
        }
        else
        {
            PLOG(PL_ERROR, "Msg2Msg::ProcessCommand() unknown command error?\n");
            return false;
        }
        i++;
    }
    return true;
}  // end Msg2Msg::ProcessCommands()
bool
Msg2Msg::AddNewRcvPipe(const char* pipeName)
{
    ListablePipe *newPipePtr = new ListablePipe(ProtoPipe::MESSAGE);
    if(!newPipePtr)
    {
        DMSG(0,"Msg2MsgAddNewRcvPipe error allocing new pipe\n");
        return false;
    }
    newPipePtr->SetNotifier(&GetSocketNotifier());
    newPipePtr->SetListener(this,&Msg2Msg::OnPipeListenEvent);//bunny the this will need to be changed
    if(!newPipePtr->Listen(pipeName))
    {
        DMSG(0,"Msg2Msg::AddNewRcvPipe: Error opening pipe to %s\n",pipeName);
        delete newPipePtr;
        return false;
    }
    rcvPipes.Append(*newPipePtr);
    return true;
}
bool
Msg2Msg::AddNewSndPipe(const char* pipeName)
{
    ListablePipe *newPipePtr = new ListablePipe(ProtoPipe::MESSAGE);
    if(!newPipePtr)
    {
        DMSG(0,"Msg2MsgAddNewSndPipe error allocing new pipe\n");
        return false;
    }
    if(!newPipePtr->Connect(pipeName))
    {
        DMSG(0,"Msg2Msg::AddNewRcvPipe: Error connecting pipe to %s\n",pipeName);
        delete newPipePtr;
        return false;
    }
    sndPipes.Append(*newPipePtr);
    return true;
}
bool
Msg2Msg::AddNewRcvSocket(const char* socketAddrStr)
{
    char charAddr[255];
    memset(charAddr,0,255);
    ProtoAddress rcv_addr;
    int rcv_port; 
    //parse socketAddrStr
    const char* index = strchr(socketAddrStr,'/');
    if(index!=NULL)
    {
        strncpy(charAddr,socketAddrStr,index-socketAddrStr);
        if(!(rcv_addr.ResolveFromString(charAddr)))
        {
            DMSG(0,"Msg2Msg::AddNewRcvSocket() Error setting address to %s\n",charAddr);
            return false;
        }
        rcv_port = atoi(index+1);
    }
    else
    {
        rcv_port = atoi(socketAddrStr);
    }
    ListableSocket *newSocketPtr = new ListableSocket(ProtoPipe::UDP);
    if(!newSocketPtr)
    {
        DMSG(0,"Msg2MsgAddNewRcvSocket error allocing new socket\n");
        return false;
    }
    newSocketPtr->SetNotifier(&GetSocketNotifier());
    newSocketPtr->SetListener(this,&Msg2Msg::OnSocketListenEvent);//bunny the this will need to be changed
    if(!(newSocketPtr->Open(rcv_port,ProtoAddress::IPv4)))
    {
        DMSG(0,"Msg2MsgAddNewRcvSocket Error Opening the socket rcv port %d\n",rcv_port);
        delete newSocketPtr;
        return false;
    }
    if(rcv_addr.IsMulticast()) //do multicast stuff
    {
        if(!newSocketPtr->JoinGroup(rcv_addr))
        {
            DMSG(0,"Msg2MsgAddNewRcvSocket Error joining the multicast group %s\n",rcv_addr.GetHostString());
            newSocketPtr->Close();
            delete newSocketPtr;
            return false;
        }
    }
    rcvSockets.Append(*newSocketPtr);
    return true;
}
bool
Msg2Msg::AddNewSndSocket(const char* socketAddrStr)
{
    char charAddr[255];
    memset(charAddr,0,255);
    ProtoAddress dst_addr;
    int dst_port;
    int src_port;//bunny setting this to dst port for now this should be controled seperatly
    //parse the string
    const char* index = strchr(socketAddrStr,'/');
    if(index!=NULL)
    {
        strncpy(charAddr,socketAddrStr,index-socketAddrStr);
        if(!dst_addr.ResolveFromString(charAddr))
        {
            DMSG(0,"Msg2Msg::AddNewSndSocket: Error setting address to %s\n",socketAddrStr);
            return false;
        }
        dst_port = atoi(index+1);
        src_port = dst_port;
        dst_addr.SetPort(dst_port);
    }
    else
    {
        DMSG(0,"Msg2Msg::AddNewSndSocket: Error missing \"/\" for send socket command\n");
        return false;
    }
    ListableSocket *newSocketPtr = new ListableSocket(ProtoSocket::UDP);
    if(!newSocketPtr)
    {
        DMSG(0,"Msg2MsgAddNewSndSocket error allocing new socket\n");
        return false;
    }
    if(!(newSocketPtr->Bind(src_port)))
    {
        DMSG(0,"Msg2MsgAddNewSndSocket: Error binding to src_port %d\n",src_port);
        delete newSocketPtr;
        return false;
    }
    if(!(newSocketPtr->Open(src_port,ProtoAddress::IPv4,false)))
    {
        DMSG(0,"msg2MsgAddNewSndSocket: Error opening socket on port %d\n",src_port);
        delete newSocketPtr;
        return false;
    }
    if(dst_addr.IsMulticast())
    {
        if(!newSocketPtr->JoinGroup(dst_addr))
        {
            DMSG(0,"Msg2MsgAddNewSndSocket: Error joinging multicast Group %s\n",dst_addr.GetHostString());
            newSocketPtr->Close();
            delete newSocketPtr;
            return false;
        }
    }
    if(!(newSocketPtr->Connect(dst_addr)))
    {
        DMSG(0,"Msg2MsgAddNewSndSocket: Error connecting to %s\n",dst_addr.GetHostString());
        newSocketPtr->Close();
        delete newSocketPtr;
        return false;
    }
    sndSockets.Append(*newSocketPtr);
    return true;
}
bool
Msg2Msg::AddNewSndFile(const char* fileName)
{
    ListableFile *newFilePtr = new ListableFile();
    if(!newFilePtr)
    {
        DMSG(0,"Msg2MsgAddNewSndSocket error allocing new socket\n");
        return false;
    }
    if(!(newFilePtr->Open(fileName,O_WRONLY | O_CREAT)))
    {
        DMSG(0,"Msg2MsgAddNewSndFile error opening the file \"%s\"\n",fileName);
        delete newFilePtr;
        return false;
    }
    sndFiles.Append(*newFilePtr);
    return true;
}
