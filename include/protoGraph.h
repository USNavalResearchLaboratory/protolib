#ifndef _PROTO_GRAPH
#define _PROTO_GRAPH

#include "protoTree.h"
#include "protoDebug.h"
#include "protoDefs.h"

// TBD - this could be re-implemented using ProtoQueue classes
//       for VerticeQueue, etc.

/**
 * @class ProtoGraph
 *
 * @brief Base class for managing graph data structures.
 */
class ProtoGraph
{
    public:
        ProtoGraph();
        virtual ~ProtoGraph();
        
        /**
         * @class Vertice
         *
         * @brief A basic ProtoGraph element, the "vertice"
         */
        class Vertice;
        /**
         * @class Edge
         *
         * @brief A basic ProtoGraph element, the "edge"
         */
        class Edge;
        
        // (TBD) We should probably give the ProtoGraph
        // a "VerticeFactory" and require vertices be
        // managed _under_ the graph (no Insert/Remove) since 
        // our Vertice class only has one AdjacencyQueue 
        // member.  Otherwise we would need to give the Vertice
        // a "GraphList" with each GraphList::Item having
        // the "AdjacencyQueue" for that corresponding graph?
        
        bool InsertVertice(Vertice& vertice);
        
        void RemoveVertice(Vertice& vertice);  // associated links are removed with vertice
        
        Vertice* FindVertice(const char* key, unsigned int keysize) const
            {return vertice_list.FindVertice(key, keysize);}
        
        bool IsInGraph(const Vertice& vertice) const
            {return (vertice.IsInQueue(vertice_list));}
        
        
        // "Connect()" creates a directed edge from "src" to "dst"
        Edge* Connect(Vertice& src, Vertice& dst);

        void Reconnect(Vertice& src, Vertice& dst, Edge& edge);
 
        void Disconnect(Vertice& src, Vertice& dst, bool duplex = false);
        
        bool IsEmpty() const
            {return vertice_list.IsEmpty();}
        
        // Disconnect and remove any inserted vertices
        // (does not delete the vertices)
        void Empty();
         
         
        /**
         * @class ProtoGraph::AdjacencyIterator
         */                
        class AdjacencyIterator
        {
            public:
                AdjacencyIterator(Vertice& vertice);
                virtual ~AdjacencyIterator();
                
                // TBD - provide some iteration options here.  For example,
                // iterate conjointly over "adjacencies" and "connectors"

                // @brief Returns next vertice _to_ which there is connection
                Vertice* GetNextAdjacency();
                
                // @brief Returns next edge _to_ which there is connection
                Edge* GetNextAdjacencyEdge()
                    {return static_cast<Edge*>(adj_iterator.GetNextItem());}
                
                // @brief Returns next vertice _from_ which there is connection
                // (note can use Vertice::GetEdgeTo(vertice) to get that edge if desired)
                Vertice* GetNextConnector();
                
                void Reset()
                {
                    adj_iterator.Reset();
                    con_iterator.Reset();
                }
                
            private:
                ProtoSortedTree::Iterator adj_iterator;
                ProtoSortedTree::Iterator con_iterator;

        };  // end class ProtoGraph::Vertice::AdjacencyIterator
        
        /**
         * @class VerticeQueue
         *
         * @brief Base class for Queues that wish to contain Vertices.  Note that
         * the VerticeQueue does _not_ itself implement any form of list or tree, etc
         * but instead leaves the specific form of data structure to derived
         * subclasses so that VerticeQueues may be implemented in different ways.  The
         * VerticeQueue really just provides a base means for Vertices to keep track
         * of which lists they are in and the pure virtual VerticeQueue::Remove() method
         * is where a Vertice should get disassociated from the given VerticeQueue derivative
         */

        class VerticeQueue
        {
            public:
                virtual ~VerticeQueue();
            
                virtual void Remove(Vertice& vertice) = 0;
                
                virtual void Empty() = 0;  // MUST remove all items from queue
                
                bool Contains(const Vertice& vertice)
                    {return (NULL != vertice.GetQueueState(*this));}
                
