#include "protoVif.h"
#include "protoNet.h"
#include "protoDebug.h"
#include "windows.h"
#include <Iphlpapi.h>
#include <stdio.h>
#include <NetCon.h>

#include <winioctl.h>
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

  bool InitTap(const char* vifName, const ProtoAddress& vifAddr, unsigned int maskLen);   
  bool ConfigureTap(char* adapterid, const char* vifName, const ProtoAddress& vifAddr,unsigned int maskLen);
  bool FindIPAddr(ProtoAddress vifAddr);
  bool Open(const char* vifName, const ProtoAddress& vifAddr, unsigned int maskLen);
  void Close();
  bool SetHardwareAddress(const ProtoAddress& ethAddr);
  void SetMAC(char * AdapterName, char * NewMAC);
  void ResetAdapter(char * AdapterName);
  bool IsValidMAC(char * str);
  bool SetARP(bool status);
  bool Write(const char* buffer, unsigned int numBytes);
  
  bool Read(char* buffer, unsigned int& numBytes);

private:

#ifndef _WIN32_WCE
  // These members facilitate Win32 overlapped I/O
  // which we use for async I/O
  HANDLE        tap_handle;
  char          adapter_id[1024];
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
bool Win32Vif::ConfigureTap(char* adapterid,const char* vifName,const ProtoAddress& vifAddr,unsigned int maskLen)
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
	char cmd[256];
	if (vifAddr.IsValid())
	{

		/* Configure the interface */
		char subnetMask[16];

		ProtoAddress subnetMaskAddr;
		subnetMaskAddr.ResolveFromString("255.255.255.255");

		ProtoAddress tmpAddr;
		subnetMaskAddr.GetSubnetAddress(maskLen, tmpAddr);
		sprintf(subnetMask, " %s ", tmpAddr.GetHostString());

		sprintf(cmd,
			"netsh interface ip set address \"%s\" static %s %s ",
			nameData, vifAddr.GetHostString(), subnetMask);
		PLOG(PL_INFO, "cmd>%s \n", cmd);
		if (LONG ret = system(cmd))
		{
			PLOG(PL_ERROR, "Win32Vif::ConfigureTap(%s) Open failed. netsh call returned: %u", nameData, ret);
			return false;
		}
	}
	else
		PLOG(PL_INFO, "Win32Vif::ConfigureTap() no valid ip addr provided. Replace mode?\n");

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
		sprintf(vif_name, "%s", vifName);
    }
    PLOG(PL_INFO,"Win32Vif::ConfigureTap() Tap configured on interface: %s\n",vifName);

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
		len = sizeof(adapter_id);
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
		
		if (tap_handle == INVALID_HANDLE_VALUE)
		{
		//PLOG(PL_INFO, "error>%d tapName>%s\n", GetLastError(),tapname);
		/* TODO: Format windows error message - put in function
		LPVOID lpMsgBuf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&lpMsgBuf, 0, NULL);
		// Display the string.
		MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCTSTR)"Error", MB_OK | MB_ICONINFORMATION);
		*/
		}
		
		if (tap_handle != INVALID_HANDLE_VALUE) 
        {
            break;
		}
	}
    
    RegCloseKey(key);
	PLOG(PL_INFO,"Win32Vif::InitTap() found tap adapter %s\n",tapname);

    if (!ConfigureTap(adapterid,vifName,vifAddr,maskLen))
        return false;

	// Squirrel away our adapter id for now - we neet it to set hardware 
	// info.  TODO: develop functions to get adapter id from vif name?
	// NOTE: Set hardware address is not currently working under windows 7
	// remove this code?
	sprintf(adapter_id, "%s", adapterid);

    if (NULL == (input_handle = CreateEvent(NULL,TRUE,FALSE,NULL)))
    {
        input_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR,"Win32Vif::InitTap() CreateEvent(input_handle) error: %s\n", ::GetErrorString());
        Close(); 
        return false;
    }
    if (NULL == (output_handle = CreateEvent(NULL,TRUE,FALSE,NULL)))
    {
        output_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR,"Win32Vif::InitTap() CreateEvent(output_handle) error: %s\n",::GetErrorString());
        Close(); 
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
    
    // Snag the virtual interface hardware address
    if (!ProtoNet::GetInterfaceAddress(vifName, ProtoAddress::ETH, hw_addr))
        PLOG(PL_ERROR, "Win32Vif::Open(%s) error: unable to get ETH address!\n", vifName);
    
	PLOG(PL_INFO,"Win32Vif::Open() vif>%s mac>%s\n", vifName, hw_addr.GetHostString());

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
            PLOG(PL_ERROR, "Win32Vif::FindIPAddr() new tableBuffer error: %s\n", ::GetErrorString());
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
                        PLOG(PL_ERROR, "Win32Vif::FindIPAddr() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                        return false;
                    }
                    delete[] tableBuffer;
                    break;
                }
            }
        }
        else
        {
            PLOG(PL_WARN, "Win32Vif::FindIPAddr(%s) warning GetIpAddrTable() error: %s\n", vifAddr.GetHostString(), GetErrorString());
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
            PLOG(PL_ERROR,"Win32Vif::FindIPAddr() AddIPAddress call failed with %d\n",status);
            return false;
        }
        return true;
    }
    return false;
} // end Win32Vif::FindIPAddr()

