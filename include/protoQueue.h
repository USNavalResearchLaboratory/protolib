#ifndef _PROTO_QUEUE
#define _PROTO_QUEUE

/**
* @class ProtoQueue
*
* @brief The ProtoQueue base class allows creation of various data
* structures that have Items that may be listed (included)
* in multiple, different queues.  The queue management classes
* defined here allow derived ProtoQueue classes to use different
* data organization techniques and a couple of basic types are
* provided here that use the more basic Protolib data structure
* classes like "ProtoList", "ProtoTree",and "ProtoSortedTree"
*
* A ProtoQueue::Item may be a member of multiple, different queues
* (and even different types of queues since the ProtoQueue::Container
* determines how the item is sorted, etc, if applicable).  However, 
* note that only one instance of an individual ProtoQueue::Item may 
* may be included in a given list. (i.e. an item can't be in the same
* list twice or more)
*
*/

#include "protoDefs.h"
#include "protoTree.h"
#include "protoDebug.h"

class ProtoQueue
{
    public: 
        class Item;
        
        virtual ~ProtoQueue();

        virtual void Remove(Item& item) = 0;
        
        // returns true if item is in other queue besides this one
        bool IsInOtherQueue(Item& item)
            {return item.IsInOtherQueue(*this);}
        
        bool Contains(Item& item) const
            {return (NULL != item.GetContainer(*this));}
        
        // Derived classes MUST implement the
        // Empty() method _and_ call it in
        // their destructor!
        virtual void Empty() = 0;
        
    /** 
         * @class Container
         * 
         * @brief The "ProtoQueue::Container is a base class
         * that enables ProtoQueue:Items to keep track of the
         * ProtoQueues in which they are included.  The intention
         * here is that derived ProtoQueue subclasses will extend
         * the "Container" as needed to keep whatever state is 
         * needed for the given data structure type.
         */ 
         
         // TBD - can we make Container _privately_ derive from ProtoTree::Item
         //       to avoid the need for the "Entry" member (i.e. save some space)
         
        class ContainerPool;
        
        class Container
        {
            friend class ProtoQueue;
            friend class Item;
            friend class ContainerPool;
            
            public:
                virtual ~Container();
            
                Item* GetItem() const
                    {return item;}
                ProtoQueue* GetQueue() const
                    {return queue;}
                
            protected:
                Container();
            
                // Note: Any derived classes MUST call this Cleanup() method in
                //       their destructor to guarantee that no indirect calls
                //       to virtual functions are made in the ~Container() 
                //       destructor.
                void Cleanup();
            
            private:
                void Associate(Item& theItem, ProtoQueue& theQueue)  
                {
                    item = &theItem;
                    queue = &theQueue;
                }      
                void Disassociate()
                {
                    item = NULL;
                    queue = NULL;
                }
                void SetQueue(ProtoQueue& theQueue)
                    {queue = &theQueue;}
                
                /**
                 * @class Entry
                 *
                 * @brief used by ProtoQueue::Item for their  
                 * list of Containers (and hence ProtoQueue)
                 * (this list is keyed by the ProtoQueue*)
                 */
                class Entry : public ProtoTree::Item
                {
                    public:
                        Entry(Container& theContainer);
                        Container& GetContainer() const
                            {return container;}
                        
                        const char* GetKey() const
                            {return ((const char*)container.GetQueueHandle());}
                        unsigned int GetKeysize() const
                            {return (sizeof(Container*) << 3);}
                    private:
                        Container&  container;
                };  // end class ProtoQueue::Container::Entry
                Entry& AccessEntry()
                    {return entry;}
                
                const ProtoQueue*const* GetQueueHandle() const
                    {return &queue;}
                
                Item*       item;
                ProtoQueue* queue;
                Entry       entry;

        };  // end class ProtoQueue::Container
        
        class Item
        {
            friend class ProtoQueue;
            
            public:
                virtual ~Item();
            
                bool IsInQueue() const
                    {return (!container_list.IsEmpty());}
            
