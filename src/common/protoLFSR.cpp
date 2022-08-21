
#include "protoLFSR.h"
#include "protoDebug.h"
#include <stdlib.h>  // for abs()

ProtoLFSR::ProtoLFSR(Polynomial polynomial, 
                     UINT32     initialState,
                     bool       reverse)
 : lfsr_poly((UINT32)polynomial),
   lfsr_state(initialState),
   lfsr_bits(GetPolySize(polynomial)),
   is_mirrored(false), byte_mode(false)
{
    lfsr_mask = ((UINT32)0xffffffff) >> (32 - lfsr_bits);
    //TRACE("lfsr_mask = 0x%04x\n", lfsr_mask);
    lfsr_state &= lfsr_mask;
    if (reverse)
    {
        Mirror();
        is_mirrored = false;
    }
}

unsigned int ProtoLFSR::GetPolySize(Polynomial poly)
{
    unsigned int numBits = 0;
    UINT32 p = (UINT32)poly;
    while (0 != p)
    {
        p >>= 1;
        numBits++;
    }
    return numBits;
}  // end ProtoLFSR::GetPolySize()


const ProtoLFSR::Polynomial ProtoLFSR::POLYNOMIAL_LIST[33] =
{
    PN_NONE,    //  0 bits (invalid)
    PN_NONE,    //  1 bit  (TBD)
    PN3,        //  2 bits
    PN7,        //  3 bits
    PN15,       //  4 bits
    PN31,       //  5 bits
    PN63,       //  5 bits
    PN127,      //  7 bits
    PN255,      //  8 bits
    PN511,      //  9 bits
    PN1023,     // 10 bits
    PN2047,     // 11 bits
    PN4095,     // 12 bits
    PN8191,     // 13 bits
    PN_NONE,    // 14 bits (TBD)
    PN_NONE,    // 15 bits (TBD)
    PN65535,    // 16 bits
    PN_NONE,    // 17 bits (TBD)  
    PN_NONE,    // 18 bits (TBD)  
    PN_NONE,    // 19 bits (TBD)
    PN_NONE,    // 20 bits (TBD)
    PN_NONE,    // 21 bits (TBD) 
    PN_NONE,    // 22 bits (TBD) 
    PN_NONE,    // 23 bits (TBD) 
    PN24BIT,    // 24 bits  
    PN_NONE,    // 25 bits  (TBD)
    PN_NONE,    // 26 bits  (TBD)
    PN_NONE,    // 27 bits  (TBD)
    PN_NONE,    // 28 bits  (TBD)
    PN_NONE,    // 29 bits  (TBD)
    PN_NONE,    // 30 bits  (TBD)
    PN_NONE,    // 31 bits  (TBD)
    PN32BIT     // 32 bits
};

void ProtoLFSR::Seek(int offsetBits)
{
    if (offsetBits < 0)
    {
        if (!IsMirrored()) 
        {
            Mirror(); // reverse here does a -1 offset on its own, so adjust
            //offsetBits++;
        }
    }
    else if (IsMirrored())
    {
        Mirror();  // reverse here does a +1 offset on its own, so adjust
        //offsetBits--;
    }
    Shift(abs(offsetBits));
    byte_mode = false;
}  // end ProtoLFSR::Seek()

void ProtoLFSR::Shift(unsigned int count)
{
    for (unsigned int i = 0; i < count; i++)
    {
        bool bit = (0 != (lfsr_state & 0x00000001));
        lfsr_state >>= 1;
        //TRACE("shifted lfsr_state = 0x%04x (bit:%d) (poly: 0x%04x\n", lfsr_state, bit, lfsr_poly);
        if (bit) lfsr_state ^= lfsr_poly;
    }
}  // end ProtoLFSR::Shift()

// This is used to "reverse load" the shift
// register towards the state it would be
// in to generate the sequence of bits
// (used by the ProtoLFSR::Sync() method)
void ProtoLFSR::LoadBit(bool bit)
{
    if (bit) lfsr_state ^= lfsr_poly;
    lfsr_state <<= 1;
    lfsr_state &= lfsr_mask;
    if (bit) lfsr_state |= 1;
}  // end ProtoLFSR::LoadBit()

// This sets the LFSR to the state it would be in _before_
// generating the first "lfsr_bits" (register length) of bits 
// provided in the "buffer".  This can be used to "sync" the 
// LFSR to a received sequence of bits.
bool ProtoLFSR::Sync(const char* buffer, unsigned int buflen, unsigned int bitOffset)
{
    if ((buflen << 3) < (lfsr_bits + bitOffset)) return false;
    Reset(0);
    for (int i = bitOffset+lfsr_bits-1; i >= (int)bitOffset; i--)
        LoadBit(GetBit(buffer, i));
    return true;
}  // end ProtoLFSR::Sync()

