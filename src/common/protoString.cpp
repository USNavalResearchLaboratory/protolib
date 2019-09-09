#include "protoString.h"

ProtoTokenator::ProtoTokenator(const char* text, char delimiter, bool stripWhiteSpace)
 : token(delimiter), strip(stripWhiteSpace), 
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

void ProtoTokenator::Reset()
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
}  // end ProtoTokenator::Reset()

const char* const ProtoTokenator::GetNextItem()
{
    if (strip)
    {
        // Strip any leading white space
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
    const char* ptr;
    if (isspace(token))
    {
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
    if (NULL != prev_item) delete[] prev_item;
    if (strip)
    {
        const char* tailPtr = next_ptr + itemLen - 1;
        while ((0 != itemLen) && (isspace(*tailPtr--)))
            itemLen--;
    }
    if (NULL == (prev_item = new char[itemLen+1]))
    {
        perror("ProtoTokenator::GetNextItem() new char[] error");
        return NULL;
    }
    strncpy(prev_item, next_ptr, itemLen);
    prev_item[itemLen] = '\0';
    if (isspace(token))
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
    return prev_item;
}  // end ProtoTokenator::GetNextItem()
