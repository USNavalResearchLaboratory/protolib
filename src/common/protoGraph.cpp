/**
* @file protoGraph.cpp
* 
* @brief
*/
#include "protoGraph.h"
#include "protoDebug.h"
#include "manetGraph.h" // for debug

ProtoGraph::VerticeQueue::VerticeQueue()
{
}

ProtoGraph::VerticeQueue::~VerticeQueue()
{    
    // Note the original plan was to call the Empty()
    // method here so that when a VerticeQueue is deleted
    // it removes any queued Vertices so they aren't left
    // with a bad pointer.  But, you can't call virtual methods
    // from a destructor in C++. So, the derived classes 
    // MUST call Empty() in their destructors!
}

/**
 * This should be called whenever a "VerticeQueue" enqueues (inserts) a "Vertice"
 */
void ProtoGraph::VerticeQueue::Associate(Vertice& vertice, QueueState& queueState)
{
    queueState.Associate(vertice, *this);
    vertice.Reference(queueState); 
}  // end ProtoGraph::VerticeQueue::Associate()

/**
 * This should be called whenever a "VerticeQueue" dequeues (removes) a "Vertice"
 */
void ProtoGraph::VerticeQueue::Disassociate(Vertice& vertice, QueueState& queueState)
{
    vertice.Dereference(queueState);
    queueState.Disassociate();
}  // end ProtoGraph::VerticeQueue::Disassociate()
        
ProtoGraph::Vertice::Vertice()
 : adjacency_queue(*this)
{
}

/**
 * Make sure no queues still refer to us by 
 * removing ourself from any queues that do)
 *
 * @note If were in an "AdjacencyQueue" the call to 
 * VerticeQueue::Remove() will _delete_ the associated 
 * "Edge" since we don't provide a pointer to
 * "EdgeFactory" here.  ("Edges" that aren't dynamically 
 *  allocated would cause a problem here!)
 */
ProtoGraph::Vertice::~Vertice()
{
    // If the call to Cleanup() causes some sort of core dump,
    // it is likely the case that the derived VerticeQueue depends
    // upon some extension of the base Vertice class for its data
    // organization.  So, derived Vertice types SHOULD/MUST call
    // Cleanup() themselves so the Cleanup() call doesn't do 
    // anything harmful.  In fact, perhaps it would be better to
    // not call Cleanup() here and require programmers that create
    // derived Vertice subclasses to call Cleanup().  The way to force
    // this may be to make the Cleanup() a pure virtual function in
    // the base class so that subclasses must implement it?  There is still
    // some leftover safety issue here where further derivations are performed ...
    // Anyway the result here is that a virtual function may end up indirectly 
    // called from this destructor which can have bad behavior
    Cleanup();
} 

void ProtoGraph::Vertice::Cleanup()
{
    while (!queue_state_tree.IsEmpty())
    {
        VerticeQueue::QueueState::Entry*entry = static_cast<VerticeQueue::QueueState::Entry*>(queue_state_tree.GetRoot());
        VerticeQueue* q = entry->GetQueueState().GetQueue();
        ASSERT(NULL != q);
        q->Remove(*this);
    }
}  // end ProtoGraph::Vertice::Cleanup()

/**
 * Default NULL key / keysize implementation for ProtoGraph::Vertice
 */
const char* ProtoGraph::Vertice::GetVerticeKey() const
{
    return NULL;
}  // end ProtoGraph::Vertice::GetVerticeKey()

unsigned int ProtoGraph::Vertice::GetVerticeKeysize() const
{
    return 0;
}  // end ProtoGraph::Vertice::GetVerticeKeysize()

ProtoTree::Endian ProtoGraph::Vertice::GetVerticeKeyEndian() const
{
    return ProtoTree::ENDIAN_BIG;

}  // end ProtoGraph::Vertice::GetVerticeKeyEndian()

bool ProtoGraph::Vertice::GetVerticeKeySigned() const
{
    return false;
}  // end ProtoGraph::Vertice::GetVerticeKeySigned()

bool ProtoGraph::Vertice::GetVerticeKeyComplement2() const
{
    return true;
}  // end ProtoGraph::Vertice::GetVerticeKeyComplement2()

ProtoGraph::VerticeQueue::QueueState::QueueState()
 : vertice(NULL), queue(NULL), entry(*this)
{
}