                Container* GetContainer(const ProtoQueue& queue) const
                {
                    const ProtoQueue* ptr = &queue;
                    ProtoQueue::Container::Entry* entry = 
                        static_cast<ProtoQueue::Container::Entry*>(container_list.Find((const char*)&ptr, sizeof(ProtoQueue*) << 3));
                    return ((NULL != entry) ? &entry->GetContainer() : NULL);
                } 
                
            protected:
                Item();
            
                // Note derived classes MUST call this Cleanup()
                // method in their destructor so the Item is safely 
                // removed from any queues in which it still resides.
                // It can't be safely done in the Item base class
                // destructor since the derived ProtoQueue type may
                // depend upon extensions to this Item base class for 
                // the specific data structure it uses.
                void Cleanup();
            
            private:
                bool IsInOtherQueue(const ProtoQueue& queue);
            
                void Reference(ProtoQueue::Container& container)
                    {container_list.Insert(container.AccessEntry());}
                void Dereference(ProtoQueue::Container& container)
                {
                    ASSERT(this == container.GetItem());
                    container_list.Remove(container.AccessEntry());
                }  

                // These members are for use by traversals and 
                // other queue manipulations as needed
                ProtoTree       container_list;
            
        };  // end class ProtoQueue::Item
        
    
        /**
         * @class ProtoQueue::ContainerPool
         *
         * @brief Container repository.  Note that a ContainerPool
         * should ony be used to cache a single (i.e. homogeneous) type
         * of Container (i.e. Container  subclass) otherwise one
         * may not "Get()" what one expects from the pool!
         */            
        class ContainerPool : public ProtoTree::ItemPool
        {
            public:
                ContainerPool();
                virtual ~ContainerPool();

                Container* Get()
                {
                    Container::Entry* entry = static_cast<Container::Entry*>(ProtoTree::ItemPool::Get());
                    return ((NULL != entry) ? &(entry->GetContainer()) : NULL);
                }

                void Put(Container& theContainer)
                    {ProtoTree::ItemPool::Put(theContainer.AccessEntry());}
                
                void Destroy();

        };  // end class ProtoQueue::ContainerPool
        
    protected:
            ProtoQueue(bool usePool = false);          // can optionally use the "builtin_container_pool"
            ProtoQueue(ContainerPool* containerPool);  // can optionally use an external container pool

            void TransferContainer(Container& container, ProtoQueue& dstQueue)
            {
                Item* item = container.GetItem();
                ASSERT(NULL != item);
                item->Dereference(container);
                container.SetQueue(dstQueue);
                item->Reference(container);
            }    

            Container* GetContainer(const Item& item) const
                {return item.GetContainer(*this);}
            
            Container* GetContainerFromPool()
                {return ((NULL != container_pool) ? container_pool->Get() : NULL);}
            
            void Associate(Item& item, Container& container)
            {
                container.Associate(item, *this);
                item.container_list.Insert(container.AccessEntry());
            }
            void Disassociate(Item& item, Container& container)
            {
                item.container_list.Remove(container.AccessEntry());
                container.Disassociate();
            }
            
            ContainerPool   builtin_container_pool;
            ContainerPool*  container_pool;
    
};  // end class ProtoQueue


/**
 * @class ProtoSimpleQueue
 *
 * @brief Functional and base class that provides a linked-list (based on ProtoList)
 * queue.  It can hold any kind of ProtoQueue::Item, but one can use
 * the "ProtoSimpleQueueTemplate" below to create variants with easier-to-use
 * iterators, etc for specific (derived) ProtoQueue::Item types
 */    
class ProtoSimpleQueue : public ProtoQueue
{
    public:
        class Container;
        class ContainerPool;
    
        ProtoSimpleQueue(bool usePool = false);
        ProtoSimpleQueue(ContainerPool* containerPool);
        virtual ~ProtoSimpleQueue();
        
        
        bool Prepend(Item& theItem);
        bool Append(Item& theItem);
        bool Insert(Item& theItem, Item& nextItem);  // insert "theItem" before "nextItem"
        bool InsertAfter(Item& theItem, Item& prevItem);  // insert "theItem" after "prevItem"
        void Remove(Item& theItem);
        
