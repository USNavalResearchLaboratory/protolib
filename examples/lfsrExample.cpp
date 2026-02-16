// This program validates and illustrates the use of the ProtoLFSR class

#include "protoTime.h"
#include "protoLFSR.h"
#include <stdio.h>
#include <stdlib.h>  // for rand(), srand()

int main(int argc, char* argv[])
{   
    ProtoTime t;
    t.GetCurrentTime();
    srand(t.usec());
    
    ProtoLFSR::Polynomial poly = ProtoLFSR::PN31;
    
    // This section validates get next/prev _bit_
    // (for both ProtoLFSR and ProtoLFSRX)
    ProtoLFSR pn(poly);
    for (int i = 0; i < 22; i++) pn.GetNextByte();
    
    const int TEST_BYTES = 10;
    printf("Generating seq: "); 
    UINT8 testBuffer[TEST_BYTES];
    for (int i = 0; i< TEST_BYTES; i++)
    {
        UINT8 byte = pn.GetNextByte();
        testBuffer[i] = byte;
        printf("%d, ", byte);
    }
    printf("\n");
    
    pn.Reset();
    pn.Sync((char*)testBuffer, TEST_BYTES);
    printf("poly: 0x%08x bits:%u state: 0x%08x\n",
            pn.GetPolynomial(), pn.GetNumBits(), pn.GetState());
    printf("   Syncing seq: ");
    for (int i = 0; i< TEST_BYTES; i++)
    {
        UINT8 byte = pn.GetNextByte();
        printf("%d, ", byte);
    }
    printf("\n");
    
    // This validates our Berlekamp-Massey "ComputePolynomial()" method
    printf("    Random seq: ");
    for (int i = 0; i < TEST_BYTES; i++)
    {
        UINT8 byte = rand() % 256;
        //UINT8 byte = i*5 + 22;
        //UINT8 byte = pn.GetNextByte();  // this should gives us a short register
        int j = 0;
        while (j < i)
        {
            if (byte == testBuffer[j])
            {
                // Non-unique, so try again
                byte = rand() % 256;
                j = 0;
            }
            else
            {
                j++;
            }
        }
        testBuffer[i] = byte;
        printf("%d, ", testBuffer[i]);
    }
    printf("\n");
    ProtoLFSRX pnx;
    pnx.ComputePolynomial((char*)testBuffer, 8*TEST_BYTES);
    UINT32 xpoly = *pnx.GetPolynomial();
    UINT32 xstate = *pnx.GetState();
    printf("xpoly: 0x%08x bits:%u state:0x%08x\n", xpoly, pnx.GetNumBits(), xstate);
    printf("  Matching seq: ");
    for (int i = 0; i< TEST_BYTES; i++)
    {
        UINT8 byte = pnx.GetNextByte();
        printf("%d, ", byte);
    }
    printf("\n");
    
    // Directly set the polynomial and see what we get
    UINT32 p = (UINT32)poly;
    pnx.SetPolynomial(&p, 5);
    printf("  Direct set seq: ");
    for (int i = 0; i< TEST_BYTES; i++)
    {
        UINT8 byte = pnx.GetNextByte();
        printf("%d, ", byte);
    }
    printf("\n");
    
    
    //return 0;
    
    // Here we just directly set the lfsrs and compare
    pn.Reset();
    pnx.Reset();
    
    int k;
    for (k = 0; k < 16; k++)
    {
        UINT32 state = pn.GetState();
        xstate = *pnx.GetState();
        printf("%d) bit:%d (state = 0x%08x) xbit:%d (xstate = 0x%08x) \n", 
                k, pn.GetNextBit(), state, pnx.GetNextBit(), xstate);
    }
    printf("\n");
    k -= 2;
    for (; k >= 0; k--)
    {
        UINT32 state = pn.GetState();
        xstate = *pnx.GetState();
        printf("%d) bit:%d (state = 0x%08x) xbit:%d (xstate = 0x%08x) \n", 
                k, pn.GetPrevBit(), state, pnx.GetPrevBit(), xstate);
    }
    
    
    //return 0;
    
    // This validates Seek() 
    for (k =0 ; k < (int)pn.GetNumBytes(); k++)
        printf("pn[%d] = 0x%02x\n", k, pn.GetNextByte());
    pn.Seek(-3*8);
    k -= 3;
    printf("seek pn[%d] = = 0x%02x\n", k, pn.GetNextByte());
    k += 2;
    pn.Seek(1*8);
    printf("seek pn[%d] = = 0x%02x\n", k, pn.GetNextByte());
    
    
    //return 0;
    // This validates get next/prev _byte_
    
    // Build a sequence starting with state from first "numBits"
    ProtoLFSR txReg(poly, 0x08);
    
    //ProtoLFSRX txReg;  // alternative ProtoLFSRX validation
    //UINT32 init = 0x08;
    //txReg.SetPolynomial(&p, 11, &init);
    
    char syncBuf[32];  // overkill buffer size
    
    printf("txReg[-1] = 0x%02x\n", txReg.GetPrevByte());
    
    txReg.FillBuffer(syncBuf, 32);
    
    ProtoLFSR rxReg(poly, 0x12);
    //ProtoLFSRX rxReg;  // alternative ProtoLFSRX validation
    //init = 0x12;
    //rxReg.SetPolynomial(&p, 11, &init);
    
    int offset = 3;
    printf("syncing to 0x%02x...\n", (UINT8)syncBuf[offset]);
    if (!rxReg.Sync(syncBuf+offset, txReg.GetNumBytes() - offset))
        printf("SYNC ERROR\n");
    
    
    printf("synced state = 0x%08x\n", rxReg.GetState());
    for (unsigned int i = 0; i < txReg.GetNumBytes(); i++)
        printf("txReg[%d] = 0x%02x\n", i, (UINT8)syncBuf[i]);
    
    for (int i = (int)txReg.GetNumBytes()-2; i >= -offset; i--)
        printf("txReg[%d] = 0x%02x\n", i, txReg.GetPrevByte());
    
    printf("\n");
    
    printf("rxReg[-1] = 0x%02x\n", rxReg.GetPrevByte());
    printf("rxReg[-2] = 0x%02x\n", rxReg.GetPrevByte());
    rxReg.GetNextByte();
    for (unsigned int i = 0; i < txReg.GetNumBytes(); i++)
        printf("rxReg[%d] = 0x%02x\n", i, rxReg.GetNextByte());
    
    //return 0;
    
    // This validates the m-sequence auto-correlation properties
    // Create reference buffer with all possible shifts of sequence
    unsigned int numBits = ProtoLFSR::GetPolySize(poly);
    const UINT32 LEN = 0x00000001 << numBits;
    char** refSeq = new char*[LEN];
    if (NULL == refSeq)
    {
        perror("lfsrExample: new refSeq error");
        return -1;
    }
    memset(refSeq, 0, LEN*sizeof(char*));
    for (unsigned int i = 0; i < LEN; i++)
    {
        if (NULL == (refSeq[i] = new char[LEN >> 3]))
        {
            perror("lfserExample: new refSeq[] error");
            for (unsigned int j = 0; j < i; j++) delete[] refSeq[j];
            delete[] refSeq;
            return -1;
        }
        ProtoLFSR lfsr(poly);
        lfsr.Seek(i);
        lfsr.FillBuffer(refSeq[i], LEN >> 3);
    }
    
    unsigned int minMask = 0;
    unsigned int mask = 0x0500;
    unsigned int wtMin = 0xffffffff;
            
    for (unsigned int i = 1; i < LEN - 1; i++)
    {
        unsigned int wt = 0;
        for (unsigned int j = 0; j < LEN>>3; j++)
        {
            unsigned char delta = (unsigned char)(refSeq[0][j] ^ refSeq[i][j]);
            wt += ProtoLFSR::GetWeight(delta);
        }
        printf("shift:%d weight:%u\n", i, wt);
        if (wt < wtMin) 
        {
            wtMin = wt;
            minMask = mask;
        }
    }
    
    printf("wtMin = %d (minMask = 0x%08x)\n", wtMin, minMask);
    
    for (unsigned int i = 0; i < LEN; i++)
        if (NULL != refSeq[i]) delete[] refSeq[i];
    delete[] refSeq;
    
    return 0;
}  // end main()
