// This the the "node test" (nt) program to exercise our node, iface, link data structures


#include "manetMsg.h"
#include "testFuncs.h"
#include <stdio.h>  // for sprintf()
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>  // for isprint
int main(int argc, char* argv[])
{

    // Original program used TRACE statements that we have changed to PL_INFO
    // for library use
    SetDebugLevel(3);

    if (argc > 1)
    {
        return ParseDump(argv[1]);
    }

    UINT32 buffer[PACKET_SIZE_MAX/4];
    ManetPkt pkt;
    if (!BuildPacket((UINT32*)buffer,pkt))
    {
        PLOG(PL_ERROR,"MsgExample::BuildPacket() failes\n");
        return -1;
    }

    // OK, let's parse a "received" packet using sent "buffer" and "pktLen"
    PLOG(PL_INFO,"msgExample: Parsing \"recvPkt\" ...\n");
    UINT16 pktLen = pkt.GetLength();

    MakeDump((char*)buffer, pktLen);

    int result = ParseBuffer(buffer, pktLen);

    PLOG(PL_INFO,"msgExample: \"recvPkt\" packet parsing completed.\n\n");

    MakeDump((char*)buffer, pktLen);

    return result;
}  // end main()