UINT32 ProtoLFSR::MirrorBits(UINT32 word, unsigned int numBits)
{
    UINT32 bit = 0x00000001 << (numBits - 1);
    UINT32 mbit = 1;
    UINT32 mirrorWord = 0;
    while (0 != bit)
    {
        if (0 != (bit & word))
            mirrorWord |= mbit;
        mbit <<= 1;
        bit >>= 1;
    }
    return mirrorWord;
}  // end ProtoLFSR::MirrorBits()

// Mirror the generator polynomial and state to generate
// the time-reversed version of the sequence
void ProtoLFSR::Mirror()
{
    // 1) "Mirror" the polynomial
    // Starting at most significant bit, mirror
    // all except most significant bit 
    // (which is always set)
    UINT32 mirrorPoly = MirrorBits(lfsr_poly, lfsr_bits - 1);
    // Set the most significant bit
    mirrorPoly |= 0x00000001 << (lfsr_bits - 1);
    lfsr_poly = mirrorPoly;
    // 2) "Mirror" the shift register state
    lfsr_state = MirrorBits(lfsr_state, lfsr_bits);
    is_mirrored = is_mirrored ? false : true;
}  // end ProtoLFSR::Mirror()

bool ProtoLFSR::GetNextBit()
{
    //TRACE("lfsr_state = 0x%04x\n", lfsr_state);
    byte_mode = false;
    if (IsMirrored()) 
    {
        Mirror();
        Shift();
    }
    bool bit = (0 != (lfsr_state & 0x00000001));
    Shift();
    return bit;
}  // end ProtoLFSR:::GetNextBit()

UINT8 ProtoLFSR::GetNextByte()
{
    if (IsMirrored())
    {
        Shift();    
        if (byte_mode)
        {
            GetNextBit();
            Shift(7);
        }
    }
    UINT8 nextByte = GetNextBit() ? 0x01 : 0x00;
    for (int i = 1; i < 8; i++)
    {
        nextByte <<= 1;
        if (GetNextBit()) nextByte |= 0x01;
    }
    byte_mode = true;
    return nextByte;
}  // end ProtoLFSR:::GetNextByte()


bool ProtoLFSR::GetPrevBit()
{
    byte_mode = false;
    if (!IsMirrored()) 
    {
        Mirror();
        Shift();
    }
    bool bit = (0 != (lfsr_state & 0x000000001));
    Shift();
    return bit;
}  // end ProtoLFSR::GetPrevBit()

UINT8 ProtoLFSR::GetPrevByte()
{
    if (!IsMirrored())
    {
        Shift();
        if (byte_mode)
        {
            GetPrevBit();
            Shift(7);
        }
    }
    UINT8 prevByte = GetPrevBit() ? 0x80 : 0x00;
    for (int i = 1; i < 8; i++)
    {
       prevByte >>= 1;
       if (GetPrevBit()) prevByte |= 0x80;
    }
    byte_mode = true;
    return prevByte;
}  // end ProtoLFSR::GetPrevByte()

void ProtoLFSR::FillBuffer(char* buffer, unsigned int buflen)
{
    for (unsigned int i = 0; i < buflen; i++)
        buffer[i] = GetNextByte();
}  // end ProtoLFSR::FillBuffer()


