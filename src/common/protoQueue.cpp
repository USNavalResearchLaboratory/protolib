
#include "protoQueue.h"

ProtoQueue::ProtoQueue(bool usePool)
 : container_pool(usePool ? &builtin_container_pool : NULL)
{
}

ProtoQueue::ProtoQueue(ContainerPool* containerPool)
 : container_pool(containerPool)
{
}

ProtoQueue::~ProtoQueue()
{
   // Note the original plan was to call the Empty()
    // method here so that when a ProtoQueue is deleted
    // it removes any queued Containers so they aren't left
    // with a bad pointer.  But, you can't call virtual methods
    // from a destructor in C++. So, the derived classes 
    // MUST call Empty() in their destructors!
    builtin_container_pool.Destroy();
}


ProtoQueue::Container::Container()
 : item(NULL), queue(NULL), entry(*this)
{
}

ProtoQueue::Container::~Container()
{
    // Derived Container class destructors MUST
    // also call "Cleanup()" so there is no danger
    // of an indirect call to a virtual function
    // (See the longer note in ProtoQueue::Item::~Item()
    //  below)
    Cleanup();
}

void ProtoQueue::Container::Cleanup()
{
    if (NULL != item) 
    {
        ASSERT(NULL != queue);
        queue->Remove(*item);   
    }
}

ProtoQueue::Container::Entry::Entry(Container& theContainer)
 : container(theContainer)
{
}

ProtoQueue::Item::Item()
{
}

ProtoQueue::Item::~Item()
{
    // If the call to Cleanup() causes some sort of core dump,
    // it is likely the case that the derived ProtoQueue depends
    // upon some extension of the base Item class for its data
    // organization.  So, derived Item types SHOULD/MUST call
    // Cleanup() themselves so the Cleanup() call here doesn't do 
    // anything harmful.  (the derived class' destructor() call to Cleanup()
    // will be called first and be safer.  In fact, perhaps it would be better to
    // not call Cleanup() here at all and require programmers that create
    // derived Item subclasses to call Cleanup().  The way to help force
    // this may be to make the Cleanup() a pure virtual function in
    // the base class so that subclasses must implement it?  There is still
    // some leftover safety issue here where further derivations are performed ...
    // (This is the same issue as the note for the Empty() method above.)
    // Perhaps there is a better design that avoids this C++ issue!
    Cleanup();
}

void ProtoQueue::Item::Cleanup()
{
    while (!container_list.IsEmpty())
    {
        Container::Entry*entry = static_cast<Container::Entry*>(container_list.GetRoot());
        ProtoQueue* theQueue = entry->GetContainer().GetQueue();
        ASSERT(NULL != theQueue);
        theQueue->Remove(*this);
    }
}  // end ProtoQueue::Item::Cleanup()

bool ProtoQueue::Item::IsInOtherQueue(const ProtoQueue& queue)
{
    ProtoTree::Iterator iterator(container_list);
    const ProtoQueue::Container::Entry* entry;
    while (NULL != (entry = static_cast<const Container::Entry*>(iterator.GetNextItem())))
    {
        if (&queue != entry->GetContainer().GetQueue())
            return true;
    }
    return false;
}  // end ProtoQueue::IsInOtherQueue()

ProtoQueue::ContainerPool::ContainerPool()
{
}

ProtoQueue::ContainerPool::~ContainerPool()
{
    Destroy();
}

void ProtoQueue::ContainerPool::Destroy()
{
    Container* container;
    while (NULL != (container = Get()))
        delete container;
}  // end ProtoQueue::ContainerPool::Destroy()

ProtoSimpleQueue::ProtoSimpleQueue(bool usePool)
 : ProtoQueue(usePool)
{
}

ProtoSimpleQueue::ProtoSimpleQueue(ContainerPool* containerPool)
 : ProtoQueue(containerPool)
{
}

ProtoSimpleQueue::~ProtoSimpleQueue()
{
    Empty();
}

bool ProtoSimpleQueue::Append(Item& theItem)
{
    Container* theContainer = GetContainerFromPool();
    if (NULL == theContainer) theContainer = CreateContainer();
    if (NULL == theContainer) return false;
    Associate(theItem, *theContainer);
    item_list.Append(*theContainer);
    return true;
}  // end ProtoSimpleQueue::Append()

bool ProtoSimpleQueue::Prepend(ProtoQueue::Item& theItem)
{
    Container* theContainer = GetContainerFromPool();
    if (NULL == theContainer) theContainer = CreateContainer();
    if (NULL == theContainer) return false;
    Associate(theItem, *theContainer);
    item_list.Prepend(*theContainer);
    return true;
}  // end ProtoSimpleQueue::Prepend()

