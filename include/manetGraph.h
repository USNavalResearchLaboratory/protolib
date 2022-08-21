#ifndef _MANET_GRAPH
#define _MANET_GRAPH

#include "protoGraph.h"
#include "protoAddress.h"

/**
* @class NetGraph
*
* @brief This "NetGraph" stuff (incl. its embedded classes) are base classes
* that will be used, along with some help from a template defined afterward,
* for maintaining state, computing Dijkstra or other traversals, for
* Mobile Ad-hoc Network (MANET) routing, etc.
*
* (It is also useful for any routing, but the motivation for this is to 
* support MANET R&D we are conducting.  The "NetGraph" contains a set of 
* "NetGraph::Interface" instances that may be connected with some associated 
* "Cost" with "NetGraph::Link" instances (NetGraph::Link's are uni-directional,
* but convenience methods for bi-directional connectivity are provided).
* Additionally, a "ManetNode" class is provide to associate a set of
* interfaces together, but these associate interfaces are only considered
* "connected" if explicitly done.
*/

// TBD - it may make sense to rename "NetGraph" defined here to "NetGraphBase" and
//       keep the name "NetGraph" free or just use it for our example
//       template usage instead of ManetGraph and actually rename this
//       entire ".h" as "netGraph.h" instead of "manetGraph.h" and then
//       "ManetGraph" would be available as a name.  
//
//       This would makes sense since "NetGraph" here essentially just adds
//       the notion of "cost" to graph edges and provides for some
//       cost-based traversals (e.g. Dijkstra).  It also defines "Interfaces"
//       as vertices that are identified by a ProtoAddress. Then the derived
//       Protolib "ManetGraph" could add ProtoTimer and other elements to
//       to links (edges) and interfaces (vertices) that are more applicable 
//       for the dynamic wireless environment that MANET strives to support.


class NetGraph : public ProtoGraph
{
    public:
        virtual ~NetGraph();
        
        /**
		* @class NetGraph::Cost
		*
		* @brief Base class for variants of "Cost" we can use for
        * various graph traversals (e.g. Dyjkstra), etc
        */
	    class Cost
        {
            public:
                virtual ~Cost();
                
                // Required overrides
                virtual const char* GetCostKey() const = 0;
                virtual unsigned int GetCostKeysize() const = 0;  // in bits
                
                // These methods help ProtoSortedTree when comparing keys (i.e. cost value)
                // (Note the ProtoSortedTree does a modified lexical sort based on these)
                virtual bool GetCostKeySigned() const = 0;
                virtual bool GetCostKeyComplement2() const = 0;
                virtual ProtoTree::Endian GetCostKeyEndian() const = 0;
                
                // Copy operator 
                virtual Cost& operator=(const Cost& cost) = 0;
                
                // Value control
                virtual void Minimize() = 0;
                
                // Addition operator
                virtual void operator+=(const Cost& cost) = 0;
                
                // Cost comparison methods
                virtual bool operator>(const Cost& cost) const = 0;
                
                virtual bool operator==(const Cost& cost) const = 0;
                
                bool operator!=(const Cost& cost) const
                    {return (!(cost == *this));}
                
                bool operator>=(const Cost& cost) const
                    {return ((*this > cost) || (*this == cost));}
            
                bool operator<(const Cost& cost) const
                    {return ((*this >= cost) ? false : true);}
                
                bool operator<=(const Cost& cost) const
                    {return ((*this > cost) ? false : true);}
                
            protected:
                Cost();
                
        };  // end class NetGraph::Cost
                   
        /**
		* @class NetGraph::SimpleCostTemplate
		*
		* @brief This wraps  "double" floating point cost value.  We assume
        * IEEE754 format for the "value" member (I do have code for 
        * converting native machine floating point values to/from 
        * IEEE754 if needed) 
        *
		* (TBD) Make "SimpleCost" a template class based upon a built-in type?
        *       (e.g., double, int, etc)
        */
		template <class SIMPLE_TYPE>
        class SimpleCostTemplate : public Cost
        {     
            public:
                SimpleCostTemplate() : value(0) {}
                SimpleCostTemplate(SIMPLE_TYPE theValue) : value(theValue) {}
                virtual ~SimpleCostTemplate() {}
                
                void SetValue(SIMPLE_TYPE theValue)
                    {value = theValue;}
                
                SIMPLE_TYPE GetValue() const
                    {return value;}
                
                operator SIMPLE_TYPE() const
                    {return value;}
                
                SimpleCostTemplate& operator=(SIMPLE_TYPE theValue)
                {
                    value = theValue;
                    return *this;
                }
                
                SimpleCostTemplate& operator=(const SimpleCostTemplate& cost)
                {
                    value = cost.value;
                    return *this;
                }
                            
                // Required overrides of pure virtual methods
                // (These are used to help sorting by value)
                const char* GetCostKey() const
                    {return ((char*)&value);}
                unsigned int GetCostKeysize() const
                    {return (sizeof(SIMPLE_TYPE) << 3);}
                virtual bool GetCostKeySigned() const
                    {return true;}
                virtual bool GetCostKeyComplement2() const
                    {return false;}
                virtual ProtoTree::Endian GetCostKeyEndian() const
                    {return ProtoTree::GetNativeEndian();}
                
                Cost& operator=(const Cost& cost) 
                {
                    ASSERT(NULL != dynamic_cast<const SimpleCostTemplate*>(&cost));
                    value = static_cast<const SimpleCostTemplate&>(cost).value;
                    return *this;
                }
                
                void Minimize()
                    { value = 0;}
                
                bool operator==(const Cost& cost) const 
                {
                    ASSERT(NULL != dynamic_cast<const SimpleCostTemplate*>(&cost));
                    return (value == static_cast<const SimpleCostTemplate&>(cost).value);
                }
                
                bool operator>(const Cost& cost) const 
                {
                    ASSERT(NULL != dynamic_cast<const SimpleCostTemplate*>(&cost));
                    return (value > static_cast<const SimpleCostTemplate&>(cost).value);
                }
                                
                void operator+=(const Cost& cost)
                {
                    ASSERT(NULL != dynamic_cast<const SimpleCostTemplate*>(&cost));
                    value += static_cast<const SimpleCostTemplate&>(cost).value;
                }
                
            protected:
                SIMPLE_TYPE value;  
            
        };  // end class NetGraph::SimpleCostTemplate
        
