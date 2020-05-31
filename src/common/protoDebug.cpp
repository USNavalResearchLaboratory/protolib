/**
* @file protoDebug.cpp
* 
* @brief Debugging functions
*/

#include "protoDebug.h"
#include "protoPipe.h" // to support OpenDebugPipe() option

#include <stdio.h>   
#include <stdlib.h>  // for abort()
#include <stdarg.h>  // for variable args

#ifdef WIN32
#ifndef _WIN32_WCE 
#include <io.h>
#include <fcntl.h>
#endif // ! _WIN32_CE
#include <Windows.h> 
#include <tchar.h>
#endif // WIN32

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef MACOSX
#include <fcntl.h>
#endif
    
#if defined(PROTO_DEBUG) || defined(PROTO_MSG)
// Note - the static debug_level, debug_log, etc variables are 
//        now "wrapped" in function calls to avoid the 
//        "static initialization order fiasco"

/**
* @brief set default to pick up errors as well as fatal errors by default.
*/
static unsigned int DebugLevel(bool set = false, unsigned int value = PL_ERROR)
{
    static unsigned int debug_level = PL_ERROR;
    if (set) debug_level = value;
    return debug_level;
}  // end DebugLevel()

/**
* @brief log to stderr by default
*/
static FILE* DebugLog(bool set = false, FILE* value = stderr)
{
    static FILE* debug_log = stderr;
    if (set) debug_log = value;
    return debug_log;
}  // end DebugLog()

FILE* GetDebugLog()
{
    return DebugLog();
}  // end GetDebugLog()

// TBD - "wrap" these assert options and "debug_pipe" in the same way
static ProtoAssertFunction* assert_function = NULL;
static void* assert_data = NULL;

#ifndef SIMULATE
static ProtoPipe debug_pipe(ProtoPipe::MESSAGE);
#endif

void SetDebugLevel(unsigned int level)
{
    unsigned int debugLevel = DebugLevel();
#if defined(WIN32) && !defined(SIMULATE)
    FILE* debugLog = DebugLog();
    if (0 != level)
    {
        if ((0 == debugLevel) && ((stderr == debugLog) || (stdout == debugLog))) 
            OpenDebugWindow();
    }
    else
    {
        if ((0 != debugLevel) && ((stderr == debugLog) || (debugLog == stdout))) 
            CloseDebugWindow();
    }
#endif // WIN32
	DebugLevel(true, level); // this sets the underlying static "debug_level" state variable
    if (level != debugLevel)
        PLOG(PL_INFO,"ProtoDebug>SetDebugLevel: debug level changed from %d to %d\n", debugLevel, level);
}  // end SetDebugLevel()

unsigned int GetDebugLevel()
{
    return DebugLevel();
}

bool OpenDebugLog(const char *path)
{
    PLOG(PL_INFO,"ProtoDebug>OpenDebugLog: debug log is being set to \"%s\"\n",path);
#ifdef OPNET  // JPH 4/26/06
	if ('\0' == *path) return false;
#endif  // OPNET
    CloseDebugLog();
    FILE* ptr = fopen(path, "w+");
    if (ptr)
    {
#if defined(WIN32) && !defined(SIMULATE)
        FILE* debugLog = DebugLog();
        if ((0 != DebugLevel()) && ((debugLog == stdout) || (debugLog == stderr))) 
            CloseDebugWindow();
#endif // WIN32 && !SIMULATE
        DebugLog(true, ptr);
        return true;
    }
    else
    {
#if defined(WIN32) && !defined(SIMULATE)
        FILE* debugLog = DebugLog();
        if ((0 != DebugLevel()) && (debugLog != stdout) && (debugLog != stderr)) 
            OpenDebugWindow();
#endif // WIN32 && !SIMULATE
        DebugLog(true, stderr);
        PLOG(PL_ERROR, "OpenDebugLog: Error opening debug log file: %s\n", path);
        return false;
    }
}  // end OpenLogFile()

void CloseDebugLog()
{
    FILE* debugLog = DebugLog();
    if (debugLog && (debugLog != stderr) && (debugLog != stdout))
    {
        fclose(debugLog);
#if defined(WIN32) && !defined(SIMULATE)
        if (0 != DebugLevel()) OpenDebugWindow();
#endif // WIN32 && !SIMULATE
    }
#ifndef SIMULATE    
    if (debug_pipe.IsOpen()) debug_pipe.Close();
#endif // !SIMULATE
    DebugLog(true, stderr);
}  // end CloseDebugLog()


