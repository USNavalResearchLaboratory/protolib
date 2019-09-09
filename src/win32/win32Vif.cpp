#include "protoVif.h"
#include "protoDebug.h"
#include "windows.h"
#include "common.h" // ljt why isn't it finding this in the 
#include <Iphlpapi.h>
#include <stdio.h>

#include <winioctl.h>
// Where did these defines go?? Is this the right location? ljt
/* ===============================================
    This file is included both by OpenVPN and
    the TAP-Win32 driver and contains definitions
    common to both.
   =============================================== */

/* =============
    TAP IOCTLs
   ============= */

#define TAP_CONTROL_CODE(request,method) \
  CTL_CODE (FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

#define TAP_IOCTL_SET_MEDIA_STATUS      TAP_CONTROL_CODE (6, METHOD_BUFFERED)
#define USERMODEDEVICEDIR "\\\\.\\Global\\"
#define NETWORK_CONNECTIONS_KEY \
		"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define TAPSUFFIX     ".tap"

class Win32Vif : public ProtoVif
{
  public:
    Win32Vif();
    ~Win32Vif();

  bool InitTap(char* vifName, const ProtoAddress& vifAddr, unsigned int maskLen);   
  bool ConfigureTap(char* adapterid, char* vifName, const ProtoAddress& vifAddr,unsigned int maskLen);
  bool FindIPAddr(ProtoAddress vifAddr);

  bool Open(const char* vifName, const ProtoAddress& vifAddr, unsigned int maskLen);
  
  void Close();
  
  bool Write(const char* buffer, unsigned int numBytes);
  
  bool Read(char* buffer, unsigned int& numBytes);
  
private:
#ifndef _WIN32_WCE
  // These members facilitate Win32 overlapped I/O
  // which we use for async I/O
  HANDLE        tap_handle;
  enum {BUFFER_MAX = 8192};
  char          read_buffer[BUFFER_MAX];
  unsigned int  read_count;
  char          write_buffer[BUFFER_MAX];
  OVERLAPPED    read_overlapped;
  OVERLAPPED    write_overlapped;

#endif // if _WIN32_WCE  
  
}; // end class ProtoVif

ProtoVif* ProtoVif::Create()
{
    return static_cast<ProtoVif*>(new Win32Vif);

}  // end ProtoVif::Create()

Win32Vif::Win32Vif()
{
}

Win32Vif::~Win32Vif()
{
}

bool Win32Vif::ConfigureTap(char* adapterid,char* vifName,ProtoAddress vifAddr,unsigned int maskLen)
{    
    ULONG status;
    HKEY key;
    DWORD nameType;
    const char nameString[] = "Name";
    char regpath[1024];
    char nameData[1024];
    long len = sizeof(nameData);

    /* Find out more about this adapter */        
    sprintf(regpath, "%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, adapterid);
    
    /* Open the adapter key */
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                          regpath, 
                          0, 
                          KEY_READ, 
                          &key);
    
    if (status != ERROR_SUCCESS)
    {
        PLOG(PL_WARN,"Win32Vif::ConfigureTap() Error opening adapter registry key:%s\n",regpath);
        return false;  
    }
    
    /* Get the adapter name */
    status =  RegQueryValueEx(key, 
                              nameString, 
                              NULL, 
                              &nameType, 
                              (LPBYTE)nameData,
                              (LPDWORD)&len);
    
    if (status != ERROR_SUCCESS) // ljt || nameType != REG_SZ)
    {
        PLOG(PL_ERROR,"Win32Vif::ConfigureTap() Error opening registry key: %s\n%s\n%s\n\n",
             NETWORK_CONNECTIONS_KEY,regpath,"Name");
        return false;;
    }
    RegCloseKey(key);

    /* Configure the interface */
    char cmd[256];
    char subnetMask[16];

    ProtoAddress subnetMaskAddr;
    subnetMaskAddr.ResolveFromString("255.255.255.255");

    ProtoAddress tmpAddr;
    subnetMaskAddr.GetSubnetAddress(maskLen, tmpAddr);
    sprintf(subnetMask," %s ", tmpAddr.GetHostString());  

    sprintf(cmd,
       "netsh interface ip set address \"%s\" static %s %s ",
        nameData, vifAddr.GetHostString(), subnetMask);    

    if (LONG ret = system(cmd))
    {
        PLOG(PL_ERROR,"Win32Vif::ConfigureTap(%s) Open failed. netsh call returned: %u", nameData, ret);
        return false;
    }

    // Rename the connection
    if (strcmp(nameData, vifName) != 0)
    {
        sprintf(cmd, "netsh interface set interface name=\"%s\" newname=\"%s\"",
                     nameData,vifName);
        if (LONG ret = system(cmd))
        {
            PLOG(PL_ERROR,"Win32Vif::ConfigureTap(%s) Rename failed (%s). netsh call returned: %u", 
                          nameData, vifName, ret);
            return false;
        }
    }
    PLOG(PL_INFO,"Win32Vif::ConfigureTap() Tap opened on interface: %s\n",vifName);

    /* set driver media status to 'connected' */
    status = TRUE;
    if (!DeviceIoControl(tap_handle, TAP_IOCTL_SET_MEDIA_STATUS,
			  &status, sizeof (status), 
              &status, sizeof (status),
              (LPDWORD)&len, NULL))
      PLOG(PL_WARN, "Win32Vif::ConfigureTap() warning: The TAP-Win32 driver rejected a TAP_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.");

    return true;
} // end Win32Five::ConfigureTap()

bool Win32Vif::InitTap(const char* vifName, const ProtoAddress& vifAddr, unsigned int maskLen)
{
	HKEY key;
    ULONG status;
	char adapterid[1024];
	char tapname[1024];
    long len;

    /// Open Registry and get a list of network adapters
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                          NETWORK_CONNECTIONS_KEY,
                          0,
                          KEY_READ,
                          &key);

    if (status != ERROR_SUCCESS)
    {
        PLOG(PL_ERROR,"Win32Vif::InitTap() Error opening registry key:%s\n",NETWORK_CONNECTIONS_KEY);
        return false;
    }

	/* find the adapter with TAPSUFFIX */
    for (int enum_index = 0; ; enum_index++) 
    {
		len = sizeof(adapterid);
		if (RegEnumKeyEx(key, enum_index, adapterid, (LPDWORD)&len, 
                        NULL, NULL, NULL, NULL) != ERROR_SUCCESS) 
        {
			RegCloseKey(key);
			PLOG(PL_ERROR,"Win32Vif::InitTap() error: Couldn't find TAP-Win32 adapter.\n");
			return false;
		}
		sprintf(tapname, USERMODEDEVICEDIR "%s" TAPSUFFIX, adapterid);
        // Get the tap handle
		tap_handle = CreateFile(tapname, GENERIC_WRITE | GENERIC_READ,
			    0, 0, OPEN_EXISTING, 
                FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
		
		if (tap_handle != INVALID_HANDLE_VALUE) 
        {
            break;
		}
	}
    
    RegCloseKey(key);
	PLOG(PL_INFO,"win32Vif::InitTap() found tap adapter %s\n",tapname);

    if (!ConfigureTap(adapterid,vifName,vifAddr,maskLen))
        return false;

    if (NULL == (input_handle = CreateEvent(NULL,TRUE,FALSE,NULL)))
    {
        input_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR,"Win32Vif::InitTap() CreateEvent(input_handle) error: %s\n", ::GetErrorString());
        Close(); // ljt??
        return false;
    }
    if (NULL == (output_handle = CreateEvent(NULL,TRUE,FALSE,NULL)))
    {
        output_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR,"Win32Vif::InitTap() CreateEvent(output_handle) error: %s\n",::GetErrorString());
        Close(); // ljt??
        return false;
    }
    
    memset(&read_overlapped,0,sizeof(read_overlapped));
    read_overlapped.hEvent = input_handle;
    memset(&write_overlapped,0,sizeof(write_overlapped));
    write_overlapped.hEvent = output_handle;
    
    StartInputNotification();

    // Start overlapped notifications
    if (NULL != GetNotifier())
    {
        // Initiate overlapped I/O ...
        DWORD bytesRead;
        if (0 != ReadFile(tap_handle, read_buffer, BUFFER_MAX, &bytesRead, &read_overlapped))
        {
            input_ready = true;
            read_count = bytesRead;
        }
        else
        {
            switch(GetLastError())
            {
                case ERROR_IO_PENDING:
                  read_count = 0;
                  input_ready = false;
                  break;
                //case ERROR_BROKEN_PIPE: 
                default:
                  PLOG(PL_ERROR,"Win32Vif::InitTap() ReadFile() error: %s\n", ::GetErrorString());
                  Close();
                  return false;
            }
        }
    }

    if (!UpdateNotification())
    {
        PLOG(PL_ERROR, "Win32Vif::InitTap() error updating notification\n");
        Close();
        return false;
    }

    return true;  
    
} // end Win32Vif::InitTap() 

