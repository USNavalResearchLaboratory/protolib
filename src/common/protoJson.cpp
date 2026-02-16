#include "protoJson.h"
#include "protoDebug.h"
#include <ctype.h>  // for tolower()
#include <string.h>

//#include "protoCheck.h"

ProtoJson::Item::Item(Type theType, Item* theParent)
 : type(theType), parent(theParent), level((NULL == theParent) ? 0 : theParent->level + 1)
{
}

ProtoJson::Item::~Item()
{
}

const char* ProtoJson::Item::GetTypeString(Type type)
{
    switch (type)
    {
        case INVALID:
            return "INVALID";
        case ENTRY:
            return "ENTRY";      
        case STRING:
            return "STRING";
        case NUMBER:
            return "NUMBER";
        case OBJECT:
            return "OBJECT";
        case ARRAY:
            return "ARRAY";
        case TRUE:
            return "TRUE";
        case FALSE:
            return "FALSE";
        case NONE:
            return "NULL";
    }
    ASSERT(0);
    return NULL;
}  // end ProtoJson::Parser::GetTypeString()

ProtoJson::String::String(Item* theParent)
 : Item(STRING, theParent), text(NULL)
{
}

ProtoJson::String::~String()
{
    if (NULL != text)
    {
        delete[] text;
        text = NULL;
    }
}

bool ProtoJson::String::SetText(const char* theText)
{
    if (NULL != text) delete[] text;
    if (NULL == (text = new char[strlen(theText)+1]))
    {
        PLOG(PL_ERROR, "ProtoJson::String::Set() new char[] error: %s\n", GetErrorString());
        return false;
    }
    strcpy(text, theText);
    return true;
}  // end ProtoJson::String::SetTezt()
            
void ProtoJson::String::SetTextPtr(char* textPtr)
{
    if (NULL != text) delete[] text;
    text = textPtr;
}  // end ProtoJson::String::SetTextPtr()

ProtoJson::Number::Number(Item* theParent)
 : Item(NUMBER, theParent), is_float(false), integer(0)
{
}


ProtoJson::Number::Number(int value, Item* theParent)
 : Item(NUMBER, theParent), is_float(false), integer(value)
{
    SetValue(value);
}        

ProtoJson::Number::Number(double value, Item* theParent)
 : Item(NUMBER, theParent), is_float(true), floating(value)
{
}

ProtoJson::Number::~Number()
{
}

// A null-terminated string MUST be supplied here
bool ProtoJson::Number::SetValue(const char* text)
{
    // If the text contains a '.' or 'e', or 'E', it could be a float
    bool isFloat = false;
    const char* ptr = text;
    while (!isFloat && ('\0' != *ptr))
    {
        switch (*ptr++)
        {
            case '.':
            case 'E':
            case 'e':
                isFloat = true;
                break;
            default:
                break;
        }
    }
    if (isFloat)
    {
        double value;
        if (1 != sscanf(text, "%lf", &value))
        {
            PLOG(PL_ERROR, "ProtoJson::Number::SetValue() error: invalid floating point number text\n");
            return false;
        } 
        SetValue(value);
    }
    else
    {
        int value;
        if (1 != sscanf(text, "%d", &value))
        {
            PLOG(PL_ERROR, "ProtoJson::Number::SetValue() error: invalid integer number text\n");
            return false;
        } 
        SetValue(value);
    }
    return true;   
}  // end ProtoJson::Number::SetValue(text)

ProtoJson::Array::Array(Item* theParent)
 : Item(ARRAY, theParent), array_buf(NULL), array_len(0)
{
}

ProtoJson::Array::~Array()
{
    Destroy();
}  

void ProtoJson::Array::Destroy()
{
    for (unsigned int i = 0; i < array_len; i++)
    {
        Item* item = array_buf[i];
        if (NULL != item) delete item;
    }
    if (NULL != array_buf)
    {
        delete[] array_buf;
        array_buf = NULL;
    }
    array_len = 0;
}  // end ProtoJson::Array::Destroy()

bool ProtoJson::Array::AppendString(const char* text)
{
    ProtoJson::String* string = new ProtoJson::String();
    if ((NULL == string) || !string->SetText(text) || !AppendValue(*string))
    {
        PLOG(PL_ERROR, "ProtoJson::Array::AppendString() error: %s\n", GetErrorString());
        if (NULL != string) delete string;
        return false;
    }
    return true;
}  // end ProtoJson::Array::AppendString()

const char* ProtoJson::Array::GetString(unsigned int index)
{
    const ProtoJson::Value* value = GetValue(index);
    if ((NULL != value) && (Item::STRING == value->GetType()))
        return static_cast<const ProtoJson::String*>(value)->GetText();
    else
        return NULL;
}  // end ProtoJson::Array::GetString()

bool ProtoJson::Array::AppendValue(Value& value)
{
    // TBD - should we do more clever memory mgmnt for better performance 
    //       at expense of a little extra memory usage?
    unsigned int len = array_len + 1;
    Value** buf = new Value*[len];
    if (NULL == buf)
    {
        PLOG(PL_ERROR, "ProtoJson::Array::AppendValue() new array buffer error: %s\n", GetErrorString());
        return false;
    }
    if (NULL != array_buf)
    {
        memcpy(buf, array_buf, array_len*sizeof(Value*));
        delete[] array_buf;
    }
    buf[array_len] = &value;
    array_buf = buf;
    array_len = len;
    value.SetParent(this);
    return true;
}  // end ProtoJson::Array::AppendValue()

// "index" MUST be in existing array range for now
void ProtoJson::Array::SetValue(unsigned int index, Value& value)
{
    ClearValue(index);
    if (index < array_len) 
    {
        array_buf[index] = &value;
        value.SetParent(this);
    }
    else
    {
        PLOG(PL_ERROR, "ProtoJson::Array::SetValue() error: out-of-bounds index!\n");
    }
}  // end ProtoJson::Array::SetValue()
            
void ProtoJson::Array::ClearValue(unsigned int index)
{
    if (index < array_len) 
    {
        Value* value = array_buf[index];
        array_buf[index] = NULL;
        delete value;
    }
}  // end ProtoJson::Array::ClearValue()


const ProtoJson::Value* ProtoJson::Array::GetValue(unsigned int index) const
{
    if (index < array_len)
    {
        return array_buf[index];
    }
    else
    {
        PLOG(PL_WARN, "ProtoJson::Array::GetValue() warning: out-of-bounds index!\n");
        return NULL;
    }
}  // end ProtoJson::Array::GetValue()

ProtoJson::Value* ProtoJson::Array::AccessValue(unsigned int index)
{
    if (index < array_len)
    {
        return array_buf[index];
    }
    else
    {
        PLOG(PL_WARN, "ProtoJson::Array::GetValue() warning: out-of-bounds index!\n");
        return NULL;
    }
}  // end ProtoJson::Array::GetValue()