        Item* GetHead() const
        {
            Container* container = item_list.GetHead();
            return ((NULL != container) ? container->GetItem() : NULL);
        }
        Item* RemoveHead();
        Item* GetTail() const
        {
            Container* container = item_list.GetTail();
            return ((NULL != container) ? container->GetItem() : NULL);
        }
        Item* RemoveTail();
        
        Item* GetPrev(Item& item) const
        {
            const Container* container = static_cast<Container*>(GetContainer(item));
            if (NULL != container)
            {
                container = static_cast<const Container*>(container->GetPrev());
                return ((NULL != container) ? container->GetItem() : NULL);
            }
            else
            {
                return NULL;
            }
        }
        
        bool IsEmpty() const
            {return (NULL != GetHead()) ? false : true;}
        
        void Empty();  // empties queue, but doesn't delete items
        
        void Destroy();  // empties queue, deleting items
        
        // TBD - add set operations
        /*
        const bool Union(ProtoSimpleQueue &unionQueue, const ProtoSimpleQueue &bQueue);
        const bool Intersection(ProtoSimpleQueue &intersectionQueue, const ProtoSimpleQueue &bQueue);
        const bool RelativeComplement(ProtoSimpleQueue &rcQueue, const ProtoSimpleQueue &bQueue);//(a - intersection of ab)
        const bool SymmetricDifference(ProtoSimpleQueue &rcQueue, const ProtoSimpleQueue &bQueue);//(union of relative complements)
        
        bool Union(const ProtoSimpleQueue &bQueue);
        bool Intersection(const ProtoSimpleQueue &bQueue);
        bool RelativeComplement(const ProtoSimpleQueue &bQueue);//(a - intersection of ab)
        bool SymmetricDifference(const ProtoSimpleQueue &bQueue);//(union of relative complements)
        */

        class Iterator : private ProtoList::Iterator
        {
            public:
                Iterator(ProtoSimpleQueue& theQueue, bool reverse = false);
                virtual ~Iterator();
                
                void Reset(bool reverse = false)
                    {ProtoList::Iterator::Reset(reverse);}
                
                ProtoQueue::Item* GetNextItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoList::Iterator::GetNextItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                ProtoQueue::Item* PeekNextItem() const
                {
                    Container* nextContainer = static_cast<Container*>(ProtoList::Iterator::PeekNextItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                ProtoQueue::Item* GetPrevItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoList::Iterator::GetPrevItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                ProtoQueue::Item* PeekPrevItem() const
                {
                    Container* nextContainer = static_cast<Container*>(ProtoList::Iterator::PeekPrevItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
        };  // end class ProtoSimpleQueue::Iterator  
        
        class Container : public ProtoQueue::Container, public ProtoList::Item
        {
           public:
                Container();
                ~Container();     
        };  // end class ProtoSimpleQueue::Container
        
        class ContainerPool : public ProtoQueue::ContainerPool
        {
            public:
                void Put(Container& theContainer)
                    {ProtoQueue::ContainerPool::Put(theContainer);}
                Container* Get()
                    {return static_cast<Container*>(ProtoQueue::ContainerPool::Get());}
        };  // end class ProtoSimpleQueue::ContainerPool  
        
    private:
        // TBD - do we want to make CreateContainer() virtual so derived classes can do more?
        Container* CreateContainer() const
            {return new Container;}
        Container* GetContainerFromPool()
            {return static_cast<Container*>(ProtoQueue::GetContainerFromPool());}
        void RemoveContainer(Container* theContainer, Item& theItem);  // returns container to pool or deletes it
        class List : public ProtoListTemplate<Container> {};
        List            item_list;
        
};  // end class ProtoSimpleQueue

template <class ITEM_TYPE>
class ProtoSimpleQueueTemplate : public ProtoSimpleQueue
{
    public:
        ProtoSimpleQueueTemplate(bool usePool = false) 
            : ProtoSimpleQueue(usePool) {}
        ProtoSimpleQueueTemplate(ContainerPool* containerPool) 
            : ProtoSimpleQueue(containerPool) {}
        virtual ~ProtoSimpleQueueTemplate() {}
        
        ITEM_TYPE* GetHead() const
            {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::GetHead());}
        ITEM_TYPE* RemoveHead()
            {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::RemoveHead());}
        ITEM_TYPE* GetTail() const
            {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::GetTail());}
        ITEM_TYPE* RemoveTail()
            {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::RemoveTail());}
        ITEM_TYPE* GetPrev(ITEM_TYPE& item)
            {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::GetPrev(item));}
        
        class Iterator : public ProtoSimpleQueue::Iterator
        {
            public:
                Iterator(ProtoSimpleQueueTemplate& theQueue, bool reverse = false)
                 : ProtoSimpleQueue::Iterator(theQueue, reverse) {}
                ~Iterator() {}
                
                void Reset(bool reverse = false)
                    {ProtoSimpleQueue::Iterator::Reset(reverse);}
                
                ITEM_TYPE* GetNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::Iterator::GetNextItem());}
                ITEM_TYPE* PeekNextItem() const
                    {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::Iterator::PeekNextItem());}
                ITEM_TYPE* GetPrevItem()
                    {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::Iterator::GetPrevItem());}
                ITEM_TYPE* PeekPrevItem() const
                    {return static_cast<ITEM_TYPE*>(ProtoSimpleQueue::Iterator::PeekPrevItem());}
                
        };  // end class ProtoSimpleQueueTemplate::Iterator    
    
};  // end class ProtoSimpleQueueTemplate


