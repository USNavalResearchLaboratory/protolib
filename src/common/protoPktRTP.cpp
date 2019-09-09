/**
* @file protoPktRTP.cpp
* 
* @brief Provides access to RTP Packets
*/
#include "protoPktRTP.h"

const UINT16 ProtoPktRTP::SEQUENCE_MAX = 0xffff;    // 16 bits, unsigned (may move to private later)
const unsigned int ProtoPktRTP::BASE_HDR_LEN = 12;  // base header size, as of RFC 3550
const unsigned int ProtoPktRTP::CSRC_COUNT_MAX = 15;
		
ProtoPktRTP::ProtoPktRTP(UINT32*        bufferPtr,
                         unsigned int   numBytes, 
                         unsigned int   pktLength,          
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
	if (0 != pktLength) 
        InitFromBuffer(pktLength);
    else
        Init();
}

ProtoPktRTP::~ProtoPktRTP()
{
}

bool ProtoPktRTP::InitFromBuffer(unsigned int   pktLength,
                                 UINT32*        bufferPtr, 
                                 unsigned int   bufferBytes, 
                                 bool           freeOnDestruct)
{
    if (ProtoPkt::InitFromBuffer(pktLength, bufferPtr, bufferBytes, freeOnDestruct))  // sets "pkt_length"
    {
        if (pktLength < BASE_HDR_LEN)
        {
            if (NULL != buffer_ptr)
                PLOG(PL_ERROR, "ProtoPktRTP::InitFromBuffer() error: insufficient buffer_ptr space (1)\n");
            return false;
        }
        if (VERSION != GetVersion())
        {
            PLOG(PL_ERROR, "ProtoPktRTP::InitFromBuffer() error: incompatible version number: %d\n", GetVersion());
            return false;
        }
        unsigned int hdrLength = BASE_HDR_LEN + GetCsrcCount()*sizeof(UINT32) + GetExtensionLength();
        if (pktLength < hdrLength)
        {
            PLOG(PL_ERROR, "ProtoPktRTP::InitFromBuffer() error: bad RTP header for given pkt_length\n");
        }
        // (TBD) We could check the padding status, too for an extra validity check
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktRTP::InitFromBuffer() error: insufficient buffer_ptr space (2)\n");
	    return false;
    }
}  // end ProtoPktRTP::InitFromBuffer()

bool ProtoPktRTP::GetExtension(Extension& extension) const
{
    if (HasExtension())
    {
        unsigned int extOffset = OFFSET_CSRC_LIST + GetCsrcCount();
        ASSERT(pkt_length > (extOffset << 2));
        return extension.InitFromBuffer(buffer_ptr + extOffset, pkt_length - (extOffset << 2));
    }
    else
    {
        return false;
    }
}  // end ProtoPktRTP::GetExtension()

bool ProtoPktRTP::Init(UINT32* bufferPtr, unsigned int bufferBytes, bool freeOnDestruct)
{
   if (NULL != bufferPtr) AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
   if (buffer_bytes >= BASE_HDR_LEN)
   {
       /// zero everything ...
       memset((char*)buffer_ptr, 0, BASE_HDR_LEN);
       SetVersion(VERSION);
       pkt_length = BASE_HDR_LEN;
       return true;
   }
   else
   {
       if (NULL != bufferPtr)
           PLOG(PL_ERROR, "ProtoPktRTP::Init() error: insufficient buffer_ptr space\n");
       pkt_length = 0;
       return false;
   }
}  // end ProtoPktRTP::Init()