ProtoJson::Object::Object(ProtoJson::Item* theParent)
 : ProtoJson::Item(OBJECT, theParent)
{
}

ProtoJson::Object::~Object()
{
    Destroy();
}

void ProtoJson::Object::Destroy()
{
    ProtoSortedTreeTemplate<Entry>::Destroy();
}  // end ProtoJson::Object::Destroy()

bool ProtoJson::Object::InsertEntry(const char* key, Value& value)
{
    Entry* entry = new Entry();
    if ((NULL == entry) || !entry->SetKey(key) || !InsertEntry(*entry))
    {
        PLOG(PL_ERROR, "ProtoJson::Object::InsertEntry() error inserting new Entry: %s\n", GetErrorString());
        if (NULL != entry) delete entry;
        return false;
    }
    entry->SetValue(&value);
    return true;
}  // end ProtoJson::Object::InsertEntry()

bool ProtoJson::Object::InsertString(const char* key, const char* text)
{
    ProtoJson::String* string = new ProtoJson::String();
    if ((NULL == string) || !string->SetText(text) || !InsertEntry(key, *string))
    {
        PLOG(PL_ERROR, "ProtoJson::Object::InsertString() error inserting new String: %s\n", GetErrorString());
        if (NULL != string) delete string;
        return false;
    }
    return true;
}  // end ProtoJson:::Object::InsertString()

bool ProtoJson::Object::InsertBoolean(const char* key, bool state)
{
    ProtoJson::Boolean* boolean = new ProtoJson::Boolean(state);
    if ((NULL == boolean) || !InsertEntry(key, *boolean))
    {
        PLOG(PL_ERROR, "ProtoJson::Object::InsertString() error inserting new Boolean: %s\n", GetErrorString());
        if (NULL != boolean) delete boolean;
        return false;
    }
    return true;
}  // end ProtoJson:::Object::InsertBoolean()

bool ProtoJson::Object::InsertEntry(Entry& entry)
{
    if (Insert(entry))
    {
        entry.SetParent(this);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoJson::Object::InsertEntry() error: %s\n", GetErrorString());
        return false;
    }
}  // end ProtoJson::Object::InsertEntry()

// Returns first String text matching "key"
const char* ProtoJson::Object::GetString(const char* key)
{
    Iterator iterator(*this);
    if (!iterator.Reset(false, key)) return NULL;
    ProtoJson::Entry* entry;
    while (NULL != (entry = iterator.GetNextEntry()))
    {
        const ProtoJson::Value* value = entry->GetValue();
        if ((NULL != value) && STRING == value->GetType())
            return static_cast<const ProtoJson::String*>(value)->GetText();
    }    
    return NULL;
}  // end ProtoJson::Object::GetString()

// Returns first Boolean value matching "key"
bool ProtoJson::Object::GetBoolean(const char* key)
{
    Iterator iterator(*this);
    if (!iterator.Reset(false, key)) return false;
    ProtoJson::Entry* entry;
    while (NULL != (entry = iterator.GetNextEntry()))
    {
        const ProtoJson::Value* value = entry->GetValue();
        if (NULL != value)
        {
            if (TRUE == value->GetType())
                return true;
            else if (FALSE == value->GetType())
                return false;
        }
    }    
    return false;
}  // end ProtoJson::Object::GetBoolean()

// Returns first Boolean value matching "key"
ProtoJson::Array* ProtoJson::Object::GetArray(const char* key)
{
    Iterator iterator(*this);
    if (!iterator.Reset(false, key)) return NULL;
    ProtoJson::Entry* entry;
    while (NULL != (entry = iterator.GetNextEntry()))
    {
        ProtoJson::Value* value = entry->AccessValue();
        if ((NULL != value) && (ARRAY == value->GetType()))
            return static_cast<ProtoJson::Array*>(value);
    }    
    return NULL;
}  // end ProtoJson::Object::GetArray()


ProtoJson::Object::Iterator::Iterator(Object& object)
 : ProtoSortedTreeTemplate<Entry>::Iterator(object), match_key(NULL)
{   
}      

ProtoJson::Object::Iterator::~Iterator()
{
    if (NULL != match_key)
    {
        delete[] match_key;
        match_key = NULL;
    }
}

bool ProtoJson::Object::Iterator::Reset(bool reverse, const char* key)
{
    if (NULL != key)
    {
        if ((NULL == match_key) || (0 != strcmp(match_key, key)))
        {
            if (NULL != match_key) delete[] match_key;
            if (NULL == (match_key = new char[strlen(key)+1]))
            {
                PLOG(PL_ERROR, "ProtoJson::Object::Iterator::Reset() new match_key error: %s\n", GetErrorString());
                return false;
            }
            strcpy(match_key, key);
        }
    }
    else if (NULL != match_key)
    {
        delete[] match_key;
        match_key = NULL;
    }
    ProtoSortedTreeTemplate<Entry>::Iterator::Reset(reverse, key, (unsigned int)((NULL != key) ? 8*(strlen(key)+1) : 0));
    return true;
}  // end ProtoJson::Object::Iterator::Reset()

ProtoJson::Entry* ProtoJson::Object::Iterator::GetNextEntry()
{
    ProtoJson::Entry* nextEntry = ProtoSortedTreeTemplate<Entry>::Iterator::GetNextItem();
    if ((NULL != nextEntry) && (NULL != match_key))
    {
        if (0 != strncmp(match_key, nextEntry->GetKey(), strlen(match_key)))
            return NULL;
    }
    return nextEntry;
}  // end ProtoJson::Object::Iterator::GetNextEntry()

ProtoJson::Entry* ProtoJson::Object::Iterator::GetPrevEntry()
{
    ProtoJson::Entry* nextEntry = ProtoSortedTreeTemplate<Entry>::Iterator::GetPrevItem();
    if (NULL != match_key)
    {
        if (0 != strncmp(match_key, nextEntry->GetKey(), strlen(match_key)))
            return NULL;
    }
    return nextEntry;
}  // end ProtoJson::Object::Iterator::GetPrevEntry()               

ProtoJson::Entry::Entry(ProtoJson::Item* theParent)
  : ProtoJson::Item(ENTRY, theParent), key(NULL), keysize(0), value(NULL)
{
}
            
ProtoJson::Entry::~Entry()
{
    if (NULL != value)
    {
        delete value;
        value = NULL;
    }
    if (NULL != key)
    {
        delete[] key;
        key = NULL;
    }
    keysize = 0;
}

bool ProtoJson::Entry::SetKey(const char* text)
{
    if (NULL != key) delete[] key;
    keysize = (unsigned int)strlen(text) + 1;  // include null termination
    if (NULL == (key = new char[keysize]))
    {
        keysize = 0;
        return false;
    }
    strcpy(key, text);
    keysize = keysize << 3;  // convert bytes to bits
    return true;
}  // end ProtoJson::Entry::SetKey()

