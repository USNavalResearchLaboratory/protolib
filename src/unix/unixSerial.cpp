
#include "protoSerial.h"
#include "protoDebug.h"

#include <termios.h> // for serial I/O stuff
#include <unistd.h>  // for close(), etc
#include <sys/ioctl.h>
#include <fcntl.h>

class UnixSerial : public ProtoSerial
{
    public:
        UnixSerial();
        static UnixSerial* Create();
        virtual ~UnixSerial();
        
        bool Open(const char* deviceName = NULL, AccessMode accessMode = RDWR, bool configure = false);
        void Close();

        bool Read(char* buffer, unsigned int& numBytes);
        bool Write(const char* buffer, unsigned int& numBytes);
        
        bool Set(StatusSignal signal);
        bool Clear(StatusSignal signal);
        bool IsSet(StatusSignal signal) const;
        
        bool SetStatus(int status) ;
        int GetStatus() const;
        
    private:
        bool SetConfiguration();
        bool GetConfiguration();
        
        static int TranslateStatusSignal(StatusSignal statusSignal);
        int GetUnixStatus() const;
        bool SetUnixStatus(int status);
      
};  // end class UnixSerial


ProtoSerial* ProtoSerial::Create()
{
    return static_cast<ProtoSerial*>(new UnixSerial);
}

UnixSerial::UnixSerial()
{
}

UnixSerial::~UnixSerial()
{
    Close();
}

bool UnixSerial::Open(const char* deviceName, AccessMode accessMode, bool configure)
{
    int flags = 0;
    switch (accessMode)
    {
        case RDONLY:
            flags = O_RDONLY;
            break;
        case WRONLY:
            flags = O_WRONLY;
            break;
        case RDWR:
            flags = O_RDWR;
            break;
    }
    TRACE("opening device ...");
    if ((descriptor = open(deviceName, flags)) < 0)
    {
        PLOG(PL_ERROR, "UnixSerial::Open() device open() error: %s\n", GetErrorString());
        return false;
    }    
    
    if (configure)
    {
        if (!SetConfiguration())
        {
            PLOG(PL_ERROR, "UnixSerial::Open() error: failed to set configuration\n", GetErrorString());
            Close();
            return false;
        }
    }
    else
    {
        TRACE("getting config ...");
        if (!GetConfiguration())
        {
            PLOG(PL_ERROR, "UnixSerial::Open() error: failed to get configuration\n", GetErrorString());
            Close();
            return false;
        }
    }
    TRACE("finalizing open ..\n");
    if (!ProtoSerial::Open())
    {
        Close();
        return false;
    }
    return true;
}  // end UnixSerial::Open()

void UnixSerial::Close()
{
    ProtoSerial::Close();
    close(descriptor);
    descriptor = INVALID_HANDLE;   
}  // end UnixSerial::Close()


bool UnixSerial::Read(char* buffer, unsigned int& numBytes)
{
    
    int result = read(descriptor, buffer, numBytes);
    if (result < 0)
    {
        numBytes = 0;
        switch (errno)
        {
            case EINTR:      // system interrupt
                TRACE("EINTR\n");
            case EAGAIN:     // no data ready
                TRACE("EAGAIN\n");
                break;
            default:
                PLOG(PL_ERROR, "UnixSerial::Read() read() error: %s\n", GetErrorString());
                return false;
        }
    }
    else
    {
        numBytes = result;
    }
    return true;
}  // end UnixSerial::Read()

bool UnixSerial::Write(const char* buffer, unsigned int& numBytes)
{
    int result = write(descriptor, buffer, numBytes);
    if (result < 0)
    {
        numBytes = 0;
        switch (errno)
        {
            case EINTR:      // system interrupt
            case EAGAIN:     // output buffer full
                break;
            default:
                PLOG(PL_ERROR, "UnixSerial::Write() write() error: %s\n", GetErrorString());
                return false;
        }
    }
    else
    {
        numBytes = result;
    }
    return true;
}  // end UnixSerial::Write()