// This function searches for an m-sequence generator polynomial 
// for a given shift register size of "m" bits        
UINT32 ProtoLFSR::PolynomialSearch(unsigned int m)
{
    const UINT32 LEN = 0x00000001 << m;
    
    char** seq = new char*[LEN];
    if (NULL == seq)
    {
        PLOG(PL_ERROR, "ProtoLFSR::PolynomialSearch() new 'seq' error: %s", GetErrorString());
        return 0;
    }
    for (UINT32 i = 0; i < LEN; i++)
    {
        seq[i] = new char[LEN >> 3];
        if (NULL == seq[i])
        {
            PLOG(PL_ERROR, "ProtoLFSR::PolynomialSearch() new 'seq[%lu] error: %s", 
                            (unsigned long)i, GetErrorString());
            for (UINT32 j = 0; j < i; j++)
                delete[] seq[j];
            delete[] seq;
            return 0;
        }
    }
    
    UINT32 maxPoly = 0;
    unsigned int maxMin = 0;
    
    // min possible poly has bit "m-1" set
    UINT32 polyMin = LEN >> 1;
    // max possible poly has all "m" bits set
    UINT32 polyMax = 0xffffffff;
    polyMax >>= (32 - m);
    
    for (UINT32 poly = polyMin; poly <= polyMax; poly++)
    {
        for (unsigned int i = 0; i < LEN; i++)
        {
            ProtoLFSR lfsr((ProtoLFSR::Polynomial)poly);
            lfsr.Seek(i);
            lfsr.FillBuffer(seq[i], LEN >> 3);
        }

        unsigned int wtMin = 0xffffffff;
        //unsigned int offset = 0;
        for (unsigned int i = 1; i < LEN-1; i++)
        {
            unsigned int wt = 0;
            for (unsigned int j = 0; j < (LEN >> 3); j++)
            {
                unsigned char delta = (unsigned char)(seq[0][j] ^ seq[i][j]);
                wt += ProtoBitmask::GetWeight(delta);
            }
            if (wt < wtMin)
            {
                wtMin = wt;
                //offset = i;
            }
        }    
        if (wtMin > maxMin)
        {
            maxMin = wtMin;
            maxPoly = poly;
        }
    }
    for (UINT32 j = 0; j < LEN; j++)
        delete[] seq[j];
    delete[] seq;
    
    return maxPoly;
}  // end ProtoLFSR::PolynomialSearch()


ProtoLFSRX::ProtoLFSRX()
 : lfsrx_poly(NULL), lfsrx_state(NULL),
   lfsrx_bits(0), lfsrx_words(0), lfsrx_mask(0),
   is_mirrored(false), byte_mode(false)
{
}

ProtoLFSRX::~ProtoLFSRX()
{
    if (NULL != lfsrx_poly)
    {
        delete[] lfsrx_poly;
        delete[] lfsrx_state;
        lfsrx_poly = NULL;
    }
}

bool ProtoLFSRX::SetPolynomial(const UINT32*    polynomial,
                               unsigned int     numBits,
                               const UINT32*    initialState,
                               bool             reverse)
{
    if (NULL != lfsrx_poly)
    {
        delete[] lfsrx_poly;
        delete[] lfsrx_state;
    }
    lfsrx_state = NULL;
    lfsrx_bits = lfsrx_words = 0;
    lfsrx_mask = 0;
    is_mirrored = byte_mode = false;
    // Determine how many UINT32 "words" to allocate?
    unsigned int numWords = numBits >> 5;
    if (0 != (numBits & 0x01f)) numWords++;
    
    if ((NULL == polynomial) || (0 == numWords))
    {
        lfsrx_poly = NULL; // LFSR is being cleared
        return true;
    }    
    
    if (NULL == (lfsrx_poly = new UINT32[numWords]))
    {
        PLOG(PL_ERROR, "ProtoLFSRX::SetPolynomial() new lfsrx_poly error: %s\n", GetErrorString());
        return false;
    }    
    if (NULL == (lfsrx_state = new UINT32[numWords]))
    {
        PLOG(PL_ERROR, "ProtoLFSRX::SetPolynomial() new lfsrx_poly error: %s\n", GetErrorString());
        delete[] lfsrx_poly;
        lfsrx_poly = NULL;
        return false;
    }
    lfsrx_bits = numBits;
    lfsrx_words = numWords;
    lfsrx_mask = (UINT32)0xffffffff;
    lfsrx_mask >>= (32 - (numBits & 0x1f)); // bitmask for most significant word
    
    //TRACE("lfsrx_mask = 0x%04x\n", lfsrx_mask);
    
    // Copy polynomial and set initial state
    memcpy(lfsrx_poly, polynomial, numWords*sizeof(UINT32));
    
    if (NULL != initialState)
        memcpy(lfsrx_state, initialState, numWords*sizeof(UINT32));
    else
        memset(lfsrx_state, 0xff, numWords*sizeof(UINT32));
    lfsrx_state[lfsrx_words-1] &= lfsrx_mask;  
    
    //TRACE("initial lfsrx_state = 0x%04x\n", lfsrx_state[0]);
    
    if (reverse)
    {
        Mirror();
        is_mirrored = false;
    }
    
    return true;
}  // end ProtoLFSRX::SetPolynomial()
 
