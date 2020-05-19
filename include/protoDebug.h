/**
* @class ProtoDebug
*
* @brief Debugging routines
*
* PROTOLIB DEBUG LEVELS:
 *
 * @li PL_FATAL=0   // The FATAL level designates very severe error events that will presumably lead the application to abort.
 * @li PL_ERROR=1;  // The ERROR level designates error events that might still allow the application to continue running.
 * @li PL_WARN=2;   // The WARN level designates potentially harmful situations.
 * @li PL_INFO=3;   // The INFO level designates informational messages that highlight the progress of the application at coarse-grained level.
 * @li PL_DEBUG=4;  // The DEBUG level designates fine-grained informational events that are most useful to debug an application.
 * @li PL_TRACE=5;  // The TRACE level designates finer-grained informational events than the DEBUG
 * @li PL_DETAIL=6; // The TRACE level designates even finer-grained informational events than the DEBUG
 * @li PL_MAX=7;    // Turn all comments on
 * @li PL_ALWAYS    // Messages at this level are always printed regardless of debug level
*/
#ifndef _PROTO_DEBUG
#define _PROTO_DEBUG
         
#include "protoDefs.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <string.h>  // for strerror()
#include <errno.h>   // for errno
#endif // if/else WIN32/UNIX

enum ProtoDebugLevel {PL_FATAL, PL_ERROR, PL_WARN, PL_INFO, PL_DEBUG, PL_TRACE, PL_DETAIL, PL_MAX, PL_ALWAYS}; 

#if defined(PROTO_DEBUG) || defined(PROTO_MSG)
void SetDebugLevel(unsigned int level);
unsigned int GetDebugLevel();

bool OpenDebugLog(const char *path);       // log debug messages to the file named "path"
void CloseDebugLog();
bool OpenDebugPipe(const char* pipeName);  // log debug messages to a datagram ProtoPipe (PLOG only)
void CloseDebugPipe();
void ProtoDMSG(unsigned int level, const char *format, ...);
void ProtoLog(ProtoDebugLevel level, const char *format, ...);
FILE* GetDebugLog();
#ifdef WIN32
void OpenDebugWindow();
void PopupDebugWindow();
void CloseDebugWindow();
#endif // WIN32
#else
inline void SetDebugLevel(unsigned int level) {}
inline unsigned int GetDebugLevel() {return PL_FATAL;}
inline bool OpenDebugLog(const char *path) {return true;}
inline void CloseDebugLog() {}
inline bool OpenDebugPipe(const char* pipeName) {return true;}
inline void CloseDebugPipe() {}
inline void ProtoDMSG(unsigned int level, const char *format, ...) {}
inline void ProtoLog(ProtoDebugLevel level, const char *format, ...) {}
#ifdef WIN32
inline void OpenDebugWindow() {}
inline void PopupDebugWindow() {}
inline void CloseDebugWindow() {}
#endif // WIN32
#endif // if/else PROTO_DEBUG || PROTO_MSG

// printf()-like debug logging routines (Note DMSG() is  deprecated)
// Call the PLOG() macro for better performance
inline void ProtoNoop() {}
#define PLOG(X, ...) ((((unsigned int)X <= ::GetDebugLevel()) || (X == PL_ALWAYS)) ? \
                            ProtoLog(X, ##__VA_ARGS__) : ProtoNoop())
#define DMSG(X, ...) ((unsigned int)X <= ::GetDebugLevel() ? ProtoDMSG(X, ##__VA_ARGS__) : ProtoNoop())

#if defined(PROTO_DEBUG) || defined(PROTO_MSG)

// The following prototype and "SetAssertFunction()" allows the behavior of the PROTO_ASSERT macro
// to be overridden.  The default behavior is just a call to the "assert()" system call (or ABORT() if no "assert()" call is available)
// IMPORTANT NOTE:  The "NDEBUG" macro can be enabled to disable the "assert()" system call even 
//                  if the "PROTO_DEBUG" is kept in place.

typedef void (ProtoAssertFunction)(bool condition, const char* conditionText, const char* fileName, int lineNumber, void* userData);
void SetAssertFunction(ProtoAssertFunction* assertFunction, void* userData = NULL); 
void ClearAssertFunction();
bool HasAssertFunction();
void ProtoAssertHandler(bool condition, const char* conditionText, const char* fileName, int lineNumber);

void PROTO_ABORT(const char *format, ...);

#ifdef HAVE_ASSERT
#ifdef __ANDROID__
#include <android/log.h>
#define PROTO_ASSERT(X) {if (HasAssertFunction()) ProtoAssertHandler(X, #X, __FILE__, __LINE__); \
else if (!((bool)(X)))  __android_log_assert(#X, "protolib", "%s line:%d", __FILE__, __LINE__);}
#else
#include <assert.h>
#define PROTO_ASSERT(X) {if (HasAssertFunction()) ProtoAssertHandler(X, #X, __FILE__, __LINE__); else assert(X);}
#endif // if/else __ANDROID__
#else
#define PROTO_ASSERT(X) \
    {if (HasAssertFunction()) ProtoAssertHandler(X, #X, __FILE__, __LINE__); \
    else if (!((bool)(X))) PROTO_ABORT("ASSERT(%s) failed at line %d in source file \"%s\"\n", #X, __LINE__, __FILE__);}
#endif // if/else HAVE_ASSERT


#ifdef TRACE
#undef TRACE
#endif // TRACE
void TRACE(const char *format, ...);

#else  // !PROTO_DEBUG

#define PROTO_ASSERT(X)
#ifndef ABORT
#define ABORT(X)
#endif // !ABORT
#ifndef TRACE
inline void TRACE(const char *format, ...) {}
#endif // !TRACE

#endif // if/else PROTO_DEBUG

// Historically, "protolib" has used an "ASSERT()" macro for assertions.  It now makes sense to 
// transition to a more explicit "PROTO_ASSERT()" - we should/will probably deprecate this
// "ASSERT()" macro definition but for now it is in many projects that use Protolib
#undef ASSERT
#define ASSERT(X) PROTO_ASSERT(X)

inline const char* GetErrorString()
{
#ifdef WIN32
    static char errorString[256];
    errorString[255] = '\0';
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                  (LPTSTR) errorString, 255, NULL);
    return errorString;
#else
    return strerror(errno);
#endif // if/else WIN32/UNIX
}


#ifdef WIN32
typedef DWORD ProtoErrorCode;
#else
typedef int ProtoErrorCode;
#endif  // if/else WIN32/UNIX

inline const char* GetErrorString(ProtoErrorCode errorCode)
{
#ifdef WIN32
    static char errorString[256];
    errorString[255] = '\0';
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  errorCode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                  (LPTSTR) errorString, 255, NULL);
    return errorString;
#else
    return strerror(errorCode);
#endif // if/else WIN32/UNIX
}

#endif // _PROTO_DEBUG
