#ifndef _PROTO_FILE
#define _PROTO_FILE

// This module defines some simple classes for manipulating files.
// Unix and Win32 platforms are supported.  Routines for iterating
// over directories are also provided.  And a file/directory list
// class is provided to manage a list of files.

#ifdef WIN32
//#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif // if/else WIN32

#ifdef _WIN32_WCE
#include <stdio.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif // if/else _WIN32_WCE

#include <ctype.h> // for "isprint()"
// required protolib files
#include "protoChannel.h"
#include "protoBitmask.h"
#include "protoCap.h"
#include "protoDefs.h"
#include "protoDebug.h"
#include "protoQueue.h"

#ifndef MIN
#define MIN(X,Y) ((X<Y)?X:Y)
#define MAX(X,Y) ((X>Y)?X:Y)
#endif // !MIN
// (TBD) Rewrite this implementation to use 
// native WIN32 APIs on that platform !!!

#ifdef _WIN32_WCE
// Here we enumerate some stuff to 
// make the ProtoFile work on WinCE
enum
{
    O_CREAT  = 0x01,
    O_TRUNC  = 0x02,    
    O_RDONLY = 0x04,
    O_WRONLY = 0x08,
    O_RDWR   = 0x10,
    O_BINARY = 0x20
};
#endif // _WIN32_WCE
        
class ProtoFile : public ProtoChannel
{
    public:
#ifdef WIN32
		typedef __int64 Offset;
#else
		typedef off_t Offset;
#endif // if/else WIN32/UNIX

        enum Type {INVALID, NORMAL, DIRECTORY};        
        ProtoFile();
        ~ProtoFile();
	    bool Open(const char* path, int theFlags = 0);
        bool Lock();
        void Unlock();
        bool Rename(const char* oldName, const char* newName);
        static bool Unlink(const char *path);
        void Close();
        bool IsOpen() const 
        {
#ifdef _WIN32_WCE
            return ((NULL != file_ptr) && ProtoChannel::IsOpen());
#else
			return ((descriptor >= 0) && ProtoChannel::IsOpen());
#endif // _WIN32_WCE
        }
        bool Read(char* buffer, unsigned int& numBytes);//numBytes going is is requested amount comming out is amount read.  Note that a return value of true with numBytes =0 means nothing was read.
        bool Readline(char* buffer, unsigned int& bufferSize);//uses bufferedRead to read in a line quickly will return false if full line is not available
        size_t Write(const char* buffer, size_t len);
        bool Seek(Offset theOffset);
		ProtoFile::Offset GetOffset() const {return (offset);}
		ProtoFile::Offset GetSize() const;
        bool Pad(Offset theOffset);  // if file size is less than theOffset, writes a byte to force filesize
        
        // static helper methods
        static ProtoFile::Type GetType(const char *path);
		static ProtoFile::Offset GetSize(const char* path);
        static time_t GetUpdateTime(const char* path);
        static bool IsLocked(const char *path);
         
        static bool Exists(const char* path)
        {
#ifdef WIN32
#ifdef _UNICODE
            wchar_t wideBuffer[MAX_PATH];
            size_t count = mbstowcs(wideBuffer, path, strlen(path)+1);
            return (0xFFFFFFFF != GetFileAttributes(wideBuffer));
#else
            return (0xFFFFFFFF != GetFileAttributes(path));
#endif // if/else _UNICODE
#else
            return (0 == access(path, F_OK));
#endif  // if/else WIN32
        }
        
        static bool IsWritable(const char* path)
        {
#ifdef WIN32
#ifdef _UNICODE
            wchar_t wideBuffer[MAX_PATH];
            size_t count = mbstowcs(wideBuffer, path, strlen(path)+1);
            DWORD attr = GetFileAttributes(wideBuffer);
#else
            DWORD attr = GetFileAttributes(path);
#endif // if/else _UNICODE
	        return ((0xFFFFFFFF == attr) ? 
                        false : (0 == (attr & FILE_ATTRIBUTE_READONLY)));
#else
            return (0 == access(path, W_OK));
#endif // if/else WIN32
        }
        
        class Path : public ProtoQueue::Item
        {
            
            // wrapper around file path string
            // with helper methods like GetBaseName(), GetDirName(), 
            // IsDirectory(), etc
            public:
                Path(const char* path) 
                    {SetPath(path);}
                ~Path() {}
                const char* GetPath() const
                    {return path_name;}
                void SetPath(const char* path)
                {
                    strncpy(path_name, path, PATH_MAX);
                    path_name[PATH_MAX] = '\0';
                }
                Type GetType() const
                    {return ProtoFile::GetType(path_name);}
                bool IsDirectory() const
                    {return (DIRECTORY == GetType());}
                