        /**
		* @class NetGraph::SimpleCostDouble
		*
		* @brief This SimpleCost variant holds a "double" floating point cost value.  
        * We assume an IEEE754 format for the "value" member (I do have 
        * code for converting native machine floating point values to/from 
        * IEEE754 if needed)
		*/
        class SimpleCostDouble : public SimpleCostTemplate<double>
        {
            public:
                SimpleCostDouble() {}
                SimpleCostDouble(double theValue) : SimpleCostTemplate<double>(theValue) {}
        };  // end class NetGraph::SimpleCostDouble
        
        class SimpleCostUINT32 : public SimpleCostTemplate<UINT32>
        {
            public:
                SimpleCostUINT32() {}
                SimpleCostUINT32(UINT32 theValue) : SimpleCostTemplate<UINT32>(theValue) {}
                // Don't use the sign bit since it's "unsigned"
                bool GetCostKeySigned() const
                    {return false;}
        };  // end class NetGraph::SimpleCostUINT32
        
        class SimpleCostUINT8 : public SimpleCostTemplate<UINT8>
        {
            public:
                SimpleCostUINT8() {}
                SimpleCostUINT8(UINT8 theValue) : SimpleCostTemplate<UINT8>(theValue) {}
                // Don't use the sign bit since it's "unsigned"
                bool GetCostKeySigned() const
                    {return false;}
        };  // end class NetGraph::SimpleCostUINT8
        
        class Interface;
        class Link;
        
        // TBD - Should the AdjacentyIterator be declared inside the Interface declaration?
        // If we did this, the templated version could return the correct types
        class AdjacencyIterator : public ProtoGraph::AdjacencyIterator
        {
            public:
                AdjacencyIterator(Interface& iface);
                virtual ~AdjacencyIterator();

                Interface* GetNextAdjacency()
                    {return static_cast<Interface*>(ProtoGraph::AdjacencyIterator::GetNextAdjacency());}
                
                Link* GetNextAdjacencyLink()
                    {return static_cast<Link*>(ProtoGraph::AdjacencyIterator::GetNextAdjacencyEdge());}
                
                Interface* GetNextConnector()
                    {return static_cast<Interface*>(ProtoGraph::AdjacencyIterator::GetNextConnector());}

        };  // end class NetGraph::AdjacencyIterator

	    /**
	    * @class NetGraph::Link
	    *
	    * @brief adds "cost" attribute to ProtoGraph::Edge 
	    */
        class Link : public Edge
        {
            public:
                virtual ~Link();
                
                // Our template below will override this for us.
                virtual const Cost& GetCost() const = 0;
                
                void SetCost(const Cost& cost);
                
                Interface* GetSrc() const
                    {return static_cast<Interface*>(Edge::GetSrc());}
                
                Interface* GetDst() const
                    {return static_cast<Interface*>(Edge::GetDst());}
                
                // Required overrides
                virtual const char* GetKey() const
                    {return GetCost().GetCostKey();}
                virtual unsigned int GetKeysize() const
                    {return (GetCost().GetCostKeysize());}
                virtual ProtoTree::Endian GetEndian() const
                    {return GetCost().GetCostKeyEndian();}
                virtual bool UseSignBit() const
                    {return GetCost().GetCostKeySigned();}
                virtual bool UseComplement2() const
                    {return GetCost().GetCostKeyComplement2();}
                
            protected:
                 Link();
                // Our template below will override this for us.
                virtual Cost& AccessCost()  = 0;
                
        };  // end class NetGraph::Link

        class Node;  // a "Node" is a "super-vertice" in a NetGraph while an
                     // "Interface" is an actual vertice.  The "Interfaces"
                     // associated with a "Node" may or may not be connected.
        /**
	    * @class NetGraph::Interface
	    *
	    * @brief adds "address" identifier to ProtoGraph::Vertice and also
        * ties "Interface" to NetGraph::Node 
        * Note the "ProtoSortedTree::Item()" aspect is for its inclusion its
        * "node" iface_list only
	    */
        class Interface : public ProtoGraph::Vertice, public ProtoSortedTree::Item
        {
            friend class NetGraph;
            public:
                Interface(Node& theNode, const ProtoAddress& addr);
                Interface(Node& theNode);
                virtual ~Interface();    
                
                const ProtoAddress& GetAddress() const 
                    {return default_addr_item.GetAddress();}
                const ProtoAddress& GetAnyAddress() const
                    {return GetAddress();}
                ProtoAddressList& GetAddressList()
                    {return addr_list;}
                
                bool SetName(const char* theName);
                const char* GetName() const
                    {return name_ptr;}
                void ClearName(); // nullifies name
                
                // Use to add additional "extra" addresses
                // Note: Any graphs currently containing this interface get be updated as well 
                bool AddAddress(const ProtoAddress& theAddress);
                bool RemoveAddress(const ProtoAddress& theAddress);
                
                bool Contains(const ProtoAddress& theAddress) const
                    {return addr_list.Contains(theAddress);}
                
                const ProtoAddress& GetDefaultAddress() const
                    {return default_addr_item.GetAddress();}
                
                //void SetAddress(const ProtoAddress& theAddress)
                //    {address = theAddress;}

                Node& GetNode() const
                    {return *node;}
                
                const Node* GetNodePtr() const
                    {return node;}

                bool ChangeNode(Node& theNode);
                 
                bool HasLinkTo(const Interface& dstIface) const
                    {return ProtoGraph::Vertice::HasEdgeTo(dstIface);}
                
                Link* GetLinkTo(const Interface& dstIface) const
                    {return static_cast<Link*>(ProtoGraph::Vertice::GetEdgeTo(dstIface));}
                
                // This maintains a simple, unsorted (non-indexed) list of Interfaces
                class SimpleList : public Vertice::SimpleList
                {
                    public:
                        SimpleList(ItemPool* itemPool = NULL);
                        virtual ~SimpleList();
                        
                        Interface* GetHead() const
                            {return static_cast<Interface*>(Vertice::SimpleList::GetHead());}
                        
                        Interface* RemoveHead()
                            {return static_cast<Interface*>(Vertice::SimpleList::RemoveHead());}
                        
                        class Iterator : public Vertice::SimpleList::Iterator
                        {
                            public:
                                Iterator(const SimpleList& theList);
                                virtual ~Iterator();

                                Interface* GetNextInterface()
                                    {return static_cast<Interface*>(GetNextVertice());}

                        };  // end class NetGraph::Interface::SimpleList::Iterator
                        
                };  // end class NetGraph::Interface::SimpleList
                