                class QueueStatePool;
                /**
                 * @class QueueState
                 *
                 * @brief The "ProtoGraph::VerticeQueue::QueueState" 
                 * class is a base class that enables
                 * the "Vertice" class to keep track of the 
                 * VerticeQueues to which it belongs.  Additionally, 
                 * those VerticeQueue subclasses can extend
                 * the VerticeQueue::QueueState class to contain 
                 * additional state that is  associated with the 
                 * given vertice in the context of that VerticeQueue
                 */            
                class QueueState
                {
                    friend class VerticeQueue;
                    friend class Vertice;
                    friend class QueueStatePool;

                    public:
                        virtual ~QueueState();

                        Vertice* GetVertice() const
                            {return vertice;}
                        VerticeQueue* GetQueue() const
                            {return queue;}

                    protected:  
                        QueueState();
                    
                        // IMPORTANT: Any derived QueueState classes MUST call
                        // cleanup in their destructor to avoid possible indirect
                        // calls to virtual functions in the ~QueueState() destructor
                        void Cleanup();

                        void Associate(Vertice& theVertice, VerticeQueue& theQueue)
                        {
                            vertice = &theVertice;
                            queue = &theQueue;
                        } 
                        void Disassociate()
                        {
                            vertice = NULL;
                            queue = NULL;
                        } 

                        void SetQueue(VerticeQueue& theQueue)
                            {queue = &theQueue;}
                        
                        /**
                         * @class Entry
                         *
                         * @brief Container used by Vertices to keep 
                         * their lists of VerticeQueueState
                         */
                        class Entry : public ProtoTree::Item
                        {
                            public:
                                Entry(QueueState& queueState);
                                QueueState& GetQueueState() const
                                    {return queue_state;}

                                const char* GetKey() const
                                    {return ((const char*)queue_state.GetQueueHandle());}
                                unsigned int GetKeysize() const
                                    {return (sizeof(VerticeQueue*) << 3);}

                            private:
                                QueueState& queue_state;  // "parent" VerticeQueueState
                        }; // end class ProtoGraph::Vertice::Queue::QueueState::Entry

                        Entry& AccessEntry() {return entry;}

                        const VerticeQueue** GetQueueHandle() const
                            {return ((const VerticeQueue**)&queue);}
                        
                    private:
                        Vertice*        vertice;
                        VerticeQueue*   queue;  // "parent" VerticeQueue
                        Entry           entry;

                };  // end class ProtoGraph::VerticeQueue::QueueState
                /**
                 * @class ProtoGraph::VerticeQueue::QueueStatePool
                 *
                 * @brief QueueState repository.  Note that a QueueStatePool
                 * should ony be used to cache a single (i.e. homogeneous) type
                 * of QueueState (i.e. QueueState subclass) otherwise one
                 * may not "Get()" what one expects from the pool!
                 */            
                class QueueStatePool : public ProtoTree::ItemPool
                {
                    public:
                        QueueStatePool();
                        virtual ~QueueStatePool();

                        void Destroy();

                        QueueState* Get()
                        {
                            QueueState::Entry* entry = static_cast<QueueState::Entry*>(ProtoTree::ItemPool::Get());
                            return ((NULL != entry) ? &(entry->GetQueueState()) : NULL);
                        }

                        void Put(QueueState& theState)
                            {ProtoTree::ItemPool::Put(theState.AccessEntry());}

                };  // end class ProtoGraph::VerticeQueue::QueueStatePool

            protected:
                VerticeQueue();
            
                void TransferQueueState(QueueState& queueState, VerticeQueue& dstQueue)
                {
                    Vertice* vertice = queueState.GetVertice();
                    ASSERT(NULL != vertice);
                    vertice->Dereference(queueState);
                    queueState.SetQueue(dstQueue);
                    vertice->Reference(queueState);
                }    
                
                QueueState* GetQueueState(const Vertice& vertice) const
                    {return vertice.GetQueueState(*this);}
                
