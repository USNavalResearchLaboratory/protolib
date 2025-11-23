#ifndef _PROTO_SERIAL
#define _PROTO_SERIAL

/** 
* @class ProtoSerial 
* @brief This class is a generic base class for serial port I/O
*/

#include "protoChannel.h"

class ProtoSerial : public ProtoChannel
{
    public:
        static ProtoSerial* Create();
        virtual ~ProtoSerial();
        
        // Serial port "satus" signal enumeration
        // The pin assignment listed is for DB9 or DB25, respectively
        
        enum StatusSignal
        {
            DTR = 0x001,   // data terminal ready, pin 4/9 or 20/25  (output signal, typically DTR -> DSR)
            RTS = 0x002,   // request-to-send,     pin 7/9 or 04/25  (output signal, typically RTS -> CTS)
            CTS = 0x004,   // clear-to-send,       pin 8/9 or 05/25  (input signal)
            DCD = 0x008,   // data carrier detect, pin 1/9 or 08/25  (input signal)
            DSR = 0x010,   // data set ready,      pin 6/9 or 06/25  (input signal)
            RNG = 0x020    // ring indicator       pin 9/9 or 22/25  (input signal)
        };
            
            
        // Device access mode 
        enum AccessMode
        {
            RDONLY,  // read-only access
            WRONLY,  // write-only access
            RDWR     // read and write access
        };
        
        // These MUST be overridden for different implementations
        // ProtoSerial::Open() should also be called at the _end_ of derived
        // implementations' Open() method
        virtual bool Open(const char*   deviceName = NULL, 
                          AccessMode    accessMode = RDWR,
                          bool          configure = false)
        {
            return ProtoChannel::Open();
        }
        virtual bool IsOpen() const
            {return ProtoChannel::IsOpen();}
        // ProtoSerial::Close() should also be called at the _beginning_ of
        // derived implementation's Close() method
        virtual void Close() 
            {ProtoChannel::Close();}
        
        // Configuration set/get routines
        bool SetBaudRate(unsigned int baudRate);
        bool SetByteSize(unsigned int byteSize);
        bool SetParity(bool useParity);
        bool SetReadTimeout(double seconds);
        bool SetLocalControl(bool status);
        bool SetEcho(bool status);
        bool SetLowLatency(bool status);
        
        unsigned int GetBaudRate() const
            {return baud_rate;}
        unsigned int GetByteSize() const
            {return byte_size;}
        bool GetParity() const
            {return use_parity;}
        double GetReadTimeout() const
            {return read_timeout;}
        unsigned int GetLocalControl() const
            {return local_control;}
        
        // Status signal set/get methods
        virtual bool Set(StatusSignal signal) = 0;
        virtual bool Clear(StatusSignal signal) = 0;
        virtual bool IsSet(StatusSignal signal) const = 0;
        
        virtual bool SetStatus(int status) = 0;
        virtual int GetStatus() const = 0;
        static bool IsSet(int status, StatusSignal signal)
            {return (0 != (status & signal));}
        
        // Recv/Send data ...
        virtual bool Read(char* buffer, unsigned int& numBytes) = 0;
        virtual bool Write(const char* buffer, unsigned int& numBytes) = 0;
            
    protected:
        ProtoSerial();
    
        virtual bool SetConfiguration() = 0;
        virtual bool GetConfiguration() = 0;
    
        // Serial port device configuration
        unsigned int        baud_rate;     // baud rate (default = 9600)
        unsigned int        byte_size;     // bits per byte (default = 8)
        bool                use_parity;    // parity or no parity (default = no parity)
        double              read_timeout;  // read timeout (sec, default = 10 seconds)
        bool                local_control; // ignore modem control lines if true (default = true)
        bool                echo_input;
        bool                low_latency;
             
};  // end class ProtoSerial

#endif // _PROTO_SERIAL