void ProtoJson::Entry::SetValue(Value* theValue)
{
    if (NULL != value) delete value;
    if (NULL != theValue) theValue->SetParent(this);
    value = theValue;
}  // end ProtoJson::Entry::SetValue()


ProtoJson::Document::Document()
 : item_count(0)
{
}  

ProtoJson::Document::~Document()
{
    item_list.Destroy();
    item_count = 0;
}  

bool ProtoJson::Document::AddItem(ProtoJson::Item& item)
{
    if (item_list.Append(item))
    {
        item.SetParent(NULL);
        item_count++;
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoJson::Document::AddItem() error: %s\n", GetErrorString());
        return false;
    }
}  // end  ProtoJson::Document::AddItem()

void ProtoJson::Document::RemoveItem(ProtoJson::Item& item)
{
    if (item_list.Contains(item))
    {
        item_list.Remove(item);
        item_count--;
    }
}  // end ProtoJson::Document::RemoveItem()
                

void ProtoJson::Document::Print(FILE* filePtr)
{
    ItemList stack;
    unsigned int stackDepth = 0;
    const char* indent = "    ";  // 4 spaces
    Iterator iterator(*this);
    ProtoJson::Value* prevValue = NULL;
    ProtoJson::Value* value;
    
    if (item_count > 1)
    {
        // If document has multiple top level items, we 
        // present them as a top level array
        fprintf(filePtr, "[\n %s", indent);
        stackDepth = 1;
    }
    while (NULL != (value = iterator.GetNextItem()))
    {
        if (NULL != prevValue)
        {
            ProtoJson::Value* savePrev = prevValue;
            while (value->GetParent() != stack.GetHead())
            {
                prevValue = stack.RemoveHead();
                switch (prevValue->GetType())
                {
                    case Value::OBJECT:
                        stackDepth--;
                        if(Value::OBJECT != savePrev->GetType())
                        {
                            fprintf(filePtr, "\n");
                            for (unsigned int i = 0; i < stackDepth; i++)
                                fprintf(filePtr, " %s", indent);
                        }
                        // else was an NONE object
                        fprintf(filePtr, "}");
                        break;
                    case Value::ARRAY:
                        stackDepth--;
                        if(Value::ARRAY != savePrev->GetType())
                        {
                            fprintf(filePtr, "\n");
                            for (unsigned int i = 0; i < stackDepth; i++)
                                fprintf(filePtr, " %s", indent);
                        }
                        // else was an NONE array
                        fprintf(filePtr, "]");
                        break;
                        
                    case Value::ENTRY:
                    {
                        Item::Type etype = static_cast<Entry*>(prevValue)->GetValue()->GetType();
                        if ((Value::ARRAY != etype) && (Value::OBJECT != etype))
                            stackDepth--;
                        break;
                    }
                    default:
                        // should never occur
                        ASSERT(0);
                        break;
                }
            }
            // Note this does _not_ comma delimit top level document items (is that correct???)
            //if ((NULL != value->GetParent()) && (value->GetParent() == prevValue->GetParent()))
            if (value->GetParent() == prevValue->GetParent())
                fprintf(filePtr, ",");
            
            if ((Value::ENTRY != savePrev->GetType()) ||
                (Value::OBJECT == value->GetType()) ||
                (Value::ARRAY == value->GetType()))
            {
                fprintf(filePtr, "\n");
                for (unsigned int i = 0; i < stackDepth; i++)
                    fprintf(filePtr, " %s", indent);
            }
        }
        
        PrintValue(filePtr, *value);
        
        switch (value->GetType())
        {
            case Value::OBJECT:
            case Value::ARRAY:
                stack.Prepend(*value);
                stackDepth++;
                break;
            case Value::ENTRY:
            {
                stack.Prepend(*value);
                Item::Type etype = static_cast<Entry*>(value)->GetValue()->GetType();
                if ((Value::ARRAY != etype) && (Value::OBJECT != etype))
                    stackDepth++;
                break;
            }
            default:
                break;
        }
        
        prevValue = value;
         
    }
    while (NULL != prevValue)
    {
        switch (prevValue->GetType())
        {
            case Value::OBJECT:
                fprintf(filePtr, "\n");
                for (unsigned int i = 0; i < stackDepth; i++)
                    fprintf(filePtr, " %s", indent);
                fprintf(filePtr, "}");
                break;
            case Value::ARRAY:
                fprintf(filePtr, "\n");
                for (unsigned int i = 0; i < stackDepth; i++)
                    fprintf(filePtr, " %s", indent);
                fprintf(filePtr, "]");
                break;
            default:
                // should be an ENTRY?
                break;
        }
        prevValue = stack.RemoveHead();
        if (NULL != prevValue) 
        {
            if (Value::ENTRY == prevValue->GetType())
            {
                Item::Type etype = static_cast<Entry*>(prevValue)->GetValue()->GetType();
                if ((Value::ARRAY != etype) && (Value::OBJECT != etype))
                    stackDepth--;
            }   
            else //if (0 != stackDepth) 
            {
                stackDepth--;
            }
        }
    }
    fprintf(filePtr, "\n");
    if (item_count > 1) fprintf(filePtr, "]\n");
    
}  // end ProtoJson::Document::Print()

void ProtoJson::Document::PrintValue(FILE* filePtr, const Value& value)
{
    switch (value.GetType())
    {
        case Value::ENTRY:
        {
            const Entry& entry = static_cast<const Entry&>(value);
            fprintf(filePtr, "\"%s\" : ", entry.GetKey());
            break;
        }
        case Value::STRING:
        {
            const char* text = static_cast<const String&>(value).GetText();
            //fprintf(filePtr, "\"%s\"", (NULL != text) ? text : "");
            PrintString(filePtr, text);
            break;
        }
        case Value::NUMBER:
        {
            const Number& number = static_cast<const Number&>(value);
            if (number.IsFloat())
                fprintf(filePtr, "%f", number.GetDouble());
            else
                fprintf(filePtr, "%d", number.GetInteger());
            break;
        }
        case Value::OBJECT:
            fprintf(filePtr, "{");
            break;
        case Value::ARRAY:
            fprintf(filePtr, "[");
            break;
        case Value::TRUE:
            fprintf(filePtr, "true");
            break;
        case Value::FALSE:
            fprintf(filePtr, "false");
            break;
        case Value::NONE:
            fprintf(filePtr, "null");
            break;
        default:
            ASSERT(0);
            break;
    }
}  // end ProtoJson::Document::PrintValue() 

void ProtoJson::Document::PrintString(FILE* filePtr, const char* text)
{
    // Converts any escapable characters to corresponding sequence
    fprintf(filePtr, "\"");
    const char* ptr = text;
    while ('\0' != *ptr)
    {
        char escape = Parser::GetEscapeCode(*ptr);
        if (0 != escape)
            fprintf(filePtr, "\\%c", escape);
        else
            fprintf(filePtr, "%c", *ptr);
        ptr++;
    }
    fprintf(filePtr, "\"");
}  // end ProtoJson::Document::PrintString()

