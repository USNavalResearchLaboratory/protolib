#include "protoFile.h"

#include <string.h>  // for strerror()
#include <stdio.h>   // for rename()
#include <time.h>  // for difftime()
#ifdef HAVE_FLOCK
#include <sys/file.h> // for flock()
#endif
#ifdef WIN32
#ifndef _WIN32_WCE
#include <errno.h>
#include <direct.h>
#include <share.h>
#include <io.h>
#endif // !_WIN32_WCE
#else
#include <unistd.h> 
#endif // if/else WIN32

#ifndef _WIN32_WCE
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif // !_WIN32_WCE

ProtoFile::ProtoFile()
#ifdef _WIN32_WCE
    : file_ptr(NULL)
#else
#endif // if/else _WIN32_WCE
{    
}

ProtoFile::~ProtoFile()
{
    if (IsOpen()) Close();
}  // end ProtoFile::~ProtoFile()

// This should be called with a full path only! (Why?)
bool ProtoFile::Open(const char* thePath, int theFlags)
{
    bool returnvalue=false;
    ASSERT(!IsOpen());	
    if (theFlags & O_CREAT)
    {
        // Create sub-directories as needed.
        char tempPath[PATH_MAX+1];
        tempPath[PATH_MAX] = '\0';
        strncpy(tempPath, thePath, PATH_MAX);
        char* ptr = strrchr(tempPath, PROTO_PATH_DELIMITER);
        if (NULL != ptr) 
        {
            *ptr = '\0';
            ptr = NULL;
            while (!ProtoFile::Exists(tempPath))
            {
                char* ptr2 = ptr;
                ptr = strrchr(tempPath, PROTO_PATH_DELIMITER);
                if (ptr2) *ptr2 = PROTO_PATH_DELIMITER;
                if (ptr)
                {
                    *ptr = '\0';
                }
                else
                {
                    ptr = tempPath;
                    break;
                }
            }
        }
        if (ptr && ('\0' == *ptr)) *ptr++ = PROTO_PATH_DELIMITER;
        while (ptr)
        {
            ptr = strchr(ptr, PROTO_PATH_DELIMITER);
            if (ptr) *ptr = '\0';
#ifdef WIN32
#ifdef _WIN32_WCE
#ifdef _UNICODE
            wchar_t wideBuffer[MAX_PATH];
            size_t pathLen = strlen(tempPath) + 1;
            if (pathLen > MAX_PATH) pathLen = MAX_PATH;
            mbstowcs(wideBuffer, tempPath, pathLen);
            if (!CreateDirectory(wideBuffer, NULL))
#else
            if (!CreateDirectory(tempPath, NULL))
#endif  // if/else _UNICODE
#else
            if (_mkdir(tempPath))
#endif // if/else _WIN32_WCE
#else
            if (mkdir(tempPath, 0755))
#endif // if/else WIN32/UNIX
            {
                PLOG(PL_FATAL, "ProtoFile::Open() mkdir(%s) error: %s\n",
                        tempPath, GetErrorString());
                return false;  
            }
            if (ptr) *ptr++ = PROTO_PATH_DELIMITER;
        }
    }    	
#ifdef WIN32
    // Make sure we're in binary mode (important for WIN32)
	theFlags |= O_BINARY;
#ifdef _WIN32_WCE
    if (theFlags & O_RDONLY)
        file_ptr = fopen(thePath, "rb");
    else
        file_ptr = fopen(thePath, "w+b");
    if (NULL != file_ptr)
#else
    // Allow sharing of read-only files but not of files being written
	if (theFlags & O_RDONLY)
		descriptor = _sopen(thePath, theFlags, _SH_DENYNO);
    else
		descriptor = _open(thePath, theFlags, 0640);
	input_handle = (HANDLE)_get_osfhandle(descriptor);
	input_event_handle = output_handle = output_event_handle = input_handle;
    if(descriptor >= 0)
#endif // if/else _WIN32_WCE
    {
        offset = 0;
		flags = theFlags;
        returnvalue = true;  // no error
    }
    else
    {       
        PLOG(PL_FATAL, "Error opening file \"%s\": %s\n", thePath, GetErrorString());
		flags = 0;
        return false;
    }
#else  // WIN32 not defined
    if((descriptor = open(thePath, theFlags, 0640)) >= 0)
    {
        offset = 0;
        returnvalue = true;  // no error
    }
    else
    {    
        PLOG(PL_FATAL, "protoFile: Error opening file \"%s\": %s\n", 
                             thePath, GetErrorString());
        return false;
    }
#endif // if/else WIN32/UNIX
    if (returnvalue)
    {
        return ProtoChannel::Open();  
    }
	return returnvalue;
}  // end ProtoFile::Open()

void ProtoFile::Close()
{
    if (IsOpen())
    {
#ifdef WIN32
#ifdef _WIN32_WCE
        fclose(file_ptr);
        file_ptr = NULL;
#else
        _close(descriptor);
        descriptor = -1;
#endif // if/else _WIN32_WCE
#else
        close(descriptor);
        descriptor = -1;
#endif // if/else WIN32
        ProtoChannel::Close();
    }
}  // end ProtoFile::Close()