                // This maintains a list of Interfaces, sorted by their "key", etc
                class SortedList : public Vertice::SortedList
                {
                    public:
                        SortedList(ItemPool* itemPool = NULL);
                        virtual ~SortedList();
                        
                        Interface* FindInterface(const ProtoAddress& addr) const
                            {return static_cast<Interface*>(FindVertice(addr.GetRawHostAddress(), addr.GetLength() << 3));}
                        
                        Interface* GetHead() const
                            {return static_cast<Interface*>(Vertice::SortedList::GetHead());}
                        
                        Interface* RemoveHead()
                            {return static_cast<Interface*>(Vertice::SortedList::RemoveHead());}
                        
                        class Iterator : public Vertice::SortedList::Iterator
                        {
                            public:
                                Iterator(SortedList& theList);
                                virtual ~Iterator();

                                Interface* GetNextInterface()
                                    {return static_cast<Interface*>(GetNextVertice());}

                        };  // end class NetGraph::Interface::SortedList::Iterator
                        
                };  // end class NetGraph::Interface::SortedList
                
                // PriorityQueue class to use for traversal purposes
                // This sorts by "Cost" associated with the interface
                class PriorityQueue : public Vertice::SortedList
                {
                    public:
                        class ItemFactory;
                    
                        PriorityQueue(ItemFactory& itemFactory);
                        virtual ~PriorityQueue();
                        
                        bool Insert(Interface& iface, const Cost& cost);
                        
                        void Remove(Interface& iface)
                            {SortedList::Remove(iface);}
                        
                        Interface* RemoveHead()
                            {return static_cast<Interface*>(SortedList::RemoveHead());}
                        
                        bool IsEmpty() const
                            {return iface_list.IsEmpty();}
                        
                        Interface* GetHead() const
                            {return static_cast<Interface*>(SortedList::GetHead());}
                        
                        const Cost* GetCost(const Interface& iface) const
                        {
                            Item* item = static_cast<Item*>(GetQueueState(iface));
                            return ((NULL != item) ? &(item->GetCost()) : NULL);
                        }
                        
                        // Move vertice from this PriorityQueue to another
                        void TransferInterface(Interface& iface, PriorityQueue& dstQueue)
                            {SortedList::TransferVertice(iface, dstQueue);}
                        
                        void TransferItem(Item& item, PriorityQueue& dstQueue)
                            {SortedList::TransferItem(item, dstQueue);}
                        
                        // These are used to query enqueued interfaces post-Dijkstra
                        Interface* GetNextHop(const Interface& iface) const
                        {
                            Item* item = static_cast<Item*>(GetQueueState(iface));  
                            return ((NULL != item) ? item->GetNextHop() : NULL);
                        }
                        Link* GetNextHopLink(const Interface& iface) const
                        {
                            Item* item = static_cast<Item*>(GetQueueState(iface));  
                            return ((NULL != item) ? item->GetNextHopLink() : NULL);
                        }
                        Interface* GetPrevHop(const Interface& iface) const
                        {
                            Item* item = static_cast<Item*>(GetQueueState(iface));  
                            return ((NULL != item) ? item->GetPrevHop() : NULL);
                        }
                        void SetRouteInfo(Interface& iface, Link* nextHopLink, Interface* prevHop)
                        {
                            Item* item = static_cast<Item*>(GetQueueState(iface));  
                            ASSERT(NULL != item);
                            item->SetNextHopLink(nextHopLink);
                            item->SetPrevHop(prevHop);
                        }
                        
                        void Adjust(Interface& iface, const Cost& newCost);
                        bool AdjustDownward(Interface& iface, const Cost& newCost,const Interface* newPrevHop = NULL);
                        bool AdjustUpward(Interface& iface, const Cost& newCost);
                        
                        // Note use of this method does not maintain priority queue sorting order!
                        bool Append(Interface& iface);  // used for Dijkstra "tree walking" only
                        
                        class Item : public SortedList::Item
                        {
                            public:
                                virtual ~Item();
                                
                                // Our subclass templates will override these
                                virtual const Cost& GetCost() const = 0;
                                virtual void SetCost(const Cost& cost) = 0;
                                
                                // "ProtoGraph::Vertice::SortedList::Item" overrides
                                // (We sort our PriorityQueue items by "cost")
                                const char* GetKey() const
                                    {return GetCost().GetCostKey();}
                                unsigned int GetKeysize() const
                                    {return GetCost().GetCostKeysize();}
                                virtual ProtoTree::Endian GetEndian() const
                                    {return GetCost().GetCostKeyEndian();}
                                virtual bool UseSignBit() const
                                    {return GetCost().GetCostKeySigned();}
                                virtual bool UseComplement2() const
                                    {return GetCost().GetCostKeyComplement2();}
                                
                                Interface* GetInterface() const
                                    {return static_cast<Interface*>(GetVertice());}
                                
                                // These are useful post-Djikstra completion
                                void SetPrevHop(Interface* prevHop)
                                    {prev_hop = prevHop;}
                                Interface* GetPrevHop() const
                                    {return prev_hop;}

                                void SetNextHopLink(Link* nextHopLink) 
                                    {next_hop_link = nextHopLink;}
                                Link* GetNextHopLink() const 
                                    {return next_hop_link;}
                                Interface* GetNextHop() const
                                    {return ((NULL != next_hop_link) ? next_hop_link->GetDst() : NULL);}

                            protected:
                                Item();
                                // Our subclass templates will override this
                                virtual Cost& AccessCost() = 0;
                                       
                            private:
                                Interface*      prev_hop;       // reverse path towards srcIFace
                                Link*           next_hop_link;  // forward path from srcIface to here   
                                
                        };  // end class NetGraph::Interface::PriorityQueue::Item
                        
                        class ItemFactory
                        {
                            public:
                                virtual ~ItemFactory();
                            
                                void Destroy()
                                    {item_pool.Destroy();}

                                Item* GetItem();

                                void PutItem(Item& item)
                                    {item_pool.PutItem(item);}
                            
                            protected:
                                ItemFactory();
                                virtual Item* CreateItem() const = 0;
                                
                                // Member variables
                                SortedList::ItemPool    item_pool;
                                
                        };  // end class NetGraph::Interface::PriorityQueue::ItemFactory
                        
                        class Iterator : public SortedList::Iterator
                        {
                            public:
                                Iterator(PriorityQueue& priorityQueue);
                                virtual ~Iterator();
                                
                                Interface* GetNextInterface() 
                                    {return static_cast<Interface*>(SortedList::Iterator::GetNextVertice());}
                                
