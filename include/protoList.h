#ifndef _PROTO_LIST
#define _PROTO_LIST

/**
* @class ProtoList
*
* @brief The ProtoList class provides a simple double linked-list
* class with a "ProtoList::Item" base class to use for
* deriving your own classes you wish to store in a 
* ProtoList.  
* 
* Note the "ProtoQueue" classes provide some more sophisticated
* options like items that can be listed in multiple lists and
* automated removal from the multiple lists upon deletion, etc
*/

#include "protoDefs.h"

/**
* @class ProtoIterable
*
* @brief The ProtoIterable is a base class used for Protolib
* list, tree, queue, etc implementations so that iterators associated
* with these data structures are automatically updated upon
* Item addition/removal, list deletion, etc
*
*/

class ProtoIterable
{
    public:
        virtual ~ProtoIterable();
    protected:
        ProtoIterable();
    
        class Item {};
        
        class Iterator
        {
            public:
                virtual ~Iterator();
                enum Action {REMOVE, PREPEND, APPEND, INSERT, EMPTY};
                
                bool IsValid() const
                    {return (NULL != iterable);}
                
            protected:
                Iterator(ProtoIterable& theIterable);
            
                // Typical list, etc data structure operations
                virtual void Update(Item* theItem, Action theAction) = 0;
                
                friend class ProtoIterable;
                
                ProtoIterable*  iterable;
                
            private:
                Iterator*       ilist_prev;
                Iterator*       ilist_next;
        };  // end class ProtoIterable::Iterator
        
        // Derived classes should call this method upon list, etc modification
        // actions (i.e. REMOVE, PREPEND, APPEND, INSERT).  The associated
        // iterators' "Update()" methods will be invoked accordingly.
        void UpdateIterators(Item* theItem, Iterator::Action theAction) const; 
        
    private:
        friend class Iterator;
        void AddIterator(Iterator& iterator);
        void RemoveIterator(Iterator& iterator);
        
        Iterator*           iterator_list_head;        
            
};  // end class ProtoIterable

/**
* @class ProtoList
*
* @brief The ProtoList is a basic (doubly) linked list implementation.  It derives
* from the ProtoIterable class so that associated ProtoList::Iterators are tracked
* and updated upon Item addition/removal, list deletion, etc
*
*/

class ProtoList : private ProtoIterable
{
    public:
        ProtoList();
        ~ProtoList();
        
        class Item;
        void Prepend(Item& item);
        void Append(Item& item);
        
        // This inserts "theItem" _before_ the "nextItem"
        void Insert(Item& theItem, Item& nextItem);
        
        // This inserts "theItem" _before_ the "nextItem"
        void InsertAfter(Item& theItem, Item& prevItem);
        
        void Remove(Item& item);
        
        void Empty();   // empties list without deleting items
        
        void Destroy(); // deletes contents
        
        bool IsEmpty() const
            {return (NULL == head);}
        
        Item* GetHead() const
            {return head;}
        Item* GetTail() const
            {return tail;}   
        
        Item* RemoveHead();
        Item* RemoveTail();
    
        class Iterator;
        friend class Iterator;
        class ItemPool;
		/**
		* @class ProtoList::Item
		*
		* @brief Base class to use for deriving your own classes you wish to store in a 
        * ProtoList.  
		*/
        class Item : public ProtoIterable::Item
        {
            public:
                virtual ~Item();
            
                const Item* GetNext() const
                    {return plist_next;}
                const Item* GetPrev() const
                    {return plist_prev;}
                
            protected:
                Item();
                
            private:
                Item*   plist_prev;
                Item*   plist_next;
                
            friend class ProtoList;
            friend class Iterator;
            friend class ItemPool;
        };  // end class ProtoList::Item()
        /**
		* @class ProtoList::Iterator
		*
		* @brief ProtoList::Item iterator.
		*/
        class Iterator : private ProtoIterable::Iterator
        {
            friend class ProtoList;
            public:
                Iterator(ProtoList& theList, bool reverse = false);
                ~Iterator();
                
                bool IsValid() const
                    {return ProtoIterable::Iterator::IsValid();}
                
                void Reset(bool reverse = false);
                Item* GetNextItem();
                Item* PeekNextItem() const
                    {return item;}
                Item* GetPrevItem();
                Item* PeekPrevItem() const;
                