bool ProtoSimpleQueue::Insert(ProtoQueue::Item& theItem, ProtoQueue::Item& nextItem)
{
    Container* theContainer = GetContainerFromPool();
    if (NULL == theContainer) theContainer = CreateContainer();
    if (NULL == theContainer) return false;
    Associate(theItem, *theContainer);
    Container* nextContainer = static_cast<Container*>(nextItem.GetContainer(*this));
    if (NULL == nextContainer)
    {
        PLOG(PL_ERROR, "ProtoSimpleQueue::Insert() error: nextItem not in this queue?!\n");
        Disassociate(theItem, *theContainer);
        if (NULL != container_pool)
            container_pool->Put(*theContainer);
        else
            delete theContainer;
        return false;
    }    
    item_list.Insert(*theContainer, *nextContainer);
    return true;
}  // end ProtoSimpleQueue::Insert()

bool ProtoSimpleQueue::InsertAfter(ProtoQueue::Item& theItem, ProtoQueue::Item& nextItem)
{
    Container* theContainer = GetContainerFromPool();
    if (NULL == theContainer) theContainer = CreateContainer();
    if (NULL == theContainer) return false;
    Associate(theItem, *theContainer);
    Container* nextContainer = static_cast<Container*>(nextItem.GetContainer(*this));
    if (NULL == nextContainer)
    {
        PLOG(PL_ERROR, "ProtoSimpleQueue::Insert() error: nextItem not in this queue?!\n");
        Disassociate(theItem, *theContainer);
        if (NULL != container_pool)
            container_pool->Put(*theContainer);
        else
            delete theContainer;
        return false;
    }    
    item_list.InsertAfter(*theContainer, *nextContainer);
    return true;
}  // end ProtoSimpleQueue::Insert()

void ProtoSimpleQueue::Remove(Item& theItem)
{
    Container* theContainer = static_cast<Container*>(ProtoQueue::GetContainer(theItem));
    if (NULL != theContainer) RemoveContainer(theContainer, theItem);
}  // end ProtoSimpleQueue::Remove()

ProtoSimpleQueue::Item* ProtoSimpleQueue::RemoveHead()
{
    Container* container = item_list.GetHead();
    if (NULL != container) 
    {
        Item* item = container->GetItem();
        ASSERT(NULL != item);
        RemoveContainer(container, *item);
        return item;
    }
    else
    {
        return NULL;
    }
}  // end ProtoSimpleQueue::RemoveHead()

ProtoSimpleQueue::Item* ProtoSimpleQueue::RemoveTail()
{
    Container* container = item_list.GetTail();
    if (NULL != container) 
    {
        Item* item = container->GetItem();
        ASSERT(NULL != item);
        RemoveContainer(container, *item);
        return item;
    }
    else
    {
        return NULL;
    }
}  // end ProtoSimpleQueue::RemoveTail()

void ProtoSimpleQueue::RemoveContainer(Container* theContainer, Item& theItem)
{
    ASSERT(NULL != theContainer);
    item_list.Remove(*theContainer);
    Disassociate(theItem, *theContainer);
    if (NULL != container_pool)
        container_pool->Put(*theContainer);
    else
        delete theContainer;
}  // end ProtoSimpleQueue::RemoveContainer()

void ProtoSimpleQueue::Empty()
{
    Container* nextContainer;
    while (NULL != (nextContainer = item_list.RemoveHead()))
    {
        ProtoQueue::Item* nextItem = nextContainer->GetItem();
        ASSERT(NULL != nextItem);
        Disassociate(*nextItem, *nextContainer);
        if (NULL != container_pool)
            container_pool->Put(*nextContainer);
        else
            delete nextContainer;
    }
}  // end ProtoSimpleQueue::Empty()

void ProtoSimpleQueue::Destroy()
{
    Container* nextContainer;
    while (NULL != (nextContainer = item_list.RemoveHead()))
    {
        ProtoQueue::Item* nextItem = nextContainer->GetItem();
        ASSERT(NULL != nextItem);
        Disassociate(*nextItem, *nextContainer);
        delete nextItem;
        if (NULL != container_pool)
            container_pool->Put(*nextContainer);
        else
            delete nextContainer;
    }
}  // end ProtoSimpleQueue::Destroy()

ProtoSimpleQueue::Iterator::Iterator(ProtoSimpleQueue& theQueue, bool reverse)
 : ProtoList::Iterator(theQueue.item_list, reverse)
{
}

ProtoSimpleQueue::Iterator::~Iterator()
{
}

ProtoSimpleQueue::Container::Container()
{
}

ProtoSimpleQueue::Container::~Container()
{
    Cleanup();
}

ProtoIndexedQueue::ProtoIndexedQueue(bool usePool)
 : ProtoQueue(usePool)
{
}

