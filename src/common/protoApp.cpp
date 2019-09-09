/**
* @file protoApp.cpp
* 
* @brief Base class for implementing protolib-based command-line applications 
*/

#include "protoApp.h"
#include <stdio.h>

#ifndef _WIN32_WCE
#include <signal.h>
#endif // !_WIN32_WCE

ProtoApp* ProtoApp::the_app = NULL;

ProtoApp::ProtoApp()
{
    the_app = this;
}

ProtoApp::~ProtoApp()
{
}

bool ProtoApp::ProcessCommandString(const char* cmdline)
{
    char** argv = new char*[1];
    if (argv) 
    {
        if (!(argv[0] = new char[strlen("protoApp") + 1])) //dummy argument which is usually pointer to app
        {
	        delete[] argv;
	        PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
	        return false;
        }
        strcpy(argv[0], "protoApp");
    }
    else
    {
        PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
        return false;
    }
    // 2) Parse "cmdline" for additional arguments
    int argc = 1;
    const char* ptr = cmdline;
    char fieldBuffer[PATH_MAX];
    while (*ptr != '\0')
    {
        // Skip leading white space
        while (('\t' == *ptr) || (' ' == *ptr)) ptr++;
        if (1 == sscanf(ptr, "%s", fieldBuffer))
        {
            // Look for "quoted" args to group args with spaces
            if ('"' == fieldBuffer[0])
            {
                // Copy contents of quote into fieldBuffer
                ptr++;  // skip '"' character
                const char* end = strchr(ptr, '"');
                if (!end)
                {
                    PLOG(PL_ERROR, "protoApp: Error parsing command line: Unterminated quoted string\n");
                    for (int i = 0; i < argc; i++) delete[] argv[i];
                    if (argv) delete[] argv;
                    return false;
                }
                unsigned int len = (unsigned int)(end - ptr);
				len = len < (PATH_MAX - 1) ? len : PATH_MAX-1;
                memcpy(fieldBuffer, ptr, len);
                fieldBuffer[len] = '\0';
                ptr = end + 1;
            }
            else
            {
                ptr += strlen(fieldBuffer);
            }
            argc++;
            char** tempv = new char*[argc];
            if (!tempv)
            {
                argc--;
                for (int i = 0; i < argc; i++) delete[] argv[i];
                if (argv) delete[] argv;
                PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
                return false;
            }
            if (argv)
            {
                memcpy(tempv, argv, (argc-1)*sizeof(char*));
                delete[] argv;
            }
            argv = tempv;
            if (!(argv[argc-1] = new char[strlen(fieldBuffer)+1]))
            {
                for (int i = 0; i < argc; i++) delete[] argv[i];
                if (argv) delete[] argv;
                PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
                return false;
            }
            strcpy(argv[argc-1], fieldBuffer);
        }  // end if (sscanf(%s))
    }  // end while (ptr);
    bool result = ProcessCommands(argc, argv);
    for (int i = 0; i < argc; i++) delete[] argv[i];
    if (argv) delete[] argv;
    return result;
}  // end ProtoApp::ProcessCommandString()


#ifndef __rtems__

#ifdef WIN32
int PASCAL Win32ProtoMain(HINSTANCE instance, HINSTANCE prevInst, LPSTR cmdline, int cmdshow)
{
#ifdef _UNICODE
    int len = wcslen((wchar_t*) cmdline);
    len = wcstombs(cmdline, (wchar_t*)cmdline, len);
    cmdline[len] = '\0';
#endif // _UNICODE
    // Scan command line for "background" command 
    bool background = false;
    char* ptr = cmdline;
    char fieldBuffer[MAX_PATH];
    while ('\0' != *ptr)
    {
        // Skip leading white space
        while (('\t' == *ptr) || (' ' == *ptr)) ptr++;
        if (1 == sscanf(ptr, "%s", fieldBuffer))
        {
            if (!strcmp(fieldBuffer, "background"))
            {
                background = true;
                // Remove "background" from cmdline
                size_t len = strlen(ptr) + 1;
                size_t bgLen = strlen("background");
                memmove(ptr, ptr+bgLen, len-bgLen);
                break;
            }
            ptr += strlen(fieldBuffer);
        }
    }
    bool pauseForUser = true;
    if (background)
        pauseForUser = false;
    else
        OpenDebugWindow();  // open a WIN32 console
#ifdef _WIN32_WCE
    if (!background) Sleep(1000); // give our WinCE debug window time to start up
#endif // _WIN32-WCE   
    
    // Convert WIN32 cmdline into argc, argv form (skipping "background" cmd)
        // 1) Set argv[0] to dummy app name ("protoApp")
    char** argv = new char*[1];
    if (argv)
    {
        if (!(argv[0] = new char[strlen("protoApp") + 1]))
        {
            delete[] argv;
            PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
            fprintf(stderr, "Program Finished - Hit <Enter> to exit");
            getchar();
            return -1;
        }
        strcpy(argv[0], "protoApp");
    }
    else
    {
        PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
        fprintf(stderr, "Program Finished - Hit <Enter> to exit");
        getchar();
        return -1;
    }
        // 2) Parse "cmdline" for additional arguments
    int argc = 1;
    ptr = cmdline;
    while (*ptr != '\0')
    {
        // Skip leading white space
        while (('\t' == *ptr) || (' ' == *ptr)) ptr++;
        if (1 == sscanf(ptr, "%s", fieldBuffer))
        {
            // Look for "quoted" args to group args with spaces
            if ('"' == fieldBuffer[0])
            {
                // Copy contents of quote into fieldBuffer
                ptr++;  // skip '"' character
                char* end = strchr(ptr, '"');
                if (!end)
                {
                    PLOG(PL_ERROR, "protoApp: Error parsing command line: Unterminated quoted string\n");
                    for (int i = 0; i < argc; i++) delete[] argv[i];
                    if (argv) delete[] argv;
                    fprintf(stderr, "Program Finished - Hit <Enter> to exit");
                    getchar();
                    return -1;
                }
                unsigned int len = (unsigned int)(end - ptr);
                len = len < (MAX_PATH - 1) ? len : MAX_PATH-1;
                memcpy(fieldBuffer, ptr, len);
                fieldBuffer[len] = '\0';
                ptr = end + 1;
            }
            else
            {
                ptr += strlen(fieldBuffer);
            }
            
            argc++;
            char** tempv = new char*[argc];
            if (!tempv)
            {
                argc--;
                for (int i = 0; i < argc; i++) delete[] argv[i];
                if (argv) delete[] argv;
                PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
                fprintf(stderr, "Program Finished - Hit <Enter> to exit");
                getchar();
                return -1;
            }
            if (argv)
            {
                memcpy(tempv, argv, (argc-1)*sizeof(char*));
                delete[] argv;
            }
            argv = tempv;
            if (!(argv[argc-1] = new char[strlen(fieldBuffer)+1]))
            {
                for (int i = 0; i < argc; i++) delete[] argv[i];
                if (argv) delete[] argv;
                PLOG(PL_ERROR, "protoApp: memory allocation error: %s\n", GetErrorString());
                fprintf(stderr, "Program Finished - Hit <Enter> to exit");
                getchar();
                return -1;
            }
            strcpy(argv[argc-1], fieldBuffer);
        }  // end if (sscanf(%s))
    }  // end while (ptr);
    int result =  ProtoMain(argc, argv, pauseForUser);
    for (int i = 0; i < argc; i++) delete[] argv[i];
    if (argv) delete[] argv;
    return result;
}  // end Win32ProtoMain()

