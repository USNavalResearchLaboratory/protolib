#include "protoString.h"
#include "protoDebug.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>  // for isspace()

ProtoTokenator::ProtoTokenator(const char*  text, 
                               char         delimiter, 
                               bool         stripWhiteSpace, 
                               unsigned int maxCount,
                               bool         reverseOrder,
                               bool         stripTokens)
 : token(delimiter), strip_whitespace(stripWhiteSpace), strip_tokens(stripTokens), 
   reverse(reverseOrder), max_count(maxCount), remain(maxCount), 
   text_ptr(text), prev_item(NULL)       
{
    Reset();
}

ProtoTokenator::~ProtoTokenator()
{
    if (NULL != prev_item)
    {
        delete[] prev_item;
        prev_item = NULL;
    }
}

bool ProtoTokenator::TokenMatch(char c) const
{
    return ((c == token) || (isspace(token) && isspace(c)));
}  // end ProtoTokenator::TokenMatch()

void ProtoTokenator::Reset(const char* text, char delimiter)
{
    remain = max_count;
    if (NULL != text) text_ptr = text;
    if ('\0' != delimiter) token = delimiter;
    if (reverse)
    {
        next_ptr = text_ptr;
        if (NULL != next_ptr)
            next_ptr += strlen(next_ptr) - 1;
        if (isspace(token))
        {
            // Advance to start of any trailing white space
            while (NULL != next_ptr)
            {
                if (isspace(*next_ptr))
                {
                    if (text_ptr == next_ptr)
                        next_ptr = NULL;
                    else
                        next_ptr--;
                }
                else
                {
                    break;
                }
            }
        }
    }
    else
    {
        next_ptr = text_ptr;
        if (isspace(token))
        {
            // advance to end of any leading white space
            while (NULL != next_ptr)
            {
                if ('\0' == *next_ptr)
                    next_ptr = NULL;
                else if (isspace(*next_ptr))
                    next_ptr++;
                else
                    break;
            }
        }
    }
}  // end ProtoTokenator::Reset()

