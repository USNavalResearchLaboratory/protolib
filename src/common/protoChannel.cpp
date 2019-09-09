/**
* @file protoChannel.cpp
*
* @brief This class serves as a base class for Protokit classes which require asynchronous I/O
*/

#include "protoChannel.h"
#include "protoDebug.h"

#ifdef WIN32
const ProtoChannel::Handle ProtoChannel::INVALID_HANDLE = INVALID_HANDLE_VALUE;
#else
#include <unistd.h>
#include <fcntl.h>
const ProtoChannel::Handle ProtoChannel::INVALID_HANDLE = -1;
#endif  // if/else WIN32/UNIX

ProtoChannel::ProtoChannel()
#ifdef WIN32
   : input_handle(INVALID_HANDLE), input_event_handle(INVALID_HANDLE), input_ready(false), 
     output_handle(INVALID_HANDLE), output_event_handle(INVALID_HANDLE), output_ready(false), 
#else
   : descriptor(INVALID_HANDLE), blocking_status(true),
#endif // if/else WIN32/UNIX
     listener(NULL), notifier(NULL)
{
#ifdef WIN32
	overlapped_read_buffer = NULL;
	overlapped_write_buffer = NULL;
#endif
}

ProtoChannel::~ProtoChannel()
{
    if (notifier) SetNotifier(NULL);
    if (listener)
    {
        delete listener;
        listener = NULL;  
    }
}

bool ProtoChannel::SetNotifier(ProtoChannel::Notifier* theNotifier)
{
    if (notifier != theNotifier)
    {
        if (IsOpen())
        {
            // 1) Detach old notifier, if any
            if (NULL != notifier)
            {
                notifier->UpdateChannelNotification(*this, 0);
                if (NULL == theNotifier)
                {
                    // Reset channel to "blocking"
                    if(!SetBlocking(true))
                        PLOG(PL_ERROR, "ProtoChannel::SetNotifier() SetBlocking(true) error\n", GetErrorString());
                }
            }
            else
            {
                // Set channel to "non-blocking"
                if(!SetBlocking(false))
                {
                    PLOG(PL_ERROR, "ProtoChannel::SetNotifier() SetBlocking(false) error\n", GetErrorString());
                    return false;
                }
            }   
            // 2) Set and update new notifier (if applicable)
            notifier = theNotifier;
            if (!UpdateNotification())
            {
                notifier = NULL;  
                return false;
            } 
        }
        else
        {
            notifier = theNotifier;
        }
    }
    return true;
}  // end ProtoChannel::SetNotifier()

void ProtoChannel::Close()
{
    {
        if (IsOpen())
        {
            StopInputNotification();
            StopOutputNotification();    
        }
    }
#ifdef WIN32
    // This cleans up things if ProtoChannel overlapped i/o was used
    if (NULL != overlapped_write_buffer)
    {
        CloseHandle(input_event_handle);
        input_event_handle = NULL;
        CloseHandle(output_event_handle);
        output_event_handle = NULL;
        delete[] overlapped_read_buffer;
        overlapped_read_buffer = NULL;
        delete[] overlapped_write_buffer;
        overlapped_write_buffer = NULL;
    }
#endif  // WIN32
        
}  // end ProtoChannel::Close()


bool ProtoChannel::StartInputNotification()
{   
    if (!InputNotification())
    {
#ifdef WIN32
        // See if we're using overlapped i/o and kickstart if applicable
        if ((NULL != overlapped_read_buffer) && (NULL != notifier))
        {
            SetNotifyFlag(NOTIFY_INPUT);
            if (!StartOverlappedRead())  // note it calls UpdateNotification() for us
            {
                UnsetNotifyFlag(NOTIFY_INPUT);
                PLOG(PL_ERROR, "ProtoChannel::StartInputNotification() error: overlapped read startup failure!\n");
                return false;
            }
            return true;
        } 
#endif // WIN32
        SetNotifyFlag(NOTIFY_INPUT);
        if (!UpdateNotification())
        {
            PLOG(PL_ERROR, "ProtoChannel::StartInputNotification() error: notification update failure!\n");
            UnsetNotifyFlag(NOTIFY_INPUT);
            return false;
        }
    }
    return true;
}  // end ProtoChannel::StartInputNotification()

