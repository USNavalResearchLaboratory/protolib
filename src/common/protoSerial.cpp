#include "protoSerial.h"

ProtoSerial::ProtoSerial()
: baud_rate(9600), byte_size(8), use_parity(false), 
  read_timeout(10.0), local_control(true),
  low_latency(false)
{
}

ProtoSerial::~ProtoSerial()
{
    Close();
}
    
bool ProtoSerial::SetBaudRate(unsigned int baudRate)
{
    baud_rate = baudRate;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetBaudRate()

bool ProtoSerial::SetByteSize(unsigned int byteSize)
{
    byte_size = byteSize;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetBaudRate()

bool ProtoSerial::SetParity(bool useParity)
{
    use_parity = useParity;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetParity()

bool ProtoSerial::SetReadTimeout(double seconds)
{
    read_timeout = seconds;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetReadTimeout()

bool ProtoSerial::SetLocalControl(bool status)
{
    local_control = status;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetLocalControl()

bool ProtoSerial::SetEcho(bool status)
{
    echo_input = status;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetEcho()

bool ProtoSerial::SetLowLatency(bool status)
{
    low_latency = status;
    return (IsOpen() ? SetConfiguration() : true);
}  // end ProtoSerial::SetLowLatency()