                void Associate(Vertice& vertice, QueueState& queueState);
                void Disassociate(Vertice& vertice, QueueState& queueState);
                
        };  // end class VerticeQueue
        
        
        class EdgePool;  
        /**
         * @class AdjacencyQueue
         *
         * @brief List of Vertices adjacent, sorted by Edge "key"
         * (i.e. instead of Vertice "key" as in case of SortedList)
         */
        class AdjacencyQueue : public VerticeQueue
        {
            friend class Vertice;
            friend class Edge;
            friend class AdjacencyIterator;
            
            protected:
                AdjacencyQueue(Vertice& srcVertice);
                virtual ~AdjacencyQueue();

                // Connect to "dstVertice" with "edge"
                void Connect(Vertice&   dstVertice, 
                             Edge&      edge);
                // Same as connect but doesn't call OnConnect
                void Reconnect(Vertice&   dstVertice, 
                             Edge&      edge);
                 
                // Remove all edges to "dstVertice"
                void Disconnect(Vertice&    dstVertice, 
                                EdgePool*   edgePool = NULL);

                // Remove a specific edge (and pool or delete it)
                void RemoveEdge(Vertice&    dstVertice, 
                                Edge&       edge, 
                                EdgePool*   edgePool = NULL);
                
                // Remove a specific edge (but don't delete it)
                void SuspendEdge(Vertice&    dstVertice, 
                                 Edge&       edge);

                Vertice& GetSrc() const
                    {return src_vertice;}

                // Note this count currently only reflects edges _to_ other Vertices
                unsigned int GetCount() const
                    {return adjacency_count;} 
           
                // Methods used to manage connector_tree
                // TBD - keep a "connector_count" ???
                void AddConnector(Edge& edge);
                void RemoveConnector(Edge& edge);
                
            private:
                void Remove(Vertice& dstVertice)
                    {Disconnect(dstVertice, NULL);}
            
                void Empty();

                Vertice&        src_vertice;
                ProtoSortedTree adjacency_tree;  // list of dst vertices I am connected _to_
                unsigned int    adjacency_count; 
                ProtoSortedTree connector_tree;  // sorted list edges connected _to_ me

        };  // end class ProtoGraph::AdjacencyQueue
        
        
        /** 
         * @class ProtoGraph::Edge
         * 
         * @brief The ProtoGraph::Edge inherits from "VerticeQueue::QueueState" so
         * for the Edge's src Vertice "adjacency_queue" that derives from VerticeQueue.
         * It inherits from ProtoSortedTree::Item since the AdjacencyQueue is implemented
         * as a ProtoSortedTree (sorted on the Edge key).  Finally, the Edge contains
         * a "tracker" member that is used by the Edge's dst Vertice to "track" which
         * edges are pointing towards that given Vertice. A "connector_list" is an element
         * of the Vertice "adjacency_queue"
         *
         */
        class Edge : public VerticeQueue::QueueState, public ProtoSortedTree::Item
        {
            friend class AdjacencyQueue;
            friend class AdjacencyIterator;
            
            public:
                Edge();
                
                virtual ~Edge();
 
                virtual void OnConnect();
                virtual void OnDisconnect();

                Vertice* GetDst() const
                    {return GetVertice();}
                
                Vertice* GetSrc() const;
                
                // Subclasses should override these to provide
                // a sorting criteria (if desired) for the 
                // "AdjacencyQueue" defined above
                virtual const char* GetKey() const;
                virtual unsigned int GetKeysize() const; 
                virtual ProtoTree::Endian GetEndian() const;
                virtual bool UseSignBit() const;
                virtual bool UseComplement2() const;
            
            private:
                class Tracker : public ProtoSortedTree::Item
                {
                    public:
                        Tracker(const Edge& edge);
                    
                        const Edge& GetEdge() const
                            {return edge;}
                    
                    private:
                        virtual const char* GetKey() const
                            {return edge.GetKey();}
                        virtual unsigned int GetKeysize() const
                            {return edge.GetKeysize();}
                        virtual ProtoTree::Endian GetEndian() const
                            {return edge.GetEndian();}
                        virtual bool UseSignBit() const
                            {return edge.UseSignBit();}
                        virtual bool UseComplement2() const
                            {return edge.UseComplement2();}

                        const Edge& edge;

                };  // end class ProtoGraph::Edge::Tracker
                