void ProtoChannel::StopInputNotification()
{
    if (InputNotification())
    {
        UnsetNotifyFlag(NOTIFY_INPUT);
        UpdateNotification();
    }
}  // end ProtoChannel::StopInputNotification()  
        
bool ProtoChannel::StartOutputNotification()
{   
    if (!OutputNotification())
    {
#ifdef WIN32
        output_ready = true;
#endif // WIN32
        SetNotifyFlag(NOTIFY_OUTPUT);
        if (!UpdateNotification())
        {
            UnsetNotifyFlag(NOTIFY_OUTPUT);
            PLOG(PL_ERROR, "ProtoChannel::StartOutputNotification() error: notification update failure!\n");
            return false;
        }
    }
    return true;
}  // end ProtoChannel::StartInputNotification()

void ProtoChannel::StopOutputNotification()
{
    if (OutputNotification())
    {
        UnsetNotifyFlag(NOTIFY_OUTPUT);
        UpdateNotification();
    }
}  // end ProtoChannel::StopOutputNotification() 
         
bool ProtoChannel::UpdateNotification()
{
    if (NULL != notifier)
    {
        if (IsOpen())
        {
            if (!SetBlocking(false))
            {
                PLOG(PL_ERROR, "ProtoChannel::UpdateNotification() SetBlocking() error\n");
                return false;  
            }
            return notifier->UpdateChannelNotification(*this, GetNotifyFlags());
        }
        return true;
    }
    else
    {
        return SetBlocking(true);
    }
}  // end ProtoChannel::UpdateNotification()

bool ProtoChannel::SetBlocking(bool blocking)
{
#ifdef UNIX
    if (IsOpen() && (blocking_status != blocking))
    {
        if (blocking)
        {
            if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) & ~O_NONBLOCK))
            {
                PLOG(PL_ERROR, "ProtoChannel::SetBlocking() fcntl(F_SETFL(~O_NONBLOCK)) error: %s\n", GetErrorString());
                return false;
            }
        }
        else
        {
            if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) | O_NONBLOCK))
            {
                PLOG(PL_ERROR, "ProtoChannel::SetBlocking() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
                return false;
            }
        }
        blocking_status = blocking;
    }
#endif // UNIX
    return true;  //Note: taken care automatically under Win32 by WSAAsyncSelect(), overlapped i/o, etc???
}  // end ProtoChannel::SetBlocking()



#ifdef WIN32

bool ProtoChannel::InitOverlappedIO()
{
    // Set up event handles for overlapped i/o notifications
    if (NULL == (input_event_handle = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        input_event_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR, "ProtoChannel::InitOverlappedIO() CreateEvent(input_event_handle) error: %s\n", ::GetErrorString());
        return false;
    }
    if (NULL == (output_event_handle = CreateEvent(NULL,TRUE,FALSE,NULL)))
    {
        output_event_handle = INVALID_HANDLE_VALUE;
        PLOG(PL_ERROR, "ProtoChannel::InitOverlappedIO() CreateEvent(input_event_handle) error: %s\n", ::GetErrorString());
        CloseHandle(input_event_handle);
        input_event_handle = NULL;
        return false;
    }

    // Initialize overlapped i/o structs
    memset(&overlapped_read, 0, sizeof(overlapped_read));
	overlapped_read.hEvent = input_event_handle;
    memset(&overlapped_write, 0, sizeof(overlapped_write));
    overlapped_write.hEvent = output_event_handle;
	// Allocate buffers for overlapped read/write
    if (NULL == overlapped_read_buffer)
    {
        if (NULL == (overlapped_read_buffer = new char[OVERLAPPED_BUFFER_SIZE]))
        {
            PLOG(PL_ERROR, "ProtoChannel::InitOverlappedIO() new overlapped_read_buffer error: %s\n", ::GetErrorString());
            CloseHandle(input_event_handle);
            input_event_handle = NULL;
            CloseHandle(output_event_handle);
            output_event_handle = NULL;
            return false;
        }
    }
    if (NULL == overlapped_write_buffer)
    {
        if (NULL == (overlapped_write_buffer = new char[OVERLAPPED_BUFFER_SIZE]))
        {
            PLOG(PL_ERROR, "ProtoChannel::InitOverlappedIO() new overlapped_write_buffer error: %s\n", ::GetErrorString());
            CloseHandle(input_event_handle);
            input_event_handle = NULL;
            CloseHandle(output_event_handle);
            output_event_handle = NULL;
            delete[] overlapped_read_buffer;
            overlapped_read_buffer = NULL;
            return false;
        }
    }
    return true;
}  // end ProtoChannel::InitOverlappedIO()

