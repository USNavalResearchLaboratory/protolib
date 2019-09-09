#ifndef _PROTO_JSON
#define _PROTO_JSON

// This provides a means for loading JSON input (from a file or text stream, etc) into a data structure
// The plan is to use this for configuration files, etc for Protolib protocol implementations.

// NOTES:
// 1) The parsing of ASCII text JSON files is pretty much complete. Work needs to be done to support UTF 
//    codings that can occur, etc.
//
// 2) Many backslash escapes are not handled or checked.  
//
// 3) The ProtoJson::Parser here loads JSON input text into a "document" data structure.  Object key,value pairs 
//    _are_ stored in dictionaries and Array elements are in indexed array structures.  A ProtoJson::Document 
//    is defined will be created with methods for iteration and queries. The ProtoJson::Document::Print() method
//    uses iteration over the document tree and can serve as an example.  Eventually, helper methods for 
//    programmatically constructing the Document/Tree will be supported, too.
//
//  4) The initial goal is to support configuration files for Protolib protocol implementations using the JSON
//     format since it is well documented, fairly structured, and more human readable/editable than some other formats.

#include "protoTree.h"
#include "protoQueue.h"

namespace ProtoJson
{
    class Item : public ProtoQueue::Item
    {
        public:
                
            virtual ~Item();    
                
            enum Type
            {
                INVALID = 0,  // for error handling
                ENTRY,        // Object key,value entries
                STRING,
                NUMBER,
                OBJECT,
                ARRAY,
                TRUE,
                FALSE,
                NONE    // we don't use 'NULL' because that collides
            };    
                
            static const char* GetTypeString(Type type);
                
            Type GetType() const
                {return type;}
            
            unsigned int GetLevel() const
                {return level;}
                    
            bool IsValue() const
                {return (type > ENTRY);}
            
            void SetParent(Item* theParent)  // TBD - make this only accessible to 'friend' classes
                {parent = theParent;}
            
            const Item* GetParent() const  // TBD - don't need this, can use stack instead??
                {return parent;}
            Item* AccessParent()  // TBD - don't need this, can use stack instead??
                {return parent;}
                 
        protected:
            Item(Type theType, Item* theParent = NULL);
        
        private:
            Type         type;
            Item*        parent;  // TBD - deprecate the "parent" member if we don't really need it
            unsigned int level;   // Item's depth in document tree (a convenience for printing, etc)
                                  // (If we implemented a container for our Document::Iterator class
                                  //  instead of using the default ProtoQueue mechanism, the container
                                  //  could be used to cache the item's level during iteration)
    };  // end class ProtoJson::Item
    
    typedef Item Value;  // for clarity and convenience
    
    class ItemList : public ProtoSimpleQueueTemplate<ProtoJson::Item> 
    {
        public:
            void AddItem(Item& item)
                {Append(item);}
    };  // end class ProtoJson::ItemList
    
    class String : public Value
    {
        public:
            String(Item* theParent = NULL);
            virtual ~String();
            
            bool Set(const char* theText);
            void SetTextPtr(char* textPtr);
            
            const char* GetText() const
                {return text;}
            
            size_t GetLength() const
                {return (NULL != text) ? strlen(text) : 0;}
            
        private:
            char*   text;     
            
    };  // end class ProtoJson::String
    
    // may be an int or double
    class Number : public Value
    {
        public:
            Number(Item* theParent = NULL);
            Number(int value, Item* theParent = NULL);          
            Number(double value, Item* theParent = NULL);
            
            virtual ~Number();
            
            bool SetValue(const char* text);
            void SetValue(double value)
            {
                floating = value;
                is_float = true;
            }
            void SetValue(int value)
            {
                integer = value;
                is_float = false;
            }
            bool IsFloat() const
                {return is_float;}
            double GetDouble() const
                {return is_float ? floating : (double)integer;}
            int GetInteger() const
                {return is_float ? (int)floating : integer;}
        private:
            bool        is_float;
            union
            {
                double  floating;
                int     integer;
            };     
    };  // end class ProtoJson::Number
    
     // This is used for JSON Object key,value pairs
    class Entry : public ProtoJson::Item, public ProtoSortedTree::Item
    {
        public:
            Entry(ProtoJson::Item* theParent = NULL);
            virtual ~Entry();
            
            bool SetKey(const char* text);
            
            const char* GetKey() const
                {return key;}
            unsigned int GetKeysize() const
                {return keysize;}
            
            void SetValue(Value* theValue);  // deletes old value, if any
            
            const Value* GetValue() const
                {return value;}
            Value* AccessValue() const
                {return value;}
            
        private:
            char*           key;
            unsigned int    keysize;
            Value*          value;
    };  // end class ProtoJson::Entry
    
    class Object : public Item, protected ProtoSortedTreeTemplate<Entry>
    {
        public:
            Object(ProtoJson::Item* theParent = NULL);
            virtual ~Object();
            void Destroy();
            
            
            bool InsertEntry(const char* key, Value& value);
            
            bool InsertEntry(Entry& entry);
            
            Entry* FindEntry(const char* key)
               {return Find(key, (strlen(key)+1)*8);}
            
