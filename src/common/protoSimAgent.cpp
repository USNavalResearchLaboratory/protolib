#include "protoSimAgent.h"

ProtoMessageSink::~ProtoMessageSink()
{
}

bool ProtoMessageSink::HandleMessage(const char* txBuffer, unsigned int len,const ProtoAddress& srcAddr) 
{
    return true;
}  // end ProtoMessageSink::HandleMessage()


ProtoSimAgent::ProtoSimAgent()
{
}

ProtoSimAgent::~ProtoSimAgent()
{
}


ProtoSimAgent::SocketProxy::SocketProxy()
 : proto_socket(NULL)
{
}

ProtoSimAgent::SocketProxy::~SocketProxy()
{
}

ProtoSimAgent::SocketProxy::List::List()
 : head(NULL)
{
} 

void ProtoSimAgent::SocketProxy::List::Prepend(SocketProxy& proxy)
{
    proxy.SetPrev(NULL);
    proxy.SetNext(head);
    if (head) head->SetPrev(&proxy);
    head = &proxy;
}  // end ProtoSimAgent::SocketProxy::List::Prepend()

void ProtoSimAgent::SocketProxy::List::Remove(SocketProxy& proxy)
{
    SocketProxy* prev = proxy.GetPrev();
    SocketProxy* next = proxy.GetNext();
    if (prev)
        prev->SetNext(next);
    else
        head = next;
    if (next)
        next->SetPrev(prev);
}  // end ProtoSimAgent::SocketProxy::List::Remove()

ProtoSimAgent::SocketProxy* ProtoSimAgent::SocketProxy::List::FindProxyByPort(UINT16 thePort)
{
    SocketProxy* next = head;
    while (next)
    {
        if (next->GetPort() == thePort)
            return next;
        else
            next = next->GetNext();   
    }
    return NULL;
}  // end ProtoSimAgent::SocketProxy::List::FindProxyByPort()

// I.T. Added 27th Feb 2007
ProtoSimAgent::SocketProxy* ProtoSimAgent::SocketProxy::List::FindProxyByIdentifier(int socketProxyID)
{
    SocketProxy* next = head;
    while (next)
    {
        if (next->GetSocketProxyID() == socketProxyID)
            return next;
        else
            next = next->GetNext();   
    }
    return NULL;
}  // end OpnetProtoSimProcess::TcpSocketProxy::TcpSockList::FindProxyByIdentifier()
