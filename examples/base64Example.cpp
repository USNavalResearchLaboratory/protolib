#include "protoBase64.h"

#include <stdio.h> // for printf() ...
#include <string.h>  // for strlen() 

int main(int argc, char* argv[])
{
    
    char text[] = "Hello, ProtoBase64 ...";
    
    unsigned int tlen = strlen(text);
    
    unsigned int maxLineLength = 0;
    bool includePadding = true;
    
    char encodeBuffer[256];
    char decodeBuffer[256];
    
    unsigned int encodeEst = ProtoBase64::ComputeEncodedSize(tlen, maxLineLength, includePadding);
    
    
    printf("base64 encoding text: \"%s\"\n", text);
    unsigned int encodeLen = ProtoBase64::Encode(text, tlen, encodeBuffer, 256, maxLineLength, includePadding);
    encodeBuffer[encodeLen] = '\0';
    
    unsigned int decodeEst = ProtoBase64::EstimateDecodedSize(encodeLen, maxLineLength);
    
    unsigned int decodeSize = ProtoBase64::DetermineDecodedSize(encodeBuffer, encodeLen);
    
    printf("base64 decoding data:  \"%s\"\n", encodeBuffer);
    unsigned int decodeLen = ProtoBase64::Decode(encodeBuffer, encodeLen, decodeBuffer, 256);
    decodeBuffer[decodeLen] = '\0';
    
    printf("base64 encode/decode sizes:: orig:%u encodeEst:%u encodeLen:%u decodeEst:%u decodeSize:%u decodeLen:%u\n",
            tlen, encodeEst, encodeLen, decodeEst, decodeSize, decodeLen);
    
    printf("base64 decoding result: \"%s\"\n", decodeBuffer);
    
}  // end main()