/* ProtoSimpleQueueTemplate usage example
      
class MyItem : public ProtoQueue::Item
{
};  

class MyList : public ProtoSimpleQueue<MyItem> {};

MyList list;

list.Add(item)
list.Remove(item)
        
MyList::Iterator it;

MyItem* nextItem = it.GetNextItem() ...

*/      
               
class ProtoIndexedQueue : public ProtoQueue
{
    public:
        virtual ~ProtoIndexedQueue();
    
        // Insert the "item" into the tree (will fail if item with equivalent key already in tree)
        bool Insert(Item& item);
        
        // Remove the "item" from the tree
        void Remove(Item& item); 
        
        bool IsEmpty() const
            {return item_tree.IsEmpty();}
        
        // Find item with exact match to "key" and "keysize" (keysize is in bits)
        Item* Find(const char* key, unsigned int keysize) const
        {
            Container* container = item_tree.Find(key, keysize);
            return ((NULL != container) ? container->GetItem() : NULL);
        }
        
        Item* FindString(const char* keyString) const
            {return Find(keyString, (unsigned int)(8*strlen(keyString)));}
        
        Item* FindClosestMatch(const char* key, unsigned int keysize) const
        {
            Container* container = item_tree.FindClosestMatch(key, keysize);
            return ((NULL != container) ? container->GetItem() : NULL);
        }
        
        // Find item which is largest prefix of the "key" (keysize is in bits)
        Item* FindPrefix(const char* key, unsigned int keysize) const
        {
            Container* container = item_tree.FindPrefix(key, keysize);
            return ((NULL != container) ? container->GetItem() : NULL);
        }
        
        void Empty();  // empties queue, but doesn't delete items
        
        void Destroy();  // empties queue, deleting items
        
        // Required overrides for ProtoIndexedQueue subclasses
        // (Override these to determine how items are sorted)
        virtual const char* GetKey(const Item& item) const = 0;
        virtual unsigned int GetKeysize(const Item& item) const = 0;
        
        class Container : public ProtoQueue::Container, public ProtoTree::Item
        {
            public:
                Container();
                ~Container();
            
            private:
                // Required ProtoTree::Item overrides
                const char* GetKey() const;
                unsigned int GetKeysize() const;
                
        };  // end class ProtoIndexedQueue::Container  
        
        class ContainerPool : public ProtoQueue::ContainerPool
        {
            public:
                void Put(Container& theContainer)
                    {ProtoQueue::ContainerPool::Put(theContainer);}
                Container* Get()
                    {return static_cast<Container*>(ProtoQueue::ContainerPool::Get());}
        };  // end class ProtoIndexedQueue::ContainerPool  
            