ProtoGraph::VerticeQueue::QueueState::~QueueState()
{
    Cleanup();
}

void ProtoGraph::VerticeQueue::QueueState::Cleanup()
{
    if (NULL != vertice) 
    {
        ASSERT(NULL != queue);
        queue->Remove(*vertice);   
    }
}  // end ProtoGraph::VerticeQueue::QueueState::Cleanup()

ProtoGraph::VerticeQueue::QueueStatePool::QueueStatePool()
{
}

ProtoGraph::VerticeQueue::QueueStatePool::~QueueStatePool()
{
    Destroy();
}

void ProtoGraph::VerticeQueue::QueueStatePool::Destroy()
{
    while (!IsEmpty())
    {
        QueueState* theState = Get();
        delete theState;
    }
}  // end ProtoGraph::VerticeQueue::QueueStatePool::Destroy()

ProtoGraph::VerticeQueue::QueueState::Entry::Entry(QueueState& theState)
 : queue_state(theState)
{
}


ProtoGraph::Vertice::SimpleList::Item::Item()
 : prev(NULL), next(NULL)
{
}

ProtoGraph::Vertice::SimpleList::Item::~Item()
{
    // Removes from "queue" if it is non-NULL
    Cleanup();
}

ProtoGraph::Vertice::SimpleList::SimpleList(ItemPool* itemPool)
 : head(NULL), tail(NULL), item_pool(itemPool)
{
}

ProtoGraph::Vertice::SimpleList::~SimpleList()
{
    Empty();
}

bool ProtoGraph::Vertice::SimpleList::Prepend(Vertice& vertice)
{
    Item* item = GetNewItem();
    if (NULL == item)
    {
        PLOG(PL_ERROR, "ProtoGraph::Vertice::SimpleList::Prepend() NewItem() error: %s\n", GetErrorString());
        return false;
    }
    Associate(vertice, *item);
    PrependItem(*item);
    return true;
}  // end ProtoGraph::Vertice::SimpleList::Prepend()

bool ProtoGraph::Vertice::SimpleList::Append(Vertice& vertice)
{
    Item* item = GetNewItem();
    if (NULL == item)
    {
        PLOG(PL_ERROR, "ProtoGraph::Vertice::SimpleList::Append() NewItem() error: %s\n", GetErrorString());
        return false;
    }
    Associate(vertice, *item);
    AppendItem(*item);
    return true;
}  // end ProtoGraph::Vertice::SimpleList::Append()

void ProtoGraph::Vertice::SimpleList::Remove(Vertice& vertice)
{
    // 1) Retrieve "Item*" from "vertice" queue state
    Item* item = static_cast<Item*>(vertice.GetQueueState(*this));
    // 2) Remove "item" from our linked list 
    ASSERT(NULL != item);
    RemoveItem(*item);
    Disassociate(vertice, *item);
    // 3) Return "item" to pool
    if (NULL != item_pool)
        item_pool->Put(*item);
    else
        delete item;
}  // end ProtoGraph::Vertice::SimpleList::Remove()

ProtoGraph::Vertice* ProtoGraph::Vertice::SimpleList::RemoveHead()
{
    if (NULL != head)
    {
        Item* item = head;
        Vertice* vertice = item->GetVertice();
        ASSERT(NULL != vertice);
        RemoveItem(*item);
        Disassociate(*vertice, *item);
        if (NULL != item_pool)
            item_pool->Put(*item);
        else
            delete item;
        return vertice; 
    }
    else
    {
        return NULL;
    }  
}  // end ProtoGraph::Vertice::SimpleList::RemoveHead()

void ProtoGraph::Vertice::SimpleList::Empty()
{
    while (!IsEmpty()) RemoveHead();
}  // end ProtoGraph::Vertice::SimpleList::Empty()

void ProtoGraph::Vertice::SimpleList::PrependItem(Item& item)
{
    item.Prepend(NULL);
    if (NULL == head)
        tail = &item;
    else
        head->Prepend(&item);
    item.Append(head);
    head = &item;
}  // end ProtoGraph::Vertice::SimpleList::PrependItem()

void ProtoGraph::Vertice::SimpleList::AppendItem(Item& item)
{
    item.Prepend(tail);
    if (NULL == tail)
        head = &item;
    else
        tail->Append(&item);
    item.Append(NULL);
    tail = &item;
}  // end ProtoGraph::Vertice::SimpleList::AppendItem()

