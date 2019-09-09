#include "protoBase64.h"
#include <string.h>  // for memset()

// This class implements Base64 encoding (and decoding) per IETF RFC 4648
// By default, no maximum line length is imposed on encoder output and 
// the output is fully padded per the RFC's recommendation.  However, options
// are provided to enforce a maximum text line length and exclude padding if desired.

// Our static encoding / decoding tables
bool ProtoBase64::initialized = false;
const char ProtoBase64::PAD64 = '=';
const char ProtoBase64::BASE64_ENCODE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";   
char ProtoBase64::BASE64_DECODE[255];

void ProtoBase64::Init()
{
    // Init to invalid value
    memset(BASE64_DECODE, -1, 255);
    for (unsigned int i = 0; i < 64; i++)
        BASE64_DECODE[(unsigned char)BASE64_ENCODE[i]] = i;
    initialized = true;
}  // end ProtoBase64::Init()

unsigned int ProtoBase64::ComputeEncodedSize(unsigned int numBytes, unsigned int maxLineLength, bool includePadding)
{
    unsigned int quads = numBytes / 3;
    unsigned int size = 4 * quads;
    unsigned int remainder = numBytes % 3;
    if (0 != remainder)
        size += (includePadding ? 4 : (remainder + 1));
    if (maxLineLength > 0)
    {
        // Account for addition of CR/LF for each _full_ line.
        unsigned int numLines = size / maxLineLength;
        size += (numLines * 2);
    }
    return size;
}  // end ProtoBase64::ComputeEncodedSize()


// This inline helper function sets a value in the encoder output buffer, making sure there is sufficient
// buffer space _and_ inserting CR/LF whenever maxLineLength (if applicable) is reached
// (This also updates the reference "outdex" and "lineLength" parameters passed in)
// Note this return "true" on success and "false" when there is insufficient buffer space
static inline bool SetOutputValue(char value, char* buffer, unsigned int buflen, unsigned int& outdex, unsigned int& lineLength, unsigned int maxLineLength)
{
    if (outdex < buflen)
    {
        buffer[outdex++] = value;
        if (++lineLength == maxLineLength)
        {
            lineLength = 0;
            if ((outdex + 1) < buflen)
            {
                buffer[outdex++] = '\r';
                buffer[outdex++] = '\n';
            }
            else
            {
                return false;  // insufficient output buffer space
            }
        }
        return true;
    }
    else
    {
        return false;  // insufficient output buffer space
    }
}  // end SetOutputValue()

unsigned int ProtoBase64::Encode(const char*    input, 
                                 unsigned int   numBytes, 
                                 char*          buffer, 
                                 unsigned int   buflen, 
                                 unsigned int   maxLineLength,
                                 bool           includePadding)
{
    if (!initialized) ProtoBase64::Init();
    // Every 3 bytes in yields 4 bytes out (plus CR/LF if maxLineLength >= 0)
    unsigned int index = 0;
    unsigned int outdex = 0;
    unsigned int lineLength = 0;
    while (numBytes > 0)
    {
        // At least one output value is generated
        unsigned char morsel = (input[index] >> 2) & 0x3f;
        if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
            return 0; // insufficient output buffer space
        morsel = (input[index++] << 4) & 0x3f;
        switch (numBytes)
        {
            case 1:  // Only 1 input byte remains
            {
                // Generate one more output value plus two pad characters (if including padding)
                if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
                    return 0; // insufficient output buffer space
                if (includePadding)
                {
                    // Two pad characters are inserted in this case
                    if (!SetOutputValue(PAD64, buffer, buflen, outdex, lineLength, maxLineLength))
                        return 0; // insufficient output buffer space
                    if (!SetOutputValue(PAD64, buffer, buflen, outdex, lineLength, maxLineLength))
                        return 0; // insufficient output buffer space
                }
                numBytes = 0;
                break;
            }
            case 2:  // Only 2 input bytes remain
            {
                // Generate two more output values plus one pad character (if including padding)
                morsel |= (input[index] >> 4) & 0x0f;
                if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
                    return 0; // insufficient output buffer space
                morsel = (input[index++] << 2) & 0x3f;
                if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
                    return 0; // insufficient output buffer space
                if (includePadding)
                {
                    // Only on epad character is inserted in this case
                    if (!SetOutputValue(PAD64, buffer, buflen, outdex, lineLength, maxLineLength))
                        return 0; // insufficient output buffer space
                }
                numBytes = 0;
                break;
            }
            default:  // 3 or more input bytes remain
            {
                // Generate 3 more output values
                morsel |= (input[index] >> 4) & 0x0f;
                if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
                    return 0; // insufficient output buffer space
                morsel = (input[index++] << 2) & 0x3f;
                morsel |= (input[index] >> 6) & 0x03;
                if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
                    return 0; // insufficient output buffer space
                morsel = input[index++] & 0x3f;
                if (!SetOutputValue(BASE64_ENCODE[morsel], buffer, buflen, outdex, lineLength, maxLineLength))
                    return 0; // insufficient output buffer space
                numBytes -= 3;
                break;
            }
        }
    }  // end while (numBytes > 0)
    // NULL-terminate the encoded base64 text if there is space
    if (outdex < buflen) buffer[outdex] = '\0';
    return outdex;
}  // end ProtoBase64::Encode()
        