        class Iterator : public ProtoTree::Iterator
        {
            public:
                Iterator(ProtoIndexedQueue& theQueue, bool reverse = false);
                virtual ~Iterator();
                
                void Reset(bool reverse = false,
                           const char*  prefix = NULL,
                           unsigned int prefixSize = 0)
                    {ProtoTree::Iterator::Reset(reverse, prefix, prefixSize);}
                
                Item* GetNextItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoTree::Iterator::GetNextItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                Item* PeekNextItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoTree::Iterator::PeekNextItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                Item* GetPrevItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoTree::Iterator::GetPrevItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                Item* PeekPrevItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoTree::Iterator::PeekPrevItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
        };  // end class ProtoIndexedQueue::Iterator  
           
    protected: 
        ProtoIndexedQueue(bool usePool = false);
        ProtoIndexedQueue(ContainerPool* containerPool);     
        Container* CreateContainer() const
            {return new Container;} 
        Container* GetContainerFromPool()
            {return static_cast<Container*>(ProtoQueue::GetContainerFromPool());}
            
    private:
        class Tree : public ProtoTreeTemplate<Container> {};
        Tree            item_tree;
        
};  // end class ProtoIndexedQueue 
        
template <class ITEM_TYPE>
class ProtoIndexedQueueTemplate : public ProtoIndexedQueue
{
    public:
        virtual ~ProtoIndexedQueueTemplate() {}
        
        // Required overrides to determine indexing
        virtual const char* GetKey(const Item& item) const = 0;
        virtual unsigned int GetKeysize(const Item& item) const = 0;
        
        // Insert the "item" into the tree (will fail if item with equivalent key already in tree)
        //bool Insert(ITEM_TYPE& item)
        //    {return ProtoIndexedQueue::Insert(item);}
        
        // Remove the "item" from the tree
	// LJT THIS HAD BEEN COMMENTED OUT
        void Remove(ITEM_TYPE& item)
            {return ProtoIndexedQueue::Remove(item);}
        
        // Find item with exact match to "key" and "keysize" (keysize is in bits)
        ITEM_TYPE* Find(const char* key, unsigned int keysize) const
            {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::Find(key, keysize));}
        
        ITEM_TYPE* FindString(const char* keyString) const
            {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::FindString(keyString));}
        
        ITEM_TYPE* FindClosestMatch(const char* key, unsigned int keysize) const
            {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::FindClosestMatch(key, keysize));}
        
        // Find item which is largest prefix of the "key" (keysize is in bits)
        ITEM_TYPE* FindPrefix(const char* key, unsigned int keysize) const
            {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::FindPrefix(key, keysize));}
        
        class Iterator : public ProtoIndexedQueue::Iterator
        {
            public:
                Iterator(ProtoIndexedQueueTemplate<ITEM_TYPE>& theQueue, bool reverse = false)
                 : ProtoIndexedQueue::Iterator(theQueue, reverse) {}
                ~Iterator() {}
                
                void Reset(bool         reverse = false,
                           const char*  prefix = NULL,
                           unsigned int prefixSize = 0)
                    {ProtoIndexedQueue::Iterator::Reset(reverse, prefix, prefixSize);}
                
                ITEM_TYPE* GetNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::Iterator::GetNextItem());}
                ITEM_TYPE* PeekNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::Iterator::PeekNextItem());}
                ITEM_TYPE* GetPrevItem()
                    {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::Iterator::GetPrevItem());}
                ITEM_TYPE* PeekPrevItem()
                    {return static_cast<ITEM_TYPE*>(ProtoIndexedQueue::Iterator::PeekPrevItem());}
        };  // end class ProtoIndexedQueueTemplate::Iterator 
        
    protected:
        ProtoIndexedQueueTemplate(bool usePool = false) 
            : ProtoIndexedQueue(usePool) {}
        ProtoIndexedQueueTemplate(ContainerPool* containerPool)
            : ProtoIndexedQueue(containerPool) {}
        
    private:
        using ProtoIndexedQueue::Remove;   // gets rid of hidden overloaded virtual function warning
           
};  // end class ProtoIndexedQueueTemplate  
    
