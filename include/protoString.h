#ifndef _PROTO_STRING

// This module will include mostly "helper" classes for parsing
// and manipulating "char" strings.  At the moment, there are
// _not_ plans to create yet another C++ "Strings" class.

#include <string.h>
#include <stdio.h>
#include <ctype.h>  // for isspace()


/***************

  The ProtoTokenator provides an easy means to  parse a string, item by item, 
  with each item separated  by a "delimiter" (white space by default)

  Optional parameters:

    1) If 'strip' is true, leading/trailing white space is removed from return items

    2) The 'maxCount' limits the number of items returned with the last item returned
       consisting of the remainder of the string (leading/trailing whitespace removed 
       if 'strip' is set to 'true'
       
    3) The 'reverse' option tokenizes the string in reverse order.
    
*******************/


// (The "strip" option removes leading/trailing white space)
class ProtoTokenator
{
    public:
        ProtoTokenator(const char*  text, 
                       char         delimiter=' ', 
                       bool         strip=true,
                       unsigned int maxCount = 0,
                       bool         reverse=false);
        ~ProtoTokenator();
        
        const char* const GetNextItem(bool detach=false);
        
        void Reset(const char* text=NULL, char delimiter='\0');
        
        // If this is called, caller is responsible to delete
        // the memory allocated for the previous item returned.
        char* DetachPreviousItem()
        {
            char* item = (char*)prev_item;
            prev_item = NULL;
            return item;
        }
        
    private:
        char         token;
        bool         strip;
        bool         reverse;
        unsigned int max_count;
        unsigned int remain;
        const char*  text_ptr;
        const char*  next_ptr;
        char*        prev_item;
};  // end class ProtoTokenator

#endif // !_PROTO_STRING