                // Only the AdjacencyQueue should invoke this method.
                Tracker& AccessTracker() 
                    {return tracker;}  
                    
                Tracker tracker;  // used by dst vertices to "track" edges _from_ src vertices
                                  // (Whenever an Edge is added to a src vertice "adjacency_queue",
                                  //  the Edge::tracker is added to the dst vertice 
                                  // "adjacency_queue::connector_tree"
                
        };  // end class ProtoGraph::Edge
       
        class EdgePool : public VerticeQueue::QueueStatePool
        {
            public:
                EdgePool();
                virtual ~EdgePool();
                
                 Edge* GetEdge()
                    {return static_cast<Edge*>(VerticeQueue::QueueStatePool::Get());}  
                
                void  PutEdge(Edge& edge)
                    {VerticeQueue::QueueStatePool::Put(edge);}     
                    
        };  // end class ProtoGraph::EdgePool
         
        
        /** 
         * @class Vertice
         *
         */
        class Vertice
        {
            friend class ProtoGraph;
            friend class VerticeQueue;
            
            public:
                virtual ~Vertice();   
            
                // Subclasses _may_ want to override these methods
                // (These are used for default sorting of
                //  of the Vertice::SortedList if used)
                // _BUT_ if these are overridden, the derived
                // class destructor _MUST_ call Vertice::Cleanup() so
                // that these _are_ not indirectly called in the Vertice
                // destructor!!!
                virtual const char* GetVerticeKey() const = 0;
                virtual unsigned int GetVerticeKeysize() const  = 0;
                virtual ProtoTree::Endian GetVerticeKeyEndian() const;
                virtual bool GetVerticeKeySigned() const;
                virtual bool GetVerticeKeyComplement2() const;
                
                void Cleanup();  // see comment immediately above
                
                bool IsInQueue(const VerticeQueue& queue) const
                    {return (NULL != GetQueueState(queue));}
                
                // This tests for "this" _to_ "dst" connection (unidirectional check)
                bool HasEdgeTo(const Vertice& dst) const
                    {return dst.IsInQueue(adjacency_queue);}
                
                Edge* GetEdgeTo(const Vertice& dst) const
                    {return static_cast<Edge*>(dst.GetQueueState(adjacency_queue));}
                
                unsigned int GetAdjacencyCount() const
                    {return adjacency_queue.GetCount();}
                
                /** 
                 * @class ProtoGraph::Vertice::SimpleList
                 *
                 * @brief Simple unsorted, doubly linked-list class used for 
                 * traversals and other purposes
                 */
                class SimpleList : public VerticeQueue
                {
                    public:
                        class ItemPool;
                        
                        SimpleList(SimpleList::ItemPool* itemPool = NULL);
                        virtual ~SimpleList();
                        
                        // required override
                        virtual void Remove(Vertice& vertice);
                        
                        bool Prepend(Vertice& vertice);
                        bool Append(Vertice& vertice);
                        
                        bool IsEmpty() const
                            {return (NULL == head);}
                        
                        // Note "Empty()" does not delete Vertices, but does
                        // delete or pool the queue state "Items"
                        void Empty();
                        
                        Vertice* GetHead() const
                            {return ((NULL != head) ? head->GetVertice() : NULL);}
                        
                        Vertice* RemoveHead();
                        
                        class Iterator;
                        
                        /**
                         * @class ProtoGraph::Vertice::SimpleList::Item
                         *
                         */
                        class Item : public QueueState
                        {
                            friend class SimpleList;
                            friend class ItemPool;
                            friend class Iterator;
                            
                            public:
                                Item();
                                virtual ~Item();
                            
                            protected:
                                void Prepend(Item* theItem) 
                                    {prev = theItem;}
                                void Append(Item* theItem) 
                                    {next = theItem;}
                             
                                Item* GetPrev() const 
                                    {return prev;}
                                Item* GetNext() const
                                    {return next;}

                            private:
                                Item*       prev;    
                                Item*       next;
                        };  // end class ProtoGraph::Vertice::SimpleList::Item
                        
