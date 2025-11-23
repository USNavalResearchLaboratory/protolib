/**
* @file protoPipe.cpp
* 
* @brief Provides a cross-platform interprocess communication mechanism for 
*  Protolib using Unix-domain sockets (UNIX) or similar locally bound sockets mechanisms (WIN32). 
*  This class extends the "ProtoSocket" class to support "LOCAL" domain interprocess communications
*/
#include "protoPipe.h"
#include "protoDebug.h"

#ifdef WIN32
#include <winreg.h>
#include <tchar.h>
#else
#include <unistd.h>  // for unlink(), close()
#include <sys/un.h>  // for unix domain sockets
#include <sys/stat.h>
#include <stdlib.h>  // for mkstemp()
#ifdef NO_SCM_RIGHTS
#undef SCM_RIGHTS
#endif  // NO_SCM_RIGHTS
#define CLI_PERM    S_IRWXU
#endif // if/else WIN32

ProtoPipe::ProtoPipe(Type theType)
 : ProtoSocket((MESSAGE == theType) ? UDP : TCP),
#ifdef WIN32
   named_event_handle(INVALID_HANDLE_VALUE)
#else  // UNIX
   unlink_tried(false)
#endif // if/else WIN32/UNIX
{
    domain = LOCAL;
    path[0] = '\0';
}

ProtoPipe::~ProtoPipe()
{
    Close();
}

#ifdef WIN32

void ProtoPipe::Close()
{
    if (IsOpen())
    {
        ProtoSocket::Close();
        // Remove registry entry if we're a listener
        if ('\0' != path[0])
        {
#ifdef _UNICODE
            wchar_t wideBuffer[PATH_MAX];
            mbstowcs(wideBuffer, path, strlen(path)+1);
            LPCTSTR namePtr = wideBuffer;
#else
            LPCSTR namePtr = path;
#endif // if/else _UNICODE
            HKEY hKey;
	        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                             _T("Software\\Protokit"), 
                                             0, KEY_SET_VALUE, &hKey))
	        {
                if (ERROR_SUCCESS != RegDeleteValue(hKey, namePtr))
                    PLOG(PL_ERROR, "ProtoPipe::Close() RegDeleteValue() error: %s\n", ::GetErrorString());
                RegCloseKey(hKey);
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPipe::Close() RegOpenKeyEx() error: %s\n", ::GetErrorString());
            }   
        }
        if (INVALID_HANDLE_VALUE != named_event_handle)
        {
            CloseHandle(named_event_handle);
            named_event_handle = INVALID_HANDLE_VALUE;
        }
    }  // end if IsOpen()
}  // end ProtoPipe::Close()

/**
 * Setup event handles and overlapped structures. Set up pipes.
 * Start overlapped notifications and initiate overlapped I/O.  
 */

bool ProtoPipe::Listen(const char* theName)
{
    // This "pipe" implementation uses the WIN32 registry to find "local" sockets
    // which are used to provide interprocess "pipe" connectivity
    if (NULL != named_event_handle) CloseHandle(named_event_handle);

    // 1) Try to open named event for "theName"
    char pipeName[MAX_PATH];
    strcpy(pipeName, "Global\\protoPipe-");
    strncat(pipeName, theName, MAX_PATH - strlen(pipeName));
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, pipeName, strlen(pipeName)+1);
    LPCTSTR namePtr = wideBuffer;
#else
    LPCTSTR namePtr = pipeName;