unsigned int ProtoBase64::EstimateDecodedSize(unsigned int numBytes, unsigned int maxLineLength)
{
    // This is a _conservative_ estimate (i.e., may over-estimate size)
    if (maxLineLength > 0)
    {
        // Account for CR/LF if line length limit is imposed
        unsigned int numLines = numBytes / (maxLineLength + 2);
        unsigned int lineFeedBytes = 2 * numLines;
        // Note, to be conservative, we don't assume a short line has CR/LF
        numBytes -= lineFeedBytes;
    }
    // Assume each 4 bytes of encoded data yields 3 bytes of decoded data
    unsigned int quads = numBytes / 4;
    unsigned int size = 3 * quads;
    unsigned int remainder = numBytes % 4;
    if (remainder > 1) // 1 byte remainder probably extraneous character
        size += (remainder - 1);
    return size;
}  // end ProtoBase64::EstimateDecodedSize()

unsigned int ProtoBase64::DetermineDecodedSize(const char* input, unsigned int numBytes)
{
    if (!initialized) Init();
    // Each 4 bytes of input (CR/LF, etc withstanding) yields 3-bytes of output
    unsigned int validBytes = 0;
    for (unsigned int index = 0; index < numBytes; index++)
    {
        char value = input[index];
        if (PAD64 == value) continue;
        char bits = BASE64_DECODE[(unsigned char)value];
        if (bits < 0) continue;  // non-Base64 character
        validBytes += 1;
    }
    // For every 4 "valid encoded bytes", 3 output bytes are generated
    unsigned int quads = validBytes / 4;
    unsigned int size = 3 * quads;
    unsigned int remainder = validBytes % 4;
    // Note "remainder" _should_ only be 0, 2, or 3
    if (remainder > 1)
        size += (remainder - 1);
    return size;
}  // end ProtoBase64::DetermineDecodedSize()

unsigned int ProtoBase64::Decode(const char* input, unsigned int numBytes, char* buffer, unsigned int buflen)
{
    if (!initialized) Init();
    // Each 4 bytes of input (CR/LF, etc withstanding) yields 3-bytes of output
    char output = 0;
    unsigned int outdex = 0;
    unsigned int offset = 0;
    for (unsigned int index = 0; index < numBytes; index++)
    {
        char value = input[index];
        if (PAD64 == value) continue;
        char bits = BASE64_DECODE[(unsigned char)value];
        if (bits < 0) continue;  // non-Base64 character
        switch (offset)  // 0, 1, 2, 3
        {
            case 0:  // First 6 bits of 24 (just save 6 bits to "output")
                output = bits << 2;
                offset = 1;
                break;
            case 1:  // Second 6 bits (use 2 bits with prev "output" and save 4)
                output |= (bits >> 4) & 0x03;
                if (outdex >= buflen) return 0; // insufficient buffer space
                buffer[outdex++] = output;
                output = bits << 4;
                offset = 2;
                break;
            case 2:  // Third 6 bits (use 4 bits with prev "output" and save 2)
                output |= (bits >> 2) & 0x0f;
                if (outdex >= buflen) return 0; // insufficient buffer space
                buffer[outdex++] = output;
                output = bits << 6;
                offset = 3;
                break;
            case 3:  // Fourth 6 bits (use all 6 bits with prev "output")
                output |= bits;
                if (outdex >= buflen) return 0; // insufficient buffer space
                buffer[outdex++] = output;
                offset = 0;
                break;
        }
    }  // end for (index = 0..numBytes)
    return outdex;
}  // end ProtoBase64::Decode()
