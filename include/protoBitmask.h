#ifndef _PROTO_BITMASK_
#define _PROTO_BITMASK_

#include "protoDefs.h" 
#include "protoDebug.h"

#include <string.h>  // for memset()
#include <stdio.h>   // for fprintf()


/**
 * @class ProtoBitmask
 * 
 * @brief This class also provides space-efficient binary storage.
 *
 * It's pretty much just a flat-indexed array of bits, but
 * keeps some state to be relatively efficient for various
 * operations.
 */

class ProtoBitmask
{    
    // Methods
    public:
        ProtoBitmask();
        ~ProtoBitmask();
        
        bool Init(UINT32 numBits);
        void Destroy();
        UINT32 Size() {return num_bits;}  // (TBD) change to "GetSize()"
        void Clear()  // set to all zero's
        {
            memset(mask, 0, mask_len);
            first_set = num_bits;
        };
        void Reset()  // set to all one's
        {
            memset(mask, 0xff, (mask_len-1));
            mask[mask_len-1] = 0x00ff << ((8 - (num_bits & 0x07)) & 0x07);
            first_set = 0;
        }
        
        bool IsSet() const {return (first_set < num_bits);}
        bool GetFirstSet(UINT32& index) const 
        {
            index = first_set;
            return IsSet();
        }
            
        bool GetLastSet(UINT32& index) const
        {
            index = num_bits - 1;
            return GetPrevSet(index);
        }
        bool Test(UINT32 index) const
        {
            return ((index < num_bits) ?
			        (0 !=  (mask[(index >> 3)] & (0x80 >> (index & 0x07)))) :
                    false);
        }
        bool CanSet(UINT32 index) const
            {return (index < num_bits);}
        
        bool Set(UINT32 index)
        {
            if (index < num_bits)
            {
                mask[(index >> 3)] |= (0x80 >> (index & 0x07));
                (index < first_set) ? (first_set = index) : 0;
                return true;
            }
            else
            {
                return false;
            }
        }
        bool Unset(UINT32 index)
        {
            if (index < num_bits)
            {
                mask[(index >> 3)] &= ~(0x80 >> (index & 0x07));
                (index == first_set) ? first_set = GetNextSet(first_set) ? first_set : num_bits : 0;
            }
            return true;
        }
        bool Invert(UINT32 index)
            {return (Test(index) ? Unset(index) : Set(index));}
        
        bool SetBits(UINT32 baseIndex, UINT32 count);
        bool UnsetBits(UINT32 baseIndex, UINT32 count);
        
        bool GetNextSet(UINT32& index) const;
        bool GetPrevSet(UINT32& index) const;
        bool GetNextUnset(UINT32& index) const;
        
        bool Copy(const ProtoBitmask &b);        // this = b
        bool Add(const ProtoBitmask & b);        // this = this | b
        bool Subtract(const ProtoBitmask & b);   // this = this & ~b
        bool XCopy(const ProtoBitmask & b);      // this = ~this & b
        bool Multiply(const ProtoBitmask & b);   // this = this & b
        bool Xor(const ProtoBitmask & b);        // this = this ^ b
        
        
        static unsigned char GetWeight(unsigned char c)
            {return WEIGHT[c];}
        
        static const unsigned char WEIGHT[256];
        static const unsigned char BITLOCS[256][8];
        
        void Display(FILE* stream);
        
    // Members
    //private:
        unsigned char*  mask;
        UINT32   mask_len;
        UINT32   num_bits;
        UINT32   first_set;  // index of lowest _set_ bit
};  // end class ProtoBitmask



/**
 * @class ProtoSlidingMask
 * 
 * @brief This class also provides space-efficient binary storage.
 *
 * More than just a flat-indexed array of bits, this
 * class can also automatically act as a sliding
 * window buffer as long as the range of _set_ bit
 * indices fall within the number of storage bits
 * for which the class initialized.
 */
 // (TBD) This class could be improved if we byte-aligned the offset? 
 