#endif // if/else _UNICODE
	named_event_handle = CreateEvent(NULL, TRUE, TRUE, namePtr);
    if (NULL == named_event_handle)
    {
        named_event_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR, "ProtoPipe::Listen() CreateEvent() error: %s\n", ::GetErrorString());
        return false;
    }
    else if (ERROR_ALREADY_EXISTS == GetLastError())
    {
        PLOG(PL_ERROR, "ProtoPipe::Listen() error: pipe already exists\n");
        Close();
        return false;
    }

    // 2) Open/Listen
    if (ProtoSocket::Listen())
    {
        // Make a registry entry so "connecting" pipes can find the port number
        HKEY hKey;
	    DWORD dwAction;
		long theError = 0;

	    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE,
									        _T("Software\\Protokit"),
									        0L,
									        NULL,
									        REG_OPTION_NON_VOLATILE,
									        KEY_ALL_ACCESS,
									        NULL,
									        &hKey,
									        &dwAction))
	    {
#ifdef _UNICODE
            mbstowcs(wideBuffer, theName, strlen(theName)+1);
            namePtr = wideBuffer;
#else
            namePtr = theName;
#endif // if/else _UNICODE;
            DWORD thePort = (DWORD)ProtoSocket::GetPort();
            if (ERROR_SUCCESS != RegSetValueEx(hKey, namePtr, 0L, REG_DWORD, 
                                               (BYTE*)&thePort, sizeof(DWORD)))
			{
				PLOG(PL_ERROR, "ProtoPipe::Listen() RegSetValueEx() error: %s\n", ::GetErrorString());
                RegCloseKey(hKey);
                Close();
                return false;
			}
            RegCloseKey(hKey);
        }
        else
		{
            PLOG(PL_ERROR, "ProtoPipe::Listen() RegCreateKeyEx() error: %s (Must be run as administrator)\n", ::GetErrorString());
            Close();
            return false;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPipe::Listen() error listening to socket\n");
        Close();
        return false;
    }
    // Save our named event path name
    strncpy(path, theName, PATH_MAX);
    return true;
}  // ProtoPipe::Listen()

bool ProtoPipe::Accept(ProtoPipe* thePipe)
{
    return ProtoSocket::Accept(static_cast<ProtoSocket*>(thePipe));
}  // end ProtoPipe::Accept()


bool ProtoPipe::Connect(const char* theName)
{
    // 1) Open the named event given the pipe name
    // This "pipe" implementation uses the registry to find "local" sockets
    // which are used to provide interprocess "pipe" connectivity
    if (INVALID_HANDLE_VALUE != named_event_handle) CloseHandle(named_event_handle);

    // 1) Try to open name event of using given name
    char pipeName[MAX_PATH];
    strcpy(pipeName, "Global\\protoPipe-");
    strncat(pipeName, theName, MAX_PATH - strlen(pipeName));
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, pipeName, strlen(pipeName)+1);
    LPCTSTR namePtr = wideBuffer;
#else
    LPCTSTR namePtr = pipeName;
#endif // if/else _UNICODE

    named_event_handle = OpenEvent(EVENT_ALL_ACCESS, FALSE, namePtr);
    if (NULL != named_event_handle)
    {
        CloseHandle(named_event_handle);
        named_event_handle = INVALID_HANDLE_VALUE;
#ifdef _UNICODE
        mbstowcs(wideBuffer, theName, strlen(theName)+1);
        namePtr = wideBuffer;
#else
        namePtr = theName;
#endif // if/else _UNICODE
        
        // Now find the port number in the registry
        DWORD thePort = 0;
        HKEY hKey;
	    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                         _T("Software\\Protokit"), 
                                         0, KEY_READ, &hKey))
	    {
            DWORD dwType;
            DWORD dwLen = sizeof(DWORD);
            if (ERROR_SUCCESS == RegQueryValueEx(hKey, namePtr, NULL, &dwType, 
                                                 (BYTE*)&thePort, &dwLen))
		    {
                if (REG_DWORD != dwType)
                {
                    PLOG(PL_ERROR, "ProtoPipe::Connect() registry entry type mismatch!\n");
                    RegCloseKey(hKey);
                    Close();
                    return false;
                }
            }
            else
            {
                PLOG(PL_ERROR, "ProtoPipe::Connect() RegQueryValueEx() error: %s\n", ::GetErrorString());
                RegCloseKey(hKey);
                Close();
                return false;
            }
            RegCloseKey(hKey);
        }
        else
        {
            PLOG(PL_ERROR, "ProtoPipe::Connect() RegOpenKeyEx() error: %s\n", ::GetErrorString());
            Close();
            return false;
        }
        
        if ((thePort < 1) || (thePort > 0x0ffff))
        {
            PLOG(PL_ERROR, "ProtoPipe::Connect() error: bad port value!?\n");
            Close();
            return false;
        }

        // Now connect to loopback address for given port
        ProtoAddress pipeAddr;
        pipeAddr.ResolveFromString("127.0.0.1");
        pipeAddr.SetPort((UINT16)thePort);
        if (!ProtoSocket::Connect(pipeAddr))
        {
            PLOG(PL_ERROR, "ProtoPipe::Connect() error connecting socket!\n");
            Close();
            return false;
        }
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPipe::Connect() error (pipe not found): %s\n", ::GetErrorString());
        return false;
    }
    return true;
}  // end ProtoPipe::Connect()