ProtoJson::Document::Iterator::Iterator(Document& document, bool depthFirst)
 : list_iterator(document.item_list), depth_first(depthFirst)
{
}

ProtoJson::Document::Iterator::~Iterator()
{
}

ProtoJson::Item* ProtoJson::Document::Iterator::GetNextItem()
{
    Value* currentItem = pending_list.RemoveHead();
    if (NULL == currentItem)
        currentItem = list_iterator.GetNextItem();
    if (NULL != currentItem)
    {
        if (Value::ARRAY == currentItem->GetType())
        {
            // Put array items into pending_list
            Array* array = static_cast<Array*>(currentItem);
            unsigned arrayLength = array->GetLength();
            for (unsigned int i = 0; i < arrayLength; i++)
            {
                unsigned int index = arrayLength - i - 1;
                Value* value = array->AccessValue(index);
                if (NULL == value)
                {
                    if (NULL == (value = new NullValue(value)))
                    {
                        PLOG(PL_ERROR, "ProtoJson::Document::Iterator::GetNextItem() new NullValue() error: %s\n", 
                                       GetErrorString());
                        return NULL;
                    }
                    array->SetValue(index, *value);
                }
                bool result = depth_first ? pending_list.Prepend(*value) : pending_list.Append(*value);
                if (!result)
                {
                    PLOG(PL_ERROR, "ProtoJson::Document::Iterator::GetNextItem() error: unable to update pending_list\n");
                    return NULL;
                }   
            }
        }
        else if (Value::OBJECT == currentItem->GetType())
        {
            // Put object entries into pending_list
            Object* object = static_cast<Object*>(currentItem);
            Object::Iterator iterator(*object);  
            iterator.Reset(true);  // reverse to make pending_list order right
            Entry* entry;
            while (NULL != (entry = iterator.GetPrevEntry()))
            {
                bool result = depth_first ? pending_list.Prepend(*entry) : pending_list.Append(*entry);
                if (!result)
                {
                    PLOG(PL_ERROR, "ProtoJson::Document::Iterator::GetNextItem() error: unable to update pending_list\n");
                    return NULL;
                } 
            }
        }
        else if (Value::ENTRY == currentItem->GetType())
        {
            // Put entry value into pending_list
            Entry* entry = static_cast<Entry*>(currentItem);
            Value* value = entry->AccessValue();
            if (NULL == value)
            {
                if (NULL == (value = new NullValue(entry)))
                {
                    PLOG(PL_ERROR, "ProtoJson::Document::Iterator::GetNextItem() new NullValue() error: %s\n", 
                                   GetErrorString());
                    return NULL;
                }
                entry->SetValue(value);
            }
            bool result = depth_first ? pending_list.Prepend(*value) : pending_list.Append(*value);
            if (!result)
            {
                PLOG(PL_ERROR, "ProtoJson::Document::Iterator::GetNextItem() error: unable to update pending_list\n");
                return NULL;
            }   
        }
    }
    return currentItem;
}  // end ProtoJson::Document::Iterator::GetNextItem()

// Delimiters for parsing
const char ProtoJson::Parser::OBJECT_START = '{';
const char ProtoJson::Parser::OBJECT_END = '}';
const char ProtoJson::Parser::ARRAY_START = '[';
const char ProtoJson::Parser::ARRAY_END = ']';
const char ProtoJson::Parser::QUOTE ='\"';
const char ProtoJson::Parser::COLON = ':';
const char ProtoJson::Parser::COMMA = ',';
const char ProtoJson::Parser::ESCAPE = '\\';
const char ProtoJson::Parser::TRUE_START = 't';
const char ProtoJson::Parser::FALSE_START = 'f';
const char ProtoJson::Parser::NULL_START = 'n';    
            
ProtoJson::Parser::Parser()
 : current_document(NULL), current_item(NULL), input_offset(0),
   input_escape_pending(false), is_escaped(false), 
   seek_colon(false), temp_buffer(NULL),
   temp_buffer_max(0), temp_buffer_len(0)
{
}

ProtoJson::Parser::~Parser()
{
    Destroy();
}

void ProtoJson::Parser::Destroy()
{
    if (NULL != current_document)
    {
        current_document->Destroy();
        delete current_document;
        current_document = NULL;
    }
    if (NULL != current_item)
    {
        delete current_item;
        current_item = NULL;
    }
    input_offset = 0;
    input_escape_pending = false;
    is_escaped = false;
    seek_colon = false;
    if (NULL != temp_buffer)
    {
        delete[] temp_buffer;
        temp_buffer = NULL;
    }
    temp_buffer_len = temp_buffer_max = 0;
}  // end ProtoJson::Parser::Destroy()

bool ProtoJson::Parser::LoadDocument(const char *path, Document* document)
{
    FILE* infile = fopen(path, "r");
    if (NULL == infile)
    {
        PLOG(PL_ERROR, "ProtoJson::Parser::LoadDocument() error opening file: %s\n", GetErrorString());
        return false;
    }
    if (NULL != document)
    {
        if (NULL != current_document)
            delete current_document;
        current_document = document;
    }
    Status status = PARSE_MORE;
    size_t result;
    char buffer[1024];
    while (0 != (result = fread(buffer, sizeof(char), 1024, infile)))
    {
        status = ProcessInput(buffer, (unsigned int)result);
        if (PARSE_ERROR == status) 
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::LoadDocument() error: invalid JSON document!\n");
            if (NULL != document) DetachDocument();
            return false;
        }
    }
    if (PARSE_MORE == status)
    {
        PLOG(PL_ERROR, "ProtoJson::Parser::LoadDocument() error: incomplete JSON document!\n");
        if (NULL != document) DetachDocument();
        return false;
    }
    else
    {
        if (NULL != document) DetachDocument();
        return true;
    }
}  // end ProtoJson::Parser::LoadDocument()


ProtoJson::Document* ProtoJson::Parser::DetachDocument()
{
    Document* doc = current_document;
    current_document = NULL;
    return doc;
}  // end ProtoJson::Parser::DetachDocument()


// This infers the Item type from the first character
// of the Item's textual representation   
ProtoJson::Item::Type ProtoJson::Parser::GetType(char c)
{
    c = tolower(c);
    switch (c)
    {
        case OBJECT_START:
            return Item::OBJECT;
        case ARRAY_START:
            return Item::ARRAY;
        case QUOTE:
            return Item::STRING;
        case TRUE_START:
            return Item::TRUE;
        case FALSE_START:
            return Item::FALSE;
        case NULL_START:
            return Item::NONE;
        default:
            return Item::NUMBER;
    }
}  // end ProtoJson::Parser::GetType()