void Win32Vif::SetMAC(char * AdapterName, char * NewMAC) 
{
	/* NOTE: Under development do not use.*/
	PLOG(PL_ERROR, "Win32Vif::SetMAC() Unable to set vif mac address.  Not currently working on windows.\n");
	return;

	ULONG status;
	//HKEY key;
	DWORD nameType;
	const char nameString[] = "Name";
	char regpath[1024];
	char nameData[1024];
	long len = sizeof(nameData);

	HKEY hListKey = NULL;
	HKEY hKey = NULL;
	
	// TODO: Use our definitions
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY,
		0, KEY_READ, &hListKey);
	if (!hListKey) {
		PLOG(PL_ERROR,"Win32Vif::SetMac() Failed to open adapter list key\n");
		return;
	}
	FILETIME writtenTime;
	char keyNameBuf[512], keyNameBuf2[512];
	DWORD keyNameBufSiz = 512;
	DWORD crap;
	int i = 0;
	bool found = false;
	while (RegEnumKeyEx(hListKey, i++, keyNameBuf, &keyNameBufSiz, 0, NULL, NULL, &writtenTime)
		== ERROR_SUCCESS) {
		_snprintf(keyNameBuf2, 512, "%s\\Connection", keyNameBuf);

		hKey = NULL;
		RegOpenKeyEx(hListKey, keyNameBuf2, 0, KEY_READ, &hKey);
		if (hKey) {
			keyNameBufSiz = 512;
			if (RegQueryValueEx(hKey, "Name", 0, &crap, (LPBYTE)keyNameBuf2, &keyNameBufSiz)
				== ERROR_SUCCESS && strcmp(keyNameBuf, AdapterName) == 0) {
				PLOG(PL_INFO,"Win32Vif::SetMac() Found Adapter ID is %s\n", keyNameBuf);
				found = true;
				break;

			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);
	if (!found) {
		PLOG(PL_ERROR,"Win32Vif::SetMac() Could not find adapter name '%s'.\nPlease make sure this is the name you gave it in Network Connections.\n", AdapterName);
		return;
	}

	RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY,
		0, KEY_READ, &hListKey);
	if (!hListKey) {
		PLOG(PL_ERROR,"Win32Vif::SetMac() Failed to open adapter list key in Phase 2\n");
		return;
	}
	i = 0;
	char buf[512];
	while (RegEnumKeyEx(hListKey, i++, keyNameBuf2, &keyNameBufSiz, 0, NULL, NULL, &writtenTime)
		== ERROR_SUCCESS) {
		hKey = NULL;
		RegOpenKeyEx(hListKey, keyNameBuf2, 0, KEY_READ | KEY_SET_VALUE, &hKey);
		if (hKey) {
			keyNameBufSiz = 512;
			//PLOG(PL_INFO,"Win32Vif::SetMac() phase2>%s\n", buf);
			if ((RegQueryValueEx(hKey, "NetCfgInstanceId", 0, &crap, (LPBYTE)buf, &keyNameBufSiz)
				== ERROR_SUCCESS) ) { //&& (strcmp(buf, keyNameBuf) == 0)) {

				if (strcmp(buf, keyNameBuf) != 0)
				{
					//PLOG(PL_INFO,"Win32Vif::SetMac() not equal buf>%s keynamebuf>%s\n", buf, keyNameBuf);
					continue;
				}

				char mac[60] = "";
				unsigned long macsize = sizeof(mac);
				DWORD type;
				char tmpMAC[60] = "";
				if (RegQueryValueEx(hKey, "NetworkAddress", 0, &type, (LPBYTE)tmpMAC, &macsize)
					== ERROR_SUCCESS)
					PLOG(PL_INFO,"Win32Vif::SetMac() interface original mac> %s \n", tmpMAC);


				if (RegSetValueEx(hKey, "NetworkAddress", 0, REG_SZ, (LPBYTE)NewMAC, strlen(NewMAC) + 1) 
					== ERROR_SUCCESS)
					PLOG(PL_INFO,"Win32Vif::SetMac() Updating adapter index %s (%s=%s) newMac>%s \n", keyNameBuf2, buf, keyNameBuf, NewMAC);
				else
				{
					
					PLOG(PL_ERROR, "Win32Vif::SetMac() set mac error>%d tapName>%s\n", GetLastError(),keyNameBuf);

					LPVOID lpMsgBuf;
					FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, GetLastError(),
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					(LPTSTR)&lpMsgBuf, 0, NULL);
					// Display the string.
					MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCTSTR)"Error", MB_OK | MB_ICONINFORMATION);
					
				}
				if (RegQueryValueEx(hKey, "NetworkAddress", 0, &type, (LPBYTE)NewMAC, &macsize)
					== ERROR_SUCCESS)
					PLOG(PL_INFO,"Win32Vif::SetMac() querying reset interface mac> %s \n", NewMAC);

				break;
			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);

}
bool Win32Vif::IsValidMAC(char * str) {

	PLOG(PL_INFO, "Win32Vif::IsValidMAC() Not implemented\n");
	return true;

	// stub
	if (strlen(str) != 18) return false;
	for (int i = 0; i < 18; i++) {
		if (str[i] == '-')
			continue;
		if ((str[i] < '0' || str[i] > '9')
			&& (str[i] < 'a' || str[i] > 'f')
			&& (str[i] < 'A' || str[i] > 'F')) {
			return false;
		}
	}
	return true;
}