                                Item* GetNextItem() 
                                    {return static_cast<Item*>(SortedList::Iterator::GetNextItem());}
                                
                        };  // end  NetGraph::Interface::PriorityQueue::Iterator
                        
                    protected:
                        // Member variables
                        ProtoSortedTree iface_list;  // sorted by "Cost" value
                        ItemFactory&    item_factory;
                        
                    private:
                        using Vertice::SortedList::Remove;   // gets rid of hidden overloaded virtual function warning
                    
                };  // end NetGraph::Interface::PriorityQueue
            
            protected:
                // Overrides for interfaces with "key" based on address
                // ProtoGraph::Vertice parent class overrides
                // Subclasses _may_ want to override these methods
                // (These are used for default sorting of
                //  of the Vertice::SortedList if used)
                // _BUT_ if these are overridden, the derived
                // class destructor _MUST_ call Vertice::Cleanup() so
                // that these _are_ not indirectly called in the Vertice
                // destructor!!!
                
                // These affect the Interface's sorting criteria
                // in the ProtoGraph::vertice_list and any Interface::SortedList
                // membership.
                virtual const char* GetVerticeKey() const;
                virtual unsigned int GetVerticeKeysize() const;
                
            private:
                // ProtoSortedTree::Item parent class overrides
                virtual const char* GetKey() const
                    {return GetVerticeKey();}
                virtual unsigned int GetKeysize() const
                    {return GetVerticeKeysize();}
                
                Node*                   node;
                NetGraph*               graph;
                ProtoAddressList        addr_list;
                ProtoAddressList::Item  default_addr_item;
                char*                   name_ptr; 
        };  // end class NetGraph::Interface
        
        // The "Node" class associates multiple interfaces together even if they
        // are not connected in a ManetGraph.  A "Node" reference is required in 
        // the "Interface" constructor.  Note that when a "Node" is deleted, it
        // deletes any Interfaces contained in its "iface_list"
        class Node
        {
            public:
                Node();
                virtual ~Node();

                void Consume(Node& foodNode);

                Interface* FindInterface(const ProtoAddress& addr) const;
//                    {return static_cast<Interface*>(iface_list.Find(addr.GetRawHostAddress(), addr.GetLength() << 3));}
//                    {return reinterpret_cast<Interface*>((void*)addr_list.GetUserData(addr));}              
                Interface* FindInterfaceByName(const char* theName);

                Interface* FindInterfaceByString(const char* theString); //looks for it as both a name and address

                bool AddInterface(Interface& iface, bool makeDefault = false);
                 
                void RemoveInterface(Interface& iface);    
                
                // Note "AppendInterface() should be deprecated
                bool AppendInterface(Interface& iface, bool makeDefault = false)
                    {return AddInterface(iface, makeDefault);}   
                
                bool Contains(const Interface& iface) const
                    {return (iface.GetNodePtr() == this);}
                
                Interface* GetAnyInterface() const
                    {return static_cast<Interface*>(iface_list.GetRoot());}
                Interface* GetDefaultInterface() const
                    {return static_cast<Interface*>(default_interface_ptr);}
                
                bool IsSymmetricNeighbor(Node& node);
                
                class InterfaceIterator : public ProtoSortedTree::Iterator
                {
                    public:
                        InterfaceIterator(Node& theNode);
                        virtual ~InterfaceIterator();
                        
                        bool HasEmptyList()
                            {return HasEmptyTree();}

                        void Reset()
                            {ProtoSortedTree::Iterator::Reset();}

                        Interface* GetNextInterface()
                            {return static_cast<Interface*>(GetNextItem());}
                    
                };  // end class NetGraph::Node::InterfaceIterator
                
                friend class InterfaceIterator;
                friend class NetGraph::Interface;
                
                // Iterate over all _neighboring_ interfaces
                class NeighborIterator
                {
                    public:
                        NeighborIterator(Node& theNode);
                        virtual ~NeighborIterator();
                        
                        void Reset();
                        
                        Interface* GetNextNeighborInterface();
                        
                        Link* GetNextNeighborLink();
                        
                    private:
                        InterfaceIterator   iface_iterator;
                        AdjacencyIterator   adj_iterator;
                };  // end class NetGraph::Node::NeighborIterator

            protected:    
                    
                ProtoSortedTree         iface_list;
                ProtoAddressList        extra_addr_list;  // contains list of ifaces' "extra" addresses

            private:
                bool AddExtraInterfaceAddress(const Interface& iface,const ProtoAddress& addr)
                    {return extra_addr_list.Insert(addr, &iface);}
                void RemoveExtraInterfaceAddress(const ProtoAddress& addr)
                    {extra_addr_list.Remove(addr);}
                bool SetDefaultInterface(Interface& iface);
                
                Interface* default_interface_ptr;
        };  // end class NetGraph::Node
                
        // NetGraph control/query methods
        bool InsertNode(Node& node, Interface* iface = NULL); //this function will be removed in upcoming releases
        bool InsertInterface(Interface& iface); // this function should be used instead of InsertNode()
        void RemoveNode(Node& node, Interface* iface = NULL);
        void RemoveInterface(Interface& iface);  // This removes the interface _and_ its links

        Interface* FindInterface(const ProtoAddress& addr) const
            {return ((Interface*)addr_list.GetUserData(addr));}
        
        Node* FindNode(const ProtoAddress& theAddress)
        {
            Interface* iface = FindInterface(theAddress);
            return ((NULL != iface) ? &iface->GetNode() : NULL);
        }
        
        Interface* FindInterfaceByName(const char *theName); 
       
        Interface* FindInterfaceByString(const char *theString); // finds the interface both by name and address
 
        Node* FindNodeByName(const char *theName) 
        {
            Interface* iface = FindInterfaceByName(theName);
            return ((NULL != iface) ? &iface->GetNode() : NULL);
        }
     
        Node* FindNodeByString(const char* theString)
        {
            Interface* iface = FindInterfaceByString(theString);
            return ((NULL != iface) ? &iface->GetNode() : NULL);
        }

        class InterfaceIterator  : public ProtoGraph::VerticeIterator
        {
            public:
                InterfaceIterator(NetGraph& theGraph);
                virtual ~InterfaceIterator();

