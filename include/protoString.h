#ifndef _PROTO_STRING

// This module will include mostly "helper" classes for parsing
// and manipulating "char" strings.  At the moment, there are
// _not_ plans to create yet another C++ "Strings" class.

/***************

  The ProtoTokenator provides an easy means to  parse a string, item by item, 
  with each item separated  by a "delimiter" (white space by default)

  Additional optional parameters:

    1) If 'stripWhitespace' is true, leading/trailing white space is removed from return items

    2) The 'maxCount' limits the number of items returned with the last item returned
       consisting of the remainder of the string (leading/trailing whitespace removed 
       if 'strip' is set to 'true'
       
    3) The 'reverse' option tokenizes the string in reverse order.
    
    4) If 'stripTokens' is true, leading (or trailing for 'reverse' case)  'extraneous' 
       token characters are stripped _only_ when 'maxCount' is limited (non-zero).  
       The main use case here is when ProtoTokenator is used to split file path strings 
       (e.g., delimited by PROTO_PATH_DELIMITER) and it removes trailing, extraneous
       file path delimiters from directory prefixes (reverse split, maxCount=1) or leading, 
       extraneous file path delimiters from the file name portion (forward split, maxCount=1)
       
*******************/


#include "protoDefs.h"  // for 'NULL' definition

// (The "strip_whitespace" option removes leading/trailing white space)
//
class ProtoTokenator
{
    public:
        ProtoTokenator(const char*  text,                   // string to parse
                       char         delimiter=' ',          // what delimeter to use
                       bool         stripWhitespace=true,   // remove leading/trailing whitespace from fields
                       unsigned int maxCount = 0,           // non-zero limits number of splits
                       bool         reverse=false,          // tokenize backwards from end of string
                       bool         stripTokens=false);     // for non-zero maxCount, strip any extra tokens
                                                            // found at last break point (useful for file paths)
        ~ProtoTokenator();
        
        const char* const GetNextItem(bool detach=false);  // if "detach" is true, caller must delete[] returned array.
                                                           // Otherwise, returned pointer is only valid until next
                                                           // call to GetNextItem() or ProtoTokenator destruction
        
        const char* GetNextPtr() const  // pointer to unparsed remainder
            {return next_ptr;}
        
        void Reset(const char* text=NULL, char delimiter='\0');  // default args retain current text/delimiter values
        
        void SetNullDelimiter() // with "stripWhitespace" enabled, this enables
            {token = '\0';}     // simple leading/trailing stripping
            
        
        // If this is called, caller is responsible to delete
        // the memory allocated for the previous item returned.
        // (This enables deferred, 'detach' decision)
        char* DetachPreviousItem()
        {
            char* item = (char*)prev_item;
            prev_item = NULL;
            return item;
        }
        bool TokenMatch(char c) const;
        
    private:
        char         token;
        bool         strip_whitespace; // strip leading/trailing whitespace if true
        bool         strip_tokens;     // string leading/trailing tokens if true
        bool         reverse;
        unsigned int max_count;
        unsigned int remain;
        const char*  text_ptr;
        const char*  next_ptr;
        char*        prev_item;
};  // end class ProtoTokenator

#endif // !_PROTO_STRING