ProtoIndexedQueue::ProtoIndexedQueue(ContainerPool* containerPool)
 : ProtoQueue(containerPool)
{
}

ProtoIndexedQueue::~ProtoIndexedQueue()
{
    Empty();
}

bool ProtoIndexedQueue::Insert(Item& theItem)
{
    Container* theContainer = GetContainerFromPool();
    if (NULL == theContainer) theContainer = CreateContainer();
    if (NULL == theContainer) return false;
    Associate(theItem, *theContainer);
    //return item_tree.Insert(*theContainer);
    //bunny Brian should we always return true here seems wrong...I'll let you do the update above or not.
    item_tree.Insert(*theContainer);
    return true;
}  // end ProtoIndexedQueue::Insert()

void ProtoIndexedQueue::Remove(Item& theItem)
{
    Container* theContainer = static_cast<Container*>(ProtoQueue::GetContainer(theItem));
    if (NULL != theContainer)
    {
        item_tree.Remove(*theContainer);
        Disassociate(theItem, *theContainer);
        if (NULL != container_pool)
            container_pool->Put(*theContainer);
        else
            delete theContainer;
    }
}  // end ProtoIndexedQueue::Remove()

void ProtoIndexedQueue::Empty()
{
    // We use the "SimpleIterator here because it is safe
    // to call from the destructor
    Container* nextContainer;
    Tree::SimpleIterator sit(item_tree);
    // We have to defer container deletion until after item_tree traversal
    // so we use this "tmpPool" (this is so the item virtuals aren't accessed)
    ContainerPool tmpPool; 
    while (NULL != (nextContainer = sit.GetNextItem()))
    {
        ProtoQueue::Item* nextItem = nextContainer->GetItem();
        ASSERT(NULL != nextItem);
        Disassociate(*nextItem, *nextContainer);
        if (NULL != container_pool)
            container_pool->Put(*nextContainer);
        else
            tmpPool.Put(*nextContainer);
    }
    tmpPool.Destroy();
    item_tree.Empty();  // sets the item_tree.root to NULL
}  // end ProtoIndexedQueue::Empty()

// "Destroy" is same as "Empty", except items are deleted
void ProtoIndexedQueue::Destroy()
{
    // We use the "SimpleIterator here because it is safe
    // to call from the destructor
    Container* nextContainer;
    Tree::SimpleIterator sit(item_tree);
    // We have to defer container deletion until after item_tree traversal
    // so we use this "tmpPool" (this is so the item virtuals aren't accessed)
    // The "trick" here is we stick the "container" directly into a pool
    // without a "Remove" from the tree (where virtuals would be invoked).
    // This is safe with the "SimpleIterator" we're using.
    ContainerPool tmpPool; 
    while (NULL != (nextContainer = sit.GetNextItem()))
    {
        ProtoQueue::Item* nextItem = nextContainer->GetItem();
        ASSERT(NULL != nextItem);
        Disassociate(*nextItem, *nextContainer);
        delete nextItem;
        if (NULL != container_pool)
            container_pool->Put(*nextContainer);
        else
            tmpPool.Put(*nextContainer);
    }
    tmpPool.Destroy();
    item_tree.Empty();  // sets the item_tree.root to NULL
}  // end ProtoIndexedQueue::Destroy()

        
ProtoIndexedQueue::Iterator::Iterator(ProtoIndexedQueue& theQueue, bool reverse)
 : ProtoTree::Iterator(theQueue.item_tree, reverse)
{
}

ProtoIndexedQueue::Iterator::~Iterator()
{
}

ProtoIndexedQueue::Container::Container()
{
}

ProtoIndexedQueue::Container::~Container()
{
    Cleanup();
}

const char* ProtoIndexedQueue::Container::GetKey() const
{
    ProtoQueue::Item* item = GetItem();
    ASSERT(NULL != item);
    ProtoIndexedQueue* iq = static_cast<ProtoIndexedQueue*>(GetQueue());
    return (iq->GetKey(*item));
}  // end ProtoIndexedQueue::Container::GetKey()

unsigned int ProtoIndexedQueue::Container::GetKeysize() const
{
    ProtoQueue::Item* item = GetItem();
    ASSERT(NULL != item);
    ProtoIndexedQueue* iq = static_cast<ProtoIndexedQueue*>(GetQueue());
    return (iq->GetKeysize(*item));
}  // end ProtoIndexedQueue::Container::GetKeysize()
                



ProtoSortedQueue::ProtoSortedQueue(bool usePool)
 : ProtoQueue(usePool)
{
}

ProtoSortedQueue::ProtoSortedQueue(ContainerPool* containerPool)
 : ProtoQueue(containerPool)
{
}