                Interface* GetNextInterface()
                    {return static_cast<Interface*>(GetNextVertice());}
                
        };  // end class NetGraph::InterfaceIterator
        
        
        class SimpleTraversal : protected ProtoGraph::SimpleTraversal
        {
            public:
                SimpleTraversal(const NetGraph&  theGraph, 
                                Interface&       startIface,
                                bool             traverseNodes = true,
                                bool             collapseNodes = true,
                                bool             depthFirst = false);
            
                virtual ~SimpleTraversal();
                
                bool Reset()
                    {return Reset(false);}
                
                Interface* GetNextInterface(unsigned int* level = NULL);
           
            protected:     
                // Override this method to filter which edges are included in traversal
                // (return "false" to disallow specific edges)
                // Note "link" will be NULL is src/dst are on same node
                virtual bool AllowLink(const Interface& srcIface, const Interface& dstIface, Link* link)
                    {return true;}
            
                bool Reset(bool constructor);  // arg for internal-use only
                
                bool    traverse_nodes;  // traverse all node interfaces
                bool    collapse_nodes;  // treat node co-interfaces as a common vertice
                
        };  // end class NetGraph::SimpleTraversal
        
        class DijkstraTraversal : public Interface::PriorityQueue::ItemFactory
        {
            public:
                virtual ~DijkstraTraversal();
                
                // Set to false by default;  If set to true the traversal will iterate over nodes as well as interfaces.
                void TraverseNodes(bool traverse);
                
                bool Reset(Interface* startIface = NULL);
                
                Interface* GetNextInterface();
                
                bool PrevHopIsValid(Interface& currentIface);
                
                void Update(Interface& startIface);
                
                void Update(Interface& ifaceA, Interface& ifaceB);
                
                // Override this method to filter which edges are included in traversal
                // (return "false" to disallow specific links)
                virtual bool AllowLink(const Interface& srcIface, const Link& link)
                    {return true;}
                
                // (use these post-Dijkstra (after "GetNextInterface" returns NULL)
                Interface* GetNextHop(const Interface& dstIface)   // from "startIface" towards "dstIface"
                    {return (queue_visited.GetNextHop(dstIface));}
                
                Interface* GetPrevHop(const Interface& dstIface)   // back towards "startIface"
                    {return (queue_visited.GetPrevHop(dstIface));}
                    
                const Cost* GetCost(const Interface& dstIface) const
                    {return (queue_visited.GetCost(dstIface));}
                
                // BFS traversal of routing tree ("tree walk")
                bool TreeWalkReset();
                Interface* TreeWalkNext(unsigned int* level = NULL);
                
            protected:
                DijkstraTraversal(NetGraph&    theGraph,   
                                  Interface*   startIface);
            
                DijkstraTraversal(NetGraph&     theGraph, 
                                  Node&         startNode,
                                  Interface*    startIface = NULL);
            
                // Note:  The template subclass provides the "ItemFactory::CreateItem()" method
                
                // Our templates below override this one
                virtual Cost& AccessCostTemp() = 0;
                    
                NetGraph&                   manet_graph;
                Interface*                  start_iface;
                Interface::PriorityQueue    queue_pending;
                Interface::PriorityQueue    queue_visited;
                
                // These two members support the "tree walk" BFS
                Interface*                  trans_iface;
                unsigned int                current_level;
                
                bool                        dijkstra_completed;
                bool                        in_update;
                bool                        traverse_nodes;
                bool                        reset_required;
        };  // end class NetGraph::DijkstraTraversal 
        
        
        // These Link and Interface Template definitions may be used by developers along
        // with the NetGraphTemplate definition below to build custom link/interface/graph types.
        template <class COST_TYPE, class IFACE_TYPE = Interface>
        class LinkTemplate : public Link
        {
            public:
                LinkTemplate() {}
                virtual ~LinkTemplate() {}
                
                // required override
                const COST_TYPE& GetCost() const
                    {return cost;}      
                
                void SetCost(COST_TYPE& theCost)
                    {cost = theCost;}
                
                IFACE_TYPE* GetSrc() const
                    {return static_cast<IFACE_TYPE*>(Edge::GetSrc());}
                
                IFACE_TYPE* GetDst() const
                    {return static_cast<IFACE_TYPE*>(Edge::GetDst());}
                
            private:
                virtual Cost& AccessCost()
                    {return static_cast<Cost&>(cost);}
                COST_TYPE   cost;
                
        };  // end class NetGraph::LinkTemplate
        
        
        template <class COST_TYPE, class MY_TYPE, class LINK_TYPE = Link, class NODE_TYPE = Node>
        class InterfaceTemplate : public Interface
        {
            public:
                InterfaceTemplate(NODE_TYPE& theNode, const ProtoAddress& addr) : Interface(theNode, addr) {}
                InterfaceTemplate(NODE_TYPE& theNode) : Interface(theNode) {}
                virtual ~InterfaceTemplate() {}
                
                LINK_TYPE* GetLinkTo(const InterfaceTemplate& dst) const
                    {return static_cast<LINK_TYPE*>(Interface::GetLinkTo(dst));}

                NODE_TYPE& GetNode() const
                    {return static_cast<NODE_TYPE&>(Interface::GetNode());}  
                
                const NODE_TYPE* GetNodePtr() const
                    {return static_cast<NODE_TYPE&>(Interface::GetNodePtr());}                   
                             
                const COST_TYPE* GetCostTo(const InterfaceTemplate& dst) const
                {
                    LINK_TYPE* link = GetLinkTo(dst);
                    return (NULL != link) ? static_cast<const COST_TYPE*>(&(link->GetCost())) : (COST_TYPE*)NULL;
                }
                
                static MY_TYPE* GetSrc(Link& theLink)
                    {return static_cast<MY_TYPE*>(theLink.GetSrc());}
                
                static MY_TYPE* GetDst(Link& theLink)
                    {return static_cast<MY_TYPE*>(theLink.GetDst());}
                
                class SimpleList : public NetGraph::Interface::SimpleList
                {
                    public:
                        SimpleList(ItemPool* itemPool = NULL) : NetGraph::Interface::SimpleList(itemPool) {}
                        virtual ~SimpleList() {}
                        
                        MY_TYPE* GetHead() const
                            {return static_cast<MY_TYPE*>(NetGraph::Interface::SimpleList::GetHead());}
                        
                        MY_TYPE* RemoveHead()
                            {return static_cast<MY_TYPE*>(NetGraph::Interface::SimpleList::RemoveHead());}
                        
                        class Iterator : public NetGraph::Interface::SimpleList::Iterator
                        {
                            public:
                                Iterator(const SimpleList& theList) : NetGraph::Interface::SimpleList::Iterator(theList) {}
                                virtual ~Iterator() {}