void ProtoLFSRX::Reset(UINT32* initialState)
{
    byte_mode = false;
    if (IsMirrored()) Mirror();
    
    if (NULL != initialState)
        memcpy(lfsrx_state, initialState, lfsrx_words*sizeof(UINT32));
    else
        memset(lfsrx_state, 0xff, lfsrx_words*sizeof(UINT32));
    lfsrx_state[lfsrx_words-1] &= lfsrx_mask;
    
}  // end ProtoLFSRX::Reset()

// This uses the Berlekamp-Massey algorithm to compute a polynomial
// from the input bit sequence in the "buffer"
bool ProtoLFSRX::ComputePolynomial(const char* buffer, int numBits)
{
    // Destroy existing polynomial and state if applicable
    if (NULL != lfsrx_poly)
    {
        delete[] lfsrx_poly;
        delete[] lfsrx_state;
    }
    lfsrx_state = NULL;
    lfsrx_bits = lfsrx_words = 0;
    lfsrx_mask = 0;
    is_mirrored = byte_mode = false;
    
    // We use "UINT32" words for our registers
    // TBD - Is allocating these temp arrays off the stack OK? (i.e. if "numBits" is large?)
    unsigned int numWords = numBits >> 5;
    if (0 != (numBits & 0x1f)) numWords++; // round up as needed
    UINT32* c = new UINT32[numWords];
    UINT32* b = new UINT32[numWords];
    UINT32* t = new UINT32[numWords];
    if ((NULL == c) || (NULL == b) || (NULL == t))
    {
        PLOG(PL_ERROR, "ProtoLFSRX::ComputePolynomial() new UINT32[%u] error(s)\n",
                numWords, GetErrorString());
        if (NULL != c) delete[] c;
        if (NULL != b) delete[] b;
        if (NULL != t) delete[] t;
        return false;
    }    
    
    memset(c, 0, numWords*sizeof(UINT32));
    memset(b, 0, numWords*sizeof(UINT32));
    
    SetBit32(c, 0, true);
    SetBit32(b, 0, true);
    
    int length = 0;
    int m = -1;
    bool linearityWarning = false;
    
    // Process input sequence, bit by bit
    for (int index = 0; index < numBits; index++)
    {
	    // compute discrepancy 'd '
        bool d = ProtoLFSR::GetBit(buffer, index); 
	    for (int i = 1; i <= length; ++i)
            d ^= GetBit32(c, i) && ProtoLFSR::GetBit(buffer, index-i);
        // recompute sequence if necessary 
	    if (d) 
        {
            // Copy c[] to t[]
            for (int i = 0; i <= index; i++)
                SetBit32(t, i, GetBit32(c, i));
            
            int deltaM = index - m;
            for (int j = 0; j <= (m+1); j++)
            {
                int i = deltaM + j;
                SetBit32(c, i, GetBit32(c, i) ^ GetBit32(b, j));
            }
	        if (length <= index/2) 
            {
		        // we have a jump in linear complexity -- check if unusual 
		        if (!linearityWarning && ((index/2 - length) > 6) && (length > 2)) 
                    linearityWarning = true;
		        length = index + 1 - length;
		        m = index;
                // Copy t[] to b[]
                for (int i = 0; i <= index; i++)
                    SetBit32(b, i, GetBit32(t, i));
	        }
	    }
    }  // end while (index < numBits)
    
    // At this point the c[] array has our polynomial bits and 
    // 'length' is the rough register length.  Just need to 
    // tweak things a little bit for our ProtoLFSR register conventions
    // Do a bit by bit check to determine actual register length
    // (Note that bit zero isn't included (always one)
    int polyBits = 0;
    for (int i = 1; i <= length; i++)
        if (GetBit32(c, i)) polyBits = i;
    int polyWords = polyBits >> 5;
    if (0 != (polyBits & 0x1f)) polyWords++;
    // Allocate lfsr_poly array for shift register taps
    if (NULL == (lfsrx_poly = new UINT32[polyWords]))
    {
        PLOG(PL_ERROR, "ProtoLFSRX::ComputePolynomial() new lfsrx_poly[%d] error: %s\n",
                       polyWords, GetErrorString());
        delete[] c; 
        delete[] b; 
        delete[] t;
        return false;
    }
    // Allocate corresponding lfsr_state for shift register contents
    if (NULL == (lfsrx_state = new UINT32[polyWords]))
    {
        PLOG(PL_ERROR, "ProtoLFSRX::ComputePolynomial() new lfsrx_state[%d] error: %s\n",
                       polyWords, GetErrorString());
        delete[] lfsrx_poly;
        lfsrx_poly = NULL;
        delete[] c; 
        delete[] b;
        delete[] t;
        return false;
    }
    
    // We copy c[] shifted by one bit (skipping alwast set bit zero)
    for (int i = 1; i <= length; i++)
        SetBit32(lfsrx_poly, i-1, GetBit32(c, i));
    lfsrx_bits = polyBits;
    lfsrx_words = polyWords;
    lfsrx_mask = (UINT32)0xffffffff;
    lfsrx_mask >>= (32 - (polyBits & 0x1f)); // bitmask for most significant word
    
    // Set a valid lfsr_state (all ones)
    memset(lfsrx_state, 0xff, lfsrx_words*sizeof(UINT32));
    lfsrx_state[lfsrx_words-1] &= lfsrx_mask;  
    
    // Now we "Sync()" to the input "buffer" bit stream by using "polyBits" of it
    // to set the initial state so it will regenerate the input "buffer" bits
    // and the "Seek()" backwards so 
    unsigned int syncBytes = polyBits >> 3;
    if (0 != (polyBits & 0x07)) syncBytes++;
    Sync(buffer, syncBytes);
    
    // TBD - put a test here to make sure the resultant LFSR actually regenerates the
    // input "buffer" sequence.  If not, the input was not linear!
    // A) temporarily save our "lfsrx_state" in 'c'
    bool result = true;
    memcpy(c, lfsrx_state, lfsrx_words*sizeof(UINT32));
    // B) Check input sequence match to lfsr output
    for (int i = 0; i < numBits; i++)
    {
        if (ProtoLFSR::GetBit(buffer, i) != GetNextBit())
        {
            PLOG(PL_ERROR, "ProtoLFSRX::ComputePolynomial() error: non-linear input sequence\n");
            linearityWarning = false;
            result = false;
            break;
        }   
    }
    // C) set state back to sync point
    memcpy(lfsrx_state, c, lfsrx_words*sizeof(UINT32));
    
    if (linearityWarning)
        PLOG(PL_WARN, "ProtoLFSRX::ComputePolynomial() warning: possible non-linear input sequence!\n");
    delete[] c; 
    delete[] b;
    delete[] t;
    return result;
}  // end ProtoLFSRX::ComputePolynomial()


