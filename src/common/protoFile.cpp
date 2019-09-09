#include "protoFile.h"

#include <string.h>  // for strerror()
#include <stdio.h>   // for rename()
#ifdef WIN32
#ifndef _WIN32_WCE
#include <errno.h>
#include <direct.h>
#include <share.h>
#include <io.h>
#endif // !_WIN32_WCE
#else
#include <unistd.h>
// Most don't have the dirfd() function
#ifndef HAVE_DIRFD
static inline int dirfd(DIR *dir) {return (dir->dd_fd);}
#endif // HAVE_DIRFD    
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


// This should be called with a full path only!
bool ProtoFile::Open(const char* thePath, int theFlags)
{
    bool returnvalue=false;
    ASSERT(!IsOpen());	
    if (theFlags & O_CREAT)
    {
        // Create sub-directories as needed.
        char tempPath[PATH_MAX];
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
            if (mkdir(tempPath))
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
    if(descriptor >= 0)
#endif // if/else _WIN32_WCE
    {
        offset = 0;
		flags = theFlags;
        returnvalue=true;  // no error
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
        returnvalue=true;  // no error
    }
    else
    {    
        PLOG(PL_FATAL, "protoFile: Error opening file \"%s\": %s\n", 
                             thePath, GetErrorString());
        return false;
    }
    if(returnvalue){
        return ProtoChannel::Open();
    }
#endif // if/else WIN32
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
    flock(descriptor, LOCK_UN);
#else
#ifdef HAVE_LOCKF
    lockf(descriptor, F_ULOCK, 0);
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
    char tempPath[PATH_MAX];
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
            if (0 != mkdir(tempPath))
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
                    return true;//reached eof
                default:
                  break;//bad error will print and return false;
            }
#endif // !_WIN32_WCE
            PLOG(PL_ERROR, "ProtoFile::Read() error: %s\n",GetErrorString());
            return false;
        } 
        else 
        {
            numBytes = result;
            return true;
        }
    }  // end while true
    //return false;//this shouldn't ever happen
}  // end ProtoFile::Read()
bool ProtoFile::bufferedRead(char* buffer, unsigned int& numBytes)
{
    unsigned int want = numBytes;
    if (savecount)
    {
        unsigned int ncopy = MIN(want, savecount);
        memcpy(buffer,saveptr,ncopy);
        savecount -= ncopy;
        saveptr += ncopy;
        buffer +=ncopy;
        want -= ncopy;
    }
    while(want){
        unsigned int blocksize=BLOCKSIZE;
        if(!Read(savebuf,blocksize)){
            PLOG(PL_ERROR, "ProtoFile::bufferedRead() error calling Read()\n");
            return false;
        }
        if(blocksize>0)
        {
            // This check skips NULLs that have been read on some
            // use of trpr via tail from an NFS mounted file
            if (!isprint(*savebuf) &&
                ('\t' != *savebuf) &&
                ('\n' != *savebuf) &&
                ('\r' != *savebuf))
                    continue;
            unsigned int ncopy= MIN(want, blocksize);
            memcpy(buffer, savebuf, ncopy);
            savecount = blocksize - ncopy;
            saveptr = savebuf + ncopy;
            buffer += ncopy;
            want -= ncopy;
        }
        else 
        {//nothing was read in
            numBytes-=want;
            return true;
        }
    }
    //we finished filling in the buffer up to numBytes
    return true;
}
bool ProtoFile::Readline(char*         buffer,
                         unsigned int& bufferSize)
{
//    TRACE("ProtoFile::Readline: enter\n");
    unsigned int count = 0;
    unsigned int length = bufferSize;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int one = 1;
        if(bufferedRead(ptr,one))
        {
            if(one==0){//hit eof return;
                //TRACE("ProtoFile::Readline: Hit end of file returning false.\n");
                bufferSize=count;//this is checked to see how much was read in
                return false;
            }
            else 
            { //read in a char check to see if its end of line char
                if (('\n' == *ptr) || ('\r' == *ptr))
                {
                   // if('\r'==*ptr) 
                    //    TRACE("ProtoFile::ReadLine: returning true r\n");
                    //if('\n'==*ptr) 
                     //   TRACE("ProtoFile::ReadLine: returning true n\n");
                    *ptr = '\0';
                    bufferSize = count;
                    return true;
                }
                count++;
                ptr++;
            }
        }
        else 
        { //error on calling ReadBuffer
            PLOG(PL_ERROR,"ProtoFile::Readline() error: ReadBuffer call failed\n");
            return false;
        }
    }
    //We've filled up the buffer provided with no end-of-line
    PLOG(PL_ERROR, "ProtoFile::Readline() error: bufferSize %d is too small)\n",bufferSize);
    return false;
}
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
 * The ProtoDirectoryIterator classes is used to
 * walk directory trees for file transmission
 */

