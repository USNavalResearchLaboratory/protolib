#include "protoString.h"
#include "protoDebug.h"

ProtoTokenator::ProtoTokenator(const char*  text, 
                               char         delimiter, 
                               bool         stripWhiteSpace, 
                               unsigned int maxCount,
                               bool         reverseOrder)
 : token(delimiter), strip(stripWhiteSpace), reverse(reverseOrder),
   max_count(maxCount), remain(maxCount), 
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
            // Advance to start of any traiing white space
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
        if (strip)
        {
            // Strip any trailing white space
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
            // Count was limited, so return remainder as last item (strip whitespace as required)
            unsigned int itemLen = next_ptr - text_ptr + 1;
            const char* headPtr = text_ptr;
            if (strip)
            {
                while ((0 != itemLen) && (isspace(*headPtr)))
                {
                    headPtr++;
                    itemLen--;
                }
            }         
            if (NULL != prev_item) delete[] prev_item;
            if (NULL == (prev_item = new char[itemLen+1]))
            {
                perror("ProtoTokenator::GetNextItem() new char[] error");
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
        if (isspace(token))
        {
            while (text_ptr != ptr)
            {
                if (isspace(*ptr))
                {
                    break;
                }
                else
                {
                    ptr--;
                }
            }
        }
        else
        {
            while (text_ptr != ptr)
            {
                if (*ptr == token)
                {
                    break;
                }
                else
                {
                    ptr--;
                }
            }
        }
        size_t itemLen = next_ptr - ptr;
        bool firstItem = false;
        if (text_ptr == ptr)
        {
            if (isspace(token))
                firstItem = isspace(*ptr) ? false : true;
            else if (*ptr == token )
                firstItem = false;
            else
                firstItem = true;
        }
        const char* headPtr = ptr;
        if (firstItem) 
            itemLen += 1;
        else
            headPtr += 1;
        if (strip)
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
            perror("ProtoTokenator::GetNextItem() new char[] error");
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
        if ((0 != max_count) && (0 == remain))
        {
            // Count was limited, so return remainder as last item (strip whitespace as required)
            unsigned int itemLen = strlen(next_ptr);
            if (strip)
            {
                const char* tailPtr = next_ptr + itemLen - 1;
                while ((0 != itemLen) && (isspace(*tailPtr--)))
                    itemLen--;
            }         
            if (NULL != prev_item) delete[] prev_item;
            if (NULL == (prev_item = new char[itemLen+1]))
            {
                perror("ProtoTokenator::GetNextItem() new char[] error");
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
        if (strip)
        {
            const char* tailPtr = next_ptr + itemLen - 1;
            while ((0 != itemLen) && (isspace(*tailPtr--)))
                itemLen--;
        }
        if (NULL != prev_item) delete[] prev_item;
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
