/**
* @file protoList.cpp
* 
* @brief The ProtoList class provides a simple double linked-list
* class with a "ProtoList::Item" base class to use for
* deriving your own classes you wish to store in a 
* ProtoList
*/

#include "protoList.h"
#include "protoDebug.h"


ProtoIterable::ProtoIterable()
 : iterator_list_head(NULL)
{
}

ProtoIterable::~ProtoIterable()
{
    // Need to tell associated iterators that 
    // we don't exist anymore
    while (NULL != iterator_list_head)
        RemoveIterator(*iterator_list_head);
}

void ProtoIterable::AddIterator(Iterator& iterator)
{
    iterator.ilist_prev = NULL;
    iterator.ilist_next = iterator_list_head;
    if (NULL != iterator_list_head)
        iterator_list_head->ilist_prev = &iterator;
    iterator_list_head = &iterator;
}  // end ProtoIterable::AddIterator()

void ProtoIterable::RemoveIterator(Iterator& iterator)
{
    if (NULL == iterator.ilist_prev)
        iterator_list_head = iterator.ilist_next;
    else
        iterator.ilist_prev->ilist_next = iterator.ilist_next;
    if (NULL != iterator.ilist_next)
        iterator.ilist_next->ilist_prev = iterator.ilist_prev;
    iterator.ilist_prev = iterator.ilist_next = NULL;
    iterator.iterable = NULL;
}  // end ProtoIterable::RemoveIterator()

// This should be called before or after theAction is executed by the iterable
// depending on the data structure.  For example, APPEND/PREPEND list actions
// use the head/tail state to update list iterators properly so these need
// to call UpdateIterator _before_ apppend/prepend while other types may
// require post-action data structure state.
void ProtoIterable::UpdateIterators(Item* theItem, Iterator::Action theAction) const
{
    Iterator* nextIterator = iterator_list_head;
    while (NULL != nextIterator)
    {
        nextIterator->Update(theItem, theAction);
        nextIterator = nextIterator->ilist_next;
    }
}  // end ProtoIterable::UpdateIterators()

ProtoIterable::Iterator::Iterator(ProtoIterable& theIterable)
 : iterable(&theIterable), ilist_prev(NULL), ilist_next(NULL)
{
    theIterable.AddIterator(*this);
}

ProtoIterable::Iterator::~Iterator()
{
    if (NULL != iterable)
        iterable->RemoveIterator(*this);
}


ProtoList::Item::Item()
{
}

ProtoList::Item::~Item()
{
}

ProtoList::ProtoList()
 : head(NULL), tail(NULL)
{
}

ProtoList::~ProtoList()
{
}

void ProtoList::Prepend(Item& item)
{
    UpdateIterators(&item, Iterator::PREPEND);
    item.plist_prev = NULL;
    if (NULL != head)
        head->plist_prev = &item;
    else
        tail = &item;
    item.plist_next = head;
    head = &item;
}  // end ProtoList::Prepend()

void ProtoList::Append(Item& item)
{
    UpdateIterators(&item, Iterator::APPEND);
    item.plist_next = NULL;
    if (NULL != tail)
        tail->plist_next = &item;
    else
        head = &item;
    item.plist_prev = tail;
    tail = &item;
}  // end ProtoList::Append()

void ProtoList::Insert(Item& theItem, Item& nextItem)
{
    theItem.plist_next = &nextItem;
    theItem.plist_prev = nextItem.plist_prev;
    if (&nextItem == head) 
        head = &theItem;
    else
        nextItem.plist_prev->plist_next = &theItem;
    nextItem.plist_prev = &theItem;
    UpdateIterators(&theItem, Iterator::INSERT);
}  // end ProtoList::Insert()