void ProtoGraph::Vertice::SimpleList::RemoveItem(Item& item)
{
    Item* prevItem = item.GetPrev();
    Item* nextItem = item.GetNext();
    if (NULL != prevItem)
        prevItem->Append(nextItem);
    else
        head = nextItem;
    if (NULL != nextItem)
        nextItem->Prepend(prevItem);
    else
        tail = prevItem;
}  // end ProtoGraph::Vertice::SimpleList::RemoveItem()


ProtoGraph::Vertice::SimpleList::Iterator::Iterator(const Vertice::SimpleList& theList, bool reverse)
 : list(theList), next_item(reverse ? theList.tail : theList.head), forward(!reverse)
{
}

ProtoGraph::Vertice::SimpleList::Iterator::~Iterator()
{
}

void ProtoGraph::Vertice::SimpleList::Iterator::Reset()
{
    next_item = forward ? list.head : list.tail;
}  // end ProtoGraph::Vertice::SimpleList::Iterator::Reset()

ProtoGraph::Vertice* ProtoGraph::Vertice::SimpleList::Iterator::GetNextVertice()
{
    Item* item = next_item;
    next_item = item ? (forward ? item->GetNext() : item->GetPrev()) : NULL;
    return item ? item->GetVertice() : NULL;
}  // end ProtoGraph::Vertice::SimpleList::Iterator::GetNextVertice()


ProtoGraph::Vertice::SimpleList::ItemPool::ItemPool()
{
}

ProtoGraph::Vertice::SimpleList::ItemPool::~ItemPool()
{
    Destroy();
}

ProtoGraph::Vertice::SimpleList::Item* ProtoGraph::Vertice::SimpleList::ItemPool::GetItem()
{
    if (IsEmpty())
    {
        Item* item = new Item();
        if (NULL == item)
        {
            PLOG(PL_ERROR, "ProtoGraph::Vertice::SimpleList::ItemPool::GetItem() new Item error: %s\n",
                           GetErrorString());
            return NULL;
        }
        else
        {
            return item;
        }
    }
    else
    {
        return static_cast<Item*>(VerticeQueue::QueueStatePool::Get());
    }
}  // end ProtoGraph::Vertice::SimpleList::ItemPool::GetItem()

ProtoGraph::Vertice::SortedList::SortedList(ItemPool* itemPool)
 : item_pool(itemPool)
{
}

ProtoGraph::Vertice::SortedList::~SortedList()
{
    // Note that Empty() here invokes the Item's
    // virtual methods GetKey(), etc ... 
    Empty();
}

bool ProtoGraph::Vertice::SortedList::Insert(Vertice& vertice)
{
    Item* item = GetNewItem();
    if (NULL == item)
    {
        DMSG(0, "ProtoGraph::Vertice::SortedList::Insert() GetNewItem() error: %s\n", GetErrorString());
        return false;
    }
    Associate(vertice, *item);
    InsertItem(*item);
    return true;
}  // end ProtoGraph::Vertice::SortedList::Insert()

bool ProtoGraph::Vertice::SortedList::Append(Vertice& vertice)
{
    Item* item = GetNewItem();
    if (NULL == item)
    {
        DMSG(0, "ProtoGraph::Vertice::SortedList::Append() GetNewItem() error: %s\n", GetErrorString());
        return false;
    }
    Associate(vertice, *item);
    AppendItem(*item);
    return true;
}  // end ProtoGraph::Vertice::SortedList::Append()

void ProtoGraph::Vertice::SortedList::Remove(Vertice& vertice)
{
    // 1) Retrieve "Item*" from "vertice" queue state
    Item* item = static_cast<Item*>(vertice.GetQueueState(*this));
    // 2) Remove "item" from our ProtoSortedTree
    ASSERT(NULL != item);
    RemoveItem(*item);
    Disassociate(vertice, *item);
    // 3) Return "item" to pool
    if (NULL != item_pool)
        item_pool->Put(*item);
    else
        delete item;
}  // end ProtoGraph::Vertice::SortedList::Remove()