/* 
// ProtoIndexedQueueTemplate usage example
      
class MyItem : public ProtoQueue::Item
{
    
};  

class MyQueue  public ProtoIndexedQueueTemplate<MyItem>
{
    // 1) SHOULD implement constructors to determine use of Container pooling
    MyQueue(bool usePool = false) : ProtoIndexedQueueTemplate<MyItem>(usePool) {}
    MyQueue(ContainerPool* externalPool) : ProtoIndexedQueueTemplate<MyItem>(externalPool) {}
    
    // 2) MUST implement these required overrides to determine indexing (i.e. sorting)
    //    (Note different ProtoIndexedQueue variants can implement these
    //     differently to have different indexing / sorting behaviors)
    const char* GetKey(const Item& item) const
        {return static_cast<const MyItem&>(item).GetKey();}
    unsigned int GetKeysize(const Item& item) const
        {return static_cast<const MyItem&>(item).GetKeysize();}
};

MyQueue queue;
MyItem item;
queue.Insert(item)
queue.Remove(item)
queue.FindItem() ...
        
MyQueue::Iterator it;

MyItem* nextItem = it.GetNextItem() ...

*/       
        
class ProtoSortedQueue : public ProtoQueue
{
    public:
        virtual ~ProtoSortedQueue();
    
        // Insert the "item" into the tree (multiple items with same key is OK)
        bool Insert(Item& item);
        
        // Remove the "item" from the tree
        void Remove(Item& item); 
        
        bool IsEmpty() const
            {return item_tree.IsEmpty();}
        
        // Find first item with exact match to "key" and "keysize" (keysize is in bits)
        Item* Find(const char* key, unsigned int keysize) const
        {
            Container* container = item_tree.Find(key, keysize);
            return ((NULL != container) ? container->GetItem() : NULL);
        }
        
        Item* GetHead() const
        {
            Container* head = item_tree.GetHead();
            return ((NULL != head) ? head->GetItem() : NULL);
        }
        
        Item* GetTail() const
        {
            Container* tail = item_tree.GetTail();
            return ((NULL != tail) ? tail->GetItem() : NULL);
        }
        
        void Empty();  // empties queue, but doesn't delete items
        
        void Destroy();  // empties queue, deleting items
        
        // Required overrides for ProtoSortedQueue subclasses
        // (Override these to determine how items are sorted)
        virtual const char* GetKey(const Item& item) const = 0;
        virtual unsigned int GetKeysize(const Item& item) const = 0;
        
        // Optional overrides for additional sorting control
        virtual ProtoTree::Endian GetEndian() const;
        virtual bool UseComplement2() const;
        virtual bool UseSignBit() const;
        
        class Container : public ProtoQueue::Container, public ProtoSortedTree::Item
        {
            public:
                Container();
                virtual ~Container();
            
            private:
                // Required ProtoTree::Item overrides
                // (Note these can only be called while the
                //  Container is "associated" with a ProtoSortedQueue)
                const char* GetKey() const;
                unsigned int GetKeysize() const;
                ProtoTree::Endian GetEndian() const;
                bool UseComplement2() const;
                bool UseSignBit() const;
                
        };  // end class ProtoSortedQueue::Container  
        
        class ContainerPool : public ProtoQueue::ContainerPool
        {
            public:
                void Put(Container& theContainer)
                    {ProtoQueue::ContainerPool::Put(theContainer);}
                Container* Get()
                    {return static_cast<Container*>(ProtoQueue::ContainerPool::Get());}
        };  // end class ProtoSortedQueue::ContainerPool  
        
        class Iterator : public ProtoSortedTree::Iterator
        {
            public:
                Iterator(ProtoSortedQueue&  theQueue, 
                         bool               reverse = false, 
                         const char*        keyMin = NULL, 
                         unsigned int       keysize = 0);
                virtual ~Iterator();
                
                void Reset(bool         reverse = false, 
                           const char*  keyMin = NULL, 
                           unsigned int keysize = 0)
                    {ProtoSortedTree::Iterator::Reset(reverse, keyMin, keysize);}
                