void ProtoList::InsertAfter(Item& theItem, Item& prevItem)
{
    theItem.plist_prev = &prevItem;
    theItem.plist_next = prevItem.plist_next;
    if (&prevItem == tail)
        tail = &theItem;
    else
        prevItem.plist_next->plist_prev = &theItem;
    prevItem.plist_next = &theItem;
    UpdateIterators(&theItem, Iterator::INSERT);
}  // end ProtoList::ProtoList::InsertAfter()

void ProtoList::Remove(Item& item)
{
    UpdateIterators(&item, Iterator::REMOVE);
    if (NULL == item.plist_prev)
        head = item.plist_next;
    else
        item.plist_prev->plist_next = item.plist_next;
    
    if (NULL == item.plist_next)
        tail = item.plist_prev;
    else
        item.plist_next->plist_prev = item.plist_prev;
    item.plist_prev = item.plist_next = NULL;
}  // end ProtoList::Remove()

ProtoList::Item* ProtoList::RemoveHead()
{
    Item* item = head;
    if (NULL != item) Remove(*item);
    return item;
}  // end ProtoList::RemoveHead()

ProtoList::Item* ProtoList::RemoveTail()
{
    Item* item = tail;
    if (NULL != item) Remove(*item);
    return item;
}  // end ProtoList::RemoveTail()

void ProtoList::Empty()
{
    UpdateIterators(NULL, Iterator::EMPTY);
    head = tail = NULL;
}  // end ProtoList::Empty()

void ProtoList::EmptyToPool(ItemPool& pool)
{
    if (NULL != tail)
    {
        tail->plist_next = pool.head;
        pool.head = head;
        Empty();
    }
}  // end ProtoList::EmptyToPool()

void ProtoList::Destroy()
{
    Item* item = head;
    while (NULL != item)
    {
        Remove(*item);
        delete item;
        item = head;
    }
}  // end ProtoList::Destroy()



ProtoList::ItemPool::ItemPool()
 : head(NULL)
{
}

ProtoList::ItemPool::~ItemPool()
{
    Destroy();
}

ProtoList::Item* ProtoList::ItemPool::Get()
{   
    Item* item = head;
    if (NULL != item) head = item->plist_next;
    return item;
}  // end ProtoList::ItemPool::Get()

void ProtoList::ItemPool::Put(Item& item)
{
    item.plist_next = head;
    head = &item;
}  // end ProtoList::ItemPool::Put()

void ProtoList::ItemPool::Destroy()
{
    Item* item;
    while (NULL != (item = Get())) delete item;
}  // end ProtoList::ItemPool::Destroy()


ProtoList::Iterator::Iterator(ProtoList& theList, bool reverse)
 : ProtoIterable::Iterator(theList)
{
    Reset(reverse);
}

void ProtoList::Iterator::Reset(bool reverse)
{
    ProtoList* list = static_cast<ProtoList*>(iterable);
    reversed = reverse;
    if (NULL != list)
        item = reverse ? list->tail : list->head;
    else
        item = NULL;
}  // end ProtoList::Iterator::Reset()

ProtoList::Iterator::~Iterator()
{
}

ProtoList::Item* ProtoList::Iterator::GetNextItem()
{
    if (reversed)
    {
        ProtoList* list = static_cast<ProtoList*>(iterable);
        if (NULL != item)
            item = item->plist_next;
        else if (NULL != list)
            item = list->head;
        reversed = false;
    }
    if (NULL != item)  
    {
        Item* next = item;
        item = item->plist_next;
        return next;
    }
    else
    {
        return NULL;
    }
}  // end ProtoList::Iterator::GetNextItem()    

ProtoList::Item* ProtoList::Iterator::GetPrevItem()
{
    if (!reversed)
    {
        ProtoList* list = static_cast<ProtoList*>(iterable);
        if (NULL != item)
            item = item->plist_prev;
        else if (NULL != list)
            item = list->tail;
        reversed = true;
    }
    if (NULL != item)  
    {
        Item* prev = item;
        item = item->plist_prev;
        return prev;
    }
    else
    {
        return NULL;
    }
}  // end ProtoList::Iterator::GetPrevItem()  