bool Win32Vif::Open(const char* vifName, const ProtoAddress& vifAddr, unsigned int maskLen)
{
    if (!InitTap(vifName, vifAddr, maskLen))
    {
        PLOG(PL_ERROR,"Win32Vif::Open(%s) error: open failed: %s\n",vifName, GetErrorString());
        return false;
    }
    strncpy(vif_name, vifName, VIF_NAME_MAX);
    if (!ProtoChannel::Open())
    {
        PLOG(PL_ERROR, "Win32Vif::Open(%s) error: couldn't install ProtoChannel\n", vifName);
        Close();
        return false;    
    }
    else
    {
        return true;
    }
}  // end Win32Vif::Open()

void Win32Vif::Close()
{
    ProtoChannel::Close();
    if (INVALID_HANDLE_VALUE != tap_handle)
    {
        CloseHandle(tap_handle);
        tap_handle = INVALID_HANDLE_VALUE;
    }
    if (INVALID_HANDLE_VALUE != input_handle)
    {
        CloseHandle(input_handle);
        input_handle = INVALID_HANDLE_VALUE;
    }
    if (INVALID_HANDLE_VALUE != output_handle)
    {
        CloseHandle(output_handle);
        output_handle = INVALID_HANDLE_VALUE;
    }
    output_ready = input_ready = false;

}  // end Win32Vif::Close()