const char* const ProtoTokenator::GetNextItem(bool detach)
{
    if (reverse)
    {
        if (strip_whitespace)
        {
            // Strip any trailing white space, if required
            while (NULL != next_ptr)
            {
                if (isspace(*next_ptr))
                {
                    if (text_ptr == next_ptr)
                        next_ptr = NULL;
                    else
                        next_ptr--;
                }
                else
                {
                    break;
                }
            }
        }
        if (NULL == next_ptr) return NULL;
        
        if ((0 != max_count) && (0 == remain))
        {
            // Count was limited, so return remainder as last item 
            // (strip whitespace from head if required)
            long itemLen = next_ptr - text_ptr + 1;
            const char* headPtr = text_ptr;
            if (strip_whitespace)
            {
                while ((0 != itemLen) && (isspace(*headPtr)))
                {
                    headPtr++;
                    itemLen--;
                }
            }         
            // (strip extraneous token(s) from tail if required)
            if (strip_tokens)
            {
                const char* tailPtr = headPtr + itemLen - 1;
                while ((0 != itemLen) && TokenMatch(*tailPtr))
                {
                    tailPtr--;
                    itemLen--;
                }
            }
            if (NULL != prev_item) delete[] prev_item;
            if (NULL == (prev_item = new char[itemLen+1]))
            {
                PLOG(PL_ERROR, "ProtoTokenator::GetNextItem() new char[] error: %s\n", GetErrorString());
                return NULL;
            }
            strncpy(prev_item, headPtr, itemLen); 
            prev_item[itemLen] = '\0';
            next_ptr = NULL;
            if (detach)
            {
                const char* item = prev_item;
                prev_item = NULL;
                return item;
            }
            else
            {
                return prev_item;
            }
        }
        const char* ptr = next_ptr;
        // Advance to next token
        while (text_ptr != ptr)
        {
            if (TokenMatch(*ptr))
                break;
            else
                ptr--;
        }    
        size_t itemLen = next_ptr - ptr;
        bool firstItem = false;
        if (text_ptr == ptr)
        {
            if (TokenMatch(*ptr))
                firstItem = false;
            else
                firstItem = true;
        }
        const char* headPtr = ptr;
        if (firstItem) 
            itemLen += 1;
        else
            headPtr += 1;
        // Strip any leading whitespace, if required
        if (strip_whitespace)
        {
            while ((0 != itemLen) && isspace(*headPtr))
            {
                headPtr++;
                itemLen--;
            }
        }
        if (NULL != prev_item) delete[] prev_item;
        if (NULL == (prev_item = new char[itemLen+1]))
        {
            PLOG(PL_ERROR, "ProtoTokenator::GetNextItem() new char[] error: %s\n", GetErrorString());
            return NULL;
        }
        strncpy(prev_item, headPtr, itemLen);
        prev_item[itemLen] = '\0';
        if (text_ptr == ptr--)
        {
            ptr = NULL;
        }
        else if (isspace(token))
        {
            // Advance to start of white space
            while (NULL != ptr)
            {
                if (isspace(*ptr))
                {
                    if (text_ptr == ptr)
                        ptr = NULL;
                    else
                        ptr--;
                }
                else
                {
                    break;
                }
            }
        }
        next_ptr = ptr;
    }
    else
    {
        // Strip any leading white space, if required
        if (strip_whitespace)
        {
            while (NULL != next_ptr)
            {
                if ('\0' == *next_ptr)
                    next_ptr = NULL;
                else if (isspace(*next_ptr))
                    next_ptr++;
                else
                    break;
            }
        }
        if (NULL == next_ptr) return NULL;
        if ((0 != max_count) && (0 == remain))
        {
            // Count was limited, so return remainder as last item 
            // (strip trailing whitespace if required)
            size_t itemLen = strlen(next_ptr);
            if (strip_whitespace)
            {
                const char* tailPtr = next_ptr + itemLen - 1;
                while ((0 != itemLen) && (isspace(*tailPtr)))
                {
                    tailPtr--;
                    itemLen--;
                }
            }         
            // (strip extraneous leading token(s) if required)
            if (strip_tokens)
            {
                while ((0 != itemLen) && TokenMatch(*next_ptr))
                {
                    next_ptr++;
                    itemLen--;
                }
            }
            if (NULL != prev_item) delete[] prev_item;
            if (NULL == (prev_item = new char[itemLen+1]))
            {
                PLOG(PL_ERROR, "ProtoTokenator::GetNextItem() new char[] error: %s\n", GetErrorString());
                return NULL;
            }
            strncpy(prev_item, next_ptr, itemLen + 1); 
            next_ptr = NULL;
            if (detach)
            {
                const char* item = prev_item;
                prev_item = NULL;
                return item;
            }
            else
            {
                return prev_item;  
            }
        }
        const char* ptr;
        if (isspace(token))
        {
            // Advance start of whitespace
            ptr = next_ptr;
            while (NULL != ptr)
            {
                if ('\0' == *ptr)
                    ptr = NULL;
                else if (isspace(*ptr))
                    break;
                else
                    ptr++;
            }
        }
        else
        {
            ptr = strchr(next_ptr, token);
        }
        size_t itemLen = (NULL == ptr) ? strlen(next_ptr) : (ptr++ - next_ptr);
        // Strip trailing whitespace, if required
        if (strip_whitespace)
        {
            const char* tailPtr = next_ptr + itemLen - 1;
            while ((0 != itemLen) && (isspace(*tailPtr--)))
                itemLen--;
        }
        if (NULL != prev_item) delete[] prev_item;
        if (NULL == (prev_item = new char[itemLen+1]))
        {
            PLOG(PL_ERROR, "ProtoTokenator::GetNextItem() new char[] error: %s\n", GetErrorString());
            return NULL;
        }
        strncpy(prev_item, next_ptr, itemLen);
        prev_item[itemLen] = '\0';
        if (strip_whitespace || isspace(token))
        {
            // advance to end of white space
            while (NULL != ptr)
            {
                if ('\0' == *ptr)
                    ptr = NULL;
                else if (isspace(*ptr))
                    ptr++;
                else
                    break;
            }
        }
        next_ptr = ptr;
    }
    if (0 != max_count) remain -= 1;
    if (detach)
    {
        const char* item = prev_item;
        prev_item = NULL;
        return item;
    }
    else
    {
        return prev_item;
    }
}  // end ProtoTokenator::GetNextItem()