ProtoGraph::Vertice* ProtoGraph::Vertice::SortedList::RemoveHead()
{
    Item* headItem = static_cast<Item*>(sorted_item_tree.GetHead());
    if (NULL != headItem)
    {
        Vertice* vertice = headItem->GetVertice();
        ASSERT(NULL != vertice);
        sorted_item_tree.Remove(*headItem);
        Disassociate(*vertice, *headItem);
        // 3) Return "item" to pool
        if (NULL != item_pool)
            item_pool->Put(*headItem);
        else
            delete headItem;
        return vertice;
    }
    else
    {
        return NULL;
    }
}  // end ProtoGraph::Vertice::SortedList::RemoveHead()

void ProtoGraph::Vertice::SortedList::Empty()
{
    while (!IsEmpty()) RemoveHead();
}  // end ProtoGraph::Vertice::SortedList::Empty()

ProtoGraph::Vertice::SortedList::Item::Item()
{
}

ProtoGraph::Vertice::SortedList::Item::~Item()
{
    Cleanup();
}

// required (and optional) overrides for ProtoSortedTree::Item
const char* ProtoGraph::Vertice::SortedList::Item::GetKey() const
{
    ASSERT(NULL != GetVertice());
    return GetVertice()->GetVerticeKey();
}  // end ProtoGraph::Vertice::SortedList::Item::GetKey()

unsigned int ProtoGraph::Vertice::SortedList::Item::GetKeysize() const
{
    ASSERT(NULL != GetVertice());
    return GetVertice()->GetVerticeKeysize(); 
}  // end ProtoGraph::Vertice::SortedList::Item::GetKeysize()

ProtoTree::Endian ProtoGraph::Vertice::SortedList::Item::GetEndian() const
{
    ASSERT(NULL != GetVertice());
    return GetVertice()->GetVerticeKeyEndian(); 
}  // end ProtoGraph::Vertice::SortedList::Item::GetEndian()

bool ProtoGraph::Vertice::SortedList::Item::UseSignBit() const
{
    ASSERT(NULL != GetVertice());
    return GetVertice()->GetVerticeKeySigned(); 
}  // end ProtoGraph::Vertice::SortedList::Item::UseSignBit()

bool ProtoGraph::Vertice::SortedList::Item::UseComplement2() const
{
    ASSERT(NULL != GetVertice());
    return GetVertice()->GetVerticeKeyComplement2(); 
}  // end ProtoGraph::Vertice::SortedList::Item::UseComplement2()



ProtoGraph::Vertice::SortedList::ItemPool::ItemPool()
{
}

ProtoGraph::Vertice::SortedList::ItemPool::~ItemPool()
{
}

ProtoGraph::Vertice::SortedList::Item* ProtoGraph::Vertice::SortedList::ItemPool::GetItem()
{
    if (IsEmpty())
    {
        Item* item = new Item();
        if (NULL == item)
        {
            PLOG(PL_ERROR, "ProtoGraph::Vertice::SortedList::ItemPool::GetItem() new Item error: %s\n",
                           GetErrorString());
            return NULL;
        }
        else
        {
            return item;
        }
    }
    else
    {
        return static_cast<Item*>(VerticeQueue::QueueStatePool::Get());
    }
}  // end ProtoGraph::Vertice::SortedList::ItemPool::GetItem()

ProtoGraph::Vertice::SortedList::Iterator::Iterator(SortedList& theList)
 : ProtoSortedTree::Iterator(theList.sorted_item_tree)
{
}

ProtoGraph::Vertice::SortedList::Iterator::~Iterator()
{
}


// begin "class ProtoGraph::AdjacencyQueue" implementation
ProtoGraph::AdjacencyQueue::AdjacencyQueue(Vertice& srcVertice)
 : src_vertice(srcVertice), adjacency_count(0)
{
}

ProtoGraph::AdjacencyQueue::~AdjacencyQueue()
{
    Empty();
}

void ProtoGraph::AdjacencyQueue::Empty()
{
    Edge* edge;
    while (NULL != (edge = static_cast<Edge*>(adjacency_tree.GetRoot())))
    {
        Vertice* dst = edge->GetDst();
        ASSERT(NULL != dst);
        Disconnect(*dst);
    }
}  // end ProtoGraph::AdjacencyQueue::Empty()

