
#ifndef _PROTO_CHECK
#define _PROTO_CHECK

// This module provides compile-time optional "new" and "delete" operators
// for Protolib (or any code that #include "protoDefs.h" or this).  These
// operator keep track of current memory allocations (of the new operator)
// and which source code file and line number from which the allocation was
// made.  This can be useful for debugging purposes and should only be 
// enabled for such purposes as there is a performance penalty when this
// is enabled.

// At compile time, use -DUSE_PROTO_CHECK (or uncomment the line below)
// that defines that macro.

// The current memory allocations can be "logged" via a call to
// ProtoCheckLogAllocations().  In the future some additional memory
// usage statistics (e.g. total size of allocations, peak, etc)
// might be added.  This is _not_ a replacement for other tools such
// as "valgrind", etc but useful for Protolib-based code.

#include <stdio.h>  // for FILE*

// UNCOMMENT this next line to enable "ProtoCheck" (or use -DUSE_PROTO_CHECK for your compiler)
//#define USE_PROTO_CHECK

#ifdef USE_PROTO_CHECK

#include <new>

#ifndef _PROTO_CHECK_IMPL

void* operator new(size_t size, const char* file, int line);

void* operator new[](size_t size, const char* file, int line);

void operator delete(void* p) throw();

void operator delete[](void *p) throw();

#define new new(__FILE__, __LINE__)

// This trick is used to log delete operator file/line 
// info just before the call to delete is made.

void ProtoCheckCacheInfo(const char* file, int line);
//void ProtoCheckDelete(void* p, const char* file, int line);
        
#define delete ProtoCheckCacheInfo(__FILE__, __LINE__), delete

#endif  // !_PROTO_CHECK_IMPL

#endif // USE_PROTO_CHECK

void ProtoCheckResetLogging();
void ProtoCheckLogAllocations(FILE* filePtr);

#endif // _PROTO_CHECK