bool ProtoJson::Parser::AddValueToParent(Item* parent, Item& value)
{
    // Values may be added to one of 3 parent types
    // 1) if NULL == parent, "value" is a root level item and MUST be an ARRAY or OBJECT, or
    // 2) Value items may be appended to ARRAYs, or
    // 3) Value items may be assigned to an Object Entry
    
    ASSERT(value.IsValue());
    
    if (NULL == parent)
    {
        //ASSERT((Item::ARRAY == value.GetType()) || (Item::OBJECT == value.GetType()));
        ASSERT(Item::INVALID != value.GetType());
        current_document->AddItem(value);
    }
    else
    {
        switch (parent->GetType())
        {
            case Item::ARRAY:
                if (!static_cast<Array*>(parent)->AppendValue(value))
                {
                    PLOG(PL_ERROR, "ProtoJson::Parser::AddValueToParent() error: unable to append array\n");
                    return false;
                }
                break;
            case Item::ENTRY:
                static_cast<Entry*>(parent)->SetValue(&value);
                break;
            default:
                PLOG(PL_ERROR, "ProtoJson::Parser::AddValueToParent() error: invalid parent type\n");
                ASSERT(0);
                return false;
        }
    }
    return true;
}  // end AddValueToParent()

bool ProtoJson::Parser::IsValidEscapeCode(char c)
{
    switch (c)
    {
        case 'b':   // backspace
        case 'f':   // form feed
        case 'n':   // new line
        case 'r':   // carriage return
        case 't':   // tab
        case '"':   // double quote
        case '\\':  // backslash
            return true;
        default:
            return false;
    }
}  // end ProtoJson::Parser::IsValidEscapeCode()

char ProtoJson::Parser::GetEscapeCode(char c)
{
    // returns escape code for input char
    // (or zero if non-escaped character)
    switch (c)
    {
        case '\b':   // backspace
            return 'b';
        case '\f':   // form feed
            return 'f';
        case '\n':   // new line
            return 'n';
        case '\r':   // carriage return
            return 'r';
        case '\t':   // tab
            return 't';
        case '"':   // double quote
            return '"';
        case '\\':  // backslash
            return '\\';
        default:
            return 0;
    }
}  // end ProtoJson::Parser::GetEscapeCode()

char ProtoJson::Parser::Unescape(char c)
{
    // returns char corresponding to escape code 
    // (or input char if non-escaped character)
    switch (c)
    {
        case 'b':   // backspace
            return '\b';
        case 'f':   // form feed
            return '\f';
        case 'n':   // new line
            return '\n';
        case 'r':   // carriage return
            return '\r';
        case 't':   // tab
            return '\t';
        case '"':   // double quote
            return '"';
        case '\\':  // backslash
            return '\\';
        default:
            return c;
    }
}  // end ProtoJson::Parser::Unescape()
    
bool ProtoJson::Parser::AddToString(String& string, const char* text, unsigned int length)
{
    // Unescape text being added
    // First, determine length of "unescaped" version of 'text'
    unsigned int count = 0;
    bool pending = input_escape_pending;
    for (unsigned int i = 0; i < length; i++)
    {
        char c = text[i];
        if (pending)
        {
            pending = false;
            if (IsValidEscapeCode(c))
            {
                count++;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoJson::Parser::AddToString() error: invalid escape sequence code '\\%c'!\n", c);
                return false;
            }
        }
        else if ('\\' == c)
        {
            pending = true;
            continue;
        }
        else
        {
            count++;
        }
    }
    size_t total = string.GetLength() + count;
    char* buffer = new char[total + 1]; // include null terminator
    if (NULL == buffer)
    {
        PLOG(PL_ERROR, "ProtoJson::Parser::AddToString() new buffer error: %s\n", GetErrorString());
        return false;
    }
    if (NULL != string.GetText())
        strcpy(buffer, string.GetText());
    if (0 != length)
    {
        // Convert/copy 'unescaped text' into new buffer
        char* ptr = buffer + string.GetLength();
        count = 0;
        for (unsigned int i = 0; i < length; i++)
        {
            char c = text[i];
            if (input_escape_pending)
            {
                input_escape_pending = false;
                // already validated escape codes above
                ptr[count++] = Unescape(c);
            }
            else if ('\\' == c)
            {
                input_escape_pending = true;
                continue;
            }
            else
            {
                ptr[count++] = c;
            }
        }
    }
    buffer[total] = '\0';
    string.SetTextPtr(buffer);
    return true;
}  // end ProtoJson::Parser::AddToString()
            
ProtoJson::Parser::Status ProtoJson::Parser::ProcessStringInput(const char* input, unsigned int length)
{
    bool start = (NULL == current_item) ? true : false;
    String* string;
    if (start)
    {
        // We're starting a new string, so create using top of stack as parent
        Item* parent = PeekStack();
        if ((NULL != parent) && ((Item::ENTRY != parent->GetType()) && (Item::ARRAY != parent->GetType())))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessStringInput() error: invalid JSON syntax\n");
            return PARSE_ERROR;
        }
        string = new String(parent);
        if (NULL == string)
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessStringInput() new String error: %s\n", GetErrorString());
            return PARSE_ERROR;
        }
        current_item = string;
    }
    else
    {
        ASSERT(Item::STRING == current_item->GetType());
        string = static_cast<String*>(current_item);
    }
    if (0 == length) return PARSE_MORE;  // need more input
    unsigned int i = 0;//start ? 1 : 0;  // to skip leading quote if applicable
    const char* startPtr = input + i;
    for (; i < length; i++)
    {
        char c = input[i];
        if (is_escaped) 
        {
            // TBD - should we convert escaped characters here,
            //       or do it within the AddToString() method?
            //       (and PrintString() should have option to
            //        output escaped or raw version of string)
            is_escaped = false;
            continue;
        }
        else if (ESCAPE == c)
        {
            is_escaped = true;
            continue;
        }
        else if (QUOTE == c)
        {
            // We've found the end of the string
            if (AddToString(*string, startPtr, i))
            {
                // consume string text and end QUOTE
                input_offset += i + 1;
                if (!AddValueToParent(string->AccessParent(), *string))
                {
                    PLOG(PL_ERROR, "ProtoJson::Parser::ProcessStringInput() error: unable to add to parent\n");
                    return PARSE_ERROR;
                }
                current_item = NULL;
                return PARSE_DONE;
            }
            else
            {
                return PARSE_ERROR;
            }
        }
    } 
    // Incomplete string, need more input
    if (AddToString(*string, startPtr, length))
    {
        input_offset += length;
        return PARSE_MORE;
    }
    else
    {
        return PARSE_ERROR;
    }
}  // end ProtoJson::Parser::ProcessStringInput()

