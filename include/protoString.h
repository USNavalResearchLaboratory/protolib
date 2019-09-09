#ifndef _PROTO_STRING

// This module will include mostly "helper" classes for parsing
// and manipulating "char" strings.  At the moment, there are
// _not_ plans to create yet another C++ "Strings" class.

#include <string.h>
#include <stdio.h>
#include <ctype.h>  // for isspace()


// class ProtoTokenator provides an easy means to 
// parse a string, item by item, with each item separated
// by a "delimiter" (white space by default)
// (The "strip" option removes leading/trailing white space)
class ProtoTokenator
{
    public:
        ProtoTokenator(const char* text, char delimiter=' ', bool strip=true);
        ~ProtoTokenator();
        
        const char* const GetNextItem();
        
        void Reset();
        
    private:
        char        token;
        bool        strip;
        const char* text_ptr;
        const char* next_ptr;
        char*       prev_item;
};  // end class ProtoTokenator

#endif // !_PROTO_STRING