void ProtoGraph::AdjacencyQueue::Connect(Vertice& dstVertice, Edge& edge)
{
    if (static_cast<VerticeQueue*>(this) != edge.GetQueue())
    {
        Associate(dstVertice, edge);
        adjacency_tree.Insert(edge);
        dstVertice.AddConnector(edge);
        adjacency_count++;
        edge.OnConnect();
    }
    else
    {
        PLOG(PL_WARN, "ProtoGraph::AdjacencyQueue::Connect() warning: edge already in adjacency queue\n");
    }
}  // end ProtoGraph::AdjacencyQueue::Connect()

void ProtoGraph::AdjacencyQueue::Reconnect(Vertice& dstVertice, Edge& edge)
{
    if (static_cast<VerticeQueue*>(this) != edge.GetQueue())
    {
        Associate(dstVertice, edge);
        adjacency_tree.Insert(edge);
        dstVertice.AddConnector(edge);
        adjacency_count++;
    }
    else
    {
        PLOG(PL_WARN, "ProtoGraph::AdjacencyQueue::Reconnect() warning: edge already in adjacency queue\n");
    }
}  // end ProtoGraph::AdjacencyQueue::Reconnect()

/**
 *  Remove all edges to "dstVertice"
 */
void ProtoGraph::AdjacencyQueue::Disconnect(Vertice&   dstVertice, 
                                            EdgePool*  edgePool)
{
    Edge* edge;
    while (NULL != (edge = static_cast<Edge*>(dstVertice.GetQueueState(*this))))
    {
        edge->OnDisconnect();
        RemoveEdge(dstVertice, *edge, edgePool);
    }
}  // end ProtoGraph::AdjacencyQueue::Disconnect()

void ProtoGraph::AdjacencyQueue::RemoveEdge(Vertice&   dstVertice, 
                                            Edge&      edge, 
                                            EdgePool*  edgePool)
{
    if (static_cast<VerticeQueue*>(this) == edge.GetQueue())
    {
        SuspendEdge(dstVertice, edge); 
        if (NULL != edgePool){
            edgePool->Put(edge);
        }
        else{
            delete &edge;
        }
    }
    else
    {
        PLOG(PL_WARN, "ProtoGraph::AdjacencyQueue::RemoveEdge() warning: edge not in queue\n");
    }
}  // end ProtoGraph::AdjacencyQueue::RemoveEdge()

void ProtoGraph::AdjacencyQueue::SuspendEdge(Vertice&   dstVertice, 
                                             Edge&      edge)
{
    if (static_cast<VerticeQueue*>(this) == edge.GetQueue())
    {
        dstVertice.RemoveConnector(edge);
        adjacency_tree.Remove(edge);
        Disassociate(dstVertice, edge);
        adjacency_count--;
    }
    else
    {
        PLOG(PL_WARN, "ProtoGraph::AdjacencyQueue::SuspendEdge() warning: edge not in queue\n");
    }
}  // end ProtoGraph::AdjacencyQueue::SuspendEdge()


void ProtoGraph::AdjacencyQueue::AddConnector(Edge& edge)
{
    connector_tree.Insert(edge.AccessTracker());
}  // end ProtoGraph::AdjacencyQueue::AddConnector()

void ProtoGraph::AdjacencyQueue::RemoveConnector(Edge& edge)
{
    connector_tree.Remove(edge.AccessTracker());
}  // end ProtoGraph::AdjacencyQueue::RemoveConnector()

ProtoGraph::AdjacencyIterator::AdjacencyIterator(Vertice & vertice)
 : adj_iterator(vertice.adjacency_queue.adjacency_tree),
   con_iterator(vertice.adjacency_queue.connector_tree)
{
}

ProtoGraph::AdjacencyIterator::~AdjacencyIterator()
{
}

ProtoGraph::Vertice* ProtoGraph::AdjacencyIterator::GetNextAdjacency()
{
    Edge* edge = static_cast<Edge*>(adj_iterator.GetNextItem());
    return ((NULL != edge) ? edge->GetDst() : NULL);
}  // end ProtoGraph::AdjacencyIterator::GetNextAdjacency()

ProtoGraph::Vertice* ProtoGraph::AdjacencyIterator::GetNextConnector()
{
    Edge::Tracker* edgeTracker = static_cast<Edge::Tracker*>(con_iterator.GetNextItem());
    return ((NULL != edgeTracker) ? edgeTracker->GetEdge().GetSrc() : NULL);
}  // end ProtoGraph::AdjacencyIterator::GetNextConnector()

ProtoGraph::Edge::Tracker::Tracker(const Edge& theEdge)
 : edge(theEdge)
{
}
        