bool ProtoJson::Parser::AddToTemp(const char* text, unsigned int length)
{
    // TBD - strip.convert escape sequences?
    if (0 == length) return true;
    unsigned int total = temp_buffer_len + length;
    if (total > temp_buffer_max)
    {
        char* buffer = new char[total+1];  // include null terminator
        if (NULL == buffer)
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::AddToTemp() new buffer error: %s\n", GetErrorString());
            return false;
        }
        //if (NULL != temp_buffer)
        if (0 != temp_buffer_len)
        {
            memcpy(buffer, temp_buffer, temp_buffer_len);
            delete[] temp_buffer;
        }
        memcpy(buffer + temp_buffer_len, text, length);
        buffer[total] = '\0';
        temp_buffer = buffer;
        temp_buffer_len = temp_buffer_max = total;
    }
    else
    {
        memcpy(temp_buffer + temp_buffer_len, text, length);
        temp_buffer[total] = '\0';
        temp_buffer_len = total;
    }
    return true;
}  // end ProtoJson::Parser::AddToTemp()

ProtoJson::Parser::Status ProtoJson::Parser::ProcessNumberInput(const char* input, unsigned int length)
{
    bool start = (NULL == current_item) ? true : false;
    Number* number;
    if (start)
    {
        // We're starting a new number, so create using top of stack as parent
        Item* parent = PeekStack();
        if ((NULL != parent) && ((Item::ENTRY != parent->GetType()) && (Item::ARRAY != parent->GetType())))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() error: invalid JSON syntax\n");
            return PARSE_ERROR;
        }
        number = new Number(parent);
        if (NULL == number)
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() new Number error: %s\n", GetErrorString());
            return PARSE_ERROR;
        }
        current_item = number;
        temp_buffer_len = 0;
    }
    else
    {
        ASSERT(Item::NUMBER == current_item->GetType());
        number = static_cast<Number*>(current_item);
    }
    if (0 == length) return PARSE_MORE;  // need more input
    for (unsigned int i = 0; i < length; i++)
    {
        char c = input[i];
        if (isspace(c) || (COMMA == c) || (OBJECT_END == c) || (ARRAY_END == c))
        {
            // end of number string found
            if (AddToTemp(input, i))
            {
                input_offset += i;  // consume number text
                if (!number->SetValue(temp_buffer))
                {
                    PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() error: invalid number text\n");
                    return PARSE_ERROR;
                }
                temp_buffer_len = 0;  // reset temp_buffer
                if (!AddValueToParent(number->AccessParent(), *number))
                {
                    PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() error: unable to add number to parent\n");
                    return PARSE_ERROR;
                }
                current_item = NULL;
                return PARSE_DONE;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() error: unable to update temp_buffer\n");
                return PARSE_ERROR;
            }
        }
    }
    // incomplete number text, need more input
    if (AddToTemp(input, length))
    {
        input_offset += length;
        return PARSE_MORE;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() error: unable to update temp_buffer\n");
        return PARSE_ERROR;
    }   
}  // end ProtoJson::Parser::ProcessNumberInput()

bool ProtoJson::Parser::FixedItemIsValid(Item::Type type)
{
    char* ptr = temp_buffer;
    // Convert temp_buffer to lower case for validation
    while ('\0' != *ptr) 
    {
        *ptr = tolower(*ptr);
        ptr++;
    }
    switch (type)
    {
        case Item::TRUE:
            return (0 == strcmp(temp_buffer, "true"));
            break;
        case Item::FALSE:
            return (0 == strcmp(temp_buffer, "false"));
            break;
        case Item::NONE:
            return (0 == strcmp(temp_buffer, "null"));
            break;
        default:
            return false;
    }
}  // end ProtoJson::Parser::FixedItemIsValid()

ProtoJson::Parser::Status ProtoJson::Parser::ProcessFixedInput(const char* input, unsigned int length)
{
    // This is used for "true", "false", and "null" value fields (i.e. fixed text)
    // Basically, the first character gives it away, but we validate
    bool start = (NULL == current_item) ? true : false;
    if (start)
    {
        // We're starting a new number, so create using top of stack as parent
        Item* parent = PeekStack();
        if ((NULL != parent) && ((Item::ENTRY != parent->GetType()) && (Item::ARRAY != parent->GetType())))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessNumberInput() error: invalid JSON syntax\n");
            return PARSE_ERROR;
        }
        // TBD if parent is an object, then this MUST be the object's value field
        char c = tolower(*input);
        switch (c)
        {
            case TRUE_START:
                current_item = new ProtoJson::Boolean(true, parent);
                break;
            case FALSE_START:
                current_item = new ProtoJson::Boolean(false, parent);
                break;
            case NULL_START:
                current_item = new ProtoJson::NullValue(parent);
                break;
            default:
                PLOG(PL_ERROR, "ProtoJson::Parser::ProcessFixedInput(%5s) error: invalid text\n", input);
                return PARSE_ERROR;
        }
    }
    ASSERT((Item::TRUE == current_item->GetType()) ||
           (Item::FALSE == current_item->GetType()) ||
           (Item::NONE == current_item->GetType()));
    if (0 == length) return PARSE_MORE;  // need more input
    for (unsigned int i = 0; i < length; i++)
    {
        char c = input[i];
        if (isspace(c) || (COMMA == c) || (OBJECT_END == c) || (ARRAY_END == c))
        {
            // end of fixed value found
            if (AddToTemp(input, i))
            {
                // consume fixed item chars
                input_offset += i;
                if (!FixedItemIsValid(current_item->GetType()))
                {
                    PLOG(PL_ERROR, "ProtoJson::Parser::ProcessFixedInput(%s) error: invalid fixed text\n", temp_buffer);
                    return PARSE_ERROR;
                }
                temp_buffer_len = 0;  // reset temp_buffer
                if (!AddValueToParent(current_item->AccessParent(), *current_item))
                {
                    PLOG(PL_ERROR, "ProtoJson::Parser::ProcessFixedInput() error: unable to add \"%s\" value to parent\n", temp_buffer);
                    return PARSE_ERROR;
                }
                current_item = NULL;
                return PARSE_DONE;
            }
            else
            {
                PLOG(PL_ERROR, "ProtoJson::Parser::ProcessFixedInput() error: unable to update temp_buffer\n");
                return PARSE_ERROR;
            }
        }
    }
    // Did not yet find end of fixed text 
    if (AddToTemp(input, length))
    {
        return PARSE_MORE;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoJson::Parser::ProcessFixedInput() error: unable to update temp_buffer\n");
        return PARSE_ERROR;
    }
}  // end ProtoJson::Parser::ProcessFixedInput()