#else  // UNIX
/**
 * This method opens a Unix-domain socket to serve as the ProtoPipe
 */

bool ProtoPipe::Open(const char* theName)
{
    // (TBD) use a semaphore to avoid issue of stale
    // Unix domain socket paths causing Open() to fail ???
    if (IsOpen()) Close();
    char pipeName[PATH_MAX] = {0};
    if(*theName!='/')
    {
#ifdef __ANDROID__
        strcpy(pipeName, "/data/local/tmp/");
#else
        strcpy(pipeName, "/tmp/");
#endif // if/else __ANDROID__
    }
    strncat(pipeName, theName, PATH_MAX-strlen(pipeName));
    struct sockaddr_un sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sun_family = AF_UNIX;
    strcpy(sockAddr.sun_path, pipeName);
#ifdef SCM_RIGHTS  /* 4.3BSD Reno and later */
    size_t len = sizeof(sockAddr.sun_len) + sizeof(sockAddr.sun_family) +
	          strlen(sockAddr.sun_path) + 1;
#else
    size_t len = strlen(sockAddr.sun_path) + sizeof(sockAddr.sun_family);
#endif // if/else SCM_RIGHTS    
    int socketType = (UDP == protocol) ? SOCK_DGRAM : SOCK_STREAM;      
    if ((handle = socket(AF_UNIX, socketType, 0)) < 0)
    {
        PLOG(PL_ERROR, "ProtoPipe::Open() socket() error: %s\n", GetErrorString());
        Close();
        return false;   
    }
    if (bind(handle, (struct sockaddr*)&sockAddr,  (socklen_t)len) < 0)
    {
        PLOG(PL_WARN, "ProtoPipe::Open() bind(%s) error: %s\n", pipeName, GetErrorString());
        Close();
        return false; 
    }
    state = IDLE;
    port = 0;
    if (!UpdateNotification())
    {
        PLOG(PL_ERROR, "ProtoPipe::Open() error updating notification\n");
        Close();
        return false;    
    }    
    strncpy(path, theName, PATH_MAX);
    return true;
}  // end ProtoPipe::Open(const char* theName)


void ProtoPipe::Close()
{
    if ('\0' != path[0])
    {
        Unlink(path);
        path[0] = '\0';
    } 
    ProtoSocket::Close();
}  // end ProtoPipe::Close();

void ProtoPipe::Unlink(const char* theName)
{
    char pipeName[PATH_MAX] = {0};
    if(*theName!='/')
    {
#ifdef __ANDROID__
        strcpy(pipeName, "/data/local/tmp/");
#else
        strcpy(pipeName, "/tmp/");
#endif // if/else __ANDROID__
    }
    strncat(pipeName, theName, PATH_MAX - strlen(pipeName));
    unlink(pipeName);
}  // end ProtoPipe::Unlink()

bool ProtoPipe::Listen(const char* theName)
{
    if (IsOpen()) Close();
    if (Open(theName))
    {
        if (TCP == protocol)
        {
            state = LISTENING;
            if (!UpdateNotification())
            {
                PLOG(PL_ERROR, "ProtoSocket::Listen() error updating notification\n");
                Close();
                return false;
            }
            if (listen(handle, 5) < 0)
            {
                PLOG(PL_ERROR, "ProtoSocket:Listen() listen() error: %s\n", GetErrorString());
                Close();
                return false;
            }
        }
        return true;    
    }
    else
    {
        if (Connect(theName))
        {
            Close();
            PLOG(PL_WARN, "ProtoPipe::Listen() error: name already in use\n");
            return false;
        }
        else
        {
#ifndef WIN32
            if (unlink_tried)
            {
                unlink_tried = false;
            }
            else
            {
                Unlink(theName);
                unlink_tried = true;
                if (Listen(theName))
                { 
                    unlink_tried = false;
                    return true;
                }
                unlink_tried = false;
            }
#endif       
            PLOG(PL_ERROR, "ProtoPipe::Listen() error opening pipe\n");     
        }
        return false;
    }   
}  // end ProtoPipe::Listen