ProtoSortedQueue::~ProtoSortedQueue()
{
    Empty();
}

bool ProtoSortedQueue::Insert(Item& theItem)
{
    Container* theContainer = GetContainerFromPool();
    if (NULL == theContainer) theContainer = CreateContainer();
    if (NULL == theContainer) return false;
    Associate(theItem, *theContainer);
    item_tree.Insert(*theContainer);
    return true;
}  // end ProtoSortedQueue::Insert()

void ProtoSortedQueue::Remove(Item& theItem)
{
    Container* theContainer = static_cast<Container*>(ProtoQueue::GetContainer(theItem));
    if (NULL != theContainer)
    {
        item_tree.Remove(*theContainer);
        Disassociate(theItem, *theContainer);
        if (NULL != container_pool)
            container_pool->Put(*theContainer);
        else
            delete theContainer;
    }
}  // end ProtoSortedQueue::Remove()

void ProtoSortedQueue::Empty()
{
    Container* nextContainer;
    ProtoSortedTree::Iterator iterator(item_tree);
    while (NULL != (nextContainer = static_cast<Container*>(iterator.GetNextItem())))
    {
        ProtoQueue::Item* nextItem = nextContainer->GetItem();
        ASSERT(NULL != nextItem);
        Disassociate(*nextItem, *nextContainer);
        if (NULL != container_pool)
            container_pool->Put(*nextContainer);
        else
            delete nextContainer;
    }
    item_tree.Empty();
}  // end ProtoSortedQueue::Empty()

void ProtoSortedQueue::Destroy()
{
    Container* nextContainer;
    ProtoSortedTree::Iterator iterator(item_tree);
    while (NULL != (nextContainer = static_cast<Container*>(iterator.GetNextItem())))
    {
        ProtoQueue::Item* nextItem = nextContainer->GetItem();
        ASSERT(NULL != nextItem);
        Disassociate(*nextItem, *nextContainer);
        delete nextItem;
        if (NULL != container_pool)
            container_pool->Put(*nextContainer);
        else
            delete nextContainer;
    }
    item_tree.Empty();
}  // end ProtoSortedQueue::Destroy()

ProtoTree::Endian ProtoSortedQueue::GetEndian() const
{
    return ProtoTree::ENDIAN_BIG;  // BIG ENDIAN by default
}

bool ProtoSortedQueue::UseComplement2() const
{
    return true;
}  // end ProtoSortedQueue::UseComplement2()

bool ProtoSortedQueue::UseSignBit() const
{
    return false;
}  // end ProtoSortedQueue::UseSignBit()

ProtoSortedQueue::Iterator::Iterator(ProtoSortedQueue&    theQueue, 
                                     bool                       reverse,
                                     const char*                keyMin,
                                     unsigned int               keysize)
 : ProtoSortedTree::Iterator(theQueue.item_tree, reverse, keyMin, keysize)
{
}

ProtoSortedQueue::Iterator::~Iterator()
{
}

ProtoSortedQueue::Container::Container()
{
}

ProtoSortedQueue::Container::~Container()
{
    Cleanup();
}

const char* ProtoSortedQueue::Container::GetKey() const
{
    ProtoQueue::Item* item = GetItem();
    ASSERT(NULL != item);
    ProtoSortedQueue* sq = static_cast<ProtoSortedQueue*>(GetQueue());
    return (sq->GetKey(*item));
}  // end ProtoSortedQueue::Container::GetKey()

unsigned int ProtoSortedQueue::Container::GetKeysize() const
{
    ProtoQueue::Item* item = GetItem();
    ASSERT(NULL != item);
    ProtoSortedQueue* sq = static_cast<ProtoSortedQueue*>(GetQueue());
    return (sq->GetKeysize(*item));
}  // end ProtoSortedQueue::Container::GetKeysize()
                                

ProtoTree::Endian ProtoSortedQueue::Container::GetEndian() const
{
    ProtoSortedQueue* sq = static_cast<ProtoSortedQueue*>(GetQueue());
    ASSERT(NULL != sq);
    return (sq->GetEndian());
}  // end ProtoSortedQueue::Container::GetEndian()

bool ProtoSortedQueue::Container::UseComplement2() const
{
    ProtoSortedQueue* sq = static_cast<ProtoSortedQueue*>(GetQueue());
    ASSERT(NULL != sq);
    return (sq->UseComplement2());
}  // end ProtoSortedQueue::Container::UseComplement2()

bool ProtoSortedQueue::Container::UseSignBit() const
{
    ProtoSortedQueue* sq = static_cast<ProtoSortedQueue*>(GetQueue());
    ASSERT(NULL != sq);
    return (sq->UseSignBit());
}  // end ProtoSortedQueue::Container::UseSignBit()