ProtoJson::Parser::Status ProtoJson::Parser::ProcessArrayInput(const char* input, unsigned int length)
{
    // Seeking array value items or ARRAY_END
    bool start = (NULL == current_item) ? true : false;
    Array* array;
    if (start)
    {
        if (NULL == (array = new Array(PeekStack())))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessArrayInput() new Array error: %s\n", GetErrorString());
            return PARSE_ERROR;
        }
        current_item = array;
    }
    else
    {
        ASSERT(Item::ARRAY == current_item->GetType());
        array = static_cast<Array*>(current_item);
    }
    if (0 == length) return PARSE_MORE;  // need more input
    for (unsigned int i = 0; i < length; i++)
    {
        char c = input[i];
        if (isspace(c))
            continue;
        else if (COMMA == c)
            // TBD - validate that a comma is indeed followed by a value???
            // skipping blank array fields is permissive, but maybe OK?
            continue;
        else if (ARRAY_END == c)
        {
            // consume white space + ARRAY_END
            input_offset += i + 1;
            //  Attach completed array to parent
            if (!AddValueToParent(array->AccessParent(), *array))
            {
                PLOG(PL_ERROR, "ProtoJson::Parser::ProcessArrayInput() error: unable to add array to parent\n");
                return PARSE_ERROR;
            }
            current_item = NULL;
            return PARSE_DONE;
        }
        else
        {
            // Should be first character of an array value item
            PushStack(*current_item);
            current_item = NULL;
            input_offset += i;
            return ProcessValueInput(input + i, length - i);
        }
    }
    input_offset += length;
    return PARSE_MORE;  // more input needed
}  // end ProtoJson::Parser::ProcessArrayInput()

ProtoJson::Parser::Status ProtoJson::Parser::ProcessEntryInput(const char* input, unsigned int length)
{
    // Seeking entry key, Value, or end-of-entry delimiter
    bool start = (NULL == current_item) ? true : false;
    Entry* entry;
    Object* object;  // parent of entry
    if (start)
    {
        // An ENTRY's parent MUST always be an OBJECT
        ASSERT((NULL != PeekStack()) && (Item::OBJECT == PeekStack()->GetType()));
        object = static_cast<Object*>(PeekStack());
        if (NULL == (entry = new Entry(object)))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessEntryInput() new Entry error: %s\n", GetErrorString());
            return PARSE_ERROR;
        }
        current_item = entry;
    }
    else
    {
        ASSERT(Item::ENTRY == current_item->GetType());
        entry = static_cast<Entry*>(current_item);
        object = static_cast<Object*>(entry->AccessParent());
    }
    // Notes:
    // 1) First, the Entry "key" is sought, cached in temp_buffer and set upon completion
    // 2) Second, the key must be followed by a colon delimiter so we find that
    // 3) Then, we push the Entry to stack and parse for it's Value field, possibly going down multiple levels
    // 4) When the Value is complete, it adds itself to the Entry and returns PARSE_DONE to upper layer (which pops stack)
    // 5) At this point, we look for the end-of-entry (either COMMA or END_OBJECT)
    //
    //   The key detail here is the Value item knows to add itself to Entry (or Array) parents upon completion.
    //   That happens in the code for the different Value input processors

    
    enum Mode
    {
        SEEK_KEY,   // accumulating key string (starts here)
        SEEK_COLON,
        SEEK_VALUE, // got key, look for value
        SEEK_TERM    // got value, look for end-of-entry (termination) delimiter
    };
    Mode mode;
    if (NULL == entry->GetKey())
        mode = SEEK_KEY;
    else if (seek_colon)
        mode = SEEK_COLON;
    else if (NULL == entry->GetValue())
        mode = SEEK_VALUE;
    else
        mode = SEEK_TERM;
    if (0 == length) return PARSE_MORE;  // need more input
    for (unsigned int i = 0; i < length; i++)
    {
        char c = input[i];
        switch (mode)
        {
            case SEEK_KEY:
                if (is_escaped)
                {
                    is_escaped = false;
                    continue;
                }
                else if (ESCAPE == c)
                {
                    is_escaped = true;
                    continue;
                }
                else if (c == QUOTE)
                {
                    // non-escaped QUOTE is end of key string
                    if (!AddToTemp(input, i))
                    {
                        PLOG(PL_ERROR, "ProtoJson::Parser::ProcessEntryInput() error: unable to add to temp_buffer\n");
                        return PARSE_ERROR;
                    }
                    if (!entry->SetKey(temp_buffer))
                    {
                        PLOG(PL_ERROR, "ProtoJson::Parser::ProcessEntryInput() error: unable to set entry key\n");
                        return PARSE_ERROR;
                    }
                    temp_buffer_len = 0;  // reset temp_buffer
                    seek_colon = true;
                    mode = SEEK_COLON;
                }
                break;
            case SEEK_COLON:
                if (COLON == c)
                {
                    // we don't need to consume anything here since
                    // it's just a mode change with no return
                    seek_colon = false;
                    mode = SEEK_VALUE;
                }
                break;
            case SEEK_VALUE:
                if (isspace(c))
                {
                    continue;  // skip white space
                }
                else
                {
                    // upper level will re-enter and mode with be SEEK_TERM if value completed
                    input_offset += i;  // consume skipped white space
                    PushStack(*entry);
                    current_item = NULL;
                    return ProcessValueInput(input + i, length - i);
                }
                break;
            case SEEK_TERM:
                if (isspace(c))
                {
                    continue;  // skip white space
                }
                switch (c)
                {
                    // Either of these indicates end-of-entry, but note we
                    // preserve the OBJECT_END for the upper layer object parsing
                    case COMMA:
                        input_offset++;  // consume comma
                    case OBJECT_END:
                        input_offset += i; // consume white space
                        if (!object->InsertEntry(*entry))
                        {
                            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessEntryInput() error: entry insertion failure\n");
                            return PARSE_ERROR;
                        }
                        current_item = NULL;  // entry is finished, upper layer will pop object off stack
                        return PARSE_DONE;
                    default:
                        PLOG(PL_ERROR, "ProtoJson::Parser::ProcessEntryInput() error: invalid JSON input\n");
                        return PARSE_ERROR;
                }
                break;
        }  // end switch (mode)
    }  // end for (...)
    // if we make it here, we need more input
    // incomplete entry text, need more input
    if (AddToTemp(input, length))
    {
        input_offset += length;
        return PARSE_MORE;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoJson::Parser::ProcessEntryInput() error: unable to update temp_buffer\n");
        return PARSE_ERROR;
    }   
    
    //input_offset += length;
    //return PARSE_MORE; 
}  // end ProtoJson::Parser::ProcessEntryInput()