bool ProtoChannel::StartOverlappedRead()
{
    if ((NULL == overlapped_read_buffer) && !InitOverlappedIO())
    {
        PLOG(PL_ERROR, "ProtoChannel::StartOverlappedRead() error: buffer allocation failed!\n");
        return false;
    }
    DWORD bytesRead;
    if (0 != ReadFile(input_handle, overlapped_read_buffer, OVERLAPPED_BUFFER_SIZE, &bytesRead, &overlapped_read))
    {
        // We got some data so use "input_ready" to get automatic notification
        overlapped_read_count = bytesRead;
        overlapped_read_index = 0;
        input_ready = true;
    }
    else
    {
        switch(GetLastError())
        {
            case ERROR_IO_PENDING:
                overlapped_read_count = overlapped_read_index = 0;
                input_ready = false;
                break;
            case ERROR_BROKEN_PIPE:
                PLOG(PL_ERROR, "ProtoPipe::Listen() ReadFile() error: %s\n", ::GetErrorString());
                Close();
                return false;
        }
    }
    return UpdateNotification();
}  // end ProtoChannel::StartOverlappedRead()

bool ProtoChannel::OverlappedRead(char* buffer, unsigned int& numBytes)
{
    // Note when "input_ready" changes state we need to do an UpdateNotification()
    bool wasInputReady = input_ready;
    LPOVERLAPPED overlapPtr = NULL;
    unsigned int want = numBytes;
    unsigned int got = 0;
    if (NULL != notifier)
    {
        // Only do overlapped I/O when a async i/o "notifier" is set
        overlapPtr = &overlapped_read;
        if (!input_ready) // an overlapped read operation is in place
        {
            DWORD bytesRead;
            if (FALSE != GetOverlappedResult(input_handle, overlapPtr, &bytesRead, FALSE))
            {
                unsigned int len = (bytesRead < want) ? bytesRead : want;
                memcpy(buffer, overlapped_read_buffer, len);
                got += len;
                overlapped_read_index = (len < bytesRead) ? len : 0;
                input_ready = true;  // assume handle is ready for reading until a pending overlapped i/o
            }
            else
            {
                numBytes = 0;
                switch (GetLastError())
                {
                    case ERROR_IO_INCOMPLETE:
                        // no data available yet
                        return true;
                    case ERROR_BROKEN_PIPE:
                        OnNotify(NOTIFY_NONE);
                        return false;
                    default:
                        PLOG(PL_ERROR, "ProtoPipe::Recv() GetOverlappedResult() error(%d): %s\n", 
                                       GetLastError(), ::GetErrorString());
                        return false;
                }
            }
        }
    }
    
    // First, copy any data remaining in "overlapped_read_buffer"?
    if ((overlapped_read_count > 0) && (got < want))
    {   
        unsigned int len  = want - got;
        if (len > overlapped_read_count) len = overlapped_read_count;
        memcpy(buffer+got, overlapped_read_buffer+overlapped_read_index, len);
        overlapped_read_count -= len;
        overlapped_read_index += len;
        got += len;
    }
    if (got < want)
    {
        // We need to try for more...
        // Note if NULL != overlapPtr (from above), more overlapped read may be triggered
        DWORD bytesRead;
        unsigned int len = want - got;
        if (len > OVERLAPPED_BUFFER_SIZE) len = OVERLAPPED_BUFFER_SIZE;
        if (0 != ReadFile(input_handle, overlapped_read_buffer, len, &bytesRead, overlapPtr))
        {
            memcpy(buffer+got, overlapped_read_buffer, bytesRead);
            got += bytesRead;
            input_ready = true;  // assume handle is ready for reading until a pending overlapped i/o
        }
        else
        {
            switch(GetLastError())
            {
                case ERROR_IO_PENDING:
                    overlapped_read_count = overlapped_read_index = 0;
                    input_ready = false;
                    break;
                case ERROR_BROKEN_PIPE:
                    if (0 == got)
                    {
                        OnNotify(NOTIFY_NONE);
                        return false;
                    }
                    break;
                default:
                    PLOG(PL_ERROR, "ProtoPipe::Recv() ReadFile(%d) error: %s\n", GetLastError(), ::GetErrorString());
                    if (0 == got) return false;
                    break;
                
            }
        }
    }
    numBytes = got;
    if ((NULL != notifier) && (input_ready != wasInputReady))
        UpdateNotification();
    return true;
    
}  // end ProtoChannel::OverlappedRead()