                bool SetCursor(Item* cursor)
                {   
                    item = cursor;
                    return true; // note list membership not validated
                }
                Item* GetCursor() const
                    {return item;}
                
                void Reverse();
                bool IsReversed() const
                    {return reversed;}
                
            private:
                // Required override for ProtoIterable to make sure any
                // iterators associated with a list are updated upon
                // Item addition or removal.
                void Update(ProtoIterable::Item* theItem, Action theAction);
            
                Item*       item;
                bool        reversed;
                
        };  // end class ProtoList::Iterator
        
        class ItemPool
        {
            friend class ProtoList;
            public:
                ItemPool();
                ~ItemPool();
                
                bool IsEmpty() const
                    {return (NULL == head);}
                        
                Item* Get();
                void Put(Item& item);
                
                void Destroy();
                
            private:
                Item* head;
        };  // end class ProtoList::ItemPool
        
        // Transfers list contents directly to pool
        // using existing linking
        void EmptyToPool(ItemPool& pool);
        
        // Generally, a ProtoList::Iterator should be
        // used instead of these
        Item* GetNextItem(Item& item)
            {return item.plist_next;}
        Item* GetPrevItem(Item& item)
            {return item.plist_prev;}
              
    private:
        Item*       head;
        Item*       tail;
       
};  // end class ProtoList

/**
* @class ProtoListTemplate
*
* @brief The ProtoListTemplate definition lets you create new list variants that
* are type checked at compile time, etc.  
*
* Note the "ITEM_TYPE" _must_
*  be a class that is derived from "ProtoList::Item"
*  A simple example is provided in the comments.
*/
template <class ITEM_TYPE>
class ProtoListTemplate : public ProtoList
{
    public:
        ProtoListTemplate() {}
        virtual ~ProtoListTemplate() {}     
        
        ITEM_TYPE* GetHead() const
            {return static_cast<ITEM_TYPE*>(ProtoList::GetHead());}
        ITEM_TYPE* GetTail() const
            {return static_cast<ITEM_TYPE*>(ProtoList::GetTail());}
        ITEM_TYPE* RemoveHead()
            {return static_cast<ITEM_TYPE*>(ProtoList::RemoveHead());}
        ITEM_TYPE* RemoveTail()
            {return static_cast<ITEM_TYPE*>(ProtoList::RemoveTail());}
        
        class Iterator : public ProtoList::Iterator
        {
            public:
                Iterator(ProtoListTemplate& theList, bool reverse = false)
                 : ProtoList::Iterator(theList, reverse) {}
                ~Iterator() {}
                
                void Reset(bool reverse = false)
                    {ProtoList::Iterator::Reset(reverse);}
                ITEM_TYPE* GetNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoList::Iterator::GetNextItem());}
                ITEM_TYPE* PeekNextItem() const
                    {return static_cast<ITEM_TYPE*>(ProtoList::Iterator::PeekNextItem());}
                ITEM_TYPE* GetPrevItem()
                    {return static_cast<ITEM_TYPE*>(ProtoList::Iterator::GetPrevItem());}
                ITEM_TYPE* PeekPrevItem() const
                    {return static_cast<ITEM_TYPE*>(ProtoList::Iterator::PeekPrevItem());}

        };  // end class ProtoListTemplate::Iterator
        
        class ItemPool : public ProtoList::ItemPool
        {
            public:
                ItemPool() {}
                ~ItemPool() {}
                
                void Put(ITEM_TYPE& item)
                    {ProtoList::ItemPool::Put(item);}

                ITEM_TYPE* Get()
                    {return static_cast<ITEM_TYPE*>(ProtoList::ItemPool::Get());}
        };  // end class ProtoListTemplate::ItemPool
        
};  // end ProtoListTemplate

/******
 * An example of how to use the ProtoListTemplate with 
 * your own type derived *from ProtoList::Item".
 *
 * Note that "ProtoList" itself may be used directly to do the same
 * thing, but using the template will save you some "static_casts", etc
 * for item retrieval, list iteration, etc and may have some other 
 * benefits.
 
 
class MyItem : public ProtoList::Item
{
    public:
        MyItem(int v) : value(v) {}
        int GetValue() const {return value;}
    private:
        int value;
};

class MyItemList : public ProtoListTemplate<MyItem> {};

MyItemList itemList;
MyItem item1(1), item2(2);

itemList.Append(item1);
itemList.Append(item2);

MyItem* headItem = itemList.GetHead();

...
        
************************************/
        