ProtoGraph::Edge::Edge()
 : tracker(*this)
{
}

ProtoGraph::Edge::~Edge()
{
}

void ProtoGraph::Edge::OnConnect()
{
}

void ProtoGraph::Edge::OnDisconnect()
{
}

const char* ProtoGraph::Edge::GetKey() const
{
    return NULL;
}  // end ProtoGraph::Edge::GetKey()

unsigned int ProtoGraph::Edge::GetKeysize() const
{
    return 0;
}  // end ProtoGraph::Edge::GetKeysize()

        
ProtoTree::Endian ProtoGraph::Edge::GetEndian() const
{
    return ProtoSortedTree::Item::GetEndian();
}  // end ProtoGraph::Edge::GetEndian()

bool ProtoGraph::Edge::UseSignBit() const
{
    return ProtoSortedTree::Item::UseSignBit();
}  // end ProtoGraph::Edge::UseSignBit()

bool ProtoGraph::Edge::UseComplement2() const
{
    return ProtoSortedTree::Item::UseComplement2();
}  // end ProtoGraph::Edge::UseComplement2()


ProtoGraph::Vertice* ProtoGraph::Edge::GetSrc() const
{
    // TBD - should we do a dynamic_cast to validate that the
    // "queue" the edge is held in is actually an "AdjacencyQueue" ???
    AdjacencyQueue* adjQueue = static_cast<AdjacencyQueue*>(QueueState::GetQueue());
    if (NULL != adjQueue)
        return &(adjQueue->GetSrc());
    else
        return NULL;
}  // end ProtoGraph::Edge::GetSrc()

ProtoGraph::EdgePool::EdgePool()
{
}

ProtoGraph::EdgePool::~EdgePool()
{
    Destroy();
}

// begin "class ProtoGraph::SimpleTraversal" implementation
ProtoGraph::SimpleTraversal::SimpleTraversal(const ProtoGraph& theGraph, 
                                             Vertice&          startVertice,
                                             bool              depthFirst)
 : graph(theGraph), start_vertice(startVertice), depth_first(depthFirst), 
   queue_pending(&item_pool), queue_visited(&item_pool)
{
    Reset();
}  

ProtoGraph::SimpleTraversal::~SimpleTraversal()
{
    queue_visited.Empty();
    queue_pending.Empty();
    item_pool.Destroy();
}

bool ProtoGraph::SimpleTraversal::Reset()
{
    current_level = 0;
    trans_vertice = NULL;
    queue_visited.Empty();
    queue_pending.Empty();
    if (!queue_pending.Append(start_vertice))
    {
        PLOG(PL_ERROR, "ProtoGraph::SimpleTraversal::Reset() error: couldn't enqueue start_vertice\n");
        return false;
    }
    return true;
}  // end ProtoGraph::SimpleTraversal::Reset()


ProtoGraph::Vertice* ProtoGraph::SimpleTraversal::GetNextVertice(unsigned int* level)
{
    Vertice* currentVertice = queue_pending.GetHead();
    if (NULL != currentVertice)
    {
        queue_pending.TransferVertice(*currentVertice, queue_visited);
        AdjacencyIterator it(*currentVertice);
        Edge* nextEdge;
        Vertice* transAdjacency = NULL;
        while (NULL != (nextEdge = it.GetNextAdjacencyEdge()))
        {
            Vertice* nextVertice = nextEdge->GetDst();
            ASSERT(NULL != nextVertice);
            if (!nextVertice->IsInQueue(queue_visited) &&
                !nextVertice->IsInQueue(queue_pending))
            {
                if (!AllowEdge(*currentVertice, *nextEdge)) continue;
                if (depth_first)
                {
                    queue_pending.Prepend(*nextVertice);
                    // (TBD) implement level tracking for Depth-first search
                }
                else
                {
                    queue_pending.Append(*nextVertice);
                    if (NULL == transAdjacency) 
                        transAdjacency = nextVertice;
                }
            }
        }
        if (NULL == trans_vertice)
        {
            trans_vertice = transAdjacency;
        }
        else if (trans_vertice == currentVertice)
        {
            current_level++;
            trans_vertice = transAdjacency;
        }
        if (NULL != level) *level = current_level;
    }
    return currentVertice;
}  // end ProtoGraph::SimpleTraversal::GetNextVertice()