// Routines to try to get an exclusive lock on a file
bool ProtoFile::Lock()
{
#ifndef WIN32    // WIN32 files are automatically locked
    fchmod(descriptor, 0640 | S_ISGID);
#ifdef HAVE_FLOCK
    if (flock(descriptor, LOCK_EX | LOCK_NB))
        return false;
    else
#else
#ifdef HAVE_LOCKF
    if (lockf(descriptor, F_LOCK, 0))  // assume lockf if not flock
        return false;
    else
#endif // HAVE_LOCKF
#endif // if/else HAVE_FLOCK
#endif // !WIN32
        return true;
}  // end ProtoFile::Lock()

void ProtoFile::Unlock()
{
#ifndef WIN32
#ifdef HAVE_FLOCK
    if (0 != flock(descriptor, LOCK_UN))
    {
        PLOG(PL_ERROR, "ProtoFile::Unlock() flock() error: %s\n", GetErrorString());
    }
#else
#ifdef HAVE_LOCKF
    if (0 != lockf(descriptor, F_ULOCK, 0))
    {
        PLOG(PL_ERROR, "ProtoFile::Unlock() lockf() error: %s\n", GetErrorString());
    }
#endif // HAVE_LOCKF
#endif // if/elseHAVE_FLOCK
    fchmod(descriptor, 0640);
#endif // !WIN32
}  // end ProtoFile::UnLock()