/**
* @brief log debug messages to a datagram ProtoPipe (PLOG only)
*/
bool OpenDebugPipe(const char* pipeName)
{
#ifndef SIMULATE
    if (!debug_pipe.Connect(pipeName))
    {
        PLOG(PL_ERROR, "OpenDebugPipe: error opening/connecting debug_pipe!\n");
        return false;
    }    
    return true;
#else
	return false;
#endif // if/else !SIMULATE
}  // end OpenDebugPipe()

void CloseDebugPipe()
{
#ifndef SIMULATE
    if (debug_pipe.IsOpen()) debug_pipe.Close();
#endif
}  // end CloseDebugPipe()


#if defined(WIN32) && !defined(SIMULATE)

static unsigned int console_reference_count = 0;

/**
 * @brief Alternative WIN32 debug window (useful for WinCE)
 * This is a simple window used instead of the
 * normal console (which is not available in WinCE)
 */
class ProtoDebugWindow
{
    public:
        ProtoDebugWindow();
        ~ProtoDebugWindow();

        bool IsOpen() {return (NULL != hwnd);}
        bool Create();
        void Destroy();
        void Print(const char* text, unsigned int len);

        void Popup() {if (hwnd) SetForegroundWindow(hwnd);}  // brings debug window to foreground
    
    private:
        static DWORD WINAPI RunInThread(LPVOID lpParameter);
        DWORD Run();
        static LRESULT CALLBACK MessageHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        enum {BUFFER_MAX = 8190};
        HANDLE              thread_handle;
        DWORD               thread_id;
        DWORD               parent_thread_id;
        HWND                hwnd;
        char                buffer[BUFFER_MAX+2];
        unsigned int        count;
        CRITICAL_SECTION    buffer_section;
        int                 content_height;
        double              scroll_fraction;

};  // end class ProtoDebugWindow

ProtoDebugWindow::ProtoDebugWindow()
 : thread_handle(NULL), thread_id(NULL), parent_thread_id(NULL), 
   hwnd(NULL), count(0),
   content_height(0), scroll_fraction(0.0)
{
    
}

ProtoDebugWindow::~ProtoDebugWindow()
{            
    Destroy();
}

bool ProtoDebugWindow::Create()
{
    Destroy();
    parent_thread_id = GetCurrentThreadId();
    if (!(thread_handle = CreateThread(NULL, 0, RunInThread, this, 0, &thread_id)))
    {
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoDebugWindow::Create() CreateThread() error: %s\n", GetErrorString());
        return false;
    }
}  // end ProtoDebugWindow::Create()

void ProtoDebugWindow::Destroy()
{
    parent_thread_id = NULL;
    if (NULL != thread_handle)
    {
        if (thread_id != GetCurrentThreadId())
        {
            if (hwnd) PostMessage(hwnd, WM_CLOSE, 0, 0);
            WaitForSingleObject(thread_handle, INFINITE);
            CloseHandle(thread_handle);
            thread_handle = NULL;
            thread_id = NULL;
        }
        else if (hwnd)
        {
            DestroyWindow(hwnd);
        }
    }
}  // end ProtoDebugWindow::Destroy()


DWORD WINAPI ProtoDebugWindow::RunInThread(LPVOID lpParameter)
{
    DWORD result = ((ProtoDebugWindow*)lpParameter)->Run();
    ExitThread(result);
    return result;
}

