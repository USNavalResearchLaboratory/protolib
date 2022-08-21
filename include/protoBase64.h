#ifndef _PROTO_BASE64
#define _PROTO_BASE64

// This class implements Base64 text encoding (and decoding) of binary data per IETF RFC 4648
// By default, no maximum line length is imposed on encoder output and 
// the output is fully padded per the RFC's recommendation.  However, options
// are provided to enforce a maximum text line length and exclude padding if desired.

#include "protoDefs.h"

class ProtoBase64
{
    public:
        // Initializes encoding/decoding tables (called automatically if user doesn't)
        static void Init(); 
    
        // Returns length (in bytes) of encoding of "numBytes" of data
        static unsigned int ComputeEncodedSize(unsigned int numBytes, 
                                               unsigned int maxLineLength = 0, 
                                               bool         includePadding = true);
              
        // Returns length of encoded data in bytes (0 if "bufferSize" too small)
        static unsigned int Encode(const char*    inputBuffer, 
                                   unsigned int   inputBytes, 
                                   char*          outputBuffer, 
                                   unsigned int   bufferSize, 
                                   unsigned int   maxLineLength = 0,
                                   bool           includePadding = true);
        
        // Returns conservative (large) estimate of decoded data for "numBytes" of base64 characters
        static unsigned int EstimateDecodedSize(unsigned int    numBytes, 
                                                unsigned int    maxLineLength = 0);
        
        // Returns actual size (in bytes) of decoding of base64 characters in "inputBuffer"
        static unsigned int DetermineDecodedSize(const char*    inputBuffer, 
                                                 unsigned int   inputBytes);
        
        // Returns length of decoded data in bytes (0 if "bufferSize" too small)
        static unsigned int Decode(const char*   inputBuffer, 
                                   unsigned int  inputBytes, 
                                   char*         outputBuffer, 
                                   unsigned int  bufferSize);
        
        // TBD - Add  alternative "Decode()" and "DetermineDecodedSize()" methods that can use 
        //       a NULL-terminated string "inputBuffer"
        
    private:
        static bool initialized;
        static const char BASE64_ENCODE[]; 
        static char BASE64_DECODE[255];
        static const char PAD64;
        
};  // end class ProtoBase64


#endif // _PROTO_BASE64