bool ProtoFile::Rename(const char* oldName, const char* newName)
{
    if (!strcmp(oldName, newName)) return true;  // no change required
    // Make sure the new file name isn't an existing "busy" file
    // (This also builds sub-directories as needed)
    if (ProtoFile::IsLocked(newName)) 
    {
        PLOG(PL_FATAL, "ProtoFile::Rename() error: file is locked\n");
        return false;    
    }
#ifdef WIN32
    // In Win32, the new file can't already exist
	if (ProtoFile::Exists(newName)) 
    {
#ifdef _WIN32_WCE
#ifdef _UNICODE
        wchar_t wideBuffer[MAX_PATH];
        size_t pathLen = strlen(newName) + 1;
        if (pathLen > MAX_PATH) pathLen = MAX_PATH;
        mbstowcs(wideBuffer, newName, pathLen);
        if (0 == DeleteFile(wideBuffer))
#else
        if (0 == DeleteFile(newName))
#endif // if/else _UNICODE
        {
            PLOG(PL_FATAL, "ProtoFile::Rename() DeleteFile() error: %s\n", GetErrorString());
            return false;
        }
#else
        if (0 != _unlink(newName))
        {
            PLOG(PL_FATAL, "ProtoFile::Rename() _unlink() error: %s\n", GetErrorString());
            return false;
        }
#endif // if/else _WIN32_WCE
    }
    // In Win32, the old file can't be open
	int oldFlags = 0;
	if (IsOpen())
	{
		oldFlags = flags;
		oldFlags &= ~(O_CREAT | O_TRUNC);  // unset these
		Close();
	}  
#endif  // WIN32
    // Create sub-directories as needed.
    char tempPath[PATH_MAX+1];
    tempPath[PATH_MAX] = '\0';
    strncpy(tempPath, newName, PATH_MAX);
    char* ptr = strrchr(tempPath, PROTO_PATH_DELIMITER);
    if (ptr) *ptr = '\0';
    ptr = NULL;
    while (!ProtoFile::Exists(tempPath))
    {
        char* ptr2 = ptr;
        ptr = strrchr(tempPath, PROTO_PATH_DELIMITER);
        if (ptr2) *ptr2 = PROTO_PATH_DELIMITER;
        if (ptr)
        {
            *ptr = '\0';
        }
        else
        {
            ptr = tempPath;
            break;
        }
    }
    if (ptr && ('\0' == *ptr)) *ptr++ = PROTO_PATH_DELIMITER;
    while (ptr)
    {
        ptr = strchr(ptr, PROTO_PATH_DELIMITER);
        if (ptr) *ptr = '\0';
#ifdef WIN32
#ifdef _WIN32_WCE
#ifdef _UNICODE
            wchar_t wideBuffer[MAX_PATH];
            size_t pathLen = strlen(tempPath) + 1;
            if (pathLen > MAX_PATH) pathLen = MAX_PATH;
            mbstowcs(wideBuffer, tempPath, pathLen);
            if (!CreateDirectory(wideBuffer, NULL))
#else
            if (!CreateDirectory(tempPath, NULL))
#endif  // if/else _UNICODE
#else
            if (0 != _mkdir(tempPath))
#endif // if/else _WIN32_WCE
#else
        if (mkdir(tempPath, 0755))
#endif // if/else WIN32/UNIX
        {
            PLOG(PL_FATAL, "ProtoFile::Rename() mkdir(%s) error: %s\n",
                    tempPath, GetErrorString());
            return false;  
        }
        if (ptr) *ptr++ = PROTO_PATH_DELIMITER;
    }  
#ifdef _WIN32_WCE
#ifdef _UNICODE
    wchar_t wideOldName[MAX_PATH];
    wchar_t wideNewName[MAX_PATH];
    mbstowcs(wideOldName, oldName, MAX_PATH);
    mbstowcs(wideNewName, newName, MAX_PATH);
    if (!MoveFile(wideOldName, wideNewName))
#else
    if (!MoveFile(oldName, newName))
#endif // if/else _UNICODE
    {
        PLOG(PL_ERROR, "ProtoFile::Rename() MoveFile() error: %s\n", GetErrorString());
#else
    if (rename(oldName, newName))
    {
        PLOG(PL_ERROR, "ProtoFile::Rename() rename() error: %s\n", GetErrorString());	
#endif // if/else _WIN32_WCE
#ifdef WIN32
        if (oldFlags) 
        {
            if (Open(oldName, oldFlags))
                offset = 0;
            else
                PLOG(PL_ERROR, "ProtoFile::Rename() error re-opening file w/ old name\n");
        }
#endif // WIN32        
        return false;
    }
    else
    {
#ifdef WIN32
        // (TBD) Is the file offset OK doing this???
        if (oldFlags) 
        {
            if (Open(newName, oldFlags))
                offset = 0;
            else
                PLOG(PL_ERROR, "ProtoFile::Rename() error opening file w/ new name\n");
        }
#endif // WIN32
        return true;
    }
}  // end ProtoFile::Rename()

bool ProtoFile::Read(char* buffer, unsigned int& numBytes)
{
    ASSERT(IsOpen());
    while (true)
    {
#ifdef WIN32
#ifdef _WIN32_WCE
        size_t result = fread(buffer, 1, numBytes, file_ptr);
#else
        size_t result = _read(descriptor, buffer, (unsigned int)(numBytes));
#endif // if/else _WIN32_WCE
#else
        ssize_t result = read(descriptor, buffer, numBytes);
#endif // if/else WIN32
        if (result < 0)
        {
#ifndef _WIN32_WCE
            numBytes = 0;
            switch (errno)
            {
                case EINTR: 
                    continue;
                case EAGAIN:
                    numBytes = 0;
                    return true; // nothing more to read for the moment
                default:
                  break; 
            }
#endif // !_WIN32_WCE
            PLOG(PL_ERROR, "ProtoFile::Read() error: %s\n", GetErrorString());
            return false;
        } 
        else 
        {
            numBytes = (unsigned int)result;
            return true;
        }
    }  // end while true
}  // end ProtoFile::Read()

bool ProtoFile::ReadPrivate(char* buffer, unsigned int& numBytes)
{
    unsigned int want = numBytes;
    if (read_count > 0)
    {
        unsigned int ncopy = MIN(want, read_count);
        memcpy(buffer, read_ptr, ncopy);
        read_count -= ncopy;
        read_ptr += ncopy;
        buffer +=ncopy;
        want -= ncopy;
    }
    while (want > 0)
    {
        unsigned int blocksize = BUFFER_SIZE;
        if (!Read(read_buffer, blocksize))
        {
            PLOG(PL_ERROR, "ProtoFile::ReadPrivate() error: Read() failure\n");
            return false;
        }
        if (blocksize > 0)
        {
            // This check skips NULLs that have been read on some
            // use of trpr via tail from an NFS mounted file
            if (!isprint(*read_buffer) &&
                ('\t' != *read_buffer) &&
                ('\n' != *read_buffer) &&
                ('\r' != *read_buffer))
            {
                    continue;
            }
            unsigned int ncopy = MIN(want, blocksize);
            memcpy(buffer, read_buffer, ncopy);
            read_count = blocksize - ncopy;
            read_ptr = read_buffer + ncopy;
            buffer += ncopy;
            want -= ncopy;
        }
        else 
        {
            // Nothing more available (EOF or EAGAIN)
            numBytes -= want;
            return true;
        }
    }
    // Filled "buffer" with requested "numBytes"
    return true;
}  // end ProtoFile::ReadPrivate()

bool ProtoFile::Readline(char*         buffer,
                         unsigned int& bufferSize)
{
    unsigned int count = 0;
    unsigned int length = bufferSize;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int one = 1;
        if (ReadPrivate(ptr, one))
        {
            if (0 == one)
            {
                // Must have reached EOF (treat as end of line)
                if (count > 0)
                {
                    *ptr = '\0';
                    bufferSize = count + 1; 
                    return true;
                }
                else
                {
                    bufferSize = 0;
                    return false;
                }
            }
            else 
            { 
                // Check to see if its end of line char
                if (('\n' == *ptr) || ('\r' == *ptr))
                {
                    *ptr = '\0';
                    bufferSize = count;
                    return true;
                }
                count++;
                ptr++;
            }
        }
        else 
        { 
            PLOG(PL_ERROR,"ProtoFile::Readline() error: ReadPrivate() failure\n");
            return false;
        }
    }
    // We've filled up the buffer provided with no end-of-line reached
    PLOG(PL_ERROR, "ProtoFile::Readline() error: line length exceeds\n", bufferSize);
    return false;
}  // end ProtoFile::Readline()