class ProtoSlidingMask
{
    public:
        ProtoSlidingMask();
        ~ProtoSlidingMask();
        
        const char* GetMask() const {return (const char*)mask;}
        
        bool Init(UINT32 numBits, UINT32 rangeMask);
        bool Resize(UINT32 numBits);
        void Destroy();
        UINT32 GetSize() const {return num_bits;}
        void Clear()
        {
            memset(mask, 0, mask_len); 
            start = end = num_bits; 
            offset = 0; 
        }
        void Reset(UINT32 index = 0)
        {
            ASSERT(0 != num_bits);
            memset(mask, 0xff, mask_len);
            mask[mask_len-1] = 0x00ff << ((8 - (num_bits & 0x07)) & 0x07);
            start = 0;
            end = num_bits - 1;
            offset = index;
            if (range_mask) offset &= range_mask;
        }
        bool IsSet() const {return (start < num_bits);}        
        bool GetFirstSet(UINT32& index) const 
        {
            index = offset;
            return IsSet();
        }
        bool GetLastSet(UINT32& index) const
        {
            UINT32 n = (end < start) ? 
                (num_bits - (start - end)) : 
                (end - start);
            index = offset + n;
            if (range_mask) index &= range_mask;
            return IsSet();
        }
        bool Test(UINT32 index) const;
        bool CanSet(UINT32 index) const;
        
        bool Set(UINT32 index);
        bool Unset(UINT32 index);
        bool Invert(UINT32 index)
            {return (Test(index) ? Unset(index): Set(index));}   
        
        bool SetBits(UINT32 index, UINT32 count);
        bool UnsetBits(UINT32 index, UINT32 count);
                
        UINT32 GetRangeMask() const {return range_mask;}
        UINT32 GetRangeSign() const {return range_sign;}
        
        // These return "false" when finding nothing
        bool GetNextSet(UINT32& index) const;
        bool GetPrevSet(UINT32& index) const;
        
        bool Copy(const ProtoSlidingMask& b);        // this = b
        bool Add(const ProtoSlidingMask & b);        // this = this | b
        bool Subtract(const ProtoSlidingMask & b);   // this = this & ~b
        bool XCopy(const ProtoSlidingMask & b);      // this = ~this & b
        bool Multiply(const ProtoSlidingMask & b);   // this = this & b
        bool Xor(const ProtoSlidingMask & b);        // this = this ^ b
        
        void Display(FILE* stream);
        void Debug(UINT32 theCount);
            
        // Calculate "circular" delta between two indices
        // (If (0 == range_mask) it is an absolute 32-bit delta
        INT32 Difference(UINT32 a, UINT32 b) const
        {
            UINT32 result = a - b;
            if (0 != range_mask)
            {
                return ((0 == (result & range_sign)) ? 
                            (result & range_mask) :
                            (((result != range_sign) || (a < b)) ? 
                                (INT32)(result | ~range_mask) : (INT32)result));
            }
            else
            {
                return (INT32)result;
            }
        }  
        
        // Compare values.  If a non-zero bit "mask" is given, the comparison
        // is a "sliding window" (signed) over the bit space.  Otherwise, it is 
        // simply and unsigned value comparison.
        // Returns -1, 0, +1 for (a < b), (a == b), and (a > b), respectively
        int Compare(UINT32 a, UINT32 b) const
        {
            if (0 != range_mask)
            {
                // "Sliding window" comparison
                INT32 delta = Difference(a, b);
                if (delta < 0)
                    return -1;
                else if (0 == delta)
                    return 0;
                else  // if delta > 0
                    return 1;
            }
            else if (a < b)
            {
                return  -1;
            }
            else if (a == b)
            {
                return 0;
            }
            else //if (a > b)
            {
                return 1;
            }
        }
        
    private:
        unsigned char*   mask;
        UINT32           mask_len;
        UINT32           range_mask;
        UINT32           range_sign;
        UINT32           num_bits;
        UINT32           start;
        UINT32           end;
        UINT32           offset;
};  // end class ProtoSlidingMask

#endif // _PROTO_BITMASK_