bool Win32Vif::SetHardwareAddress(const ProtoAddress& ethAddr)
{
	/* NOTE: Under development do not use.*/
	PLOG(PL_ERROR, "Win32Vif::SetHardwareAddress() Unable to set vif hardware address.  Not currently working on windows.\n");
	return true;

	// LJT: I made a stab at setting this but setting the macAddr
	// doesn't work.  It gets set in the registry but never assigned to the 
	// actual vif.  Taking too much time when usefuleness is unknown.
	// leaving code as a hint as a potential technique
	PLOG(PL_INFO, "Win32Vif::SetHardwareAddress(mac) value: %s\n", ethAddr.GetHostString());

	if (ProtoAddress::ETH != ethAddr.GetType())
	{
		PLOG(PL_ERROR, "Win32Vif::SetHardwareAddress() error: invalid address type!\n");
		return false;
	}
	const UINT8* addr = (const UINT8*)ethAddr.GetRawHostAddress();
	char mac[64];

	sprintf(mac, " %02x-%02x-%02x-%02x-%02x-%02x",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	// IsValidMAC is not fully implemented
	if (IsValidMAC(mac))
	{
		SetMAC(adapter_id, mac);
		ResetAdapter(vif_name);
	}

	return true;
}  // end Win32Vif::SetHardwareAddress()

bool Win32Vif::SetARP(bool status)
{
	PLOG(PL_ERROR, "Win32Vif::SetARP() unimplemented function on WIN32");
	return false;

	// end Win32Vif::SetARP()
}
void Win32Vif::ResetAdapter(char * AdapterName) {

	/* NOTE: Under development do not use.*/
	PLOG(PL_INFO, "Win32Vif::ResetAdapter() unimplemented function on WIN32");
	return;

	struct _GUID guid = { 0xBA126AD1, 0x2166, 0x11D1, 0 };
	memcpy(guid.Data4, "\xB1\xD0\x00\x80\x5F\xC1\x27\x0E", 8);
	//unsigned short * buf = new unsigned short[strlen(AdapterName) + 1];
	char * buf = new char[strlen(AdapterName) + 1];
	void(__stdcall *NcFreeNetConProperties) (NETCON_PROPERTIES *);
	HMODULE NetShell_Dll = LoadLibrary("Netshell.dll");
	if (!NetShell_Dll) {
		PLOG(PL_ERROR,"Win32Vif::ResetAdapter() Couldn't load Netshell.dll\n");
		return;
	}
	NcFreeNetConProperties = (void(__stdcall *)(struct tagNETCON_PROPERTIES *))GetProcAddress(NetShell_Dll, "NcFreeNetconProperties");
	if (!NcFreeNetConProperties) {
		PLOG(PL_ERROR,"Win32Vif::ResetAdapter() Couldn't load required DLL function\n");
		return;
	}

	for (unsigned int i = 0; i <= strlen(AdapterName); i++) {
		buf[i] = AdapterName[i];
	}
	CoInitialize(0);
	INetConnectionManager * pNCM = NULL;
	HRESULT hr = ::CoCreateInstance(guid,
		NULL,
		CLSCTX_ALL,
		__uuidof(INetConnectionManager),
		(void**)&pNCM);
	if (!pNCM)
		PLOG(PL_ERROR,"Win32Vif::ResetAdapter() Failed to instantiate required object\n");
	else {
		IEnumNetConnection * pENC;
		pNCM->EnumConnections(NCME_DEFAULT, &pENC);
		if (!pENC) {
			PLOG(PL_ERROR,"Win32Vif::ResetAdapater() Could not enumerate Network Connections\n");
		}
		else {
			INetConnection * pNC;
			ULONG fetched;
			NETCON_PROPERTIES * pNCP;
			do {
				pENC->Next(1, &pNC, &fetched);
				if (fetched && pNC) {
					pNC->GetProperties(&pNCP);
					if (pNCP) {
						//(0 == strncmp(interfaceName, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN))
						//if (wcscmp(pNCP->pszwName, buf) == 0) {
						// Convert to char *
						char * tmpBuf = new char[strlen(buf) + 1];
						wcstombs(tmpBuf, pNCP->pszwName, strlen(tmpBuf));
						if (0 == strncmp(buf, tmpBuf, strlen(buf)))
						{
							PLOG(PL_INFO, "Win32Vif::ResetAdapater() Resetting interface %s", tmpBuf);
							pNC->Disconnect();
							pNC->Connect();
						}
						NcFreeNetConProperties(pNCP);
					}
				}
			} while (fetched);
			pENC->Release();
		}
		pNCM->Release();
	}

	FreeLibrary(NetShell_Dll);
	CoUninitialize();
}
//TODO: Validate mac