DWORD ProtoDebugWindow::Run()
{
    content_height = 0;
    scroll_fraction = 1.0;
    InitializeCriticalSection(&buffer_section);
    EnterCriticalSection(&buffer_section);
    memset(buffer, '\0', BUFFER_MAX+2);
    LeaveCriticalSection(&buffer_section);
    count = 0;

    HINSTANCE theInstance = GetModuleHandle(NULL);    
    // Register our msg_window class
    WNDCLASS cl;
    cl.style = CS_HREDRAW | CS_VREDRAW;
    cl.lpfnWndProc = MessageHandler;
    cl.cbClsExtra = 0;
    cl.cbWndExtra = 0;
    cl.hInstance = theInstance;
    cl.hIcon = NULL;
    cl.hCursor = NULL;
    cl.hbrBackground = NULL;
    cl.lpszMenuName = NULL;

    
    TCHAR moduleName[256];
    DWORD nameLen = GetModuleFileName(GetModuleHandle(NULL), moduleName, 256);
    if (0 != nameLen)
		_tcsncat(moduleName, _T(" Debug"), 256 - nameLen);
	const LPCTSTR myName = (0 != nameLen) ? moduleName : _T("ProtoDebugWindow"); // default name

    cl.lpszClassName = myName;

    if (!RegisterClass(&cl))
    {
        LPVOID lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                      FORMAT_MESSAGE_FROM_SYSTEM | 
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, GetLastError(),
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                      (LPTSTR) &lpMsgBuf, 0, NULL );
        // Display the string.
        MessageBox( NULL, (LPCTSTR)lpMsgBuf, (LPCTSTR)"Error", MB_OK | MB_ICONINFORMATION );
        // Free the buffer.
        LocalFree(lpMsgBuf);
        DeleteCriticalSection(&buffer_section);
        PLOG(PL_ERROR, "ProtoDebugWindow::Win32Init() Error registering message window class!\n");
        return GetLastError();
    }
    hwnd = CreateWindow(myName, 
                        myName,
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | 
                        WS_SIZEBOX | WS_VISIBLE | WS_VSCROLL,       
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        NULL, NULL, theInstance, this);
    if (NULL == hwnd)
    {
        Destroy();
        LPVOID lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                      FORMAT_MESSAGE_FROM_SYSTEM | 
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, GetLastError(), 0,
                     (LPTSTR)&lpMsgBuf, 0, NULL );
        // Display the string.
        MessageBox( NULL, (LPCTSTR)lpMsgBuf, (LPCTSTR)_T("Error"), MB_OK | MB_ICONINFORMATION);
        // Free the buffer.
        LocalFree(lpMsgBuf);
        UnregisterClass(cl.lpszClassName, theInstance);
        DeleteCriticalSection(&buffer_section);
        return GetLastError();
    }
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnregisterClass(cl.lpszClassName, theInstance);
    DeleteCriticalSection(&buffer_section);
    return msg.wParam;
}  // end ProtoDebugWindow::Run()



void ProtoDebugWindow::Print(const char* text, unsigned int len)
{
    ASSERT(NULL != hwnd);
    EnterCriticalSection(&buffer_section);
    if (len > BUFFER_MAX)
    {
        memcpy(buffer, text + len - BUFFER_MAX, BUFFER_MAX);
        count = BUFFER_MAX;
    }
    else
    {
        unsigned int space = BUFFER_MAX - count;
        unsigned int move = (len > space) ? len - space : 0;
        if (move) memmove(buffer, buffer+move, count - move);
        memcpy(buffer+count-move, text, len);
        count += len - move;
    }
#ifdef _UNICODE
    *((wchar_t*)(buffer+count)) = 0;
#else
    buffer[count] = '\0';
#endif // end if/else _UNICODE

    LeaveCriticalSection(&buffer_section);
    RECT rect;
    GetClientRect(hwnd, &rect);
    InvalidateRect(hwnd, &rect, FALSE);
    PostMessage(hwnd, WM_PAINT, 0, 0);
}  // end ProtoDebugWindow::Print()