bool Win32Vif::Write(const char* buffer, unsigned int numBytes) 
{
    // ljt is this quite right??

    DWORD bytesWritten = 0;
    OVERLAPPED overlappedPtr;

    // set up overlapped structure fields
    overlappedPtr.Offset     = 0; 
    overlappedPtr.OffsetHigh = 0; 
    overlappedPtr.hEvent     = NULL; 

    const char* bufPtr = buffer;
    
    if (NULL != GetNotifier())
    {
        if (!output_ready)
        {
            if (FALSE != GetOverlappedResult(tap_handle,&write_overlapped,&bytesWritten,FALSE))
            {
                output_ready = true;
                overlappedPtr = write_overlapped;
                bufPtr = write_buffer;
                if (numBytes > BUFFER_MAX) numBytes = BUFFER_MAX;
                memcpy(write_buffer,buffer,numBytes);
            }
            else
            {
                if (GetDebugLevel() >= 2)
                    PLOG(PL_ERROR,"Win32Vif::Write() GetOverlappedResult() error: %s\n",::GetErrorString());
                return false;
            }
        }
    }

    if (0 != WriteFile(tap_handle,bufPtr,numBytes,&bytesWritten,(LPOVERLAPPED)&overlappedPtr))
    {
        numBytes = bytesWritten;
        if (0 == numBytes) 
        {
          output_ready = false;
        }
        return true;
    }
    else
    {
        numBytes = 0;
        switch (GetLastError())
        {
        case ERROR_IO_PENDING:
          return true;
        case ERROR_BROKEN_PIPE:
          OnNotify(NOTIFY_NONE);
          break;
        default:
          PLOG(PL_ERROR,"Win32Vif::Write() WriteFile() error(%d): %s\n",GetLastError(),::GetErrorString());
          break;
        }
        return false;
    }
    
    return true;
}  // end Win32Vif::Write()