                void SetCursor(ProtoSortedQueue& theQueue, ProtoQueue::Item& theItem)
                {
                    Container* container = static_cast<ProtoSortedQueue::Container*>(theItem.GetContainer(theQueue));
                    ProtoSortedTree::Iterator::SetCursor(container);
                }
                
                Item* GetNextItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoSortedTree::Iterator::GetNextItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                Item* PeekNextItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoSortedTree::Iterator::PeekNextItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                Item* GetPrevItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoSortedTree::Iterator::GetPrevItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
                Item* PeekPrevItem()
                {
                    Container* nextContainer = static_cast<Container*>(ProtoSortedTree::Iterator::PeekPrevItem());
                    return ((NULL != nextContainer) ? nextContainer->GetItem() : NULL);
                }
        };  // end class ProtoSortedQueue::Iterator  
            
    protected:
        ProtoSortedQueue(bool usePool = false);
        ProtoSortedQueue(ContainerPool* containerPool);     
        Container* CreateContainer() const
            {return new Container;} 
        Container* GetContainerFromPool()
            {return static_cast<Container*>(ProtoQueue::GetContainerFromPool());}
            
    private:
        class Tree : public ProtoSortedTreeTemplate<Container> {};
        Tree            item_tree;
        
};  // end class ProtoSortedQueue

template <class ITEM_TYPE>
class ProtoSortedQueueTemplate : public ProtoSortedQueue
{
    public:
        virtual ~ProtoSortedQueueTemplate() {}
        
        // Required overrides to determine sorting index of enqueued items
        virtual const char* GetKey(const Item& item) const = 0;
        virtual unsigned int GetKeysize(const Item& item) const = 0;
        
        // Optional overrides to fine tune sorting further
        virtual ProtoTree::Endian GetEndian() const
            {return ProtoSortedQueue::GetEndian();}
        virtual bool UseComplement2() const
            {return ProtoSortedQueue::UseComplement2();}
        virtual bool UseSignBit() const
            {return ProtoSortedQueue::UseSignBit();}
        
        // Insert the "item" into the tree 
        bool Insert(ITEM_TYPE& item)
            {return ProtoSortedQueue::Insert(item);}
        
        // Remove the "item" from the tree
        void Remove(ITEM_TYPE& item)
            {ProtoSortedQueue::Remove(static_cast<Item&>(item));}
        
        // Find firat item with exact match to "key" and "keysize" (keysize is in bits)
        ITEM_TYPE* Find(const char* key, unsigned int keysize) const
            {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::Find(key, keysize));}
        
        ITEM_TYPE* GetHead() const
            {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::GetHead());}
        ITEM_TYPE* GetTail() const
            {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::GetTail());}
        
        class Iterator : public ProtoSortedQueue::Iterator
        {
            public:
                Iterator(ProtoSortedQueueTemplate& theQueue, bool reverse = false,
                         const char* keyMin = NULL, unsigned int keysize = 0)
                 : ProtoSortedQueue::Iterator(theQueue, reverse, keyMin, keysize) {}
                ~Iterator() {}
                
                void Reset(bool reverse = false)
                    {ProtoSortedQueue::Iterator::Reset(reverse);}
                
                ITEM_TYPE* GetNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::Iterator::GetNextItem());}
                ITEM_TYPE* PeekNextItem()
                    {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::Iterator::PeekNextItem());}
                ITEM_TYPE* GetPrevItem()
                    {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::Iterator::GetPrevItem());}
                ITEM_TYPE* PeekPrevItem()
                    {return static_cast<ITEM_TYPE*>(ProtoSortedQueue::Iterator::PeekPrevItem());}
        };  // end class ProtoSortedQueueTemplate::Iterator 
        
    protected:
        ProtoSortedQueueTemplate(bool usePool = false) 
            : ProtoSortedQueue(usePool) {}
        ProtoSortedQueueTemplate(ContainerPool* containerPool)
            : ProtoSortedQueue(containerPool) {}
    
    private:
        using ProtoSortedQueue::Remove;   // gets rid of hidden overloaded virtual function warning   
        
};  // end class ProtoSortedQueueTemplate          
#endif // _PROTO_QUEUE