size_t ProtoFile::Write(const char* buffer, size_t len)
{
    ASSERT(IsOpen());
    size_t put = 0;
    while (put < len)
    {
#ifdef WIN32
#ifdef _WIN32_WCE
        size_t result = fwrite(buffer+put, 1, len-put, file_ptr);
#else
        size_t result = _write(descriptor, buffer+put, (unsigned int)(len-put));
#endif // if/else _WIN32_WCE
#else
        size_t result = write(descriptor, buffer+put, len-put);
#endif // if/else WIN32
        if (result <= 0)
        {
 #ifndef _WIN32_WCE
            if (EINTR != errno)
#endif // !_WIN32_WCE
            {
                PLOG(PL_FATAL, "ProtoFile::Write() write(%d) result:%d error: %s\n", len, result, GetErrorString());
                return 0;
            }
        }
        else
        {
            offset += (Offset)result;
            put += result;
        }
    }  // end while (put < len)
    return put;
}  // end ProtoFile::Write()

bool ProtoFile::Seek(Offset theOffset)
{
    ASSERT(IsOpen());
#ifdef WIN32
#ifdef _WIN32_WCE
    // (TBD) properly support big files on WinCE
    Offset result = fseek(file_ptr, (long)theOffset, SEEK_SET);
#else
    Offset result = _lseeki64(descriptor, theOffset, SEEK_SET);
#endif // if/else _WIN32_WCE
#else
    Offset result = lseek(descriptor, theOffset, SEEK_SET);
#endif // if/else WIN32
    if (result == (Offset)-1)
    {
        PLOG(PL_FATAL, "ProtoFile::Seek() lseek() error: %s\n", GetErrorString());
        return false;
    }
    else
    {
        offset = result;
        return true; // no error
    }
}  // end ProtoFile::Seek()

bool ProtoFile::Pad(Offset theOffset)
{
    if (theOffset > GetSize())
    {
        if (Seek(theOffset - 1))
        {
            char byte = 0;
            if (1 != Write(&byte, 1))
            {
                PLOG(PL_FATAL, "ProtoFile::Pad() write error: %s\n", GetErrorString());
                return false;
            }
        }
        else
        {
            PLOG(PL_FATAL, "ProtoFile::Pad() seek error: %s\n", GetErrorString());
            return false;
        }
    }
    return true; 
}  // end ProtoFile::Pad()

ProtoFile::Offset ProtoFile::GetSize() const
{
    ASSERT(IsOpen());
#ifdef _WIN32_WCE
    DWORD fileSize = GetFileSize(_fileno(file_ptr), NULL);
    return ((Offset)fileSize);
#else
#ifdef WIN32
    struct _stati64 info;                  // instead of "struct _stat"
    int result = _fstati64(descriptor, &info);     // instead of "_fstat()"
#else
    struct stat info;
    int result = fstat(descriptor, &info);
#endif // if/else WIN32
    if (result)
    {
        PLOG(PL_FATAL, "Error getting file size: %s\n", GetErrorString());
        return 0;   
    }
    else
    {
        return info.st_size;
    }
#endif // if/else _WIN32_WCE
}  // end ProtoFile::GetSize()


/***********************************************
 * The ProtoFile::DirectoryIterator classes are used to
 * walk directory trees
 */

ProtoFile::DirectoryIterator::DirectoryIterator()
    : current(NULL)
{
}

ProtoFile::DirectoryIterator::~DirectoryIterator()
{
    Close();
}

bool ProtoFile::DirectoryIterator::Open(const char *thePath)
{
    if (NULL != current) Close();
    if (NULL == thePath)
    {
        PLOG(PL_ERROR, "ProtoFile::DirectoryIterator::Open() error: NULL path?!\n");
        return false;
    }
    
    if (NORMAL == GetType(thePath))
    {
        // This is a non-directory file path so create a "Directory" item 
        // to cache the path
        if (NULL == (current = new Directory(thePath)))
        {
            PLOG(PL_ERROR, "ProtoFile::DirectoryIterator::Open() new Directory error: %s\n", GetErrorString());
            return false;
        }
#ifdef WIN32
        current->hSearch = INVALID_HANDLE_VALUE;
#else
        current->dptr = NULL;
#endif  // if/else WIN32
        path_len = (int)strlen(current->Path());
        path_len = MIN(PATH_MAX, path_len);
        return true;
    }
#ifdef WIN32
#ifdef _WIN32_WCE
    if (!ProtoFile::Exists(thePath))
#else
    if (_access(thePath, 0))
#endif // if/else _WIN32_WCE
#else
    if (access(thePath, X_OK))
#endif // if/else WIN32
    {
        PLOG(PL_ERROR, "ProtoFile::DirectoryIterator::Open() error: can't access directory: %s\n", thePath);
        return false;
    }
    current = new Directory(thePath);
    if ((NULL != current) && current->Open())
    {
        path_len = (int)strlen(current->Path());
        path_len = MIN(PATH_MAX, path_len);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoFile::DirectoryIterator::Open() error: can't open directory: %s\n", thePath);
        if (current) delete current;
        current = NULL;
        return false;
    }
}  // end ProtoFile::DirectoryIterator::Open()