LRESULT CALLBACK ProtoDebugWindow::MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) 
    {
        case WM_CREATE:
        {
            CREATESTRUCT *info = (CREATESTRUCT*)lParam;
            ProtoDebugWindow* dbg = (ProtoDebugWindow*)info->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dbg);
            RECT rect;
            GetClientRect(hwnd, &rect);
            SetScrollRange(hwnd, SB_VERT, 0, 0, FALSE);
            SetScrollPos(hwnd, SB_VERT, 0, FALSE);
            return 0;
        }

        case WM_SIZE:
        {
            ProtoDebugWindow* dbg = (ProtoDebugWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            int newHeight = HIWORD(lParam);
            if (dbg->content_height > newHeight)
            {
                SetScrollRange(hwnd, SB_VERT, 0, newHeight, TRUE);
            }
            else
            {
                SetScrollRange(hwnd, SB_VERT, 0, 0, TRUE);
                dbg->scroll_fraction = 1.0;
            }
            int pos = (int)((dbg->scroll_fraction * newHeight) + 0.5);
            SetScrollPos(hwnd, SB_VERT, pos, TRUE);
            return 0;
        }

        case WM_VSCROLL:
        {
            ProtoDebugWindow* dbg = (ProtoDebugWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            RECT rect;
            GetClientRect(hwnd, &rect);
            switch (LOWORD(wParam))
            {
                case SB_BOTTOM:
                    SetScrollPos(hwnd, SB_VERT, rect.bottom, TRUE);
                    dbg->scroll_fraction = 1.0;
                    break;
                case SB_TOP:
                    SetScrollPos(hwnd, SB_VERT, 0, TRUE);
                    dbg->scroll_fraction = 0.0;
                    break;
                case SB_THUMBPOSITION:
                case SB_THUMBTRACK:
                {
                    int pos = HIWORD(wParam);
                    SetScrollPos(hwnd, SB_VERT, pos, TRUE);
                    dbg->scroll_fraction = (double)pos / rect.bottom;
                    break;
                }
                default:
                    break;
            }
            InvalidateRect(hwnd, &rect, FALSE);
            PostMessage(hwnd, WM_PAINT, 0, 0);
            return 0;
        }

        case WM_PAINT:
        {
            ProtoDebugWindow* dbg = (ProtoDebugWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            char tempBuffer[BUFFER_MAX+2];
            EnterCriticalSection(&dbg->buffer_section);
            memcpy(tempBuffer, dbg->buffer, BUFFER_MAX+2);
            LeaveCriticalSection(&dbg->buffer_section);
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            RECT textRect = rect;
#ifndef _WIN32_WCE
            HFONT oldFont = (HFONT)SelectObject(hdc, GetStockObject(ANSI_FIXED_FONT));
#endif // !_WIN32_WCE
            HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(BLACK_PEN));
            int textHeight = 
                DrawText(hdc, (LPCTSTR)tempBuffer, -1, &textRect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
            int windowHeight = rect.bottom - rect.top;
            if (textHeight > windowHeight) 
            {
                int delta = textHeight - windowHeight;
                delta = (int)((delta * dbg->scroll_fraction) + 0.5);
                rect.top -= delta;
                bool updateScroll = dbg->content_height <= windowHeight;
                dbg->content_height = textHeight;
                SetScrollRange(hwnd, SB_VERT, 0, rect.bottom, TRUE);
            }
            DrawText(hdc, (LPCTSTR)tempBuffer, -1, &rect, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
#ifndef _WIN32_WCE
            SelectObject(hdc, oldFont);
#endif // _WIN32_WCE
            SelectObject(hdc, oldPen);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
        {
            ProtoDebugWindow* dbg = (ProtoDebugWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            dbg->hwnd = NULL;
            console_reference_count = 0;
            PostQuitMessage(0);
            if (NULL != dbg->parent_thread_id)
                PostThreadMessage(dbg->parent_thread_id, WM_QUIT, 0, 0);
            return 0;
        }

        // (TBD) We could pick up WM_CLOSE to see if it's a user click or app command

        default:
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}  // end ProtoDebugWindow::MessageHandler()

#ifdef _WIN32_WCE
static ProtoDebugWindow debug_window;
#endif // _WIN32_WCE

#endif // WIN32


/**
 * @brief Display string if statement's debug level is large enough
          This call is DEPRECATED!
 */
void ProtoDMSG(unsigned int level, const char *format, ...)
{	
    if (level <= DebugLevel())
    {
        FILE* debugLog = DebugLog();
        va_list args;
        va_start(args, format);
#ifdef _WIN32_WCE
        if (debug_window.IsOpen() && ((stderr == debugLog) || (stdout == debugLog)))
        {
            char charBuffer[2048];
            charBuffer[2047] = '\0';
            int count = _vsnprintf(charBuffer, 2047, format, args);
#ifdef _UNICODE
            wchar_t wideBuffer[2048];
            count = mbstowcs(wideBuffer, charBuffer, count);
            count *= sizeof(wchar_t);
            const char* theBuffer = (char*)wideBuffer;
#else
            const char* theBuffer = charBuffer;
#endif // if/else _UNICODE
            debug_window.Print(theBuffer, count);
        }
        else
#endif  // _WIN32_WCE
        {
            vfprintf(debugLog, format, args);
            fflush(debugLog);
        }
        va_end(args);   
    }
}  // end ProtoDMSG();


/**
 * @brief Provides a basic logging facility for Protlib that uses the typical logging levels
 * to allow different levels for logging and run-time debugging of protolib applications.  
 */
void ProtoLog(ProtoDebugLevel level, const char* format, ...)
{
    if (((unsigned int)level <= DebugLevel()) || (PL_ALWAYS == level)) 
    {
        va_list args;
        va_start(args, format);
        const char* header = "";
		switch(level) 
        {  // Print out the Logging Type before the message
            case PL_FATAL: header = "Proto Fatal: ";
                break;
			case PL_ERROR: header = "Proto Error: "; 
                break; 
			case PL_WARN: header = "Proto Warn: "; 
                break; 
			case PL_INFO: header = "Proto Info: "; 
                break; 
			case PL_DEBUG: header = "Proto Debug: "; 
                break; 
			case PL_TRACE: header = "Proto Trace: "; 
                break; 
			case PL_DETAIL: header = "Proto Detail: ";
                break; 
			case PL_MAX: header = "Proto Max: "; 
                break; 
			default:
				break;
		} 
        size_t headerLen = strlen(header);
		FILE* debugLog = DebugLog();
#ifdef _WIN32_WCE
        if (debug_window.IsOpen() && !debug_pipe.IsOpen() && ((stderr == debugLog) || (stdout == debugLog)))
        {
            char charBuffer[8192];
            charBuffer[8191] = '\0';
            int count = _vsnprintf(charBuffer, 8191, format, args);
#ifdef _UNICODE
            wchar_t wideBuffer[8192];
            count = mbstowcs(wideBuffer, charBuffer, count);
            count *= sizeof(wchar_t);
            const char* theBuffer = (char*)wideBuffer;
#else
            const char* theBuffer = charBuffer;
#endif // if/else _UNICODE
            debug_window.Print(theBuffer, count);
        }
        else 
#endif  // _WIN32_WCE
#ifndef SIMULATE
        if (debug_pipe.IsOpen())
#else
   	    if (false)//always fails for simulation
#endif // if/else n SIMULATE
        {
            char buffer[8192];
            buffer[8191] = '\0';
#ifdef _WIN32_WCE
            unsigned int count = (unsigned int)_vsnprintf(buffer, 8191, format, args) + 1;
#else
#ifdef WIN32
            strcpy(buffer, header);
            unsigned int count = (unsigned int)_vsnprintf(buffer + headerLen, 8191-headerLen, format, args) + 1;
            count += (unsigned int)headerLen;
#else
            strcpy(buffer, header);
            unsigned int count = (unsigned int)vsnprintf(buffer + headerLen, 8191-headerLen, format, args) + 1;
            count += headerLen;
#endif
#endif  // if/else _WIN32_WCE
            if (count > 8192) count = 8192;
#ifndef SIMULATE
            if (!debug_pipe.Send(buffer, count))
#else 
			if (true)
#endif //if/else n SIMULATE
            {
                // We have no recourse but to go to stderr here
                fprintf(stderr, "PLOG() error: unable to send to debug pipe!!!\n");
                vfprintf(stderr, format, args);
                fflush(stderr);
            }
        }
        else
        {
#ifdef __ANDROID__
            android_LogPriority prio;
            switch(level)
            {
                case PL_FATAL:
                    prio = ANDROID_LOG_FATAL;
                    break;
                case PL_ERROR:
                    prio = ANDROID_LOG_ERROR;
                    break;
                case PL_WARN:
                    prio = ANDROID_LOG_WARN;
                    break;
                case PL_INFO:
                    prio = ANDROID_LOG_INFO;
                    break;
                case PL_DEBUG:
                    prio = ANDROID_LOG_DEBUG;
                    break;
                case PL_TRACE:
                case PL_DETAIL: /* explicit fallthrough */
                case PL_MAX:    /* explicit fallthrough */
                case PL_ALWAYS: /* explicit fallthrough */
                    prio = ANDROID_LOG_VERBOSE;
                    break;
                default:
                    prio = ANDROID_LOG_DEFAULT;
                    break;
            }
            __android_log_vprint(prio, "protolib", format, args);
#else
            fprintf(debugLog, "%s", header);
            if (vfprintf(debugLog, format, args) < 0)
            {
                // perror() seems more resilient for some reason (at least on Mac OSX)
                perror(""); // flushes any partial printout from failed vfprintf()
                char buffer[8192];
                buffer[8191] = '\0';
                strcpy(buffer, header);
                va_end(args);
                va_start(args, format);
                int count = vsnprintf(buffer + headerLen, 8191 - headerLen, format, args);
                if ('\n' == buffer[headerLen + count - 1])
                    buffer[headerLen + count - 1] = '\0';
                perror(buffer);
                clearerr(debugLog);
            }
            fflush(debugLog);
#endif
        }
        va_end(args);   
    }

} // end ProtoLog()

// LP 11-01-05 - replaced
// #ifdef WIN32
#if defined(WIN32) && !defined(SIMULATE)
// end LP

/**
 * Open console for displaying debug messages
 */
void OpenDebugWindow()
{
	// Open a console window and redirect stderr and stdout to it
    if (0 == console_reference_count)
    {
#ifndef _WIN32_WCE
		int fdStd;
		HANDLE hStd;
		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		/* ensure references to current console are flushed and closed
		* before freeing the console. To get things set up in case we're
		* not a console application, first re-open the std streams to
		* NUL with no buffering, and close invalid file descriptors
		* 0, 1, and 2. The std streams will be redirected to the console
		* once it's created. */

		if (_get_osfhandle(0) < 0)
			_close(0);
		freopen("//./NUL", "r", stdin);
		setvbuf(stdin, NULL, _IONBF, 0);
		if (_get_osfhandle(1) < 0)
			_close(1);
		freopen("//./NUL", "w", stdout);
		setvbuf(stdout, NULL, _IONBF, 0);
		if (_get_osfhandle(2) < 0)
			_close(2);
		freopen("//./NUL", "w", stderr);
		setvbuf(stderr, NULL, _IONBF, 0);

		FreeConsole();

		if (!AllocConsole()) {
			//ERROR_WINDOW("Cannot allocate windows console!");
			return;
		}
		//SetConsoleTitle("My Nice Console");

		// set the screen buffer to be big enough to let us scroll text
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
		coninfo.dwSize.Y = 1024;
		//coninfo.dwSize.X = 100;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

		// redirect unbuffered STDIN to the console
		hStd = GetStdHandle(STD_INPUT_HANDLE);
		fdStd = _open_osfhandle((intptr_t)hStd, _O_TEXT);
		_dup2(fdStd, _fileno(stdin));
		SetStdHandle(STD_INPUT_HANDLE, (HANDLE)_get_osfhandle(_fileno(stdin)));
		_close(fdStd);

		// redirect unbuffered STDOUT to the console
		hStd = GetStdHandle(STD_OUTPUT_HANDLE);
		fdStd = _open_osfhandle((intptr_t)hStd, _O_TEXT);
		_dup2(fdStd, _fileno(stdout));
		SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)_get_osfhandle(_fileno(stdout)));
		_close(fdStd);

		// redirect unbuffered STDERR to the console
		hStd = GetStdHandle(STD_ERROR_HANDLE);
		fdStd = _open_osfhandle((intptr_t)hStd, _O_TEXT);
		_dup2(fdStd, _fileno(stderr));
		SetStdHandle(STD_ERROR_HANDLE, (HANDLE)_get_osfhandle(_fileno(stderr)));
		_close(fdStd);

		// Set Con Attributes
		//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
		//	FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
			ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
		//SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),
		//	ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
		/*
        AllocConsole();
        //int hCrt = _open_osfhandle((long) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
        //FILE* hf = _fdopen(hCrt, "w" );
        // *stdout = *hf;
		FILE* hf; 
		freopen_s(&hf, "CONOUT$", "w", stdout);
        int i = setvbuf(stdout, NULL, _IONBF, 0 );
        
        //hCrt = _open_osfhandle((long) GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
        //hf = _fdopen(hCrt, "r" );
        // *stdin = *hf;
		freopen_s(&hf, "CONIN$", "r", stdin);

        //hCrt = _open_osfhandle((long) GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
		//hf = _fdopen(hCrt, "w" );
        // *stderr = *hf;
		freopen_s(&hf, "CONERR$", "w", stderr);
        i = setvbuf(stderr, NULL, _IONBF, 0);
		*/
#endif // if !_WIN32_CE
    }
#ifdef _WIN32_WCE
    if (!debug_window.IsOpen()) debug_window.Create();
#endif // _WIN32_WCE
    console_reference_count++;
}  // end OpenDebugWindow() 

#ifndef _WIN32_WCE
static HWND GetDebugWindowHandle(void)
{
   #define MY_BUFSIZE 1024 // Buffer size for console window titles.
   HWND hwndFound;         // This is what is returned to the caller.
#ifdef _UNICODE
   wchar_t pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
                                       // WindowTitle.
   wchar_t pszOldWindowTitle[MY_BUFSIZE]; // Contains original
                                       // WindowTitle.
#else
   char pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
                                       // WindowTitle.
   char pszOldWindowTitle[MY_BUFSIZE]; // Contains original
                                       // WindowTitle.
#endif
   // Fetch current window title
   GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);
   // Format a "unique" NewWindowTitle.
   wsprintf(pszNewWindowTitle, _T("%d/%d"),
               GetTickCount(),
               GetCurrentProcessId());
   // Change current window title.
   SetConsoleTitle(pszNewWindowTitle);
   // Ensure window title has been updated.
   Sleep(40);
   // Look for NewWindowTitle.
   hwndFound=FindWindow(NULL, pszNewWindowTitle);
   // Restore original window title.
   SetConsoleTitle(pszOldWindowTitle);
   return(hwndFound);
}  // end GetDebugWindowHandle()
#endif // !WIN32_WCE

void PopupDebugWindow() 
{
#ifdef _WIN32_WCE
    if (debug_window.IsOpen())
        debug_window.Popup();
    else
        OpenDebugWindow();
#else
    HWND debugWindow = GetDebugWindowHandle();
    if (debugWindow) 
        SetForegroundWindow(debugWindow);
    else
        OpenDebugWindow();
#endif  // if/else _WIN32_WCE
}

void CloseDebugWindow()
{
	if (console_reference_count > 0)
	{
		console_reference_count--;
		if (0 == console_reference_count)
		{
#ifdef _WIN32_WCE
			debug_window.Destroy();
#else
			FreeConsole();
#endif // if/else _WIN32_CE
		}
	}
}
#endif // WIN32

#endif // PROTO_DEBUG || PROTO_MSG


#ifdef PROTO_DEBUG

void TRACE(const char *format, ...)
{
    FILE* debugLog = DebugLog();
    va_list args;
    va_start(args, format);
#ifdef _WIN32_WCE
    if (debug_window.IsOpen() && ((stderr == debugLog) || (stdout == debugLog)))
    {
        char charBuffer[2048];
        charBuffer[2047] = '\0';
        int count = _vsnprintf(charBuffer, 2047, format, args);
#ifdef _UNICODE
        wchar_t wideBuffer[2048];
        count = mbstowcs(wideBuffer, charBuffer, count);
        count *= sizeof(wchar_t);
        const char* theBuffer = (char*)wideBuffer;
#else
        const char* theBuffer = charBuffer;
#endif // if/else _UNICODE
        debug_window.Print(theBuffer, count);
    }
    else
#endif  // _WIN32_WCE
    {
#ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_ERROR, "protolib", format, args);
#else
        if (vfprintf(debugLog, format, args) < 0)
        {
            // perror() seems more resilient for some reason (at least on Mac OSX)
            perror("");
            char buffer[8192];
            buffer[8191] = '\0';
            va_end(args);
            va_start(args, format);
            int count = vsnprintf(buffer, 8191, format, args);
            if ('\n' == buffer[count - 1])
                buffer[count - 1] = '\0';
            perror(buffer);
            clearerr(debugLog);
        }
        fflush(debugLog);
        
#endif  // if/else __ANDROID__
    }
    va_end(args);
}  // end TRACE();

void PROTO_ABORT(const char *format, ...)
{
#ifndef _WIN32_WCE  // TBD add an fprintf?
    FILE* debugLog = DebugLog();
    va_list args;
    va_start(args, format);
    vfprintf(debugLog, format, args);
    fflush(debugLog);
    va_end(args); 
    abort();
#endif // !_WIN32_WCE
}

void SetAssertFunction(ProtoAssertFunction* assertFunction, void* userData) 
{
    assert_function = assertFunction;
    assert_data = userData;
}  // end SetAssertFunction()

void ClearAssertFunction()
{
    assert_function = NULL;
}  // end ClearAssertFunction()

bool HasAssertFunction() 
{
    return (NULL != assert_function);
}  // end HasAssertHandler()

void ProtoAssertHandler(bool condition, const char* conditionText, const char* fileName, int lineNumber)
{
    if (NULL != assert_function) assert_function(condition, conditionText, fileName, lineNumber, assert_data);
}  // end ProtoAssertHandler()

#endif // PROTO_DEBUG