bool ProtoChannel::OverlappedWrite(const char* buffer, unsigned int& numBytes)
{
    // Note when "output_ready" changes state we need to do an UpdateNotification()
    bool wasOutputReady = output_ready;
    const char* bufferPtr = buffer;
    LPOVERLAPPED overlapPtr = NULL;

    if (NULL != notifier)
    {
        // We only do actual overlapped i/o when an async notifier has been set
        overlapPtr = &overlapped_write;
        if (!output_ready)
        {
            DWORD bytesWritten;
            if (FALSE == GetOverlappedResult(output_handle, overlapPtr, &bytesWritten, FALSE))
            {
                switch (GetLastError())
                {
                    case ERROR_IO_INCOMPLETE:
                        // Still not output ready
                        numBytes = 0;
                        return true;
                    default:
                        PLOG(PL_ERROR, "ProtoChannel::OverlappedWrite() GetOverlappedResult() error: %s\n", GetErrorString());
                        numBytes = 0;
                        return false;
                }
            }
        }
        // Copy data to overlapped i/o buffer, limiting "numBytes" as necessary
        if (numBytes > OVERLAPPED_BUFFER_SIZE) numBytes = OVERLAPPED_BUFFER_SIZE;


        memcpy(overlapped_write_buffer, buffer, numBytes);
		bufferPtr = overlapped_write_buffer;
    }
    DWORD bytesWritten;
    if (FALSE == WriteFile(output_handle, bufferPtr, numBytes, &bytesWritten, overlapPtr))
    {
        switch (GetLastError())
        {
            case ERROR_IO_PENDING:
                // assume requested numBytes (trimmed) will be written
                output_ready = false;
                break;
            case ERROR_BROKEN_PIPE:
                numBytes = 0;
                OnNotify(NOTIFY_NONE);
                // no break here on purpose
            default:
                numBytes = 0;
                PLOG(PL_ERROR,"Win32Vif::Write() WriteFile() error(%d): %s\n",GetLastError(),::GetErrorString());
                return false;;
        }
    }
    else
    {
        numBytes = bytesWritten;
        output_ready = true;
    }
    
    if ((NULL != notifier) && (output_ready != wasOutputReady))
        UpdateNotification();
    
    return true;
}   // end ProtoChannel::OverlappedWrite()

#endif // WIN32


