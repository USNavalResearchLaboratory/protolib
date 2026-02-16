/**
* @file protoPktRTP.cpp
* 
* @brief Provides access to RTP Packets
*/
#include "protoPktRTP.h"

const UINT16 ProtoPktRTP::SEQUENCE_MAX = 0xffff;    // 16 bits, unsigned (may move to private later)
const unsigned int ProtoPktRTP::BASE_HDR_LEN = 12;  // base header size, as of RFC 3550
const unsigned int ProtoPktRTP::CSRC_COUNT_MAX = 15;
		
ProtoPktRTP::ProtoPktRTP(void*          bufferPtr,
                         unsigned int   numBytes, 
                         unsigned int   pktLength,          
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
	    if (0 != pktLength) 
            InitFromBuffer(pktLength);
        else
            InitIntoBuffer();
    }
}

ProtoPktRTP::~ProtoPktRTP()
{
}

bool ProtoPktRTP::InitFromBuffer(unsigned int   pktLength,
                                 void*          bufferPtr, 
                                 unsigned int   bufferBytes, 
                                 bool           freeOnDestruct)
{
    if (ProtoPkt::InitFromBuffer(pktLength, bufferPtr, bufferBytes, freeOnDestruct))  // sets "pkt_length"
    {
        if (pktLength < BASE_HDR_LEN)
        {
            if (NULL != bufferPtr) DetachBuffer();
            PLOG(PL_ERROR, "ProtoPktRTP::InitFromBuffer() error: insufficient pktLength\n");
            return false;
        }
        if (VERSION != GetVersion())
        {
            if (NULL != bufferPtr) DetachBuffer();
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
        PLOG(PL_ERROR, "ProtoPktRTP::InitFromBuffer() error: insufficient buffer space\n");
	    return false;
    }
}  // end ProtoPktRTP::InitFromBuffer()

bool ProtoPktRTP::GetExtension(Extension& extension)
{
    if (HasExtension())
    {
        unsigned int extOffset = OFFSET_CSRC_LIST + GetCsrcCount();
        ASSERT(ProtoPkt::GetLength() > (extOffset << 2));
        return extension.InitFromBuffer(AccessBuffer32(extOffset), ProtoPkt::GetLength() - (extOffset << 2));
    }
    else
    {
        return false;
    }
}  // end ProtoPktRTP::GetExtension()

bool ProtoPktRTP::Init(void* bufferPtr, unsigned int bufferBytes, bool freeOnDestruct)
{
   if (NULL != bufferPtr) AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
   if (GetBufferLength() >= BASE_HDR_LEN)
   {
       /// zero everything ...
       memset((char*)AccessBuffer(), 0, BASE_HDR_LEN);
       SetVersion(VERSION);
       ProtoPkt::SetLength(BASE_HDR_LEN);
       return true;
   }
   else
   {
       if (NULL != bufferPtr) DetachBuffer();
       PLOG(PL_ERROR, "ProtoPktRTP::Init() error: insufficient buffer_ptr space\n");
       ProtoPkt::SetLength(0);
       return false;
   }
}  // end ProtoPktRTP::Init()

bool ProtoPktRTP::AppendCsrc(UINT32 csrcId)
{
    UINT8 index = GetCsrcCount();  // count before adding next contributing source (CSRC) (next index to use)
    if ((index < CSRC_COUNT_MAX) && ((BASE_HDR_LEN + (index << 2)) <= GetBufferLength()))
    {            
		SetWord32(OFFSET_CSRC_LIST+index, csrcId);
        UINT8& byte = AccessUINT8(OFFSET_CSRC_COUNT);
        byte &= 0xf0;  // zero out any previous value
		byte |= ++index;  //  update
		ProtoPkt::SetLength(4 + ProtoPkt::GetLength());
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
    bool result = extension.Init(AccessBuffer32(extOffset), GetBufferLength() - (extOffset << 2));
    if (result) extension.AttachRtpPacket(this);
    return result;
}  // end ProtoPktRTP::AttachExtension()
        
bool ProtoPktRTP::PackExtension(Extension& extension)
{
    unsigned int newLength = (OFFSET_CSRC_LIST + GetCsrcCount()) << 2;  // in bytes
    ASSERT(ProtoPkt::GetLength() == newLength);  // ???
    newLength += extension.GetLength();
    if (newLength <= GetBufferLength())
    {
        ProtoPkt::SetLength(newLength);
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
		memcpy(AccessBuffer(ProtoPkt::GetLength()), paddingPtr, (NULL != paddingPtr) ? numBytes : 0);
		ProtoPkt::SetLength(ProtoPkt::GetLength() + numBytes - GetPaddingLength());
		SetUINT8(ProtoPkt::GetLength() - 1, numBytes);
		SetFlag(PADDING);
	}
}  // end ProtoPktRTP::SetPadding()


ProtoPktRTP::Extension::Extension(void*         bufferPtr, 
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

bool ProtoPktRTP::Extension::InitFromBuffer(void*           bufferPtr, 
                                            unsigned int    bufferBytes, 
                                            bool            freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    if (GetBufferLength() > (OFFSET_LENGTH << 1))
    {
        unsigned int extLength = GetDataLength() + 4;
        if (extLength <= GetBufferLength())
        {
            ProtoPkt::SetLength(extLength);
            return true;
        }
    }
    if (NULL != bufferPtr) DetachBuffer();
    PLOG(PL_ERROR, "ProtoPktRTP::Extension::InitFromBuffer() error: insufficient buffer space\n");
    return false;
}  // end ProtoPktRTP::InitFromBuffer()

bool ProtoPktRTP::Extension::Init(void*         bufferPtr, 
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
    if ((4 + numBytes) <= GetBufferLength())
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