void ProtoLFSRX::Seek(int offsetBits)
{
    if (offsetBits < 0)
    {
        if (!IsMirrored()) 
        {
            Mirror(); // reverse here does a -1 offset on its own, so adjust
            //offsetBits++;
        }
    }
    else if (IsMirrored())
    {
        Mirror();  // reverse here does a +1 offset on its own, so adjust
        //offsetBits--;
    }
    Shift(abs(offsetBits));
    byte_mode = false;
}  // end ProtoLFSRX::Seek()

bool ProtoLFSRX::GetNextBit()
{
    //TRACE("lfsrx_state = 0x%04x\n", lfsrx_state[0]);
    byte_mode = false;
    if (IsMirrored()) 
    {
        Mirror();
        Shift();
    }
    bool bit = (0 != (lfsrx_state[0] & 0x00000001));
    Shift();
    return bit;
}  // end ProtoLFSRX:::GetNextBit()

UINT8 ProtoLFSRX::GetNextByte()
{
    if (IsMirrored())
    {
        Shift();    
        if (byte_mode)
        {
            GetNextBit();
            Shift(7);
        }
    }
    UINT8 nextByte = GetNextBit() ? 0x01 : 0x00;
    for (int i = 1; i < 8; i++)
    {
        nextByte <<= 1;
        if (GetNextBit()) nextByte |= 0x01;
    }
    byte_mode = true;
    return nextByte;
}  // end ProtoLFSRX:::GetNextByte()


bool ProtoLFSRX::GetPrevBit()
{
    byte_mode = false;
    if (!IsMirrored()) 
    {
        Mirror();
        Shift();
    }
    bool bit = (0 != (lfsrx_state[0] & 0x000000001));
    Shift();
    return bit;
}  // end ProtoLFSRX::GetPrevBit()

UINT8 ProtoLFSRX::GetPrevByte()
{
    if (!IsMirrored())
    {
        Shift();
        if (byte_mode)
        {
            GetPrevBit();
            Shift(7);
        }
    }
    UINT8 prevByte = GetPrevBit() ? 0x80 : 0x00;
    for (int i = 1; i < 8; i++)
    {
       prevByte >>= 1;
       if (GetPrevBit()) prevByte |= 0x80;
    }
    byte_mode = true;
    return prevByte;
}  // end ProtoLFSRX::GetPrevByte()