                                MY_TYPE* GetNextInterface()
                                    {return static_cast<MY_TYPE*>(NetGraph::Interface::SimpleList::Iterator::GetNextInterface());}

                        };  // end class NetGraph::InterfaceTemplate::SimpleList::Iterator
                        
                };  // end class NetGraph::InterfaceTemplate::SimpleList
                
                class SortedList : public NetGraph::Interface::SortedList
                {
                    public:
                        SortedList(ItemPool* itemPool = NULL) : NetGraph::Interface::SortedList(itemPool) {}
                        virtual ~SortedList() {}
                        
                        
                        MY_TYPE* FindInterface(const ProtoAddress& addr) const
                            {return static_cast<MY_TYPE*>(FindVertice(addr.GetRawHostAddress(), addr.GetLength() << 3));}

                        MY_TYPE* FindInterfaceByName(const char* name)
                            {return static_cast<MY_TYPE*>(FindVertice(name,(strlen(name) & 0x01) ? (strlen(name) << 3) : ((strlen(name)+1) << 3)));}

                        MY_TYPE* GetHead() const
                            {return static_cast<MY_TYPE*>(Vertice::SortedList::GetHead());}
                        
                        MY_TYPE* RemoveHead()
                            {return static_cast<MY_TYPE*>(Vertice::SortedList::RemoveHead());}
                        
                        class Iterator : public NetGraph::Interface::SortedList::Iterator
                        {
                            public:
                                Iterator(SortedList& theList) : NetGraph::Interface::SortedList::Iterator(theList) {}
                                virtual ~Iterator() {}

                                MY_TYPE* GetNextInterface()
                                    {return static_cast<MY_TYPE*>(NetGraph::Interface::SortedList::Iterator::GetNextInterface());}

                        };  // end class NetGraph::InterfaceTemplate::SortedList::Iterator
                        
                };  // end class NetGraph::InterfaceTemplate::SortedList
                
                class PriorityQueue : public NetGraph::Interface::PriorityQueue
                {
                    public:
                        PriorityQueue() : NetGraph::Interface::PriorityQueue(builtin_item_factory) {}
                        PriorityQueue(ItemFactory& itemFactory) : NetGraph::Interface::PriorityQueue(itemFactory) {}
                        virtual ~PriorityQueue() {}
                        
                        class Item : public NetGraph::Interface::PriorityQueue::Item
                        {
                            public:
                                Item() {}
                                virtual ~Item() {}

                                const Cost& GetCost() const
                                    {return static_cast<const Cost&>(cost);}
                                
                                void SetCost(const Cost& theCost)
                                    {cost = static_cast<const COST_TYPE&>(theCost);}

                            private:
                                virtual Cost& AccessCost()
                                    {return static_cast<Cost&>(cost);}
                                COST_TYPE cost;
                        };  // end class NetGraph::PriorityQueue::Item
                        
                        class ItemFactory : public NetGraph::Interface::PriorityQueue::ItemFactory
                        {
                            public:
                                ItemFactory() {}
                                virtual ~ItemFactory() {}
                            
                            protected:
                                NetGraph::Interface::PriorityQueue::Item* CreateItem() const
                                    {return static_cast<NetGraph::Interface::PriorityQueue:: Item*>(new Item);}  // creates new templated "Item"
                            
                        };  // end class NetGraph::InterfaceTemplate::PriorityQueue::ItemFactory()
                        
                    private:
                        ItemFactory builtin_item_factory;
                        
                };  // end class NetGraph::InterfaceTemplate::PriorityQueue
                
        };  // end class NetGraph::InterfaceTemplate
        
        template <class COST_TYPE, class NODE_TYPE = NetGraph::Node>
        class DefaultInterfaceTemplate : public InterfaceTemplate<COST_TYPE, NODE_TYPE, DefaultInterfaceTemplate<COST_TYPE> >
        {
            public:
                DefaultInterfaceTemplate(NODE_TYPE& theNode, const ProtoAddress& addr) 
                    : InterfaceTemplate<COST_TYPE, NODE_TYPE, DefaultInterfaceTemplate>(theNode, addr) {}
                virtual ~DefaultInterfaceTemplate() {}
        };  // end class NetGraph::DefaultInterfaceTemplate
        
        
        template <class IFACE_TYPE = Interface, class LINK_TYPE = Link>
        class NodeTemplate : public Node
        {
            public:
                NodeTemplate() {}
                virtual ~NodeTemplate() {}
                
                IFACE_TYPE* FindInterface(const ProtoAddress& addr) const
                    {return static_cast<IFACE_TYPE*>(NetGraph::Node::FindInterface(addr));}
                
                IFACE_TYPE* FindInterfaceByName(const char* theName)
                    {return static_cast<IFACE_TYPE*>(NetGraph::Node::FindInterfaceByName(theName));}

                IFACE_TYPE* GetDefaultInterface() const
                    {return static_cast<IFACE_TYPE*>(NetGraph::Node::GetDefaultInterface());}
                
                class InterfaceIterator : public NetGraph::Node::InterfaceIterator
                {
                    public:
                        InterfaceIterator(Node& node) : NetGraph::Node::InterfaceIterator(node) {}
                        virtual ~InterfaceIterator() {}
                        
                        IFACE_TYPE* GetNextInterface() 
                            {return static_cast<IFACE_TYPE*>(NetGraph::Node::InterfaceIterator::GetNextInterface());}
                        
                };  // end class NetGraphTemplate::Node::InterfaceIterator
                class NeighborIterator : public NetGraph::Node::NeighborIterator
                {
                    public:
                        NeighborIterator(Node& node) : NetGraph::Node::NeighborIterator(node) {}
                        virtual ~NeighborIterator() {}
                        
                        IFACE_TYPE* GetNextNeighborInterface()
                            {return static_cast<IFACE_TYPE*>(NetGraph::Node::NeighborIterator::GetNextNeighborInterface());}
                        
                        LINK_TYPE* GetNextNeighborLink()
                            {return static_cast<LINK_TYPE*>(NetGraph::Node::NeighborIterator::GetNextNeighborLink());}
                };  // end class NetGraph::NodeTemplate::NeighborIterator

        };  // end class NetGraph::NodeTemplate
        
        
    protected:
        NetGraph();