ProtoList::Item* ProtoList::Iterator::PeekPrevItem() const
{
    ProtoList* list = static_cast<ProtoList*>(iterable);
    Item* prevItem = NULL;
    if (NULL != list)
    {
        if (reversed)
            prevItem = item;
        else
            prevItem = (NULL != item) ? item->plist_prev : list->tail; 
    }
    return prevItem;
}  // end ProtoList::Iterator::PeekPrevItem()  

void ProtoList::Iterator::Reverse()
{
    if (reversed)
    {
        if (NULL != item)
        {
            item = item->plist_next;
        }
        else
        {
            ProtoList* list = static_cast<ProtoList*>(iterable);
            if (NULL != list)
                item = list->GetHead();
            else
                item = NULL;
        }
        reversed = false;
    }
    else
    {
        if (NULL != item)
        {
            item = item->plist_prev;
        }
        else
        {
            ProtoList* list = static_cast<ProtoList*>(iterable);
            if (NULL != list)
                item = list->GetTail();
            else
                item = NULL;
        }
        reversed = true;
    }
}  // end ProtoList::Iterator::Reverse()

void ProtoList::Iterator::Update(ProtoIterable::Item* theItem, Action theAction)
{
    ProtoList::Item* listItem = static_cast<ProtoList::Item*>(theItem);
    switch (theAction)
    {
        case REMOVE:
            if (listItem == item)
            {
                if (reversed)
                    item = listItem->plist_prev;
                else
                    item = listItem->plist_next;
            }
            break;
        case PREPEND:
        {
            ProtoList* list = static_cast<ProtoList*>(iterable);
            if (reversed)
            {
                if (NULL == item)
                    item = listItem;
            }
            else if (list->head == item)
            {
                item = listItem;
            }
            break;
        }
        case APPEND:
        {
            ProtoList* list = static_cast<ProtoList*>(iterable);
            if (reversed)
            {
                if (list->tail == item)
                    item = listItem;
            }
            else if (NULL == item)
            {
                item = listItem;
            }
            break;
        }
        case INSERT:
            if (reversed)
            {
                if (listItem->plist_prev == item)
                    item = listItem;
            }
            else
            {
                if (listItem->plist_next == item)
                    item = listItem;
            }
            break;
        case EMPTY:
            item = NULL;
            break;
    }
}  // end ProtoList::Iterator::Update()


ProtoStack::ProtoStack()
 : head(NULL), tail(NULL)
{
}

ProtoStack::~ProtoStack()
{
}


void ProtoStack::Push(Item& item)
{
    item.pstack_next = head;
    head = &item;
    if (NULL == tail) tail = &item;
}  // end ProtoStack::Push(Item& item)


ProtoStack::Item* ProtoStack::Pop()
{
    Item* item = head;  
    if (NULL != item)
    {
        if (NULL != item->pstack_next)
            head = item->pstack_next;
        else
            head = tail = NULL;
    }   
    return item;
}  // end ProtoStack::Pop()

void ProtoStack::Put(Item& item)
{
    item.pstack_next = NULL;
    if (NULL != tail)
        tail->pstack_next = &item;
    else
        head = tail = &item;
}  // end ProtoStack::Put()

void ProtoStack::Destroy()
{
    Item* item;
    while (NULL != (item = Pop()))
        delete item;
}  // end ProtoStack::Destroy()

ProtoStack::Item::Item()
 : pstack_next(NULL)
{
}

ProtoStack::Item::~Item()
{
}

ProtoStack::Iterator::Iterator(const ProtoStack& theStack)
 : stack(theStack), next(theStack.GetHead())
{
}

ProtoStack::Iterator::~Iterator()
{
}

ProtoStack::Item* ProtoStack::Iterator::GetNextItem()
{
    Item* item = next;
    if (NULL != item)
        next = item->pstack_next;
    return item;
}  // end ProtoStack::Iterator::GetNextItem()