bool ProtoPipe::Accept(ProtoPipe* thePipe)
{
    return ProtoSocket::Accept(static_cast<ProtoSocket*>(thePipe));   
}  // end ProtoPipe::Accept()

bool ProtoPipe::Connect(const char* theName)
{
    // (TBD) Don't "bind()" connecting sockets???
    // Open a socket as needed
    if (!IsOpen())
    {
        char pipeName[PATH_MAX];
#ifdef __ANDROID__
        strcpy(pipeName, "/data/local/tmp/protoSocketXXXXXX");
#else
        strcpy(pipeName, "/tmp/protoSocketXXXXXX");
#endif // if/else __ANDROID__
        int fd = mkstemp(pipeName); 
        if (fd < 0)
        {
            PLOG(PL_ERROR, "ProtoPipe::Connect() mkstemp() error: %s\n", GetErrorString());
            return false;
        }
        else
        {
            close(fd);
            unlink(pipeName);
        } 
        if (!Open(pipeName+5))
        {
            PLOG(PL_ERROR, "ProtoPipe::Connect() error opening local domain socket\n");
            return false;
        }
        
        // Try to make socket flush before closing
        if (TCP == protocol)
        {
            struct linger so_linger;
            so_linger.l_onoff = 1;
            so_linger.l_linger = 5000;
            if (setsockopt(handle, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0)
                PLOG(PL_ERROR, "ProtoPipe::Connect() setsockopt(SO_LINGER) error: %s\n", GetErrorString());
        }
        // I can't remember why we do this step ...
        if (chmod(pipeName, CLI_PERM) < 0)
        {
	        PLOG(PL_ERROR, "ProtoPipe::Connect(): chmod() error: %s\n", GetErrorString());
            Close();
	        return false;
        }
    }
    
    // Now try to connect to server 
    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    if(*theName!='/')
    {
#ifdef __ANDROID__
        strcpy(serverAddr.sun_path, "/data/local/tmp/");
#else
        strcpy(serverAddr.sun_path, "/tmp/");
#endif // if/else __ANDROID__
    }
    size_t pathMax = sizeof(serverAddr.sun_path);
    strncat(serverAddr.sun_path, theName, pathMax - strlen(serverAddr.sun_path));
#ifdef SCM_RIGHTS  // 4.3BSD Reno and later 
    size_t addrLen = sizeof(serverAddr.sun_len) + sizeof(serverAddr.sun_family) +
	              strlen(serverAddr.sun_path) + 1;
#else
    int addrLen = strlen(serverAddr.sun_path) + sizeof(serverAddr.sun_family);
#endif // SCM_RIGHTS
    // Make sure socket is "blocking" before connect attempt for "local socket
    ProtoPipe::Notifier* savedNotifier = notifier;
    if (NULL != savedNotifier) SetNotifier((ProtoPipe::Notifier*)NULL);   
    if (connect(handle, (struct sockaddr*)&serverAddr, (socklen_t)addrLen) < 0)
    {
	    PLOG(PL_DEBUG, "ProtoPipe::Connect(): connect() error: %s\n", GetErrorString());
	    Close();
        // Restore socket notification if applicable
        if (NULL != savedNotifier) SetNotifier(savedNotifier);
	    return false;
    }
    // Restore socket notification if applicable
    if (NULL != savedNotifier) SetNotifier(savedNotifier);
    state = CONNECTED;
    if (!UpdateNotification())
    {   
        PLOG(PL_ERROR, "ProtoPipe::Connect() error updating notification\n");
        Close();
        return false;
    } 
    return true;
}  // end ProtoPipe::Connect()


#endif // if/else WIN32/UNIX