ProtoJson::Parser::Status ProtoJson::Parser::ProcessObjectInput(const char* input, unsigned int length)
{
    // Seeking object key,value items or OBJECT_END
    bool start = (NULL == current_item) ? true : false;
    Object* object;
    if (start)
    {
        if (NULL == (object = new Object(PeekStack())))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessObjectInput() new Object error: %s\n", GetErrorString());
            return PARSE_ERROR;
        }
        current_item = object;
    }
    else
    {
        ASSERT(Item::OBJECT == current_item->GetType());
        object = static_cast<Object*>(current_item);
    }
    if (0 == length) return PARSE_MORE;  // need more input
    for (unsigned int i = 0; i < length; i++)
    {
        char c = input[i];
        if (isspace(c))
            continue;
        else if (COMMA == c)
            // TBD - validate that a comma is indeed followed by a value???
            // skipping blank array fields is permissive, but maybe OK?
            continue;
        else if (OBJECT_END == c)
        {
            input_offset += i + 1;  // consume white space + OBJECT_END
            // Attach completed object to parent
            if (!AddValueToParent(object->AccessParent(), *object))
            {
                PLOG(PL_ERROR, "ProtoJson::Parser::ProcessObjectInput() error: unable to add object to parent\n");
                return PARSE_ERROR;
            }
            current_item = NULL;    // current_item completed
            return PARSE_DONE;
        }
        else if (QUOTE == c)
        {
            // Should be first character of an object key string
            PushStack(*current_item);
            current_item = NULL;
            input_offset += i + 1;  // consume whitespace + QUOTE
            return ProcessEntryInput(input + i + 1, length - (i + 1));
        }
        else
        {
            // Invalid JSON syntax 
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessObjectInput() error: invalid Object content\n");
            return PARSE_ERROR;
        }
    }
    input_offset += length;
    return PARSE_MORE;  // more input needed
}  // end ProtoJson::Parser::ProcessObjectInput()

ProtoJson::Parser::Status ProtoJson::Parser::ProcessValueInput(const char* input, unsigned int length)
{
    ASSERT(NULL == current_item);
    if (0 == length) return PARSE_MORE;
    // This is invoked to process ARRAY or OBJECT Item items
    // The first char of the "input" should be beginning of some Item item
    // (string, number, object, array, true, false, null)
    Item::Type type = GetType(input[0]);
    switch (type)
    {
        case Item::STRING:
            // consume QUOTE (string start delimiter)
            input_offset++;
            return ProcessStringInput(input+1, length-1);
        case Item::NUMBER:
            return ProcessNumberInput(input, length);
            break;
        case Item::OBJECT:
            // consume OBJECT_START delimiter
            input_offset++;
            return ProcessObjectInput(input+1, length-1);
            break;
        case Item::ARRAY:
            // consume ARRAY_START delimiter
            input_offset++;
            return ProcessArrayInput(input+1, length-1);
            break;
        case Item::TRUE:
        case Item::FALSE:
        case Item::NONE:
            return ProcessFixedInput(input, length);
        default:
            // Will not occur
            ASSERT(0);
            return PARSE_ERROR;
    }
}  // end Status ProtoJson::Parser::ProcessValueInput(()

// This routine will become the heart of
// our ProtoJson parser that processes input
// to form a ProtoJson document/tree

// This consumes all of the input provided with the following return values:
//  1) PARSE_ERROR indicating a JSON syntax error (or memory allocation error)
//  2) PARSE_MORE indicating more input is needed to  have a complete, valid JSON document
//  3) PARSE_DONE indicating a completed JSON document/stanza 
ProtoJson::Parser::Status ProtoJson::Parser::ProcessInput(const char* inputBuffer, unsigned int inputLength)
{
    if (NULL == current_document)
    {
        if (NULL == (current_document = new Document()))
        {
            PLOG(PL_ERROR, "ProtoJson::Parser::ProcessInput() new document error: %s\n", GetErrorString());
            return PARSE_ERROR;
        }
    }
    // Top level call to process some JSON input 
    Status result = PARSE_MORE;
    input_offset = 0;
    while (input_offset < inputLength)
    {
        //ProtoCheckLogAllocations(stdout);
        const char* input = inputBuffer + input_offset;
        unsigned int length = inputLength - input_offset;
        // TBD - should we pop the stack down in the lower parser layers instead?
        if (NULL == current_item) current_item = PopStack();
        if (NULL == current_item)
        {
            bool NONE = true;
            result = PARSE_DONE;  // default result for NONE document
            // Seeking top level item
            for (unsigned int i = 0; i < length; i++)
            {
                char c = input[i];
                // Skip white space
                if (isspace(c)) continue;
                switch (c)
                {
                    case ARRAY_START:
                    {
                        // consume skipped white space + ARRAY_START
                        NONE = false;
                        input_offset += (i + 1);
                        unsigned int remainder = (length > i) ? length - (i+1) : 0;
                        result = ProcessArrayInput(input + (i + 1), remainder);
                        break;
                    }
                    case OBJECT_START:
                    {
                        // consume skipped white space + OBJECT_START
                        NONE = false;
                        input_offset += (i + 1);
                        unsigned int remainder = (length > i) ? length - (i+1) : 0;
                        result = ProcessObjectInput(input + (i + 1), remainder);
                        break;
                    }
                    case QUOTE:
                    {
                        // consume skipped white space + QUOTE
                        NONE = false;
                        input_offset += (i + 1);
                        unsigned int remainder = (length > i) ? length - (i+1) : 0;
                        result = ProcessStringInput(input + (i + 1), remainder);
                        break;
                    }
                    case TRUE_START:
                    case FALSE_START:
                    case NULL_START:
                    {
                        NONE = false;
                        input_offset += i;
                        unsigned int remainder = (length > i) ? length - i : 0;
                        result = ProcessFixedInput(input + i, remainder);
                        break;
                    }
                    default:
                    {
                        if (0 == isdigit(c))
                            return PARSE_ERROR; // invalid input at top level 
                        NONE = false;
                        input_offset += i;
                        unsigned int remainder = (length > i) ? length - i : 0;
                        result = ProcessNumberInput(input + i, remainder);
                        break;
                    }
                        
                }
                break; // an increment of non-whitespace parsing occurred
            }
            if (NONE) input_offset += length;  // consumes skipped white space
            if (PARSE_ERROR == result) break;
        }
        else
        {
            switch (current_item->GetType())
            {
                case Item::ENTRY:
                    result = ProcessEntryInput(input, length);
                    break;
                case Item::STRING:
                    result = ProcessStringInput(input, length);
                    break;
                case Item::NUMBER:
                    result = ProcessNumberInput(input, length);
                    break;
                case Item::OBJECT:
                    result = ProcessObjectInput(input, length);
                    break;
                case Item::ARRAY:
                    result = ProcessArrayInput(input, length);
                    break;
                default:
                    result = ProcessFixedInput(input, length);
                    break;
            }
            if (PARSE_ERROR == result) break;
            //ASSERT ((result != PARSE_MORE) || (input_offset < inputLength));
        }
    }  // end while (length > 0)
    if (PARSE_DONE == result)
    {
        // Only really "DONE" if no pending current_item or stack, else need more input
        if ((NULL != current_item) || (NULL != PeekStack()))
            return PARSE_MORE;  // more input needed
        else
            return PARSE_DONE;
    }
    else
    {
        return result;
    }
}  // end ProtoJson::Parser::ProcessInput()