ProtoDirectoryIterator::ProtoDirectoryIterator()
    : current(NULL)
{

}

ProtoDirectoryIterator::~ProtoDirectoryIterator()
{
    Close();
}

bool ProtoDirectoryIterator::Open(const char *thePath)
{
    if (current) Close();
#ifdef WIN32
#ifdef _WIN32_WCE
    if (thePath && !ProtoFile::Exists(thePath))
#else
    if (thePath && _access(thePath, 0))
#endif // if/else _WIN32_WCE
#else
    if (thePath && access(thePath, X_OK))
#endif // if/else WIN32
    {
        PLOG(PL_FATAL, "ProtoDirectoryIterator: can't access directory: %s\n", thePath);
        return false;
    }
    current = new ProtoDirectory(thePath);
    if (current && current->Open())
    {
        path_len = (int)strlen(current->Path());
        path_len = MIN(PATH_MAX, path_len);
        return true;
    }
    else
    {
        PLOG(PL_FATAL, "ProtoDirectoryIterator: can't open directory: %s\n", thePath);
        if (current) delete current;
        current = NULL;
        return false;
    }
}  // end ProtoDirectoryIterator::Init()

void ProtoDirectoryIterator::Close()
{
    ProtoDirectory* d;
    while ((d = current))
    {
        current = d->parent;
        d->Close();
        delete d;
    }
}  // end ProtoDirectoryIterator::Close()

bool ProtoDirectoryIterator::GetPath(char* pathBuffer)
{
    if (current)
    {
        ProtoDirectory* d = current;
        while (d->parent) d = d->parent;
        strncpy(pathBuffer, d->Path(), PATH_MAX);
        return true;
    }
    else
    {
        pathBuffer[0] = '\0';
        return false;
    }
}