/**
* @class ProtoStack
* 
* @brief The ProtoStack class is like the ProtoList and similarly provides a
*  ProtoStackTemplate that can be used for deriving easier-to-
* use variants for custom items sub-classed from the ProtoStack::Item.
*
* The ProtoStack is a singly linked-list with Push() and Pop() methods
* but does keep track of both the list head and tail and so can be
* optionally "appended" and used as a FIFO.  Additionally, an iterator
* is provided to explore the list from head to tail.
*
* TBD - implement as derivation from ProtoIterable ...
*/
class ProtoStack
{
    public:
        ProtoStack();
        ~ProtoStack();
        
        class Item;
        
        // These methods manipulate the ProtoStack as a "stack"
        void Push(Item& item);  // prepend item to list head
        Item* Pop();            // removes/returns list head
        Item* Peek() const
            {return head;}
        
        // These methods manipulate the ProtoStack as a "FIFO"
        void Put(Item& item);  // append item to list tail
        Item* Get()            // remove/returns list head
            {return Pop();}
            
        bool IsEmpty() const
            {return (NULL == head);}
        Item* GetHead() const
            {return head;}
        Item* GetTail() const
            {return tail;}
        void Destroy();
        
        class Iterator;
        friend class Iterator;
		/** 
		* @class ProtoStack::Item
		*
		* @brief Base class to use for deriving your own classes you wish to store in a 
        * ProtoStack.
		*/
        class Item
        {
            public:
                virtual ~Item();
                
            protected:
                Item();
                
            private:
                Item*   pstack_next;
                
            friend class ProtoStack;
            friend class Iterator;
        };  // end class ProtoStack::Item()
        /**
		* @class ProtoStack::Iterator
		*
		* @brief ProtoStack::Item Iterator
		*/
        class Iterator
        {
            public:
                Iterator(const ProtoStack& theStack);
                ~Iterator();
                
                void Reset()
                    {next = stack.head;}
                
                Item* GetNextItem();
                Item* PeekNextItem() const
                    {return next;}
                
            private:
                const ProtoStack&   stack;
                Item*               next;
        };  // end class ProtoStack::Iterator
                         
    private:
        Item*   head;
        Item*   tail;
        
};  // end class ProtoStack

/**
* @class ProtoStackTemplate
*
* @brief The ProtoStackTemplate definition lets you create new list variants that
* are type checked at compile time, etc.  Note the "ITEM_TYPE" _must_
* be a class that is derived from "ProtoStack::Item"
*
* A simple example is provided in the comments.
*/
template <class ITEM_TYPE>
class ProtoStackTemplate : public ProtoStack
{
    public:
        ProtoStackTemplate() {}
        virtual ~ProtoStackTemplate() {}     
        
        void Push(ITEM_TYPE& item)
            {ProtoStack::Push(item);}
        ITEM_TYPE* Pop()
            {return static_cast<ITEM_TYPE*>(ProtoStack::Pop());}
        ITEM_TYPE* Peek()
            {return static_cast<ITEM_TYPE*>(ProtoStack::Peek());}
        ITEM_TYPE* Get()
            {return static_cast<ITEM_TYPE*>(ProtoStack::Get());}
        ITEM_TYPE* GetHead() const
            {return static_cast<ITEM_TYPE*>(ProtoStack::GetHead());}
        ITEM_TYPE* GetTail() const
            {return static_cast<ITEM_TYPE*>(ProtoStack::GetTail());}
        
        class Iterator : public ProtoStack::Iterator
        {
            public:
                Iterator(const ProtoStackTemplate& theList, bool reverse = false)
                 : ProtoStack::Iterator(theList) {}
                ~Iterator() {}
                
                void Reset()
                    {ProtoStack::Iterator::Reset();}
                ITEM_TYPE* GetNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoStack::Iterator::GetNextItem());}
                ITEM_TYPE* PeekNextItem() const
                    {return static_cast<ITEM_TYPE*>(ProtoStack::Iterator::PeekNextItem());}

        };  // end class ProtoStackTemplate::Iterator
        
};  // end ProtoStackTemplate

#endif  // _PROTO_LIST