void ProtoFile::DirectoryIterator::Close()
{
    Directory* d;
    while (NULL != (d = current))
    {
        current = d->parent;
        d->Close();
        delete d;
    }
}  // end ProtoFile::DirectoryIterator::Close()

bool ProtoFile::DirectoryIterator::GetPath(char* pathBuffer) const
{
    if (NULL != current)
    {
        Directory* d = current;
        while (NULL != d->parent) d = d->parent;
        strncpy(pathBuffer, d->Path(), PATH_MAX);
        return true;
    }
    else
    {
        pathBuffer[0] = '\0';
        return false;
    }
}  // end DirectoryIterator::GetPath()

#ifdef WIN32
bool ProtoFile::DirectoryIterator::GetNextPath(char* fileName, bool includeFiles, bool includeDirs)
{
	if (!current) return false;
    if (INVALID_HANDLE_VALUE == current->hSearch)
    {
        bool result = includeFiles;
        if (result)
        {
            strncpy(fileName, current->path, PATH_MAX);
            size_t len = strlen(fileName);
            if (PROTO_PATH_DELIMITER == fileName[len - 1])
                fileName[len - 1] = '\0';
        }
        current = NULL;
        return result;
    }

	bool success = true;
	while(success)
	{
		WIN32_FIND_DATA findData;
		if (current->hSearch == INVALID_HANDLE_VALUE)
		{
			// Construct search string
			current->GetFullName(fileName);
			strcat(fileName, "\\*");
#ifdef _UNICODE
            wchar_t wideBuffer[MAX_PATH];
            mbstowcs(wideBuffer, fileName, MAX_PATH);
            if (INVALID_HANDLE_VALUE ==
			    (current->hSearch = FindFirstFile(wideBuffer, &findData)))
#else
			if (INVALID_HANDLE_VALUE ==
			    (current->hSearch = FindFirstFile(fileName, &findData)))
#endif // if/else _UNICODE
			    success = false;
			else
				success = true;
		}
		else
		{
			success = (0 != FindNextFile(current->hSearch, &findData));
		}

		// Do we have a candidate file?
		if (success)
		{
#ifdef _UNICODE
            char cFileName[MAX_PATH];
            wcstombs(cFileName, findData.cFileName, MAX_PATH);
            char* ptr = strrchr(cFileName, PROTO_PATH_DELIMITER);
#else
			char* ptr = strrchr(findData.cFileName, PROTO_PATH_DELIMITER);
#endif // if/else _UNICODE
			if (ptr)
				ptr += 1;
			else
#ifdef _UNICODE
                ptr = cFileName;
#else
				ptr = findData.cFileName;
#endif // if/else _UNICODE

			// Skip "." and ".." directories
			if (ptr[0] == '.')
			{
				if ((1 == strlen(ptr)) ||
					((ptr[1] == '.') && (2 == strlen(ptr))))
				{
					continue;
				}
			}
			current->GetFullName(fileName);
			strcat(fileName, ptr);
			ProtoFile::Type type = ProtoFile::GetType(fileName);
			if (includeFiles && (ProtoFile::NORMAL == type))
			{
                // I don't think this commented code is needed anymore
                //size_t nameLen = strlen(fileName);
                //nameLen = MIN(PATH_MAX, nameLen);
				//nameLen -= path_len;
				//memmove(fileName, fileName+path_len, nameLen);
				//if (nameLen < PATH_MAX) fileName[nameLen] = '\0';
				return true;
			}
			else if (ProtoFile::DIRECTORY == type)
			{
				Directory *dir = new Directory(ptr, current);
				if ((NULL != dir) && dir->Open())
				{
					// Push sub-directory onto stack and search it
					current = dir;
                    if (includeDirs)
                    {
                        // TBD - should we remove trailing PATH_DELIMITER?
                        current->GetFullName(fileName);
                        return true;
                    }  
					return GetNextPath(fileName, includeFiles, includeDirs);
				}
				else
				{
					// Couldn't open try next one
                    if (NULL != dir)
                    {
                        // if "includeDirs", return path, even if we can't open it
					    if (includeDirs)
                            dir->GetFullName(fileName);
                        delete dir;
                        if (includeDirs)
                            return true;
                    }
				}
			}
			else
			{
				// ProtoFile::INVALID file, try next one
			}
		}  // end if(success)
	}  // end while(success)

	// if parent, popup a level and continue search
	if (current->parent)
	{
		current->Close();
		Directory *dir = current;
		current = current->parent;
		delete dir;
		return GetNextPath(fileName, includeFiles, includeDirs);
	}
	else
	{
		current->Close();
		delete current;
		current = NULL;
		return false;
	}	
}  // end ProtoFile::DirectoryIterator::GetNextPath()  (WIN32)
#else
bool ProtoFile::DirectoryIterator::GetNextPath(char* fileName, bool includeFiles, bool includeDirs)
{   
    //TRACE("enter ProtoFile::DirectoryIterator::GetNextPath() current:%p files:%d dirs:%d ...\n", current, includeFiles, includeDirs);
    if (NULL == current) return false;
    if (NULL == current->dptr)
    {
        bool result = includeFiles;
        if (result)
        {
            strncpy(fileName, current->path, PATH_MAX);
            size_t len = strlen(fileName);
            if (PROTO_PATH_DELIMITER == fileName[len-1])
                fileName[len-1] = '\0';
        }
        current = NULL;
        return result;
    }
    struct dirent *dp;
    while (NULL != (dp = readdir(current->dptr)))
    {
        // Make sure it's not "." or ".."
        if (dp->d_name[0] == '.')
        {
            if ((1 == strlen(dp->d_name)) ||
                ((dp->d_name[1] == '.' ) && (2 == strlen(dp->d_name))))
            {
                continue;  // skip "." and ".." directory names
            }
        }
        current->GetFullName(fileName);
        strcat(fileName, dp->d_name);
        //TRACE("Getting type for %s ... ", fileName);
        ProtoFile::Type type = ProtoFile::GetType(fileName);        
        if (includeFiles && (ProtoFile::NORMAL == type))
        {
            //TRACE("NORMAL type\n");
            return true;
        } 
        else if (ProtoFile::DIRECTORY == type)
        {
            //TRACE("DIRECTORY type\n");
            Directory *dir = new Directory(dp->d_name, current);
            if ((NULL != dir) && dir->Open())
            {
                // Push sub-directory onto stack and search it
                current = dir;
                if (includeDirs)
                {
                    current->GetFullName(fileName);
                    return true;
                }
                return GetNextPath(fileName, includeFiles, includeDirs);
            }
            else
            {
				// Couldn't open try next one
                if (NULL != dir)
                {
					if (includeDirs)
                        dir->GetFullName(fileName);
                    delete dir;
                    if (includeDirs)
                        return true;
                }
			}
        }
        else
        {
            //TRACE("INVALID type\n");
            // ProtoFile::INVALID, try next one
        }
    }  // end while(readdir())
   
    // Pop up a level and recursively continue or finish if done
    if (current->parent)
    {
        char path[PATH_MAX+1];
        path[PATH_MAX] = '\0';
        current->parent->GetFullName(path);
        current->Close();
        Directory *dir = current;
        current = current->parent;
        delete dir;
        return GetNextPath(fileName, includeFiles, includeDirs);
    }
    else
    {
        current->Close();
        delete current;
        current = NULL;
        return false;  // no more files remain
    }      
}  // end ProtoFile::DirectoryIterator::GetNextPath() (UNIX)