            private:
                char    path_name[PATH_MAX + 1];
        };  // end class ProtoFile::Path
        
        private:
            class Directory;  // forward declaration for the DirectoryIterator
    
    public:
        class DirectoryIterator
        {
            public:
                DirectoryIterator();
                ~DirectoryIterator();
                bool Open(const char* thePath);
                void Close();
                bool GetPath(char* pathBuffer) const;
                // "path" here _MUST_ be at least PATH_MAX long! (should use Path object instead)
                bool GetNextPath(char* path, bool includeFiles=true, bool includeDirs=true);
                bool GetNextFile(char* path)
                    {return GetNextPath(path, true, false);}
                bool GetNextDirectory(char* path)
                    {return GetNextPath(path, false, true);}
            private:
                Directory*  current;
                int         path_len;
        };  // end class ProtoFile::DirectoryIterator
        
        // linked list of Path items, with added directory tree iteration capability
        class PathList : public ProtoSimpleQueueTemplate<Path> 
        {
            public:
                bool AppendPath(const char* thePath);    
                class PathIterator 
                {
                    // This class iterates over the path list and walks any directory
                    // trees found. (Alternatively, use the PathList::Iterator to 
                    // simpy iterate over the linked list of Path items if that is 
                    // needed instead).
                    public:
                        // Use current, non-zero initTime so only files modified _after_ Init() are returned
                        // Otherwise, the first pass iteration will include _all_ files
                        PathIterator(PathList& pathList, bool updatesOnly = false, time_t initTime = 0);
                        ~PathIterator();
                        void Init(bool updatesOnly = false, time_t initTime = 0)
                        {
                            updates_only = updatesOnly;
                            big_time = initTime;
                            Reset();
                        }
                        void Reset()
                        {
                            last_time = big_time;
                            list_iterator.Reset();
                        }    
                        bool AppendPath(const char* thePath);
                        // TBD - replace all the "char*" args with Path reference args instead
                        // (and the "time_t initTime" should be a ProtoTime instead ...)
                        bool GetNextPath(char* path, bool includeFiles=true, bool includeDirs=true);
                        bool GetNextFile(char* path)
                            {return GetNextPath(path, true, false);}
                        bool GetNextDirectory(char* path)
                            {return GetNextPath(path, false, true);}
                        
                        const Path* GetCurrentPathItem() const
                            {return list_iterator.PeekPrevItem();}                        
                        
                    private:
                        Iterator            list_iterator;
                        DirectoryIterator   dir_iterator;  
                        time_t              big_time;
                        time_t              last_time;
                        bool                updates_only;
                    
                };  // end class ProtoFile::PathList::PathIterator
                
        };  // end class ProtoFile::PathList
        
        class PathTable : public ProtoIndexedQueueTemplate<Path> 
        {
            // Table of Path items, indexed by path name for quick lookup
            private:
                const char* GetKey(const Item& item) const
                    {return static_cast<const Path&>(item).GetPath();}
                unsigned int GetKeysize(const Item& item) const
                    {return ((unsigned int)strlen(static_cast<const Path&>(item).GetPath()) << 3);}
        };  // end class ProtoFile::PathTable
        
        
    private:
        bool ReadPrivate(char* buffer, unsigned int& numBytes);  // helper for Readline()
    
        // This is used by the ProtoFile::DirectoryIterator
        class Directory
        {
            public:           
                char        path[PATH_MAX+1];
                Directory*  parent;
#ifdef WIN32
                HANDLE      hSearch;
#else  // UNIX
                DIR*        dptr;
#endif  // if/else WIN32/UNIX    
                Directory(const char *thePath, Directory* theParent = NULL);
                ~Directory();
                void GetFullName(char* namePtr);
                bool Open();
                void Close();

                const char* Path() const {return path;}
                void RecursiveCatName(char* ptr);
        };  // end class ProtoFile::Directory    
            
#ifdef WIN32
#ifdef _WIN32_WCE
        FILE*           file_ptr;
#else
		int				descriptor;
#endif // if/else _WIN32_WCE
		__int64         offset;
#else  // UNIX
        off_t           offset;
#endif // if/else WIN32/UNIX
		int				flags;
        // This is used for buffered reading that enables
        // better performance for ProtoFile::Readline()
        // (TBD - allocate this only when needed or split to separate "FastReader" class?)
        enum {BUFFER_SIZE = 2048};
        char            read_buffer[BUFFER_SIZE];
        char*           read_ptr;
        unsigned int    read_count;             
};  // end class ProtoFile
        
#endif // _PROTO_FILE