        // Netgraph::Interface calls these when adding or removing  addresses 
        bool AddInterfaceAddress(const Interface& iface, const ProtoAddress& addr)
            {return addr_list.Insert(addr, &iface);}
        void RemoveInterfaceAddress(const ProtoAddress& addr)
            {addr_list.Remove(addr);}
        void SuspendInterface(Interface& iface)
            {vertice_list.Remove(iface);}
        bool ResumeInterface(Interface& iface)
            {return vertice_list.Insert(iface);}   
    
        // Our template provides a type-safe "public" override of this
        Link* Connect(Interface& srcIface, Interface& dstIface, const Cost& cost);
        bool Connect(Interface& srcIface, Interface& dstIface, const Cost& cost, bool duplex);
        
        Link* Reconnect(Interface& srcIface, Interface& dstIface, const Cost& cost);
        bool Reconnect(Interface& srcIface, Interface& dstIface, const Cost& cost, bool duplex);
        
    private:
        ProtoAddressList addr_list; // list of _all_ addresses of all contained interfaces
     
};  // end class NetGraph


// The "NetGraph" stuff declared above are the base (interface) classes for
// actual subclasses we will derive and use.  The following provides a template
// class based on the "Cost" type.  This lets us easily derive different types
// of "Cost" subclasses and then derive a "NetGraph" subclass that uses that
// new "Cost" subclass for its Dijkstra, etc.  Cool!, huh?
//
// Note that further below we declare a "ManetGraph" subclass as an example
// that uses the "NetGraph::SimpleCost" that was declared above.

template <class COST_TYPE = NetGraph::SimpleCostDouble, 
          class IFACE_TYPE = NetGraph::DefaultInterfaceTemplate<COST_TYPE>, 
          class LINK_TYPE = NetGraph::LinkTemplate<COST_TYPE, IFACE_TYPE>, 
          class NODE_TYPE = NetGraph::NodeTemplate<IFACE_TYPE> >
class NetGraphTemplate : public NetGraph
{
    public:
        NetGraphTemplate() {}
        virtual ~NetGraphTemplate() {}
        
        // These typedefs let us use DerivedGraph::Interface and DerivedGraph::Link types that 
        // are synonomous with passed-in template interface/link types
        typedef COST_TYPE Cost;
        typedef IFACE_TYPE Interface;
        typedef LINK_TYPE Link;
        typedef NODE_TYPE Node;
        
        NODE_TYPE* FindNode(const ProtoAddress& theAddress)
            {return static_cast<NODE_TYPE*>(NetGraph::FindNode(theAddress));}
        
        IFACE_TYPE* FindInterface(const ProtoAddress& addr) const
            {return static_cast<IFACE_TYPE*>(NetGraph::FindInterface(addr));}
                
        IFACE_TYPE* FindInterfaceByName(const char* name)
            {return static_cast<IFACE_TYPE*>(NetGraph::FindInterfaceByName(name));}

        IFACE_TYPE* FindInterfaceByString(const char* theString)
            {return static_cast<IFACE_TYPE*>(NetGraph::FindInterfaceByString(theString));}
       
        // Note for the "SimpleTypeTemplate" and its derivatives for "double", "int", etc
        // have casting/conversion operators defined that allow direct use of the
        // corresponding "simple" type (i.e. "double", etc) for the "cost" argument here.
        LINK_TYPE* Connect(IFACE_TYPE& srcIface, IFACE_TYPE& dstIface, const COST_TYPE& cost)
            {return static_cast<LINK_TYPE*>(NetGraph::Connect(srcIface, dstIface, cost));}
        
        bool Connect(IFACE_TYPE& srcIface, IFACE_TYPE& dstIface, const COST_TYPE& cost, bool duplex)
            {return NetGraph::Connect(srcIface, dstIface, cost, duplex);}
        
        LINK_TYPE* Reconnect(Interface& srcIface, Interface& dstIface, const Cost& cost)
            {return static_cast<LINK_TYPE*>(NetGraph::Reconnect(srcIface, dstIface, cost));}
        
        bool Reconnect(IFACE_TYPE& srcIface, IFACE_TYPE& dstIface, const COST_TYPE& cost, bool duplex)
            {return NetGraph::Reconnect(srcIface, dstIface, cost, duplex);}
        
        // TBD - provide a GetLinkList() to get list of links from "srcIface" to "dstIface"
        LINK_TYPE* GetLink(IFACE_TYPE& srcIface, IFACE_TYPE& dstIface)
            {return static_cast<LINK_TYPE*>(srcIface.GetLinkTo(dstIface));}
        
        
        class AdjacencyIterator : public NetGraph::AdjacencyIterator
        {
            public:
                AdjacencyIterator(IFACE_TYPE& iface) : NetGraph::AdjacencyIterator(iface) {}
                virtual ~AdjacencyIterator() {}
                
                IFACE_TYPE* GetNextAdjacency()
                    {return static_cast<IFACE_TYPE*>(NetGraph::AdjacencyIterator::GetNextAdjacency());}
                
                LINK_TYPE* GetNextAdjacencyLink()
                    {return static_cast<LINK_TYPE*>(NetGraph::AdjacencyIterator::GetNextAdjacencyLink());}
                
                IFACE_TYPE* GetNextConnector()
                    {return static_cast<IFACE_TYPE*>(NetGraph::AdjacencyIterator::GetNextConnector());}
                
        };  // end class NetGraphTemplate::AdjacencyIterator
        
        class InterfaceIterator : public NetGraph::InterfaceIterator
        {
            public:
                InterfaceIterator(NetGraphTemplate& theGraph) : NetGraph::InterfaceIterator(theGraph) {}
                virtual ~InterfaceIterator() {}

                IFACE_TYPE* GetNextInterface()
                    {return static_cast<IFACE_TYPE*>(NetGraph::InterfaceIterator::GetNextVertice());}
                
        };  // end class NetGraphTemplate::InterfaceIterator
        
        class SimpleTraversal : public NetGraph::SimpleTraversal
        {
            public:
                SimpleTraversal(const NetGraphTemplate& theGraph, 
                                IFACE_TYPE&             startIface,
                                bool                    traverseNodes = true,
                                bool                    collapseNodes = true,
                                bool                    depthFirst = false)
                    : NetGraph::SimpleTraversal(theGraph, startIface, traverseNodes, collapseNodes, depthFirst) {}
                virtual ~SimpleTraversal() {}
                
                IFACE_TYPE* GetNextInterface(unsigned int* level = NULL)
                    {return static_cast<IFACE_TYPE*>(NetGraph::SimpleTraversal::GetNextInterface(level));}
                
        };  // end class NetGraphTemplate::SimpleTraversal
        