bool Win32Vif::Read(char* buffer, unsigned int& numBytes)  
{

    DWORD bytesRead = 0;
    unsigned int len = 0;
    if (NULL != GetNotifier())
    {
        if (!input_ready)
        {
            // We had a pending overlapped read operation
            if (FALSE != GetOverlappedResult(tap_handle,&read_overlapped,&bytesRead,FALSE))
            {
                numBytes = read_count = bytesRead;
                memcpy(buffer,read_buffer,bytesRead);
                input_ready = true;
            }
            else
            {
                numBytes = 0;
                switch (GetLastError())
                {
                case ERROR_BROKEN_PIPE:
                  OnNotify(NOTIFY_NONE);
                  return false;
                default:
                  PLOG(PL_ERROR,"Win32Vif::Read() GetOverlappedResult() error (%d): %s\n",GetLastError(), ::GetErrorString());
                  return false;
                }                
            }
        }  
    }
    // Do we have any data in our "read_buffer"? 
    if (read_count > 0)
    {
        memcpy (buffer,read_buffer,read_count);
        numBytes = read_count;
        read_count = 0;
        return true;
    }
    
    // Read more as needed, triggering overlapped I/O
    memset(&read_overlapped,0,sizeof(read_overlapped));
    read_overlapped.hEvent = input_handle;
    memset(&write_overlapped,0,sizeof(write_overlapped));
    write_overlapped.hEvent = output_handle; // ljt need me?

    bytesRead = 0;
    len = BUFFER_MAX;   
    if (0 != ReadFile(tap_handle,read_buffer,len,&bytesRead,&read_overlapped))
    {
        memcpy(buffer,read_buffer,bytesRead);
        numBytes = bytesRead;
        input_ready = true;
    }
    else
    {
        switch(GetLastError())
        {
        case ERROR_IO_PENDING:
          read_count = 0;
          input_ready = false;
          break;
        case ERROR_BROKEN_PIPE: 
          if (0 == bytesRead)
          {
              OnNotify(NOTIFY_NONE);
              return false;
          }
          break;
        default:
          PLOG(PL_ERROR,"Win32Vif::Read() error: %s\n", ::GetErrorString());
          if (0 == bytesRead) return false;
          break;
        }
    }

    numBytes = bytesRead;   
    return true;
}  // end Win32Vif::Read()

bool Win32Vif::FindIPAddr(ProtoAddress vifAddr)
{
    // Iterate through addresses looking for an address match
    ULONG bufferSize = 0;
    ULONG index = 0;
    if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
    {
        char* tableBuffer = new char[bufferSize];
        if (NULL == tableBuffer)
        {   
            PLOG(PL_ERROR, "ProtoSocket::GetInterfaceName() new tableBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
        if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
        {
            for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
            {
                MIB_IPADDRROW* entry = &(addrTable->table[i]);
                ProtoAddress tempAddress;
                tempAddress.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr, 4);
                if (tempAddress.HostIsEqual(vifAddr))
                {
                    return true; // ljt fix me
                    MIB_IFROW ifEntry;  
                    index = entry->dwIndex;
                    if (NO_ERROR != GetIfEntry(&ifEntry))
                    {   
                        PLOG(PL_ERROR, "ProtoSocket::GetInterfaceName() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                        return false;
                    }
                    delete[] tableBuffer;
                    break;
                }
            }
        }
        else
        {
            PLOG(PL_WARN, "ProtoSocket::GetInterfaceName(%s) warning GetIpAddrTable() error: %s\n", vifAddr.GetHostString(), GetErrorString());
        }
        delete[] tableBuffer;
    }

    if (index)
    {
        // we found one - add another address
        // ljt need to check if this is a vif etc
        // maybe don't do this at all? just fail
        // if we've found one?

        ULONG status = TRUE;
        UINT tmpIPAddr;
        UINT tmpIPMask;
        ULONG NTEContext = 0;
        ULONG NTEInstance = 0;
        tmpIPAddr = inet_addr(vifAddr.GetHostString());
        tmpIPMask = inet_addr("255.255.255.0");
        if ((status = AddIPAddress(tmpIPAddr,
                                   tmpIPMask,
                                   index,
                                   &NTEContext,
                                   &NTEInstance)) != NO_ERROR)
        {
            PLOG(PL_ERROR,"Win32Vif::Open() AddIPAddress call failed with %d\n",status);
            return false;
        }
        return true;
    }
    return false;
} // end Win32Vif::FindIPAddr()