            void RemoveEntry(Entry& entry)
            {
                    Remove(entry);
                    entry.SetParent(NULL);
            }
            
            class Iterator : protected ProtoSortedTreeTemplate<Entry>::Iterator
            {
                public:
                    Iterator(Object& object, bool reverse = false);
                    ~Iterator();
                    // If "key" is non-NULL, return matching entries only
                    Entry* GetNextEntry(const char* key = NULL);
                    Entry* GetPrevEntry(const char* key = NULL);
                private:
                    char* match_key;
                
            };  // end class ProtoJson::Object::Iterator
        
    };  // end class ProtoJson::Object
    
    class Array : public Value
    {
        public:
            Array(Item* theParent = NULL);
            virtual ~Array();
            
            void Destroy();
            
            unsigned int GetLength() const
                {return array_len;}
            
            bool AppendValue(Value& value);
            
            const Value* GetValue(unsigned int index) const;
            Value* AccessValue(unsigned int index);
            
            // "index" MUST be in existing array range for now
            void SetValue(unsigned int index, Value& value);
            void ClearValue(unsigned int index);  // remove/delete Value at index
                   
        private:
            Item**          array_buf;
            unsigned int    array_len;
    };  // end class ProtoJson::Array
    
    class TrueValue : public Value
    {
        public:
            TrueValue(Item* theParent = NULL) : Item(TRUE, theParent) {}
            virtual ~TrueValue() {}
    };  // end class ProtoJson::TrueValue
    
    class FalseValue : public Value
    {
        public:
            FalseValue(Item* theParent = NULL) : Item(FALSE, theParent) {}
            virtual ~FalseValue() {}
    };  // end class ProtoJson::FalseValue
    
    class NullValue : public Value
    {
        public:
            NullValue(Item* theParent = NULL) : Item(NONE, theParent) {}
            virtual ~NullValue() {}
    };  // end class ProtoJson::NullValue
    
    class Document : public ItemList
    {
        public:
            Document();
            ~Document();
            
            bool AddItem(ProtoJson::Item& item);
            void RemoveItem(ProtoJson::Item& item);
            unsigned int GetItemCount() const
                {return item_count;}
            
            void Print(FILE* filePtr);  // TBD - add a PrintToBuffer() method (or ConvertToText())
            static void PrintValue(FILE* filePtr, const Value& value);
            class Iterator
            {
                public:
                    Iterator(Document& document, bool depthFirst = true);
                    ~Iterator();
                    
                    ProtoJson::Item* GetNextItem();
                    
                private:
                    ItemList::Iterator  list_iterator;
                    bool                depth_first;
                    ItemList            pending_list; 
            };
            friend class Iterator;
        private:
            ItemList        item_list;
            unsigned int    item_count;
            
    };  // end class ProtoJson::Document
    
    class Parser
    {
        public:
            Parser();
            ~Parser();
            
            void Destroy();
            
            bool LoadDocument(const char *path);
            
            Document* AccessDocument()
                {return current_document;}
            
            Document* DetachDocument();
            
            enum Status
            {
                PARSE_ERROR,
                PARSE_MORE,
                PARSE_DONE
            };      
                
            Status ProcessInput(const char* inputBuffer, unsigned int inputLength);
             
        private:  
            // Delimiters for parsing
            static const char OBJECT_START;
            static const char OBJECT_END;
            static const char ARRAY_START;
            static const char ARRAY_END;
            static const char QUOTE;
            static const char COLON;
            static const char COMMA;
            static const char ESCAPE;
            static const char TRUE_START;
            static const char FALSE_START;
            static const char NULL_START;    
            
            // Methods used in internal parsing
            static Item::Type GetType(char c);  // infers item type from first non-whitespace char of Value
            bool AddValueToParent(Item* theParent, Item& value);
            bool AddToString(String& string, const char* text, unsigned int length);
            Status ProcessStringInput(const char* input, unsigned int length);
            bool AddToTemp(const char* text, unsigned int length);
            Status ProcessNumberInput(const char* input, unsigned int length);
            bool FixedItemIsValid(Item::Type type);
            Status ProcessFixedInput(const char* input, unsigned int length);
            Status ProcessArrayInput(const char* input, unsigned int length);
            Status ProcessEntryInput(const char* input, unsigned int length);
            Status ProcessObjectInput(const char* input, unsigned int length);
            Status ProcessValueInput(const char* input, unsigned int length);
            
            void PushStack(Item& item)
                {parser_stack.Prepend(item);}
            Item* PopStack()
                {return parser_stack.RemoveHead();}
            Item* PeekStack()
                {return parser_stack.GetHead();}
            
            Document*       current_document;
            ItemList        parser_stack;
            Item*           current_item;
            unsigned int    input_offset;
            bool            is_escaped;   // for caching escape parse state
            bool            seek_colon;   // for caching Object "key : value" parse state
            char*           temp_buffer;
            unsigned int    temp_buffer_max;
            unsigned int    temp_buffer_len;
            
    };  // end class ProtoJson::Parser
    
}  // end namespace ProtoJson

#endif // _PROTO_JSON