                        // Move vertice from this SimpleList to another
                        void TransferVertice(Vertice& vertice, SimpleList& dstSimpleList)
                        {
                            Item* item = static_cast<Item*>(vertice.GetQueueState(*this));
                            ASSERT(NULL != item);
                            TransferItem(*item, dstSimpleList);
                        }
                        
                        void TransferItem(Item& item, SimpleList& dstSimpleList)
                        {
                            Vertice* vertice = item.GetVertice();
                            ASSERT(NULL != vertice);
                            RemoveItem(item);
                            TransferQueueState(item, dstSimpleList);
                            dstSimpleList.AppendItem(item);
                        }
                    
                        /**
                         * @class ProtoGraph::Vertice::SimpleList::ItemPool
                         *
                         * @brief Container class for Items
                         */
                        class ItemPool : public VerticeQueue::QueueStatePool
                        {
                            public:
                                ItemPool();
                                virtual ~ItemPool();
                                
                                Item* GetItem();
                                
                                void PutItem(Item& item)
                                    {VerticeQueue::QueueStatePool::Put(item);}
                                
                        };  // end class ProtoGraph::Vertice::SimpleList::ItemPool
                        
                        /**
                         * @class ProtoGraph::Vertice::SimpleList::Iterator
                         */
                        class Iterator
                        {
                            public:
                                Iterator(const SimpleList& theSimpleList, bool reverse = false);
                                virtual ~Iterator();

                                void Reset();
                                Vertice* GetNextVertice();
                                
                            private:
                                const SimpleList&   list;
                                Item*               next_item;
                                bool                forward;

                        };  // end class ProtoGraph::Vertice::SimpleList::Iterator
                        friend class Iterator;

                    protected:
                        Item* GetNewItem()
                            {return ((NULL != item_pool) ? item_pool->GetItem() : new Item);}
                        void AppendItem(Item& item);
                        void PrependItem(Item& item);
                        void RemoveItem(Item& item);
                            
                        Item*       head;
                        Item*       tail;
                        ItemPool*   item_pool;
                        
                };  // end class ProtoGraph::Vertice::SimpleList()
                
                
                /**
                 * @class SortedList
                 *
                 * @brief This maintains a list of Vertices, 
                 * sorted by their "key", whatever that happens to be ...
                 */
                class SortedList : public VerticeQueue
                {
                    public:
                        class ItemPool;
                    
                        SortedList(ItemPool* itemPool = NULL);
                        virtual ~SortedList();
                        
                        // required override
                        virtual void Remove(Vertice& vertice);
                        
                        bool Insert(Vertice& vertice);
                        
                        Vertice* FindVertice(const char* key, unsigned int keysize) const
                        {
                            Item* item = static_cast<Item*>(sorted_item_tree.Find(key, keysize));
                            return (NULL != item) ? item->GetVertice() : NULL;
                        }
                        
                        Vertice* GetHead() const
                        {
                            Item* headItem = static_cast<Item*>(sorted_item_tree.GetHead());
                            return ((NULL != headItem) ? headItem->GetVertice() : NULL);
                        }
                        
                        Vertice* RemoveHead();
                        
                        // Only use when you don't want the list sorted!
                        // (you should probably use simple list instead)
                        bool Append(Vertice& vertice);
                        
                        bool IsEmpty() const
                            {return sorted_item_tree.IsEmpty();}
                        
                        void Empty();
                        
                        /**
                         * @class ProtoGraph::Vertice::SortedList::Item  
                         *
                         * @brief "Item" is our QueueState subclass 
                         * "container" for vertices in the list
                         */
                        class Item : public VerticeQueue::QueueState, public ProtoSortedTree::Item
                        {
                            public:
                                Item();
                                virtual ~Item();
                                
                                // required (and optional) overrides for ProtoSortedTree::Item
                                // (these default implementations use Vertice::GetVerticeKey(), etc)
                                virtual const char* GetKey() const;
                                virtual unsigned int GetKeysize() const;
                                virtual ProtoTree::Endian GetEndian() const;
                                virtual bool UseSignBit() const;
                                virtual bool UseComplement2() const;
                        };  // end class ProtoGraph::Vertice::SortedList::Item  
                        