bool ProtoPktRTP::AppendCsrc(UINT32 csrcId)
{
    UINT8 index = GetCsrcCount();  // count before adding next contributing source (CSRC) (next index to use)
    if ((index < CSRC_COUNT_MAX) && ((BASE_HDR_LEN + (index << 2)) <= buffer_bytes))
    {            
		buffer_ptr[OFFSET_CSRC_LIST+index] = htonl(csrcId);  
        reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_CSRC_COUNT] &= 0xf0;  // zero out any previous value
		reinterpret_cast<UINT8*>(buffer_ptr)[OFFSET_CSRC_COUNT] |= ++index;  //  update
		pkt_length += 4;
		return true;
	}
	else
	{
        PLOG(PL_ERROR, "ProtoPktRTP::AppendCsrc() error: insufficient buffer_ptr or max CSRC count exceeded\n");
		return false;   // no more room in header (according to RFC 3550)
	} 
}  // end ProtoPktRTP::AppendCsrc()

bool ProtoPktRTP::AttachExtension(Extension& extension)
{
    unsigned int extOffset = OFFSET_CSRC_LIST + GetCsrcCount();
    bool result = extension.Init(buffer_ptr + extOffset, buffer_bytes - (extOffset << 2));
    if (result) extension.AttachRtpPacket(this);
    return result;
}  // end ProtoPktRTP::AttachExtension()
        
bool ProtoPktRTP::PackExtension(Extension& extension)
{
    unsigned int newLength = (OFFSET_CSRC_LIST + GetCsrcCount()) << 2;  // in bytes
    ASSERT(pkt_length == newLength); 
    newLength += extension.GetLength();
    if (newLength <= buffer_bytes)
    {
        pkt_length = newLength;
        SetFlag(EXTENSION);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktRTP::PackExtension() error: insufficient buffer_ptr space\n");
        return false;
    }
}
// end ProtoPktRTP::PackExtension()


void ProtoPktRTP::SetPadding(UINT8 numBytes, char* paddingPtr) 
{
	if (numBytes > 0)
	{
		memcpy(reinterpret_cast<UINT8*>(buffer_ptr)+pkt_length, paddingPtr, 
		       (NULL != paddingPtr) ? numBytes : 0);
		pkt_length += numBytes - GetPaddingLength();
		reinterpret_cast<UINT8*>(buffer_ptr)[pkt_length-1] = numBytes;  // fill in padding count
		SetFlag(PADDING);
	}
}  // end ProtoPktRTP::SetPadding()


ProtoPktRTP::Extension::Extension(UINT32*       bufferPtr, 
                                  unsigned int  numBytes, 
                                  bool          initFromBuffer,
                                  bool          freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct), rtp_pkt(NULL)
{
    if (initFromBuffer) InitFromBuffer(bufferPtr, numBytes, freeOnDestruct);
}


ProtoPktRTP::Extension::~Extension()
{
}

bool ProtoPktRTP::Extension::InitFromBuffer(UINT32*         bufferPtr, 
                                            unsigned int    bufferBytes, 
                                            bool            freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    if (buffer_bytes > (OFFSET_LENGTH << 1))
    {
        unsigned int extLength = GetDataLength() + 4;
        if (extLength <= buffer_bytes)
        {
            pkt_length = extLength;
            return true;
        }
    }
    if (NULL != buffer_ptr)
        PLOG(PL_ERROR, "ProtoPktRTP::Extension::InitFromBuffer() error: insufficient buffer space\n");
    return false;
}  // end ProtoPktRTP::InitFromBuffer()

bool ProtoPktRTP::Extension::Init(UINT32*       bufferPtr, 
                                  unsigned int  bufferBytes,
                                  bool          freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    if (bufferBytes >= 4)
    {
        SetDataLength(0);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktRTP::Extension::Init() error: insufficient buffer space\n");
        return false;
    }
}  // end ProtoPktRTP::Extension::Init()

bool ProtoPktRTP::Extension::SetData(const char* dataPtr, unsigned int numBytes)
{
    if ((4 + numBytes) <= buffer_bytes)
    {
        memcpy((char*)AccessData(), dataPtr, numBytes);
        SetDataLength(numBytes);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoPktRTP::Extension::SetData() error: insufficient buffer space\n");
        return false;
    }
}  // end ProtoPktRTP::Extension::SetData()
