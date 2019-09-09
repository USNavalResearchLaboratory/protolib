#ifndef _TEST_FUNCS
#define _TEST_FUNCS
#include "manetMsg.h"

#define PACKET_SIZE_MAX 1024

enum SmfMsg
{
    SMF_RESERVED = 0,
    SMF_HELLO    = 1
};

// SMF TLV types
enum SmfTlv
{
    SMF_TLV_RESERVED        = 0,
    SMF_TLV_RELAY_ALGORITHM = 1,
    SMF_TLV_HELLO_INTERVAL  = 2,
    SMF_TLV_RELAY_WILLING   = 3,
    SMF_TLV_LINK_STATUS     = 4,
    SMF_TLV_MPR_SELECT      = 5,
    SMF_TLV_RTR_PRIORITY    = 6
};

// SMF relay algorithm types
enum SmfRelayAlgorithm
{
    SMF_RELAY_CF        = 0,
    SMF_RELAY_SMPR      = 1,
    SMF_RELAY_ECDS      = 2,
    SMF_RELAY_MPR_CDS   = 3
};

// SMF neighbor link states
enum SmfLinkStatus
{
    SMF_LINK_RESERVED  = 0,
    SMF_LINK_LOST      = 1,
    SMF_LINK_HEARD     = 2,
    SMF_LINK_SYMMETRIC = 3
};


int BuildPacket(UINT32* buffer, ManetPkt& pkt);
void MakeDump(char* buffer, unsigned int buflen);
int ParseDump(const char* file);
unsigned int ParseDump(const char* file, UINT32* msgBuffer);
int ParseBuffer(UINT32* msgBuffer, unsigned int msgLength);

#endif // _TEST_FUNCS
