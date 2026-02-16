#include "protoCap.h"
#include "protoDebug.h"
#include "protoAddress.h"
#include "protoSocket.h"
/** This implementation of ProtoCap depends upon the "RawEther" packet capture API
 */

#include <W32NAPI.h>  // for RawEther headers

// "Autolink in appropriate RawEther library"
#define RAWETHER_NORMAL
#ifdef RAWETHER_NORMAL
#pragma comment(lib, "W32NAPI.lib")
#endif

/** We cheat a little here to get async I/O for
 *  the RawEther packet reception
 *  (The reference PKTQ struct has a "HANDLE hMessagePostedEvent"
 *  which is set when new data is written to the queue.
 *   Thus, we can use this as our ProtoChannel "input_handle"
 */

#include <PKTQ.h>

typedef struct _W32N_OPEN_CONTEXT
{

   HANDLE   hRealHandle;   // The "real" handle from CreateFile...

   LPWSTR   pszAdapterName;

   BOOLEAN  bMaxTotalSizeValid;
   ULONG    nMaxTotalSize;

   BOOLEAN  bCurrentAddressValid;
   UCHAR    CurrentAddress[ 8 ];
   ULONG    CurrentAddressLength;

   WCHAR    MessageQueueName[ 32 ];

   PKTQ     *pPKTQ;        // Receive Packet Message Queue
} W32N_OPEN_CONTEXT, *PW32N_OPEN_CONTEXT;


class RawEtherCap : public ProtoCap
{
    public:
        RawEtherCap();
        virtual~RawEtherCap();
        
        // These must be overridden for different implementations
        virtual bool Open(const char* interfaceName = NULL);
        virtual bool IsOpen() {return (INVALID_HANDLE_VALUE != control_handle);}
        virtual void Close();
        virtual bool Send(const char* buffer, unsigned int buflen);
        virtual bool Forward(char* buffer, unsigned int buflen) ;
        virtual bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
            
    private:
        HANDLE  control_handle;
        HANDLE  adapter_handle;
        BYTE    adapter_address[6];  // adapter MAC address
            
};  // end class RawEtherCap

ProtoCap* ProtoCap::Create()
{
    return static_cast<ProtoCap*>(new RawEtherCap);
}  // end ProtoCap::Create();

RawEtherCap::RawEtherCap()
 :  control_handle(INVALID_HANDLE_VALUE),
    adapter_handle(INVALID_HANDLE_VALUE)
{
}

RawEtherCap::~RawEtherCap()
{
    Close();
}

bool RawEtherCap::Open(const char* interfaceName)
{
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            PLOG(PL_ERROR, "RawEtherCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             PLOG(PL_ERROR, "RawEtherCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
    }
    
    DWORD libVersion = W32N_GetLibraryApiVersion();
    if( !W32N_IsApiCompatible(libVersion, W32N_API_VERSION))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() Incompatible RawEther library version!\n");
        return false;
    }
    // Open Handle On The Driver Control Channel
    if (INVALID_HANDLE_VALUE == (control_handle = W32N_OpenControlChannel()))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() W32N_OpenControlChannel() error: %s\n", GetErrorString());
        return false;
    }
    // Check API Version compatibility
    DWORD driverVersion;
    if (0 != W32N_GetDriverApiVersion(control_handle, &driverVersion))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() W32N_GetDriverApiVersion() failure!\n");
        Close();
        return false;
    }
    if(!W32N_IsApiCompatible(driverVersion, W32N_API_VERSION))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() Incompatible RawEther driver version!\n");
        Close();
        return false;
    }

    DWORD dwBytesWritten;
    char  EnumBuffer[ 128 ];
    if (0 != W32N_EnumProtocolBindings(control_handle,
                                       (PBYTE )EnumBuffer,
                                       sizeof(EnumBuffer),
                                       &dwBytesWritten))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() W32N_EnumProtocolBindings() error!\n");
        Close();
        return false;
    }

    LPWSTR psCurrent = (LPWSTR )EnumBuffer;
    unsigned int ifCount = 0;
    while( *psCurrent != L'\0' )
    {
        char ifName[256];
        wcstombs(ifName, psCurrent, wcslen(psCurrent)+1);

        //
        // Move To The Next String
        //
        psCurrent += wcslen( psCurrent );
        ++psCurrent;
        ifCount++;
    }