bool UnixSerial::SetConfiguration()
{
    // Set serial port configuration
    struct termios attr;
    if (tcgetattr(descriptor, &attr) < 0)
    {
        PLOG(PL_ERROR, "UnixSerial::SetConfiguration: Error getting serial port configuration!\n");
        return false;   
    }
    
    // TBD - Should we init to a default CLOCAL config first?
    //cfmakeraw(&attr);
    //attr.c_cflag |= CLOCAL;
    
    // PARITY
    if (use_parity)
        attr.c_cflag |= PARENB;   // use parity
    else
        attr.c_cflag &= ~PARENB;  // no parity
    // BYTE SIZE
    attr.c_cflag &= ~CSIZE;  // First, clear byte size mask
    switch (byte_size)
    {
        case 5:
            attr.c_cflag |= CS5;
            break;
        case 6:
            attr.c_cflag |= CS6;
            break;
        case 7:
            attr.c_cflag |= CS7;
            break;
        case 8:
            attr.c_cflag |= CS8;
            break;
        default:
            PLOG(PL_ERROR, "UnixSerial::SetConfiguration: invalid byte size setting!\n");
            return false;
    }
    
    // LOCAL CONTROL
    if (local_control)
        attr.c_cflag |= CLOCAL;  // local flow control (ignore modem control lines)
    else
        attr.c_cflag &= ~CLOCAL; // slave to modem control lines
    
    attr.c_cflag |= CREAD;
    
    // ECHO
    if (echo_input)
        attr.c_lflag |= ECHO;  // local flow control (ignore modem control lines)
    else
        attr.c_lflag &= ~ECHO; // slave to modem control lines
    
    // BAUD RATE
    speed_t speed;
    switch(baud_rate)
    {
        // TBD - support more rates
        case 1200:
            speed = B1200;
            break;
        case 2400:
            speed = B2400;
            break;
        case 4800:
            speed = B4800;
            break;
        case 9600:
            speed = B9600;
            break;
        case 19200:
            speed = B19200;
            break;
        case 38400:
            speed = B38400;
            break;
        case 57600:
            speed = B57600;
            break;
        default:
            PLOG(PL_ERROR, "UnixSerial::SetConfiguration() error: unsupported or invalid baud rate\n");
            return false;
    }
    if (0 != cfsetispeed(&attr, speed))
    {
        PLOG(PL_ERROR, "UnixSerial::SetConfiguration() cfsetispeed() error: %s\n", GetErrorString());
        return false;   
    }
    if (0 != cfsetospeed(&attr, speed))
    {
        PLOG(PL_ERROR, "UnixSerial::SetConfiguration() cfsetospeed() error: %s\n", GetErrorString());
        return false;   
    }
    // READ TIMEOUT
    if (read_timeout < 0.0)  
    {
        // wait forever for at least one character
        attr.c_cc[VTIME]    = 0;
        attr.c_cc[VMIN]     = 1; 
    }
    else if (0.0 == read_timeout)
    {
        // return immediately with what has been buffered
        attr.c_cc[VTIME]    = 0;
        attr.c_cc[VMIN]     = 0; 
    }
    else
    {
        // return when 1 char is read or on timeout
        if (read_timeout < 0.1) read_timeout = 0.1;
        attr.c_cc[VTIME] = (int)(read_timeout*10.0 + 0.5);
        attr.c_cc[VMIN]  = 0; 
    }
   
    // Set the attributes
    if (tcsetattr(descriptor, TCSANOW, &attr) < 0)
    {
        PLOG(PL_ERROR, "UnixSerial::SetConfiguration() tcsetattr() error: %s\n", GetErrorString());
        return false;   
    }

#ifdef ASYNC_LOW_LATENCY  // (LINUX only?)  
    // (TBD) Move this out of the SetConfiguration() method?
    // _Attempt_ to set low latency if applicable
    struct serial_struct serinfo;
    if (0 != ioctl(descriptor, TIOCGSERIAL, &serinfo) < 0)
    {
        PLOG(PL_ERROR, "UnixSerial::SetConfiguration() warning: ioctl(TIOCGSERIAL) failure: %s\n", GetErrorString());
    }
    else
    {
        if (low_latency)
            serinfo.flags |= ASYNC_LOW_LATENCY;
        else
            serinfo.flags &= ~ASYNC_LOW_LATENCY;
        if (ioctl(descriptor, TIOCSSERIAL, &serinfo) < 0) 
        {
            PLOG(PL_ERROR, "UnixSerial::SetConfiguration() warning: ioctl(TIOCSSERIAL) error: %s\n", GetErrorString());
        }
    }
#endif // ASYNC_LOW_LATENCY
    return true;
}  // UnixSerial::SetConfiguration()