                        // Move vertice from this SortedList to another
                        void TransferVertice(Vertice& vertice, SortedList& dstList)
                        {
                            Item* item = static_cast<Item*>(vertice.GetQueueState(*this));
                            ASSERT(NULL != item);
                            TransferItem(*item, dstList);
                        }
                        
                        void TransferItem(Item& item, SortedList& dstList)
                        {
                            Vertice* vertice = item.GetVertice();
                            ASSERT(NULL != vertice);
                            RemoveItem(item);
                            TransferQueueState(item, dstList);
                            dstList.InsertItem(item);
                        }
                        
                        /**
                         * @class ProtoGraph::Vertice::SortedList::ItemPool
                         *
                         */
                        class ItemPool : public VerticeQueue::QueueStatePool
                        {
                            public:
                                ItemPool();
                                virtual ~ItemPool();
                                
                                Item* GetItem();
                                
                                void PutItem(Item& item)
                                    {VerticeQueue::QueueStatePool::Put(item);}
                                
                        };  // end class ProtoGraph::Vertice::SortedList::ItemPool    
                        
                        /**
                         * @class ProtoGraph::Vertice::SortedList::Iterator
                         */
                        class Iterator : public ProtoSortedTree::Iterator
                        {
                            public:
                                Iterator(SortedList& theList);
                                virtual ~Iterator();

                                Item* GetNextItem()
                                    {return static_cast<Item*>(ProtoSortedTree::Iterator::GetNextItem());}
                                
                                Vertice* GetNextVertice()
                                {
                                    Item* nextItem = GetNextItem();
                                    return ((NULL != nextItem) ? nextItem->GetVertice() : NULL);
                                }
                        };  // end class ProtoGraph::Vertice::SortedList::Iterator
                        friend class Iterator;
                        
                    protected:
                        Item* GetNewItem()
                            {return ((NULL != item_pool) ? item_pool->GetItem() : new Item);}
                        void InsertItem(Item& item)
                            {
                                ASSERT(0 != item.GetKeysize());
                                sorted_item_tree.Insert(item);
                            }
                        void RemoveItem(Item& item)
                            {sorted_item_tree.Remove(item);}
                        void AppendItem(Item& item)
                            {sorted_item_tree.Append(item);}
                        
                        ProtoSortedTree sorted_item_tree;
                        ItemPool*       item_pool;
                        
                };  // end class ProtoGraph::Vertice::SortedList
                
                friend class AdjacencyQueue;
                friend class AdjacencyIterator;
                
            protected:
                Vertice();
            
                // These are used by the friend class ProtoGraph.  We keep these
                // protected since the ProtoGraph instance in which the connections
                // are made (or removed) maintains the pools of Edge and Connector
                // instances and public access to these methods could result
                // in mismanagement of the pooled items.  I.e., the 
                // "ProtoGraph::Connect()", "ProtoGraph::Disconnect()", etc
                // methods MUST be used instead of using these directly.
            
                // Note here the "edge" is the connection from src->dst while
                // the "connector" allows dst vertices to know who is connected
                // to them.  
                void Connect(Vertice& dst, Edge& edge)
                        {adjacency_queue.Connect(dst, edge);}
                
                void Reconnect(Vertice& dst, Edge& edge)
                        {adjacency_queue.Reconnect(dst, edge);}
                
                void Disconnect(Vertice& dst, EdgePool* edgePool = NULL)
                    {adjacency_queue.Disconnect(dst, edgePool);}
                
                void RemoveEdge(Vertice& dst, Edge& edge, EdgePool* edgePool = NULL)
                    {adjacency_queue.RemoveEdge(dst, edge, edgePool);}
                
                void SuspendEdge(Vertice& dst, Edge& edge)
                    {adjacency_queue.SuspendEdge(dst, edge);}
            
            private:    
                // These are called by a connected src Vertice::adjacency_queue
                // so a dst Vertice can know of connections _to_ itself
                void AddConnector(Edge& edge)
                    {adjacency_queue.AddConnector(edge);}
                void RemoveConnector(Edge& edge)
                    {adjacency_queue.RemoveConnector(edge);}
                    