void ProtoLFSRX::FillBuffer(char* buffer, unsigned int buflen)
{
    for (unsigned int i = 0; i < buflen; i++)
        buffer[i] = GetNextByte();
}  // end ProtoLFSRX::FillBuffer()

// This sets the LFSR to the state it would be in _before_
// generating the first "lfsr_bits" (register length) of bits 
// provided in the "buffer".  This can be used to "sync" the 
// LFSR to a received sequence of bits.
bool ProtoLFSRX::Sync(const char* buffer, unsigned int buflen, unsigned int bitOffset)
{
    if ((buflen << 3) < (lfsrx_bits + bitOffset)) return false;
    
    // These 3 lines do a Reset() with an all 0's initial state
    byte_mode = false;
    if (IsMirrored()) Mirror();
    memset(lfsrx_state, 0, lfsrx_words*sizeof(UINT32));
            
    for (int i = bitOffset+lfsrx_bits-1; i >= (int)bitOffset; i--)
        LoadBit(ProtoLFSR::GetBit(buffer, i));
    return true;
}  // end ProtoLFSRX::Sync()

// This is used to "reverse load" the shift
// register towards the state it would be
// in to generate the sequence of bits
// (used by the ProtoLFSR::Sync() method)
void ProtoLFSRX::LoadBit(bool bit)
{
    if (bit) // the bit shifted in is set
    {   for (unsigned int i = 0; i < lfsrx_words; i++)
            lfsrx_state[i] ^= lfsrx_poly[i];
    }
    
    // This loop shifts the lfsrx_state register to the "left"
    UINT32* statePtr = lfsrx_state;
    for (unsigned int i = 0; i < lfsrx_words; i++)
    {
        bool saveBit = (0 != (*statePtr & 0x80000000));
        *statePtr <<= 1;
        if (bit) *statePtr |= 0x00000001;
        bit = saveBit;
        statePtr++;
    }
    // Mask the most significant word
    lfsrx_state[lfsrx_words - 1] &= lfsrx_mask;
}  // end ProtoLFSRX::LoadBit()

void ProtoLFSRX::Shift(unsigned int count)
{
    for (unsigned int i = 0; i < count; i++)
    {
        // This loop shifts the lfsrx_state register to the "right"
        bool bit = false;
        UINT32* statePtr = lfsrx_state + lfsrx_words - 1;
        for (unsigned int i = 0; i < lfsrx_words; i++)
        {
            bool saveBit = (0 != (*statePtr & 0x00000001));
            *statePtr >>= 1;
            if (bit) *statePtr |= 0x80000000;
            bit = saveBit;
            statePtr--;
        }
        //TRACE("shifted lfsrx_state = 0x%04x (bit:%d poly: 0x%04x)\n", lfsrx_state[0], bit, lfsrx_poly[0]);
        if (bit) // the bit shifted out of the register was set
        {   for (unsigned int i = 0; i < lfsrx_words; i++)
                lfsrx_state[i] ^= lfsrx_poly[i];
        }
    }
}  // end ProtoLFSRX::Shift()

// Mirror the generator polynomial and state to generate
// the time-reversed version of the sequence
void ProtoLFSRX::Mirror()
{
    // Note this could be implemented more efficiently for the
    // case of 0 == (lfsrx_bits % 32)  (TBD - do this)
    // 1) "Mirror" the polynomial
    // Starting at 2nd most significant bit, mirror the bits
    // (most significant bit is always set)
    unsigned int i = 0;               // least significant bit
    unsigned int j = lfsrx_bits - 2;  // 2nd most significant bit
    while (i < j)
    {
        // Swap bit[i] and bit[j]
        bool jbit = GetBit32(lfsrx_poly, j);
        SetBit32(lfsrx_poly, j, GetBit32(lfsrx_poly, i));
        SetBit32(lfsrx_poly, i, jbit);
        i++;
        j--;
    }
    // Note the most significant bit was left set.
    
    // 2) "Mirror" the entire shift register state
    i = 0;               // least significant bit
    j = lfsrx_bits - 1;  // most significant bit
    while (i < j)
    {
        // Swap bit[i] and bit[j]
        bool jbit = GetBit32(lfsrx_state, j);
        SetBit32(lfsrx_state, j, GetBit32(lfsrx_state, i));
        SetBit32(lfsrx_state, i, jbit);
        i++;
        j--;
    }
    is_mirrored = is_mirrored ? false : true;
}  // end ProtoLFSRX::Mirror()