#ifdef WIN32
bool ProtoDirectoryIterator::GetNextFile(char* fileName)
{
	if (!current) return false;
	bool success = true;
	while(success)
	{
		WIN32_FIND_DATA findData;
		if (current->hSearch == (HANDLE)-1)
		{
			// Construct search string
			current->GetFullName(fileName);
			strcat(fileName, "\\*");
#ifdef _UNICODE
            wchar_t wideBuffer[MAX_PATH];
            mbstowcs(wideBuffer, fileName, MAX_PATH);
            if ((HANDLE)-1 == 
			    (current->hSearch = FindFirstFile(wideBuffer, &findData)))
#else
			if ((HANDLE)-1 == 
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
			if (ProtoFile::NORMAL == type)
			{
                size_t nameLen = strlen(fileName);
                nameLen = MIN(PATH_MAX, nameLen);
				nameLen -= path_len;
				memmove(fileName, fileName+path_len, nameLen);
				if (nameLen < PATH_MAX) fileName[nameLen] = '\0';
				return true;
			}
			else if (ProtoFile::DIRECTORY == type && search_dirs)
			{

				ProtoDirectory *dir = new ProtoDirectory(ptr, current);
				if (dir && dir->Open())
				{
					// Push sub-directory onto stack and search it
					current = dir;
					return GetNextFile(fileName);
				}
				else
				{
					// Couldn't open try next one
					if (dir) delete dir;
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
		ProtoDirectory *dir = current;
		current = current->parent;
		delete dir;
		return GetNextFile(fileName);
	}
	else
	{
		current->Close();
		delete current;
		current = NULL;
		return false;
	}	
}  // end ProtoDirectoryIterator::GetNextFile()  (WIN32)
#else
bool ProtoDirectoryIterator::GetNextFile(char* fileName)
{   
    if (!current) return false;
    struct dirent *dp;
    while((dp = readdir(current->dptr)))
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
        ProtoFile::Type type = ProtoFile::GetType(fileName);        
        if (ProtoFile::NORMAL == type)
        {
            int nameLen = strlen(fileName);
            nameLen = MIN(PATH_MAX, nameLen);
            nameLen -= path_len;
            memmove(fileName, fileName+path_len, nameLen);
            if (nameLen < PATH_MAX) fileName[nameLen] = '\0';
            return true;
        } 
        else if (ProtoFile::DIRECTORY == type && search_dirs)
        {
            ProtoDirectory *dir = new ProtoDirectory(dp->d_name, current);
            if (dir && dir->Open())
            {
                // Push sub-directory onto stack and search it
                current = dir;
                return GetNextFile(fileName);
            }
            else
            {
                // Couldn't open this one, try next one
                if (dir) delete dir;
            }
        }
        else
        {
            // ProtoFile::INVALID, try next one
        }
    }  // end while(readdir())
   
    // Pop up a level and recursively continue or finish if done
    if (current->parent)
    {
        char path[PATH_MAX];
        current->parent->GetFullName(path);
        current->Close();
        ProtoDirectory *dir = current;
        current = current->parent;
        delete dir;
        return GetNextFile(fileName);
    }
    else
    {
        current->Close();
        delete current;
        current = NULL;
        return false;  // no more files remain
    }      
}  // end ProtoDirectoryIterator::GetNextFile() (UNIX)
#endif // if/else WIN32
void
ProtoDirectoryIterator::Recursive(bool stepIntoDirs)
{
    search_dirs = stepIntoDirs;
    return;
}
ProtoDirectoryIterator::ProtoDirectory::ProtoDirectory(const char*    thePath, 
                                                       ProtoDirectory* theParent)
    : parent(theParent),
#ifdef WIN32
    hSearch((HANDLE)-1)
#else
    dptr(NULL)
#endif // if/else WIN32 
{
    strncpy(path, thePath, PATH_MAX);
    size_t len  = MIN(PATH_MAX, strlen(path));
    if ((len < PATH_MAX) && (PROTO_PATH_DELIMITER != path[len-1]))
    {
        path[len++] = PROTO_PATH_DELIMITER;
        if (len < PATH_MAX) path[len] = '\0';
    }
}

ProtoDirectoryIterator::ProtoDirectory::~ProtoDirectory()
{
    Close();
}

bool ProtoDirectoryIterator::ProtoDirectory::Open()
{
    Close();  // in case it's already open   
    char fullName[PATH_MAX];
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
		return false;
	else if (attr & FILE_ATTRIBUTE_DIRECTORY)
		return true;
	else
		return false;
#else
    if((dptr = opendir(fullName)))
        return true;
    else    
        return false;
#endif // if/else WIN32
    
} // end ProtoDirectoryIterator::ProtoDirectory::Open()

void ProtoDirectoryIterator::ProtoDirectory::Close()
{
#ifdef WIN32
    if (hSearch != (HANDLE)-1) 
	{
		FindClose(hSearch);
		hSearch = (HANDLE)-1;
	}
#else
    if (dptr)
    {
        closedir(dptr);
        dptr = NULL;
    }
#endif  // if/else WIN32
}  // end ProtoDirectoryIterator::ProtoDirectory::Close()


void ProtoDirectoryIterator::ProtoDirectory::GetFullName(char* ptr)
{
    ptr[0] = '\0';
    RecursiveCatName(ptr);
}  // end ProtoDirectoryIterator::ProtoDirectory::GetFullName()

void ProtoDirectoryIterator::ProtoDirectory::RecursiveCatName(char* ptr)
{
    if (parent) parent->RecursiveCatName(ptr);
    size_t len = MIN(PATH_MAX, strlen(ptr));
    strncat(ptr, path, PATH_MAX-len);
}  // end ProtoDirectoryIterator::ProtoDirectory::RecursiveCatName()

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

ProtoFileList::ProtoFileList()
 : this_time(0), big_time(0), last_time(0),
   updates_only(false), head(NULL), tail(NULL), next(NULL)
{ 
}
        
ProtoFileList::~ProtoFileList()
{
    Destroy();
}

void ProtoFileList::Destroy()
{
    while ((next = head))
    {
        head = next->next;
        delete next;   
    }
    tail = NULL;
}  // end ProtoFileList::Destroy()

bool ProtoFileList::Append(const char* path)
{
    FileItem* theItem = NULL;
    switch(ProtoFile::GetType(path))
    {
        case ProtoFile::NORMAL:
            theItem = new FileItem(path);
            break;
        case ProtoFile::DIRECTORY:
            theItem = new DirectoryItem(path);
            break;
        default:
            // Allow non-existent files for update_only mode
            // (TBD) allow non-existent directories?
            if (updates_only) 
            {
                theItem = new FileItem(path);
            }
            else
            {
                PLOG(PL_FATAL, "ProtoFileList::Append() Bad file/directory name: %s\n",
                        path);
                return false;
            }
            break;
    }
    if (theItem)
    {
        theItem->next = NULL;
        if ((theItem->prev = tail))
            tail->next = theItem;
        else
            head = theItem;
        tail = theItem;
        return true;
    }
    else
    {
        PLOG(PL_FATAL, "ProtoFileList::Append() Error creating file/directory item: %s\n",
                GetErrorString());
        return false;
    }
}  // end ProtoFileList::Append()

bool ProtoFileList::Remove(const char* path)
{
    FileItem* nextItem = head;
    size_t pathLen = strlen(path);
    pathLen = MIN(pathLen, PATH_MAX);
    while (nextItem)
    {
        size_t nameLen = strlen(nextItem->Path());
        nameLen = MIN(nameLen, PATH_MAX);
        nameLen = MAX(nameLen, pathLen);
        if (!strncmp(path, nextItem->Path(), nameLen))
        {
            if (nextItem == next) next = nextItem->next;
            if (nextItem->prev)
                nextItem->prev = next = nextItem->next;
            else
                head = nextItem->next;
            if (nextItem->next)
                nextItem->next->prev = nextItem->prev;
            else
                tail = nextItem->prev;
            return true;
        }
    }
    return false;
}  // end ProtoFileList::Remove()

bool ProtoFileList::GetNextFile(char* pathBuffer)
{
    if (!next)
    {
        next = head;
        reset = true;
    }
    if (next)
    {
        if (next->GetNextFile(pathBuffer, reset, updates_only,
                              last_time, this_time, big_time))
        {
            reset = false;
            return true;
        }
        else
        {
            if (next->next)
            {
                next = next->next;
                reset = true;
                return GetNextFile(pathBuffer);
            }
            else
            {
                reset = false;
                return false;  // end of list
            }   
        }
    }
    else
    {
        return false;  // empty list
    }
}  // end ProtoFileList::GetNextFile()

void ProtoFileList::GetCurrentBasePath(char* pathBuffer)
{
    if (next)
    {
        if (ProtoFile::DIRECTORY == next->GetType())
        {
            strncpy(pathBuffer, next->Path(), PATH_MAX);
            size_t len = strlen(pathBuffer);
            len = MIN(len, PATH_MAX);
            if (PROTO_PATH_DELIMITER != pathBuffer[len-1])
            {
                if (len < PATH_MAX) pathBuffer[len++] = PROTO_PATH_DELIMITER;
                if (len < PATH_MAX) pathBuffer[len] = '\0';
            }   
        }
        else  // ProtoFile::NORMAL
        {
            const char* ptr = strrchr(next->Path(), PROTO_PATH_DELIMITER);
            if (ptr++)
            {
                size_t len = ptr - next->Path();
                strncpy(pathBuffer, next->Path(), len);
                pathBuffer[len] = '\0';
            }
            else
            {
                pathBuffer[0] = '\0';
            }
        }        
    }
    else
    {
        pathBuffer[0] = '\0';
    }
}  // end ProtoFileList::GetBasePath()


ProtoFileList::FileItem::FileItem(const char* thePath)
 : prev(NULL), next(NULL)
{
    size_t len = strlen(thePath);
    len = MIN(len, PATH_MAX);
    strncpy(path, thePath, PATH_MAX);
    size = ProtoFile::GetSize(thePath);    
} 

ProtoFileList::FileItem::~FileItem()
{
}

bool ProtoFileList::FileItem::GetNextFile(char*   thePath,
                                         bool    reset,
                                         bool    updatesOnly,
                                         time_t  lastTime,
                                         time_t  thisTime,
                                         time_t& bigTime)
{
    if (reset)
    {
        if (updatesOnly)
        {
            time_t updateTime = ProtoFile::GetUpdateTime(thePath);
            if (updateTime > bigTime) bigTime = updateTime;
            if ((updateTime <= lastTime) || (updateTime > thisTime))
                return false;
        }
        strncpy(thePath, path, PATH_MAX);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoFileList::FileItem::GetNextFile()

ProtoFileList::DirectoryItem::DirectoryItem(const char* thePath)
 : ProtoFileList::FileItem(thePath)
{    
    
}

ProtoFileList::DirectoryItem::~DirectoryItem()
{
    diterator.Close();
}

bool ProtoFileList::DirectoryItem::GetNextFile(char*   thePath,
                                               bool    reset,
                                               bool    updatesOnly,
                                               time_t  lastTime,
                                               time_t  thisTime,
                                               time_t& bigTime)
{
     if (reset)
     {
         /* For now we are going to poll all files in a directory individually
           since directory update times aren't always changed when files are
           are replaced within the directory tree ... uncomment this code
           if you only want to check directory nodes that have had their
           change time updated
        if (updates_only)
        {
            // Check to see if directory has been touched
            time_t update_time = ProtoFile::GetUpdateTime(path);
            if (updateTime > bigTime) *bigTime = updateTime;
            if ((updateTime <= lastTime) || (updateTime > thisTime))
                return false;
        } */
        if (!diterator.Open(path))
        {
            PLOG(PL_FATAL, "ProtoFileList::DirectoryItem::GetNextFile() Directory iterator init error\n");
            return false;   
        } 
     }
     strncpy(thePath, path, PATH_MAX);
     size_t len = strlen(thePath);
     len = MIN(len, PATH_MAX);
     if ((PROTO_PATH_DELIMITER != thePath[len-1]) && (len < PATH_MAX))
     {
         thePath[len++] = PROTO_PATH_DELIMITER;
         if (len < PATH_MAX) thePath[len] = '\0';
     }  
     char tempPath[PATH_MAX];
     while (diterator.GetNextFile(tempPath))
     {
         size_t maxLen = PATH_MAX - len;
         strncat(thePath, tempPath, maxLen);
         if (updatesOnly)
         {
            time_t updateTime = ProtoFile::GetUpdateTime(thePath);
            if (updateTime > bigTime) bigTime = updateTime;
            if ((updateTime <= lastTime) || (updateTime > thisTime))
            {
                thePath[len] = '\0';
                continue;
            }
         }
         return true;
     }
     return false;
}  // end ProtoFileList::DirectoryItem::GetNextFile()