#endif // if/else WIN32

ProtoFile::Directory::Directory(const char* thePath, 
                                Directory*  theParent)
    : parent(theParent),
#ifdef WIN32
    hSearch(INVALID_HANDLE_VALUE)
#else
    dptr(NULL)
#endif // if/else WIN32 
{
    strncpy(path, thePath, PATH_MAX);
    path[PATH_MAX] = '\0';
    size_t len  = MIN(PATH_MAX, strlen(path));
    if ((len < PATH_MAX) && (PROTO_PATH_DELIMITER != path[len-1]))
    {
        path[len++] = PROTO_PATH_DELIMITER;
        if (len < PATH_MAX) path[len] = '\0';
    }
}

ProtoFile::Directory::~Directory()
{
    Close();
}

bool ProtoFile::Directory::Open()
{
    Close();  // in case it's already open   
    char fullName[PATH_MAX+1];
    fullName[PATH_MAX] = '\0';
    GetFullName(fullName);  
    // Get rid of trailing PROTO_PATH_DELIMITER
    size_t len = MIN(PATH_MAX, strlen(fullName));
    if (PROTO_PATH_DELIMITER == fullName[len-1]) fullName[len-1] = '\0';
#ifdef WIN32
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, fullName, MAX_PATH);
    DWORD attr = GetFileAttributes(wideBuffer);
#else
    DWORD attr = GetFileAttributes(fullName);
#endif // if/else _UNICODE
	if (0xFFFFFFFF == attr)   
    {
        PLOG(PL_ERROR, "ProtoFile::Directory::Open(%s) error: %s\n", fullName, GetErrorString());
        return false;
    }
	else if (0 != (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
		return true;
    }
	else   
    {
        PLOG(PL_ERROR, "ProtoFile::Directory::Open(%s) error: not a directory\n", fullName);
        return false;
    }
#else
    if (NULL != (dptr = opendir(fullName)))
    {
        return true;
    }
    else    
    {
        PLOG(PL_ERROR, "ProtoFile::Directory::Open(%s) error: %s\n", fullName, GetErrorString());
        return false;
    }
#endif // if/else WIN32
    
} // end ProtoFile::Directory::Open()