// begin "class ProtoGraph" implementation
ProtoGraph::ProtoGraph()
 : vertice_list(&vertice_list_item_pool)
{
}

ProtoGraph::~ProtoGraph()
{
    Empty();
    vertice_list_item_pool.Destroy();
    edge_pool.Destroy();
}

ProtoGraph::Edge* ProtoGraph::GetEdge()
{
    Edge* edge = edge_pool.GetEdge();
    if (NULL == edge) edge = CreateEdge();
    if (NULL == edge)
    {
        PLOG(PL_ERROR, "ProtoGraph::GetEdge() error: unable to allocate Edge\n");
        return NULL;
    }
    return edge;
}  // end ProtoGraph::GetEdge()

/**
 * Iterate over "vertice_list", disconnecting any
 * adjacencies and then remove each vertice
 */
void ProtoGraph::Empty()
{
    // Note the removed edges are pooled and the
    // vertices removed are _not_ deleted
    while (!vertice_list.IsEmpty())
    {
        Vertice* v = vertice_list.GetHead();
        AdjacencyIterator it(*v);
        Vertice* a;
        // (TBD) This could be optimized a little more using it.GetNextEdge() instead ???
        while (NULL != (a = it.GetNextAdjacency()))
            v->Disconnect(*a, &edge_pool);
        vertice_list.Remove(*v);
    }
}  // end ProtoGraph::Empty()

bool ProtoGraph::InsertVertice(Vertice& vertice)
{
    if (vertice.IsInQueue(vertice_list))
    {
        PLOG(PL_ERROR, "ProtoGraph::InsertVertice() error: vertice already in graph!\n");
        return false;
    }
    else
    {
        return vertice_list.Insert(vertice);
    }
}  // end ProtoGraph::InsertVertice()

void ProtoGraph::RemoveVertice(Vertice& vertice)
{
    // 1) Remove all edges to/from this vertice
    AdjacencyIterator it(vertice);
    Vertice* next;
    // a) Remove all edges from this vertice to any "adjacent" vertices
    while (NULL != (next = it.GetNextAdjacency()))
    {
        Disconnect(vertice, *next);
    }
    // b) Remove all edges from any "connector" vertices to this vertice
    while (NULL != (next = it.GetNextConnector()))
        Disconnect(*next, vertice);
    
    // 2) Remove "vertice" from the graph "vertice_list"
    vertice_list.Remove(vertice);
}  // end ProtoGraph::RemoveVertice()

/**
 * Get an "Edge" from our "edge_pool" or create one and
 * then add the dst/edge to the src's "adjacency_queue"
 */
ProtoGraph::Edge* ProtoGraph::Connect(Vertice& src, Vertice& dst)
{
    ASSERT(src.IsInQueue(vertice_list) && dst.IsInQueue(vertice_list));
    // 1) Fetch an "Edge" from our "edge_pool" or create one
    Edge* edge = GetEdge();
    if (NULL == edge)
    {
        PLOG(PL_ERROR, "ProtoGraph::Connect() error: CreateEdge() error: %s\n", GetErrorString());
        return NULL;
    }
    // 2) Add the dst/edge to the src's "adjacency_queue"
    src.Connect(dst, *edge);
    return edge;
}  // end ProtoGraph::Connect()

/**
 * Call Reconnect for the source and dst using the provided edge.
 */
void ProtoGraph::Reconnect(Vertice& src, Vertice& dst,Edge& edge)
{
    ASSERT(src.IsInQueue(vertice_list) && dst.IsInQueue(vertice_list));
    src.Reconnect(dst,edge);
    return;
}
/**
 * Remove "dst" from src.adjacency_queue and return all
 * edges to "edge_pool"
 */
void ProtoGraph::Disconnect(Vertice& src, Vertice& dst, bool duplex)
{
    // Remove "dst" from src.adjacency_queue and return all edges to "edge_pool"
    src.Disconnect(dst, &edge_pool);
    if (duplex) dst.Disconnect(src, &edge_pool);
}  // end ProtoGraph::Disconnect()

ProtoGraph::VerticeIterator::VerticeIterator(ProtoGraph& theGraph)
 : Vertice::SortedList::Iterator(theGraph.vertice_list)
{
}

ProtoGraph::VerticeIterator::~VerticeIterator()
{
}