        class DijkstraTraversal : public NetGraph::DijkstraTraversal
        {
            public:
                DijkstraTraversal(NetGraphTemplate&   theGraph, 
                                  Node&               startNode,
                                  IFACE_TYPE*         startIface = NULL) 
                        : NetGraph::DijkstraTraversal(theGraph, startNode, startIface)
                {
                    Reset();
                }
                
                virtual ~DijkstraTraversal() {}
                
                IFACE_TYPE* GetNextInterface()
                    {return static_cast<IFACE_TYPE*>(NetGraph::DijkstraTraversal::GetNextInterface());}
                
                // (use these post-Dijkstra (i.e., after "GetNextInterface" returns NULL)
                const COST_TYPE* GetCost(const IFACE_TYPE& iface) const 
                    {return static_cast<const COST_TYPE*>(NetGraph::DijkstraTraversal::GetCost(iface));}
                
                IFACE_TYPE* GetNextHop(const IFACE_TYPE& dstIface)  // from "startIface" towards "dstIface"
                    {return static_cast<IFACE_TYPE*>(queue_visited.GetNextHop(dstIface));}
                
                IFACE_TYPE* GetPrevHop(const IFACE_TYPE& dstIface) // back towards "startIface"
                    {return static_cast<IFACE_TYPE*>(queue_visited.GetPrevHop(dstIface));}
                
                // BFS traversal of routing tree ("tree walk")
                IFACE_TYPE* TreeWalkNext(unsigned int* level = NULL)
                    {return static_cast<IFACE_TYPE*>(NetGraph::DijkstraTraversal::TreeWalkNext(level));}
                
            protected:
                Cost& AccessCostTemp() 
                    {return static_cast<Cost&>(cost_temp);}    
            
                // Required override of NetGraph::Interface::PriorityQueue::ItemFactory::CreateItem()
                NetGraph::Interface::PriorityQueue::Item* CreateItem() const 
                    {return static_cast<NetGraph::Interface::PriorityQueue::Item*>(new typename IFACE_TYPE::PriorityQueue::Item);}
                    
                COST_TYPE   cost_temp;
            
        };  // end class NetGraphTemplate::DijkstraTraversal
        
    protected:
        // Override of ProtoGraph::CreateEdge()
        virtual Edge* CreateEdge() const 
            {return static_cast<Edge*>(new LINK_TYPE);}
    
};  // end class NetGraphTemplate

// TBD - Rename the above "NetGraphTemplate" -> "NetGraphBase" and then declare 
// a new "NetGraphTemplate" here (using NetGraphBase) that allows users to
// provide their own Node (and Link?) derivatives as well as Cost type
// so that the proper return types are automatically provided from 
// Iterators, etc ... need to think about this a little ... i.e. the "Node"
// is not a problem, but the templating of the COST_TYPE member of Link 
// is problematic ... A trick might be to have Dijkstra Traversal keep
// a Link just for cost storage ... i.e. to contain the templating on
// the COST_TYPE contained in the Link??  more thought needed!


// And finally, here we use our "NetGraph" base class and "NetGraphTemplate"
// to generate an _example_ (but usable) "ManetGraph" class that uses the 
// "NetGraph::SimpleCostDouble" type as its "cost" metric for Dijkstra, etc
/**
* @class ManetGraph
*
* @brief Derived from NetGraphTemplate using "SimpleCostDouble" as its COST_TYPE.
* Hopefully a suitable graph structure for keeping and exploring multi-hop network state. 
* Supports a notion of multiple interfaces per node, etc.
*/


class ManetInterface;   // predeclared so it can be passed to templated ManetLink definition below
class ManetNode;        // predeclared so it can be passed to templated ManetInterface definition below

// Define a ManetLink type that uses the "SimpleCostDouble" as its cost metric
class ManetLink : public NetGraph::LinkTemplate<NetGraph::SimpleCostDouble, ManetInterface> {};

// Define a ManetInterface type to help "wire up" our example templated ManetGraph below
class ManetInterface : public NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, ManetInterface, ManetLink, ManetNode> 
{
    public:
        ManetInterface(ManetNode& theNode, const ProtoAddress& addr)
            : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, ManetInterface, ManetLink, ManetNode>(theNode, addr) {}
        ManetInterface(ManetNode& theNode)
            : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, ManetInterface, ManetLink, ManetNode>(theNode) {}
};  // end class ManetInterface

// Define a "ManetNode" type that has the ManetInterface thpye
class ManetNode : public NetGraph::NodeTemplate<ManetInterface> {};

// Finally, declare a ManetGraph from our NetGraphTemplate
// Note this also creates ManetGraph::Interface and ManetGraph::Link typedef 
// equivalents to ManetInterface/ManetLink
class ManetGraph : public NetGraphTemplate<NetGraph::SimpleCostDouble, ManetInterface, ManetLink, ManetNode> {};


// Note that the following equivalent types are "nested in the ManetGraph namespace via typedef
// ManetGraph::Node == ManetNode
// ManetGraph::Interface == ManetInterface
// ManetGraph::Link == ManetLink


// This simple definition gives you the same sort of "default" ManetGraph
// (the above steps illustrate how to create an opportunity for custom Interface and Link classes)
//  class ManetGraph : public NetGraphTemplate<> {};


//
// Also, this could have been similarly (and perhaps more elegantly) done with a namespace as follows:
//
// namespace Manet
// {
//      typedef NetGraph::SimpleCostDouble Cost;
//      class Interface;
//      class Node;
//      class Link : public NetGraph::LinkTemplate<Cost, Interface> {};
//      class Interface : public NetGraph::InterfaceTemplate<Cost, Node, Interface, Link> {};
//      class Graph : public NetGraphTemplate<Cost, Node, Interface, Link> {};
// };  // end namespace Manet
//
// This would result in Maent::Node, Manet::Interface, Manet::Link, and Manet::Graph classes
// (And equivalent Manet::Graph::Node, Manet::Graph::Interface and Manet::Graph::Link class names.
//  Other elements (e.g., enums, classed, etc) could be also added to the namespace as needed)

// To summarize, one can define their own custom "Cost", "Node", "Interface", and/or "Link" subclasses 
// and then use the template to define your own custom "Graph" class made up of these elements.
// Alternatively, one could subclass from "NetGraph" directly, but the templates provide the convenience
// of iterators, etc that return proper typenames (so arduous casting is not needed)


#endif // _MANET_GRAPH