bool UnixSerial::GetConfiguration()
{
    // Get serial port attributes
    struct termios attr;
    if (tcgetattr(descriptor, &attr) < 0)
    {
        PLOG(PL_ERROR, "UnixSerial::SetConfiguration: Error getting serial port configuration!\n");
        return false;   
    }
    
    // BAUD RATE
    speed_t speed = cfgetispeed(&attr);
    switch(speed)
    {
        // TBD - support more rates
        case B1200:
            baud_rate = 1200;
            break;
        case B2400:
            baud_rate = 2400;
            break;
        case B4800:
            baud_rate = 4800;
            break;
        case B9600:
            baud_rate = 9600;
            break;
        case B19200:
            baud_rate = 19200;
            break;
        case B38400:
            baud_rate = 38400;
            break;
        case B57600:
            baud_rate = 57600;
            break;
        default:
            PLOG(PL_ERROR, "UnixSerial::GetConfiguration() error: unsupported or invalid baud rate\n");
            return false;
    }
    
    // BYTE SIZE
    switch (attr.c_cflag & CSIZE)
    {
        case CS5:
            byte_size = 5; // 5 bits per byte
            break;
        case CS6:
            byte_size = 6; // 6 bits per byte
            break;
        case CS7:
            byte_size = 7; // 7 bits per byte
            break;
        case CS8:
            byte_size = 8; // 8 bits per byte
            break;
        default:
            PLOG(PL_ERROR, "UnixSerial::GetConfiguration() error: unsupported or invalid byte size\n");
            return false;
    }
    
    // PARITY
    use_parity = (0 != (attr.c_cflag & PARENB));  
    
    // LOCAL CONTROL
    local_control = (0 != (attr.c_cflag & CLOCAL));  
    
    // READ TIMEOUT
    if (0 != attr.c_cc[VTIME])
        read_timeout = 0.1 * (double)attr.c_cc[VTIME];
    else if (0 != attr.c_cc[VMIN])
        read_timeout = -1.0;  // TBD - set VMIN to 1 ???
    else
        read_timeout = 0.0;
    
    return true;
    
}  // end  UnixSerial::GetConfiguration()


int UnixSerial::TranslateStatusSignal(StatusSignal statusSignal)
{
    switch (statusSignal)
    {
        case DTR:
            return (int)TIOCM_DTR; 
        case RTS:
            return TIOCM_RTS;
        case CTS:
            return TIOCM_CTS;
        case DCD:
            return TIOCM_CD;
        case DSR:
            return TIOCM_DSR;
        case RNG:
            return TIOCM_RNG;
    }
    return 0;
}  // end UnixSerial::TranslateStatusSignal()


bool UnixSerial::SetUnixStatus(int unixStatus)
{
    if (!IsOpen()) return 0;
    if (ioctl(descriptor, TIOCMSET, &unixStatus) < 0)
    {
        PLOG(PL_ERROR, "ProtoSerial::SetStatus() ioctl(TIOCMSET) error: %s\n", GetErrorString());
        return false;   
    }
    return true;
}  // end UnixSerial::SetUnixStatus()

bool UnixSerial::SetStatus(int status)
{
    int unixStatus = 0;
    if (0 != (status & DTR))
        unixStatus |= TIOCM_DTR;
    if (0 != (status & RTS))
        unixStatus |= TIOCM_RTS;
    if (0 != (status & CTS))
        unixStatus |= TIOCM_CTS;
    if (0 != (status & DCD))
        unixStatus |= TIOCM_CD;
    if (0 != (status & DSR))
        unixStatus |= TIOCM_DSR;
    if (0 != (status & RNG))
        unixStatus |= TIOCM_RNG;
    return SetUnixStatus(unixStatus);
}  // end UnixSerial::SetStatus()

int UnixSerial::GetUnixStatus() const
{
    if (!IsOpen()) return 0;
    int unixStatus;
    if (ioctl(descriptor, TIOCMGET, &unixStatus) < 0)
    {
        PLOG(PL_ERROR, "ProtoSerial::GetStatus() ioctl(TIOCMGET) error: %s\n", GetErrorString());
        return 0;   
    }
    return unixStatus;
}  // end ProtoSerial::GetUnixStatus()

int UnixSerial::GetStatus() const
{
    int unixStatus = GetUnixStatus();
    int status = 0;
    if (0 != (unixStatus & TIOCM_DTR))
        status |= DTR;
    if (0 != (unixStatus & TIOCM_RTS))
        status |= RTS;
    if (0 != (unixStatus & TIOCM_CTS))
        status |= CTS;
    if (0 != (unixStatus & TIOCM_CD))
        status |= DCD;
    if (0 != (unixStatus & TIOCM_DSR))
        status |= DSR;
    if (0 != (unixStatus & TIOCM_RNG))
        status |= RNG;
    return status;
}  // end UnixSerial::GetStatus()

bool UnixSerial::Set(StatusSignal statusSignal)
{
    int unixStatus = GetUnixStatus();
    unixStatus |= TranslateStatusSignal(statusSignal);
    return SetUnixStatus(unixStatus);
}  // end UnixSerial::Set()

bool UnixSerial::Clear(StatusSignal statusSignal)
{
    int unixStatus = GetUnixStatus();
    unixStatus &= ~TranslateStatusSignal(statusSignal);
    return SetUnixStatus(unixStatus);
}  // end UnixSerial::Clear()

bool UnixSerial::IsSet(StatusSignal statusSignal) const
{
    int unixStatus = GetUnixStatus();
    int unixSignal = TranslateStatusSignal(statusSignal);
    return (0 != (unixStatus & unixSignal));
}  // end UnixSerial::IsSet()