                VerticeQueue::QueueState* GetQueueState(const VerticeQueue& queue) const
                {
                    const VerticeQueue* ptr = &queue;
                    VerticeQueue::QueueState::Entry* entry = 
                        static_cast<VerticeQueue::QueueState::Entry*>(queue_state_tree.Find((const char*)&ptr, sizeof(VerticeQueue*) << 3));
                    return ((NULL != entry) ? &entry->GetQueueState() : NULL);
                } 
                void Reference(VerticeQueue::QueueState& queueState)
                    {queue_state_tree.Insert(queueState.AccessEntry());}
                void Dereference(VerticeQueue::QueueState& queueState)
                {
                    ASSERT(this == queueState.GetVertice());
                    queue_state_tree.Remove(queueState.AccessEntry());
                }  

                // Queue of adjacent vertices 
                // (uses "Edge" for queue state)
                AdjacencyQueue  adjacency_queue;
                
                // These members are for use by traversals and 
                // other queue manipulations as needed
                ProtoTree       queue_state_tree;
              
        };  // end class ProtoGraph::Vertice
                
        /**
         * @class ProtoGraph::VerticeIterator
         */
        class VerticeIterator : public Vertice::SortedList::Iterator
        {
            public:
                VerticeIterator(ProtoGraph& theGraph);
                virtual ~VerticeIterator();

                // Note: Inherits the following
                // void Reset();
                // Vertice* GetNextVertice();
                
        };  // end class ProtoGraph::VerticeIterator
        friend class VerticeIterator;
        
        /**
         * @class ProtoGraph::SimpleTraversal
         *
         * @brief (TBD) We may want to make a separate "Traversal" base class
         * that SimpleTraversal (and others) derive from so that a
         * ProtoGraph can inform associated Traversals if the 
         * graph state changes ???
         */
        class SimpleTraversal
        {
            public:
                SimpleTraversal(const ProtoGraph& theGraph, 
                                Vertice&          startVertice,
                                bool              depthFirst = false);
                virtual ~SimpleTraversal();
                bool Reset();
                Vertice* GetNextVertice(unsigned int* level = NULL);
                
                // Override this method to filter which edges are included in traversal
                // (return "false" to disallow specific edges)
                // (Note that "edge->GetDst()" can be used to get the dst vertice)
                virtual bool AllowEdge(const Vertice& srcVertice, const Edge& edge)
                    {return true;}

            protected:
                // Members
                const ProtoGraph&             graph;
                Vertice&                      start_vertice;
                bool                          depth_first;    // false == breadth-first search
                unsigned int                  current_level;
                Vertice*                      trans_vertice;  // level transition marker
                Vertice::SimpleList           queue_pending;      
                Vertice::SimpleList           queue_visited;      
                
                Vertice::SimpleList::ItemPool item_pool;
                
        };  // end class ProtoGraph::SimpleTraversal

    protected:
        // Subclasses may wish to override this if
        // they use different Edge subclass variants
        virtual Edge* CreateEdge() const
            {return new Edge;}
        // "GetEdge()" tries to get an edge from edge_pool else calls "CreateEdge()"
        Edge* GetEdge();
        void PutEdge(Edge& edge)
            {edge_pool.Put(edge);}
        
        // These let ProtoGraph subclasses control Connect() a little more specifically if desired
        void Connect(Vertice& src, Vertice& dst, Edge& edge)
            {src.Connect(dst, edge);}
        
        void RemoveEdge(Vertice& src, Vertice& dst, Edge& edge)
            {src.RemoveEdge(dst, edge, &edge_pool);}
        
        void SuspendEdge(Vertice& src, Vertice& dst, Edge& edge)
            {src.SuspendEdge(dst, edge);}
        
        // Member variables
        Vertice::SortedList             vertice_list;
    
        // Pools of vertice items and edges
        Vertice::SortedList::ItemPool   vertice_list_item_pool; 
        EdgePool                        edge_pool;
        
};  // end class ProtoGraph

#endif // _PROTO_GRAPH