void ProtoFile::Directory::Close()
{
#ifdef WIN32
    if (hSearch != INVALID_HANDLE_VALUE)
	{
		FindClose(hSearch);
		hSearch = INVALID_HANDLE_VALUE;
	}
#else
    if (NULL != dptr)
    {
        closedir(dptr);
        dptr = NULL;
    }
#endif  // if/else WIN32
}  // end ProtoFile::Directory::Close()

void ProtoFile::Directory::GetFullName(char* ptr)
{
    ptr[0] = '\0';
    RecursiveCatName(ptr);
}  // end ProtoFile::Directory::GetFullName()

void ProtoFile::Directory::RecursiveCatName(char* ptr)
{
    if (NULL != parent) parent->RecursiveCatName(ptr);
    size_t len = MIN(PATH_MAX, strlen(ptr));
    strncat(ptr, path, PATH_MAX-len);
}  // end ProtoFile::Directory::RecursiveCatName()

// Below are some static routines for getting file/directory information

// Is the named item a valid directory or file (or neither)??
ProtoFile::Type ProtoFile::GetType(const char* path)
{
#ifdef WIN32
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, path, MAX_PATH);
    DWORD attr = GetFileAttributes(wideBuffer);
#else
    DWORD attr = GetFileAttributes(path);
#endif // if/else _UNICODE
	if (0xFFFFFFFF == attr)
		return INVALID;  // error
	else if (attr & FILE_ATTRIBUTE_DIRECTORY)
		return DIRECTORY;
	else
		return NORMAL;
#else
    struct stat file_info;  
    if (stat(path, &file_info)) 
        return INVALID;  // stat() error
    else if ((S_ISDIR(file_info.st_mode)))
        return DIRECTORY;
    else 
        return NORMAL;
#endif // if/else WIN32
}  // end ProtoFile::GetType()

ProtoFile::Offset ProtoFile::GetSize(const char* path)
{
#ifdef _WIN32_WCE
    WIN32_FIND_DATA findData;
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, path, MAX_PATH);
    if (INVALID_HANDLE_VALUE != FindFirstFile(wideBuffer, &findData))
#else
    if (INVALID_HANDLE_VALUE != FindFirstFile(path, &findData))
#endif // if/else _UNICODE
    {
		Offset fileSize = findData.nFileSizeLow;
		if (sizeof(Offset) > 4)
			fileSize |= ((Offset)findData.nFileSizeHigh) << (8*sizeof(DWORD));
        return fileSize;
    }
    else
    {
        PLOG(PL_ERROR, "Error getting file size: %s\n", GetErrorString());
        return 0;
    }
#else
#ifdef WIN32
    struct _stati64 info;               // instead of "struct _stat"
    int result = _stati64(path, &info); // instead of "_stat()"
#else
    struct stat info;
    int result = stat(path, &info);
#endif // if/else WIN32    
    if (result)
    {
        //DMSG(0, "Error getting file size: %s\n", GetErrorString());
        return 0;   
    }
    else
    {
        return info.st_size;
    }
#endif // if/else _WIN32_WCE
}  // end ProtoFile::GetSize()

time_t ProtoFile::GetUpdateTime(const char* path)
{
#ifdef _WIN32_WCE
    WIN32_FIND_DATA findData;
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, path, MAX_PATH);
    if (INVALID_HANDLE_VALUE != FindFirstFile(wideBuffer, &findData))
#else
    if (INVALID_HANDLE_VALUE != FindFirstFile(path, &findData))
#endif // if/else _UNICODE
    {
        ULARGE_INTEGER ctime = {findData.ftCreationTime.dwLowDateTime,
                                findData.ftCreationTime.dwHighDateTime};
        ULARGE_INTEGER atime = {findData.ftLastAccessTime.dwLowDateTime,
                                findData.ftLastAccessTime.dwHighDateTime};
        ULARGE_INTEGER mtime = {findData.ftLastWriteTime.dwLowDateTime,
                                findData.ftLastWriteTime.dwHighDateTime};
        if (ctime.QuadPart < atime.QuadPart) ctime = atime;
        if (ctime.QuadPart < mtime.QuadPart) ctime = mtime;
        const ULARGE_INTEGER epochTime = {0xD53E8000, 0x019DB1DE};
        ctime.QuadPart -= epochTime.QuadPart;
        // Convert sytem time to seconds
        ctime.QuadPart /= 10000000;
        return ctime.LowPart;
    }
    else
    {
        PLOG(PL_ERROR, "Error getting file size: %s\n", GetErrorString());
        return 0;
    }
#else
#ifdef WIN32
    struct _stati64 info;               // instead of "struct _stat"
    int result = _stati64(path, &info); // instead of "_stat()"
#else
    struct stat info; 
    int result = stat(path, &info);