#ifdef _UNICODE
    wchar_t wideBuffer[PATH_MAX];
    mbstowcs(wideBuffer, interfaceName, strlen(interfaceName)+1);
    LPTSTR namePtr = wideBuffer;
#else
    LPTSTR namePtr = wideBuffer;
#endif // if/else _UNICODE
    if (INVALID_HANDLE_VALUE == (adapter_handle = W32N_OpenLowerAdapter(namePtr)))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() W32N_OpenLowerAdapter() error: %s\n", GetErrorString());
        Close();
        return false;
    }

   UINT  addrLen = 6;
   NDIS_STATUS nNdisStatus;

   if (W32N_NdisQueryInformation(adapter_handle,
                                 OID_802_3_CURRENT_ADDRESS,
                                 adapter_address, &addrLen,
                                 &nNdisStatus))
   {
        PLOG(PL_ERROR, "RawEtherCap::Open() W32N_NdisQueryInformation(OID_802_3_CURRENT_ADDRESS) error: %s\n", GetErrorString());
        Close();
        return false;
   }
                  

    //W32N_EnableLoopback(adapter_handle, TRUE);  // this was temporarily enabled for testing

    if (W32N_EnableReceiver(adapter_handle, TRUE))
    {
        PLOG(PL_ERROR, "RawEtherCap::Open() W32N_EnableReceiver() error: %s\n", GetErrorString());
        Close();
        return false;
    }

    // Here is where we steal the PKTQ event to trigger async input notification
    W32N_OPEN_CONTEXT* openContext = (W32N_OPEN_CONTEXT*)adapter_handle;
    PKTQ* q = openContext->pPKTQ;
    input_handle = openContext->pPKTQ->hMessagePostedEvent;
                  
    if (!ProtoCap::Open(interfaceName))
    {
        PLOG(PL_ERROR, "ProtoLinuxCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    
    return true;
}  // end RawEtherCap::Open()

void RawEtherCap::Close()
{
    if (IsOpen())
    {
        ProtoCap::Close();
        if (INVALID_HANDLE_VALUE != adapter_handle)
        {
            input_handle = NULL;
            W32N_EnableReceiver(adapter_handle, FALSE);
            W32N_CloseAdapter(adapter_handle);
            adapter_handle = INVALID_HANDLE_VALUE;
        }
        if (INVALID_HANDLE_VALUE != control_handle)
        {
            W32N_ShutdownDriver();
            control_handle = INVALID_HANDLE_VALUE;
        }
    }
}  // end RawEtherCap::Close()

bool RawEtherCap::Send(const char* buffer, unsigned int buflen)
{
    NDIS_STATUS nNdisStatus;
    if (ERROR_SUCCESS != W32N_TransmitFrame(adapter_handle, (BYTE*)buffer, buflen, &nNdisStatus))
    {
        PLOG(PL_ERROR, "RawEtherCap::Send() W32N_TransmitFrame() error: %s\n", GetErrorString());
        return false;
    }
    else
    {
        return true;
    }
}  // end RawEtherCap::Send()

bool RawEtherCap::Forward(char* buffer, unsigned int buflen)
{
    // Substitute source MAC addr with our MAC addr
    memcpy((buffer+6), adapter_address, 6);
    NDIS_STATUS nNdisStatus;
    if (ERROR_SUCCESS != W32N_TransmitFrame(adapter_handle, (BYTE*)buffer, buflen, &nNdisStatus))
    {
        PLOG(PL_ERROR, "RawEtherCap::Forward() W32N_TransmitFrame() error: %s\n", GetErrorString());
        return false;
    }
    else
    {
        return true;
    }
}  // end RawEtherCap::Send()

bool RawEtherCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    // With loopback disabled, all packets seen should be INBOUND
    if (direction) *direction = INBOUND;
    DWORD frameSize = numBytes;
    if (frameSize > 1536) frameSize = 1536;
    if (W32N_ReadFrame(adapter_handle, (BYTE*)buffer, &frameSize, 0))
    {
        numBytes = frameSize;
    }
    else
    {
        numBytes = 0;
    }
    return true;
}  // end RawEtherCap::Recv()