/**
 * @brief This WIN32 code lets us build a console app if we like
 */
int Win32ProtoMainConsole(int argc, char* argv[])
{
    bool pauseForUser = false;
#ifndef _WIN32_WCE
    HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (INVALID_HANDLE_VALUE != hConsoleOutput)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
        pauseForUser = ((csbi.dwCursorPosition.X==0) && (csbi.dwCursorPosition.Y==0));
        if ((csbi.dwSize.X<=0) || (csbi.dwSize.Y <= 0)) pauseForUser = false;
    }
    else
    {
        // We're not a "console" application, so create one
        // This could be commented out or made a command-line option
        OpenDebugWindow();
        pauseForUser = true;
    }
#endif // _WIN32_WCE
    return ProtoMain(argc, argv, pauseForUser);
}  // end Win32ProtoMainConsole()
#endif // WIN32

int ProtoMain(int argc, char* argv[], bool pauseForUser)
{
    int exitCode = 0;
    ProtoApp* theApp = ProtoApp::GetApp();

    if (!theApp)
    {
        fprintf(stderr, "protoApp: error: no app was instantiated\n");
        return -1;   
    }
    
#ifdef WIN32
    if (pauseForUser) theApp->Win32Init();
#endif  // WIN32
 
    // Pass command-line options to application instance and startup
    if (!theApp->OnStartup(argc, argv))
    {
        theApp->OnShutdown();
        //fprintf(stderr, "protoApp: Error on startup!\n");
        if (pauseForUser)
        {
#ifdef _WIN32_WCE
            MessageBox(NULL, _T("Program Finished - Hit <OK> to exit"), 
                       _T("ProtoDebug Notice"), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
#else
            fprintf(stderr, "Program Finished - Hit <Enter> to exit");
            getchar();
#endif // if/else _WIN32_WCE
        }
        return -1;  
    } 
#ifndef _WIN32_WCE
    signal(SIGTERM, ProtoApp::SignalHandler);
    signal(SIGINT,  ProtoApp::SignalHandler);
#endif // _WIN32_WCE

    exitCode = theApp->Run();

    theApp->OnShutdown();
    if (pauseForUser)
    {
#ifdef _WIN32_WCE
        MessageBox(NULL, _T("Program Finished - Hit <OK> to exit"), 
                   _T("ProtoDebug Notice"), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
#else
        fprintf(stderr, "Program Finished - Hit <Enter> to exit");
        getchar();
#endif // if/else _WIN32_WCE
    }
    //TRACE("ProtoMain() exiting with code %d...\n", exitCode);
    
#ifdef USE_PROTO_CHECK
    ProtoCheckLogAllocations(stderr);  // TBD - output to proto debug log?
#endif  // USE_PROTO_CHECK     
    
    return exitCode;  // exitCode contains "signum" causing exit
}  // end ProtoMain();

#endif // !__rtems__

#ifndef _WIN32_WCE
void ProtoApp::SignalHandler(int sigNum)
{
#ifndef __rtems__
    switch(sigNum)
    {
        case SIGTERM:
        case SIGINT:
        {
            ProtoApp* theApp = ProtoApp::GetApp();
            if (theApp)
                theApp->Stop(sigNum);  // causes theApp's main loop to exit
            break;
        }
            
        default:
            fprintf(stderr, "protoApp: Unexpected signal: %d\n", sigNum);
            break; 
    }  
#endif // !__rtems__
}  // end ProtoApp::SignalHandler()
#endif // !_WIN32_WCE