#endif // if/else WIN32   
    if (result) 
    {
        return (time_t)0;  // stat() error
    }
    else 
    {
#ifdef WIN32
        // Hack because Win2K and Win98 seem to work differently
		//__time64_t updateTime = MAX(info.st_ctime, info.st_atime);
        time_t updateTime = MAX(info.st_ctime, info.st_atime);
		updateTime = MAX(updateTime, info.st_mtime);
		return ((time_t)updateTime);
#else
        return info.st_ctime; 
#endif // if/else WIN32
    } 
#endif // if/else _WIN32_WCE
}  // end ProtoFile::GetUpdateTime()

bool ProtoFile::IsLocked(const char* path)
{
    // If file doesn't exist, it's not locked
    if (!Exists(path)) return false;      
    ProtoFile testFile;
#ifdef WIN32
    if(!testFile.Open(path, O_WRONLY | O_CREAT))
#else
    if(!testFile.Open(path, O_WRONLY | O_CREAT))    
#endif // if/else WIN32
    {
        return true;
    }
    else if (testFile.Lock())
    {
        // We were able to lock the file successfully
        testFile.Unlock();
        testFile.Close();
        return false;
    }
    else
    {
        testFile.Close();
        return true;
    }
}  // end ProtoFile::IsLocked()

bool ProtoFile::Unlink(const char* path)
{
    // Don't unlink a file that is open (locked)
    if (ProtoFile::IsLocked(path))
    {
        return false;
    }

#ifdef _WIN32_WCE
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, path, MAX_PATH);
    if (0 == DeleteFile(wideBuffer))
#else
    if (0 == DeleteFile(path))
#endif // if/else _UNICODE
    {
        PLOG(PL_FATAL, "ProtoFile::Unlink() DeletFile() error: %s\n", GetErrorString());
        return false;
    }
#else
#ifdef WIN32
    if (_unlink(path))
#else
    if (unlink(path))
#endif // if/else WIN32
    {
        PLOG(PL_FATAL, "ProtoFile::Unlink() unlink error: %s\n", GetErrorString());
        return false;
    }
#endif // if/else _WIN32_WCE
    else
    {
        return true;
    }

}  // end ProtoFile::Unlink()


bool ProtoFile::PathList::AppendPath(const char* thePath)
{
    Path* pathItem = new Path(thePath);
    if (NULL == pathItem)
    {
        PLOG(PL_ERROR, "ProtoFile::PathList::Append() new Path error: %s\n", GetErrorString());
        return false;
    }
    Append(*pathItem);
    return true;
}  // end  ProtoFile::PathList::AppendPath()

ProtoFile::PathList::PathIterator::PathIterator(PathList& pathList, bool updatesOnly, time_t initTime)
    : list_iterator(pathList)
{
    Init(updatesOnly, initTime);
}

ProtoFile::PathList::PathIterator::~PathIterator()
{
    dir_iterator.Close();
}

bool ProtoFile::PathList::PathIterator::GetNextPath(char* path, bool includeFiles, bool includeDirs)
{
    while (true)
    {
        // First, get any files from pending directory iterator
        while (dir_iterator.GetNextPath(path, includeFiles, includeDirs))
        {
            if (updates_only)
            {
                time_t updateTime = ProtoFile::GetUpdateTime(path);
                //TRACE("path:%s updateTime:%ld last_time:%ld big_time:%ld\n", path, updateTime, last_time, big_time);
                if (difftime(updateTime, big_time) > 0.0) big_time = updateTime;
                if (difftime(updateTime, last_time) <= 0.0)
                    continue;  // this file has not been updated
            }
            return true;
        }
        dir_iterator.Close();
        Path* nextPath = list_iterator.GetNextItem();
        if (NULL == nextPath)
        {
            break;  // reached end of list
        }
        bool validPath = false;
        switch (ProtoFile::GetType(nextPath->GetPath()))
        {
            case DIRECTORY:
                if (!dir_iterator.Open(nextPath->GetPath()))
                    PLOG(PL_ERROR, "ProtoFile::PathList::PathIterator::GetNextPath() error: unable to open directory: %s\n", 
                                   nextPath->GetPath());
                if (includeDirs)
                    validPath = true;  // even return paths for directories we can't open
                break;
            case NORMAL:
                if (includeFiles)
                    validPath = true;
                break;
            default:
                break;
        }
        if (validPath)
        {
            if (updates_only)
            {
                time_t updateTime = ProtoFile::GetUpdateTime(nextPath->GetPath());
                //TRACE("path:%s updateTime:%ld last_time:%ld big_time:%ld\n", path, updateTime, last_time, big_time);
                if (difftime(updateTime, big_time) > 0.0) big_time = updateTime;
                if (difftime(updateTime, last_time) <= 0.0)
                    continue;  // this file has not been updated
            }
            strncpy(path, nextPath->GetPath(), PATH_MAX);
            return true;
        }
    }
    return false;
}  // end ProtoFile::PathList::PathIterator::GetNextPath()
        
