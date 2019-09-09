#include "manetGraph.h"
#include <protoDebug.h>
#include <new>

NetGraph::Cost::Cost()
{
}

NetGraph::Cost::~Cost()
{
}

NetGraph::Link::Link()
{
}

NetGraph::Link::~Link()
{
}

void NetGraph::Link::SetCost(const Cost& cost)
{
    AccessCost() = cost;
}  // end NetGraph::Link::SetCost()

NetGraph::Interface::Interface(Node& theNode, const ProtoAddress& addr)
 : node(&theNode),graph(NULL),default_addr_item(addr), name_ptr(NULL)
{
    addr_list.InsertItem(default_addr_item); 
}

NetGraph::Interface::Interface(Node& theNode)
 : node(&theNode), graph(NULL), default_addr_item(PROTO_ADDR_NONE), name_ptr(NULL)
{
}

NetGraph::Interface::~Interface()
{
    if(graph != NULL) graph->RemoveInterface(*this);
    Cleanup();  // Note "Cleanup()" would also remove it from "graph"
    if (!addr_list.IsEmpty())
    {
        node->RemoveInterface(*this);
        addr_list.RemoveItem(default_addr_item);
        // Note "addr_list" destructor destroys any remaining items in list
    }
    else if (NULL != name_ptr)
    {
        node->RemoveInterface(*this);
    }
    if (NULL != name_ptr) delete[] name_ptr;
}

bool NetGraph::Interface::SetName(const char* theName)
{
    int length = strlen(theName);
    char* namePtr = new char[length+1];
    namePtr[length] = '\0';
    bool reattach = false;//we will reattach iff a name already existed
    if (NULL == namePtr)
    {
        PLOG(PL_ERROR,"NetGraph::Interface::SetName(): new name_ptr error: %s\n", GetErrorString());
        return false;
    }
    if (NULL != name_ptr) 
    {
        if (addr_list.IsEmpty())
        {
            reattach = true;
            // Is using name as ProtoGraph::vertice_list index and Node::iface_list index
            node->RemoveInterface(*this);       // will reattach below
            if (NULL != graph)
                graph->SuspendInterface(*this); // will "resume" below
        }
        delete[] name_ptr;  // delete old "name_ptr"
    }
    name_ptr = namePtr;
    strcpy(name_ptr, theName);
    if (addr_list.IsEmpty() && reattach)
    {
        // Now that we have an "indice" for this interface, it can be fully
        // associated with its "graph" (if applicable) and "node"
        if (NULL != graph)  // if no addr, graph is only non-NULL if there was prior name
        {
            if (!graph->ResumeInterface(*this))
            {
                PLOG(PL_ERROR,"NetGraph::Interface::SetName() error: unable to reattach interface to graph: %s\n", GetErrorString());
                graph->RemoveInterface(*this);
                return false;
            }
        }
        if (!node->AddInterface(*this))
        {
            PLOG(PL_ERROR,"NetGraph::Interface::SetName() error: unable to add interface to node: %s\n", GetErrorString());
            delete[] name_ptr;
            name_ptr = NULL;
            return false;
        }
    }    
    return true;
}  // end NetGraph::Interface::SetName()

void NetGraph::Interface::ClearName()
{
    if (NULL != name_ptr)
    {
        if (addr_list.IsEmpty())
        {
            node->RemoveInterface(*this);
            if (NULL != graph) graph->RemoveInterface(*this); // all links are lost
        }
        delete[] name_ptr;
        name_ptr = NULL;
    }
}  // NetGraph::Interface::ClearName()

// TBD - add a "makeDefault" optional argument to make the new address the default?
bool NetGraph::Interface::AddAddress(const ProtoAddress& theAddress)
{
    // Add the new address
    if (addr_list.IsEmpty())
    {
        if (NULL != name_ptr)
        {
            // We're adding an address to a "name-only" interface,
            // so we need to remove/reinsert this iface from the Node & Graph   
            // so that is properly indexed. 
            node->RemoveInterface(*this);
            if (NULL != graph)
                graph->SuspendInterface(*this); // temporarily remove from ProtoGraph::vertice_list
        }
        default_addr_item.SetAddress(theAddress);
        if (NULL != graph) // note that "graph" will be non-null iff there _was_ a name_ptr
        {
            if (!graph->ResumeInterface(*this))
            {
                PLOG(PL_ERROR, "NetGraph::Interface::AddAddress() error: failed to add interface to graph\n");
                default_addr_item.SetAddress(PROTO_ADDR_NONE);
                return false;
            }
            if (!graph->AddInterfaceAddress(*this, theAddress))
            {
                PLOG(PL_ERROR, "NetGraph::Interface::AddAddress() error: failed to add address to graph\n");
                graph->RemoveInterface(*this);
                default_addr_item.SetAddress(PROTO_ADDR_NONE);
                return false;
            }
        }
        if (!node->AddInterface(*this))
        {
            // This should only occur if we're out of memory
            PLOG(PL_ERROR, "NetGraph::Interface::AddAddress() error: failed to add interface to node\n");
            if (NULL != graph) graph->RemoveInterface(*this); // removes links, too
            default_addr_item.SetAddress(PROTO_ADDR_NONE);
            return false;
        }
    }
    else if (addr_list.Insert(theAddress))
    {
        // If already had address, it's a given that we're already in the node's iface_list
        if (!node->AddExtraInterfaceAddress(*this, theAddress))
        {
            PLOG(PL_ERROR, "NetGraph::Interface::AddAddress() error: failed to add address to node\n");
            RemoveAddress(theAddress);
            return false;
        }
        if (NULL != graph)
        {
            if (!graph->AddInterfaceAddress(*this, theAddress))
            {
                PLOG(PL_ERROR, "NetGraph::Interface::AddAddress() error: failed to add address to graph\n");
                RemoveAddress(theAddress);
                return false;
            }
        }
    }
    else
    {
        PLOG(PL_ERROR, "NetGraph::Interface::AddAddress() error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end NetGraph::Interface::AddAddress()

bool NetGraph::Interface::RemoveAddress(const ProtoAddress& theAddress)
{
#ifdef PROTO_DEBUG
    ASSERT(Contains(theAddress));  // not good practice to call RemoveAddress()  
#else
    if (!Contains(theAddress)) return false;
#endif     
    if (theAddress.HostIsEqual(default_addr_item.GetAddress()))
    {
        // This is the address used for Node/Graph indices, so do some extra work here
        node->RemoveInterface(*this);
        if (NULL != graph)
        {
            addr_list.RemoveItem(default_addr_item);
            if (addr_list.IsEmpty() && (NULL == name_ptr))
            {
                // Are not allowed to remove only address of interface with no name
                addr_list.InsertItem(default_addr_item);
                if (!node->AddInterface(*this))
                    PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reattach interface to node\n");
                return false;
            }
            addr_list.InsertItem(default_addr_item);
            graph->SuspendInterface(*this);  // we make sure to "resume" it below
        }
        addr_list.RemoveItem(default_addr_item);
        if (addr_list.IsEmpty())
        {
            default_addr_item.SetAddress(PROTO_ADDR_NONE);
            if (NULL != name_ptr)
            {
                if (NULL != graph)
                {
                    // reinserts interface into graph now indexed by "name"
                    if (!graph->ResumeInterface(*this))
                    {
                        PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reinsert interface into graph\n");
                        if (!node->AddInterface(*this))  // probably will fail, too
                            PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reattach interface to node\n");
                        return false;
                    }
                    graph->RemoveInterfaceAddress(theAddress);
                }
                // Put iface into node iface_list now indexed by name
                if (!node->AddInterface(*this))
                {
                    PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reattach interface to node\n");
                    if (NULL != graph) graph->RemoveInterface(*this); // note all links are lost
                    return false;  // note address was removed, but ...
                }
            }
            else
            {
                // No remaining name or address, so remove from graph, if applicable
                if (NULL != graph) graph->RemoveInterface(*this); // note all links are lost
            }
        }
        else
        {
            // We'll use a remaining address as the the new "default" address
            ProtoAddressList::Item* rootItem = addr_list.RemoveRootItem();
            default_addr_item = *rootItem;
            delete rootItem;
            addr_list.InsertItem(default_addr_item);
            if (NULL != graph)
            {
                // reinserts interface into graph now indexed by new default address
                if (!graph->ResumeInterface(*this))
                {
                    PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reinsert interface into graph\n");
                    if (!node->AddInterface(*this))  // probably will fail, too
                        PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reattach interface to node\n");
                    return false;
                }
                graph->RemoveInterfaceAddress(theAddress);
            }
            // Put iface into node iface_list now indexed by new default address
            if (!node->AddInterface(*this))
            {
                PLOG(PL_ERROR, "NetGraph::Interface::RemoveAddress() error: failed to reattach interface to node\n");
                if (NULL != graph) graph->RemoveInterface(*this); // note all links are lost
                return false;  // note address was removed, but ...
            }
        }
    }
    else
    {
        addr_list.Remove(theAddress);
        node->RemoveExtraInterfaceAddress(theAddress);
        if (NULL != graph) graph->RemoveInterfaceAddress(theAddress);
    }
    return true; 
}  // end NetGraph::Interface::RemoveAddress()

bool NetGraph::Interface::ChangeNode(Node& theNode)
{
    ASSERT(NULL != node);
    node->RemoveInterface(*this);
    node = &theNode;
    if(!theNode.AddInterface(*this))
    {
        PLOG(PL_ERROR, "NetGraph::Interface::SetNode: error: AddInterface returned false\n");
        return false;
    }
    return true;
}  // end NetGraph::Interface::ChangeNode()

NetGraph::AdjacencyIterator::AdjacencyIterator(Interface& theInterface)
 : ProtoGraph::AdjacencyIterator(theInterface)
{
}

NetGraph::AdjacencyIterator::~AdjacencyIterator()
{
}

NetGraph::Interface::SimpleList::SimpleList(ItemPool* itemPool)
 : Vertice::SimpleList(itemPool)
{
}

NetGraph::Interface::SimpleList::~SimpleList()
{
}

NetGraph::Interface::SimpleList::Iterator::Iterator(const SimpleList& theList)
 : ProtoGraph::Vertice::SimpleList::Iterator(theList)
{
}

NetGraph::Interface::SimpleList::Iterator::~Iterator()
{
}

NetGraph::Interface::SortedList::SortedList(ItemPool* itemPool)
 : Vertice::SortedList(itemPool)
{
}

NetGraph::Interface::SortedList::~SortedList()
{
}

NetGraph::Interface::SortedList::Iterator::Iterator(SortedList& theList)
 : ProtoGraph::Vertice::SortedList::Iterator(theList)
{
}

NetGraph::Interface::SortedList::Iterator::~Iterator()
{
}

NetGraph::Interface::PriorityQueue::Item::Item()
 : prev_hop(NULL), next_hop_link(NULL)
{
}

NetGraph::Interface::PriorityQueue::Item::~Item()
{
}

NetGraph::Interface::PriorityQueue::ItemFactory::ItemFactory()
{
}

NetGraph::Interface::PriorityQueue::ItemFactory::~ItemFactory()
{
    Destroy();
}

NetGraph::Interface::PriorityQueue::Item* NetGraph::Interface::PriorityQueue::ItemFactory::GetItem()
{
    Item* item = static_cast<PriorityQueue::Item*>(item_pool.QueueStatePool::Get());
    if (NULL == item) item = CreateItem();
    if (NULL == item)
        PLOG(PL_ERROR, "NetGraph::Interface::PriorityQueue::ItemFactory::GetItem() CreateItem() error: %s\n",
                       GetErrorString());
    return item;
}  // end NetGraph::Interface::PriorityQueue::ItemFactory::GetItem()

NetGraph::Interface::PriorityQueue::PriorityQueue(PriorityQueue::ItemFactory& itemFactory)
 : item_factory(itemFactory)
{
}

NetGraph::Interface::PriorityQueue::~PriorityQueue()
{
    Empty();
    item_factory.Destroy();
}

bool NetGraph::Interface::PriorityQueue::Insert(Interface& iface, const Cost& cost)
{
    Item* item = item_factory.GetItem();
    if (NULL == item)
    {
        PLOG(PL_ERROR, "NetGraph::Interface::PriorityQueue::Insert() error: couldn't allocate item\n");
        return false;
    }
    item->SetCost(cost);
    Associate(iface, *item);
    InsertItem(*item);
    return true;
}  // end NetGraph::Interface::PriorityQueue::Insert()

void NetGraph::Interface::PriorityQueue::Adjust(Interface& iface, const Cost& newCost)
{
    Item* item = static_cast<Item*>(GetQueueState(iface));
    ASSERT(item != NULL);
    RemoveItem(*item);
    item->SetCost(newCost);
    InsertItem(*item);
}  // end ProtoGraph::Vertice::PriorityQueue::Adjust()

bool NetGraph::Interface::PriorityQueue::AdjustDownward(Interface& iface, const Cost& newCost, const Interface* newPrevHop)
{
    Item* item = static_cast<Item*>(GetQueueState(iface));
    ASSERT(item != NULL);
    if (newCost < item->GetCost())
    {
        RemoveItem(*item);
        item->SetCost(newCost);
        InsertItem(*item);
        return true;
    } 
    else
    {
        return false;
    }
}  // end ProtoGraph::Vertice::PriorityQueue::AdjustDownward()

bool NetGraph::Interface::PriorityQueue::AdjustUpward(Interface& iface, const Cost& newCost)
{
    Item* item = static_cast<Item*>(GetQueueState(iface));
    ASSERT(item != NULL);
    if (newCost > item->GetCost())
    {
        //bunny TBD this is wrong!  we don't know for certain that this is the correct "path"
        //we need to search for alternative shorter paths (upstream only!) and update the cost and route table information
        RemoveItem(*item);
        item->SetCost(newCost);
        InsertItem(*item);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoGraph::Vertice::PriorityQueue::AdjustUpward()

bool NetGraph::Interface::PriorityQueue::Append(Interface& iface)
{
    Item* item = item_factory.GetItem();
    if (NULL == item)
    {
        PLOG(PL_ERROR, "NetGraph::Interface::PriorityQueue::Insert() error: couldn't allocate item\n");
        return false;
    }
    Associate(iface, *item);
    SortedList::AppendItem(*item);
    return true;
}  // end NetGraph::Interface::PriorityQueue::Append()

NetGraph::Interface::PriorityQueue::Iterator::Iterator(PriorityQueue& theQueue)
 : SortedList::Iterator(theQueue)
{
}

NetGraph::Interface::PriorityQueue::Iterator::~Iterator()
{
}

const char* NetGraph::Interface::GetVerticeKey() const
{
    if (addr_list.IsEmpty())
        return name_ptr;
    else
        return default_addr_item.GetAddress().GetRawHostAddress();
}  // end NetGraph::Interface::GetVerticeKey()

unsigned int NetGraph::Interface::GetVerticeKeysize() const
{
    // Perhaps it is worthwhile to add a "key_size" member for efficiency?
    if (addr_list.IsEmpty())
    {
        if (NULL != name_ptr)
        {
            unsigned int nameSize = strlen(name_ptr);
            if (0 == (nameSize & 0x01)) nameSize += 1;  // includes '\0' to make needed odd byte size
            return (nameSize << 3);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return (default_addr_item.GetAddress().GetLength() << 3);
    }
}  // end NetGraph::Interface::GetVerticeKeysize()

NetGraph::Node::Node()
 : iface_list(true), default_interface_ptr(NULL)
{
}

NetGraph::Node::~Node()
{
    // Delete all interfaces
    Interface* iface;
    while (NULL != (iface = static_cast<Interface*>(iface_list.RemoveHead())))
    {
        delete iface;
    }
}    
   
// Remove interfaces from target node ("foodNode") 
// and append to this node
void NetGraph::Node::Consume(Node& foodNode)
{
    Interface* iface;
    while(NULL != (iface= foodNode.GetDefaultInterface()))
    {
        //foodNode.RemoveInterface(*iface);
        iface->ChangeNode(*this);
    }
}  // end NetGraph::Node::Consume()

bool NetGraph::Node::AddInterface(Interface& iface, bool makeDefault)
{
    if(iface_list.IsEmpty()) makeDefault = true;
    if(!iface_list.Insert(iface))
    {
        PLOG(PL_ERROR, "NetGraph::Node::AddInterface() error: Insert failed on the iface_list!\n");
        return false;
    }
    ProtoAddressList::Iterator iterator(iface.GetAddressList());
    ProtoAddress addr;
    while(iterator.GetNextAddress(addr))
    {
        if (addr.HostIsEqual(iface.GetDefaultAddress())) continue; // don't insert default addr in "extra_addr_list"
        if(!extra_addr_list.Insert(addr, &iface))
        {
            PLOG(PL_ERROR, "NetGraph::Node::AddInterface() error: appending %s to the addr_list?!\n",addr.GetHostString());
            RemoveInterface(iface);
            return false;
        }
    }
    if (makeDefault) default_interface_ptr = &iface;
    return true;
}  // end NetGraph::AddInterface)

void NetGraph::Node::RemoveInterface(Interface& iface)
{
    iface_list.Remove(iface);
    ProtoAddressList::Iterator iterator(iface.GetAddressList());
    ProtoAddress addr;
    while(iterator.GetNextAddress(addr)) 
        extra_addr_list.Remove(addr);
    //check to see if was the default and reassign a default if it was
    if (&iface == default_interface_ptr)
        default_interface_ptr = (NetGraph::Interface*)(iface_list.GetRoot());
}  // end NetGraph::Node::RemoveInterface()

bool NetGraph::Node::IsSymmetricNeighbor(Node& node)
{
    NetGraph::Node::InterfaceIterator it(*this);
    NetGraph::Interface* interface = it.GetNextInterface();
    while(NULL != interface)
    {
        NetGraph::Node::InterfaceIterator it2(node);
        NetGraph::Interface* interface2 = it2.GetNextInterface();
        while(NULL != interface2)
        {
            if(interface->HasLinkTo(*interface2) &&
               interface2->HasLinkTo(*interface))
            {
                return true;
            }
            interface2 = it2.GetNextInterface();
        }   
        interface = it.GetNextInterface();
    }
    return false;
}
NetGraph::Interface* NetGraph::Node::FindInterface(const ProtoAddress& addr) const
{
    // First, look in "iface_list"
    Interface* iface = static_cast<Interface*>(iface_list.Find(addr.GetRawHostAddress(), addr.GetLength() << 3));
    if (NULL == iface) // Not in "iface_list", so look for it in "extra_addr_list"
        iface = (Interface*)(extra_addr_list.GetUserData(addr));
    return iface;
}  // end NetGraph::Node::FindInterface()

NetGraph::Interface* NetGraph::Node::FindInterfaceByName(const char* theName)
{
    int nameSize = strlen(theName);
    if (0 == (nameSize & 0x01)) nameSize++; // ensures odd byte count to differentiate from addrs
    nameSize <<= 3;
    NetGraph::Interface* iface = static_cast<Interface*>(iface_list.Find(theName, nameSize));
    if (NULL == iface)
    {
        // O(N) search for iface with matching name
        InterfaceIterator iterator(*this);
        while (NULL != (iface = iterator.GetNextInterface()))
        {
            const char* ifaceName = iface->GetName();
            if ((NULL != ifaceName) && (0 ==  strcmp(ifaceName, theName))) 
                break;
        }
    }
    return iface;
}  // end NetGraph::Node::FindInterfaceByName()

NetGraph::Interface* NetGraph::Node::FindInterfaceByString(const char* theString)
{
    NetGraph::Interface* iface = FindInterfaceByName(theString);
    if(NULL == iface)
    {
        ProtoAddress addr;
        addr.ResolveFromString(theString);
        if(addr.IsValid())
        {
            iface = FindInterface(addr);
        }
    }
    return iface;
}

bool NetGraph::Node::SetDefaultInterface(Interface& iface)
{
    // Make sure this "iface" is valid (has address or name)
    if (iface.GetAddress().IsValid() || (NULL != iface.GetName()))
    {
        default_interface_ptr = &iface;
        return true;
    }
    else
    {
       PLOG(PL_ERROR,"NetGraph::Node::SetDefaultInterface() error: iface has no address or name!\n");
       return false;
    }
}  // end NetGraph::Node::SetDefaultInterface()


NetGraph::Node::InterfaceIterator::InterfaceIterator(Node& node)
 : ProtoSortedTree::Iterator(node.iface_list)
{
}

NetGraph::Node::InterfaceIterator::~InterfaceIterator()
{
}

NetGraph::Node::NeighborIterator::NeighborIterator(Node& node)
 : iface_iterator(node), adj_iterator(*node.GetDefaultInterface())
{
    Reset();
}

NetGraph::Node::NeighborIterator::~NeighborIterator()
{
}

void NetGraph::Node::NeighborIterator::Reset()
{
    iface_iterator.Reset();
    Interface* firstLocalIface = iface_iterator.GetNextInterface();
    // use in-place new operator to instate adj_iterator for next local interface
    if (NULL != firstLocalIface)
    {
        adj_iterator.~AdjacencyIterator();
        new (&adj_iterator) AdjacencyIterator(*firstLocalIface);
    }
}  // end NetGraph::Node::NeighborIterator::Reset()


NetGraph::Interface* NetGraph::Node::NeighborIterator::GetNextNeighborInterface()
{
    // This avoids calling an invalid "adj_iterator" (initialized with invalid iface reference)
    if (iface_iterator.HasEmptyList()) return NULL;
    Interface* iface = adj_iterator.GetNextAdjacency();
    if (NULL == iface)
    {
        Interface* nextLocalInterface; 
        while (NULL != (nextLocalInterface = iface_iterator.GetNextInterface()))
        {
            // use in-place new operator to instantiate adj_iterator for next local interface
            adj_iterator.~AdjacencyIterator();
            new (&adj_iterator) AdjacencyIterator(*nextLocalInterface);
            iface = adj_iterator.GetNextAdjacency();
            if (NULL != iface) break;
        }
    }
    return iface;
}  // end NetGraph::Node::NeighborIterator::GetNextNeighborInterface()

NetGraph::Link* NetGraph::Node::NeighborIterator::GetNextNeighborLink()
{
    // This avoids calling an invalid "adj_iterator" (initialized with invalid iface reference)
    if (iface_iterator.HasEmptyList()) return NULL;
    Link* link = adj_iterator.GetNextAdjacencyLink();
    if (NULL == link)
    {
        Interface* nextLocalInterface; 
        while (NULL != (nextLocalInterface = iface_iterator.GetNextInterface()))
        {
            // use in-place new operator to instate adj_iterator for next local interface
            adj_iterator.~AdjacencyIterator();
            new (&adj_iterator) AdjacencyIterator(*nextLocalInterface);
            link = adj_iterator.GetNextAdjacencyLink();
            if (NULL != link) break;
        }
    }
    return link;
}  // end NetGraph::Node::NeighborIterator::GetNextNeighborInterface()

NetGraph::NetGraph()
{
}

NetGraph::~NetGraph()
{
}

// this function will be removed in upcoming releases
bool NetGraph::InsertNode(Node& node, Interface* iface)
{
    if (NULL != iface)
    {
        // Insert just the specified interface
        if (node.Contains(*iface))
        {
            return InsertInterface(*iface); 
        }
        else
        {
            PLOG(PL_ERROR, "NetGraph::InsertNode() error: iface doesn't belong to node!\n");
            return false;
        }
    }
    else 
    {
        // Insert all of the interfaces
        bool hasIface = false;
        Node::InterfaceIterator it(node);
        while (NULL != (iface = it.GetNextInterface()))
        {
            hasIface = true;
            if (!InsertInterface(*iface))
            {
                PLOG(PL_ERROR, "NetGraph::InsertNode() error: failed to add iface to graph!\n");
                RemoveNode(node);
                return false;
            }
        }
        if (!hasIface)
        {
            PLOG(PL_ERROR, "NetGraph::InsertNode() error: node has no interfaces?!\n");
            return false;
        } 
        return true;
    }
}  // end NetGraph::InsertNode()

void NetGraph::RemoveNode(Node& node, Interface* iface)
{
    if (NULL == iface)
    {
        // Remove all interfaces associated with this node
        Node::InterfaceIterator it(node);
        Interface* iface;
        while (NULL != (iface = it.GetNextInterface()))
        {
            RemoveInterface(*iface);
        }
    }
    else
    {   
        // Remove only the interface specified
        RemoveInterface(*iface);
    }                                  
}  // end NetGraph::RemoveNode()

bool NetGraph::InsertInterface(Interface& iface)
{
    if (!iface.GetAddress().IsValid() && (NULL == iface.GetName()))
    {
        PLOG(PL_ERROR, "NetGraph::InsertInterface() error: iface has no address or name!\n");
        return false;
    }
    // Can only be in one graph at once
    if (NULL != iface.graph) iface.graph->RemoveInterface(iface);
    if (ProtoGraph::InsertVertice(iface))
    {
        ProtoAddressList::Iterator iterator(iface.GetAddressList());
        ProtoAddress addr;
        while(iterator.GetNextAddress(addr))
        {
            if(!addr_list.Insert(addr, &iface))
            {
                PLOG(PL_ERROR, "NetGraph::InsertInterface() error: cannot add address %s to the netgraph addr_list\n",addr.GetHostString());
                RemoveInterface(iface);
                return false;
            }
        }
        iface.graph = this;
    }
    return true;
} // end NetGraph::InsertInterface()

void NetGraph::RemoveInterface(Interface& iface)
{
    ProtoAddressList::Iterator iterator(iface.GetAddressList());
    ProtoAddress addr;
    while(iterator.GetNextAddress(addr)) addr_list.Remove(addr); 
    if (vertice_list.Contains(iface)) RemoveVertice(iface);
    iface.graph = NULL;
}  // end NetGraph::RemoveInterface()


NetGraph::Interface* NetGraph::FindInterfaceByName(const char* theName)
{
    if (NULL == theName) return NULL;
    unsigned int nameSize = strlen(theName);
    if (0 == (nameSize & 0x01)) nameSize++;
    nameSize <<= 3;
    Interface* iface = static_cast<Interface*>(vertice_list.FindVertice(theName, nameSize));
    if (NULL == iface)
    {
        InterfaceIterator iterator(*this);
        while(NULL != (iface = iterator.GetNextInterface()))
        {
            const char* ifaceName = iface->GetName();
            if ((NULL != ifaceName) && (0 == strcmp(ifaceName,theName))) return iface;
        }
    }
    return iface;
}  // end FindInterfaceByName()

NetGraph::Interface* NetGraph::FindInterfaceByString(const char* theString)
{
    NetGraph::Interface* iface = FindInterfaceByName(theString);
    if(NULL == iface)
    {
        ProtoAddress addr;
        addr.ResolveFromString(theString);
        if(addr.IsValid())
        {
            iface = FindInterface(addr);
        }
    }
    return iface;
}

NetGraph::Link* NetGraph::Connect(Interface& srcIface, Interface& dstIface, const Cost& cost)
{
    // TBD - allow multiple links srcIface <-> dstIface ???
    Link* link = srcIface.GetLinkTo(dstIface);
    // Is there already a link, "srcIface" to "dstIface"?
    if (NULL != link)
    {
        PLOG(PL_WARN, "NetGraph::Connect() error: srcIface -> dstIface already connected\n");
        return Reconnect(srcIface, dstIface, cost);
    }
    // Get a new link from our edge factory (from pool or created as needed)
    // IMPORTANT NOTE: We _cannot_ call ProtoGraph::Connect() here directly
    // since it is important to set the link cost _before_ putting the connection
    // in place so that the adjacency queue gets ordered properly (i.e. by cost)
    link = static_cast<Link*>(GetEdge());
    if (NULL == link)
    {
        PLOG(PL_ERROR, "NetGraph::Connect() error: unable to allocate link\n");
        return NULL;
    }
    link->SetCost(cost);
    ProtoGraph::Connect(srcIface, dstIface, *link);
    return link;
}  // end NetGraph::Connect(directed link only)


bool NetGraph::Connect(Interface& srcIface, Interface& dstIface, const Cost& cost, bool duplex)
{
    Link* link = Connect(srcIface, dstIface, cost);
    if (!duplex) return (NULL != link);
    if (NULL == link)
    {
        PLOG(PL_ERROR, "NetGraph::Connect() error: unable to allocate forward link\n");
        return false;
    }
    if (NULL == Connect(dstIface, srcIface, cost))
    {
        PLOG(PL_ERROR, "NetGraph::Connect() error: unable to allocate reverse link\n");
        Disconnect(srcIface, dstIface, true);
        return false;
    }
    return true;   
}  // end NetGraph::Connect(duplex option)

NetGraph::Link* NetGraph::Reconnect(Interface& srcIface, Interface& dstIface, const Cost& cost)
{
    // TBD - allow multiple links srcIface <-> dstIface ???
    Link* link = srcIface.GetLinkTo(dstIface);
    // Is there already a link, "srcIface" to "dstIface"?
    if (NULL == link)
    {
        PLOG(PL_WARN, "NetGraph::Reconnect() error: srcIface -> dstIface not connected\n");
        return NULL;
    }
    else if (cost == link->GetCost())
    {
        return link;
    }
    this->SuspendEdge(srcIface, dstIface, *link);
    link->SetCost(cost);
    ProtoGraph::Reconnect(srcIface,dstIface,*link);
    return link;
}  // end NetGraph::Reconnect()

bool NetGraph::Reconnect(Interface& srcIface, Interface& dstIface, const Cost& cost, bool duplex)
{
    Link* link = Reconnect(srcIface, dstIface, cost);
    if (!duplex) return (NULL != link);
    if (NULL == link)
    {
        PLOG(PL_ERROR, "NetGraph::Reconnect() error: unable to reconnect forward link\n");
        return false;
    }
    if (NULL == Reconnect(dstIface, srcIface, cost))
    {
        PLOG(PL_ERROR, "NetGraph::Reconnect() error: unable to reconnect reverse link\n");
        Disconnect(srcIface, dstIface, true);
        return false;
    }
    return true;
}  // end NetGraph::Reconnect()
            

NetGraph::InterfaceIterator::InterfaceIterator(NetGraph& theGraph)
  : ProtoGraph::VerticeIterator(static_cast<ProtoGraph&>(theGraph))
{
}

NetGraph::InterfaceIterator::~InterfaceIterator()
{
}

NetGraph::SimpleTraversal::SimpleTraversal(const NetGraph& theGraph, 
                                           Interface&      startIface,
                                           bool            traverseNodes,
                                           bool            collapseNodes,
                                           bool            depthFirst)
 : ProtoGraph::SimpleTraversal(theGraph, startIface, depthFirst), 
   traverse_nodes(traverseNodes), collapse_nodes(collapseNodes)
{
    Reset(true);
}

NetGraph::SimpleTraversal::~SimpleTraversal()
{
}

bool NetGraph::SimpleTraversal::Reset(bool constructor)
{
    if (!constructor)
    {
        if (!ProtoGraph::SimpleTraversal::Reset())    
        {
            PLOG(PL_ERROR, "NetGraph::SimpleTraversal::Reset() error: couldn't enqueue start_vertice\n");
            return false;
        }
    }
    if (traverse_nodes && collapse_nodes)
    {
        // Init with all interfaces of current node at level zero and init
        // trans_vertice equal to last (note that NetGraph::SimpleTraversal::Reset()
        // already inits queue_pending with start_vertice)
        Interface& startIface = static_cast<Interface&>(start_vertice);
        Node::InterfaceIterator ifaceIterator(startIface.GetNode());
        Interface* nextIface;
        while (NULL != (nextIface = ifaceIterator.GetNextInterface()))
        {
            if (static_cast<Interface*>(&start_vertice) == nextIface)
                continue;
            ASSERT(!nextIface->IsInQueue(queue_visited) && !nextIface->IsInQueue(queue_pending));
            if (!AllowLink(startIface, *nextIface, NULL)) continue;
            if (depth_first)
                queue_pending.Prepend(*nextIface);
            else
                queue_pending.Append(*nextIface);
        }
    }
    return true;
}  // end ProtoGraph::SimpleTraversal::Reset()

// This traversal can use all of its nodes' local interfaces
NetGraph::Interface* NetGraph::SimpleTraversal::GetNextInterface(unsigned int* level)
{
    Interface* currentIface = static_cast<Interface*>(queue_pending.GetHead());
    if (NULL != currentIface)
    {
        queue_pending.TransferVertice(*currentIface, queue_visited);
        Vertice* transAdjacency = NULL;  // used to find "level" transitions
        Interface* nextIface;
        if (traverse_nodes && !collapse_nodes)
        {
            // Here we treat other ifaces on same node as adjacent
            Node::InterfaceIterator ifaceIterator(currentIface->GetNode());
            while (NULL != (nextIface = ifaceIterator.GetNextInterface()))
            {
                if (!nextIface->IsInQueue(queue_visited) &&
                    !nextIface->IsInQueue(queue_pending))
                {
                    if (!AllowLink(*currentIface, *nextIface, NULL)) continue;
                    if (depth_first)
                    {
                        queue_pending.Prepend(*nextIface);
                        // (TBD) depth tracking for Depth-first search
                    }
                    else
                    {
                        queue_pending.Append(*nextIface);
                        if (NULL == transAdjacency) 
                            transAdjacency = nextIface;
                    }
                }
            }
        }
        // Now iterate to other neighboring interfaces
        AdjacencyIterator adjacencyIterator(*currentIface);
        Link* nextLink;
        while (NULL != (nextLink = adjacencyIterator.GetNextAdjacencyLink()))
        {
            nextIface = nextLink->GetDst();
            ASSERT(NULL != nextIface);
            if (!nextIface->IsInQueue(queue_visited) &&
                !nextIface->IsInQueue(queue_pending))
            {
                if (!AllowLink(*currentIface, *nextIface, nextLink)) continue;
                if (depth_first)
                {
                    queue_pending.Prepend(*nextIface);
                    // (TBD) depth tracking for Depth-first search
                }
                else
                {
                    queue_pending.Append(*nextIface);
                    if (NULL == transAdjacency) 
                        transAdjacency = nextIface;
                }
                if (traverse_nodes && collapse_nodes)
                {
                    // Here co-interfaces are treated as equivalent adjacencies
                    Node::InterfaceIterator ifaceIterator(nextIface->GetNode());
                    Interface* nextCoface;    
                    while (NULL != (nextCoface = ifaceIterator.GetNextInterface()))
                    {
                        if (nextCoface == nextIface) continue;
                        ASSERT(!nextCoface->IsInQueue(queue_visited) && !nextCoface->IsInQueue(queue_pending));
                        if (depth_first)
                            queue_pending.Prepend(*nextCoface);
                        else
                            queue_pending.Append(*nextCoface);
                    }
                }
            }
        }
        if (NULL == trans_vertice)
        {
            trans_vertice = transAdjacency;
        }
        else if (trans_vertice == currentIface)
        {
            current_level++;
            trans_vertice = transAdjacency;
        }
        if (NULL != level) *level = current_level;
    }
    return currentIface;
    
}  // end NetGraph::SimpleTraversal::GetNextInterface()

/*
// This traversal can use all of its nodes' local interfaces
NetGraph::Interface* NetGraph::SimpleTraversal::GetNextInterface(unsigned int* level)
{
    if (traverse_nodes)
    {
        //TRACE("enter NetGraph::SimpleTraversal::GetNextInterface() ...\n");
        Interface* currentIface = static_cast<Interface*>(queue_pending.GetHead());
        if (NULL == currentIface) return NULL; // we're done
        //TRACE("level %u trans_vertice:%p currentIface:%p\n", level, trans_vertice, static_cast<Vertice*>(currentIface));
        if ((static_cast<Vertice*>(currentIface) == trans_vertice) || (NULL == trans_vertice))
        {
            if (NULL != trans_vertice) current_level++;
            // Enqueue adjacencies (and their co-interfaces)of queue_pending as next level
            Vertice::SimpleList::Iterator penderator(queue_pending);
            Interface* nextPending;
            Interface* transIface = NULL;
            //int p = 0;
            while (NULL != (nextPending = static_cast<Interface*>(penderator.GetNextVertice())))
            {
                if (nextPending == transIface) break;
                //TRACE(" pending iface %d ...\n", p++);
                AdjacencyIterator adjacencyIterator(*nextPending);
                Link* nextLink;
                while (NULL != (nextLink = adjacencyIterator.GetNextAdjacencyLink()))
                {
                    Interface* nextIface = nextLink->GetDst();
                    ASSERT(NULL != nextIface);
                    if (!nextIface->IsInQueue(queue_visited) &&
                        !nextIface->IsInQueue(queue_pending))
                    {
                        if (!AllowLink(*nextPending, *nextIface, nextLink)) continue;
                        Node::InterfaceIterator ifaceIterator(nextIface->GetNode());
                        Interface* nextCoface;
                        while (NULL != (nextCoface = ifaceIterator.GetNextInterface()))
                        {
                            if (NULL == transIface) transIface = nextCoface;
                            if (depth_first)
                            {
                                queue_pending.Prepend(*nextCoface);
                                // (TBD) depth tracking for Depth-first search
                            }
                            else
                            {
                                //TRACE("  Appending level %u co-interface ...\n", current_level);
                                queue_pending.Append(*nextCoface);
                            }
                        }
                    }
                }
            }
            trans_vertice = static_cast<Vertice*>(transIface);
        }
        queue_pending.TransferVertice(*currentIface, queue_visited);
        if (NULL != level) *level = current_level;
        return currentIface;  
    }
    else
    {
        // If not traversing nodes, then just use ProtoGraph::SimpleTraversal
        return static_cast<Interface*>(ProtoGraph::SimpleTraversal::GetNextVertice(level));
    }
}  // end NetGraph::SimpleTraversal::GetNextInterface()
*/
        

NetGraph::DijkstraTraversal::DijkstraTraversal(NetGraph&    theGraph,   
                                               Interface*   startIface)
 : manet_graph(theGraph), start_iface(startIface),
   queue_pending(static_cast<ItemFactory&>(*this)), 
   queue_visited(static_cast<ItemFactory&>(*this)),
   trans_iface(NULL), current_level(0), dijkstra_completed(false), in_update(false), traverse_nodes(false), reset_required(false)
{

}

NetGraph::DijkstraTraversal::DijkstraTraversal(NetGraph&    theGraph,   
                                               Node&        startNode,  
                                               Interface*   startIface)
 : manet_graph(theGraph), start_iface((NULL != startIface) ? startIface : startNode.GetDefaultInterface()),
   queue_pending(static_cast<ItemFactory&>(*this)), 
   queue_visited(static_cast<ItemFactory&>(*this)),
   trans_iface(NULL), current_level(0), dijkstra_completed(false), in_update(false), traverse_nodes(false), reset_required(false)
{
    // ASSERT(&start_iface->GetNode() == &startNode);
}

NetGraph::DijkstraTraversal::~DijkstraTraversal()
{
    queue_pending.Empty();
    queue_visited.Empty();
}

void
NetGraph::DijkstraTraversal::TraverseNodes(bool traverse)
{
    traverse_nodes = traverse;
}

bool NetGraph::DijkstraTraversal::Reset(Interface* startIface)
{
    // (TBD) use a dual queue approach so that we can avoid 
    //      visiting every iface twice everytime we run the Dijkstra.
    //      I.e., we could maintain "visited" & "unvisited" queue, removing ifaces
    //      from that queue as they are visited, and then at the end of
    //      the Dijkstra, update the cost of any remaining unvisited ifaces
    //      to COST_MAX and then swap visited and unvisited queues for next
    //      Dijkstra run ... Thus each iface would be visited only once _or_ as
    //      needed for Dijkstra instead of once _plus_ as needed for Dijkstra
    queue_visited.Empty();
    queue_pending.Empty();
    Cost& startCost = AccessCostTemp();
    startCost.Minimize();
    
    if (NULL != startIface)
        start_iface = startIface;
    
    if (NULL != start_iface)
    {
        if(traverse_nodes)
        {
            Node::InterfaceIterator ifaceIterator(start_iface->GetNode());
            Interface* nextIface;
            while (NULL != (nextIface = ifaceIterator.GetNextInterface()))
            {
                if (!queue_pending.Insert(*nextIface, startCost))
                {
                    PLOG(PL_ERROR, "NetGraph::DijkstraTraversal::Reset() error: couldn't enqueue a start_iface (traverse_nodes)\n");
                    return false;
                }   
            }
        } 
        else 
        {
            if (!queue_pending.Insert(*start_iface, startCost))
            {
                PLOG(PL_ERROR, "NetGraph::DijkstraTraversal::Reset() error: couldn't enqueue start_iface\n");
                return false;
            }
        }
        //TRACE("dijkstra_completed = false bunny1\n");
        dijkstra_completed = false;
    }
    else
    {
        //TRACE("dijkstra_completed = true bunny2\n");
        dijkstra_completed = true;
    }
    return true;
}  // end NetGraph::DijkstraTraversal::Reset()


// Dijkstra traversal step
NetGraph::Interface* NetGraph::DijkstraTraversal::GetNextInterface()
{
    // (TBD) We could be a little more efficient if we worked with PriorityQueue::Items directly
    Interface* currentIface = queue_pending.GetHead();
    if (NULL != currentIface)
    {
        queue_pending.TransferInterface(*currentIface, queue_visited);
        const Cost* currentCost = queue_visited.GetCost(*currentIface);
        ASSERT(NULL != currentCost);
        //TRACE("     %s being updated with cost %f\n",currentIface->GetAddress().GetHostString(),((ManetGraph::Cost*)currentCost)->GetValue());
        AdjacencyIterator linkIterator(*currentIface);
        Link* nextLink;
        while ((nextLink = linkIterator.GetNextAdjacencyLink()))
        {
            Interface* nextDst = nextLink->GetDst();
            ASSERT(NULL != nextDst);
            if (!AllowLink(*currentIface, *nextLink)) continue;
            const Cost& linkCost = nextLink->GetCost();
            Cost& nextCost = AccessCostTemp();
            nextCost = linkCost;
            nextCost += *currentCost;
            
            Node::InterfaceIterator ifaceIterator(nextDst->GetNode());
            if(traverse_nodes)
                nextDst = ifaceIterator.GetNextInterface();
            do
            {
                bool saveState = true;
                if(!in_update)
                {
                    if (nextDst->IsInQueue(queue_pending))
                    {
                       // We have found another path to this pending iface.
                        //  If it is a shorter path update the cost to lower value
                        if (!queue_pending.AdjustDownward(*nextDst, nextCost, currentIface))
                                saveState = false;
                    }
                    else if (!nextDst->IsInQueue(queue_visited))
                    {
                        // This is the first path found to this iface,
                        // so enqueue it in our "queue_pending" queue
                        if (!queue_pending.Insert(*nextDst, nextCost))
                        {
                            PLOG(PL_ERROR, "NetGraph::DijkstraTraversal::GetNextInterface() error: couldn't enqueue iface\n");
                            return NULL;
                        }
                    }
                    else
                    {
                        saveState = false;
                    }
                } 
                else  // in_update
                {
                    // visited queue nodes may not have been updated yet...we need a different way to check.
                    Interface* prevHopIface;
                    bool isInVisited = false;
                    bool isNewIface = false;
                    Interface::PriorityQueue* queuePtr = &queue_pending;
                    if(nextDst->IsInQueue(queue_visited))
                    {
                        isInVisited = true;
                        queuePtr = &queue_visited;
                    } 
                    else if (!nextDst->IsInQueue(queue_pending)) 
                    {
                        //new interface so just go ahead and add it
                        isNewIface = true;
                    }
                    
                    if(isNewIface)
                    {
                        if (!queue_pending.Insert(*nextDst, nextCost))
                        {
                            PLOG(PL_ERROR, "NetGraph::DijkstraTraversal::GetNextInterface() error: couldn't enqueue iface\n");
                            return NULL;
                        }
                        
                    } 
                    else if (queuePtr->AdjustDownward(*nextDst, nextCost, currentIface)) // try and move it down
                    {
                        //TRACE(" %s moved downward\n",nextDst->GetAddress().GetHostString());
                        if(isInVisited)
                        {
                            //TRACE("  was in visited queue adding it to the pending queue so children can be updated\n");
                            queue_visited.TransferInterface(*nextDst, queue_pending); //it moved down so put it in the pending queue to updated it's children...
                        }
                    }
                    else if(NULL != (prevHopIface = queuePtr->GetPrevHop(*nextDst)))
                    {
                        //need to check if we were the previous hop
                        //Interface* bunnyTemp = queuePtr->GetNextHop(*currentIface);
                        //if(bunnyTemp)
                        //    TRACE("bunny next hop current is %s\n",bunnyTemp->GetAddress().GetHostString());
                        //TRACE("checking next nbr from current iface %s \n",currentIface->GetAddress().GetHostString());
                        //TRACE("prev hop %s ",prevHopIface->GetAddress().GetHostString());
                        //TRACE("of dst %s iface\n",nextDst->GetAddress().GetHostString());
                        bool linkWasUsed = false;
                        if(traverse_nodes)
                        {
                            if (currentIface->GetNode().Contains(*prevHopIface))
                                linkWasUsed = true;
                        }
                        else
                        {
                            if (currentIface == prevHopIface)
                                linkWasUsed = true;
                        }
                        if(linkWasUsed)
                        {
                            //TRACE("link was used was it longer? if yes then\n");
                            if(queuePtr->AdjustUpward(*nextDst, nextCost))
                            {
                                //TRACE("link we are bailing out and setting the required reset bool true!\n");
                                //we need to verify that this is the currently shortest path and if not fix it! 
                                //New function required or expand AdjustUpward to take care of this.
                                
                                //using class variable to tell update to do a full reset...
                                //Fixing AdjustingUpward and putting the some queue update stuff will then we can remove this
                                reset_required = true; 
                                saveState = false;
                                return NULL;
                                //queue_visited.TransferInterface(*nextDst, queue_pending);
                                //we need to check neighbors to see if they used this path and update them if they did.
                            }
                            else
                            {
                                //TRACE("link was same length as before not saving state\n");
                                saveState = false;
                            }
                        } 
                        else 
                        {
                            //TRACE("link wasn't used not saving\n");
                            saveState = false;
                        }
                    } 
                    else 
                    {
                        //TRACE("no prev hop\n");
                        //no previous hop this can happen if currentIface is on the root node
                        saveState = false;
                    }
                    // I _think_ this is currently broken?  It seems we would need to shuffle nodes 
                    // from "queue_visited" to "queue_pending" here to work properly? ... Note that
                    // it depends heavily on "Update()" being called with a proper "startIface"
                    // This checks for shorter path to a "visited" iface
                    // (must be in "Update()" mode! (i.e. "in_update == true")
                    /*{
                        // Update the routing tree state 
                        if (currentIface == start_iface)
                            queue_visited.SetRouteInfo(*nextDst, nextLink, currentIface);
                        else
                            queue_visited.SetRouteInfo(*nextDst, queue_visited.GetNextHopLink(*currentIface), currentIface);
                    }
                    saveState = false;*/
                }
                // Save our routing tree state as we go
                if(saveState)
                {
                    //if (currentIface == start_iface)
                    if(start_iface->GetNode().Contains(*currentIface))
                    {
                        //leaving debug statements here but commented out for the time being JD
                        if(nextDst==NULL)
                        {
                            //TRACE("nextDst is null!\n");
                        }
                        else
                        {
                            //TRACE("nextDst is %s\n",nextDst->GetAddress().GetHostString());
                        }
                        if(nextLink==NULL)
                        {
                            //TRACE("nextLink is null!\n");
                        } 
                        else 
                        {
                            //TRACE("nextLink src is %s dst is ",nextLink->GetSrc()->GetAddress().GetHostString());
                            //TRACE("%s\n",nextLink->GetDst()->GetAddress().GetHostString());
                        }
                        if(currentIface==NULL)
                        {
                            //TRACE("currentIface is null!\n");
                        } 
                        else 
                        {
                            //TRACE("currentIface is %s\n",currentIface->GetAddress().GetHostString());
                        }
                        queue_pending.SetRouteInfo(*nextDst, nextLink, currentIface);
                    }
                    else
                    {
                        if(queue_visited.Contains(*currentIface))
                            queue_pending.SetRouteInfo(*nextDst, queue_visited.GetNextHopLink(*currentIface), currentIface);
                        else
                            queue_pending.SetRouteInfo(*nextDst, queue_pending.GetNextHopLink(*currentIface), currentIface);
                    }
                }
                if(traverse_nodes)
                    nextDst = ifaceIterator.GetNextInterface();
                else
                    nextDst = NULL;
            } while (nextDst != NULL);
        }  // end while ((nextLink = linkIterator.GetNextAdjacencyLink()))
    }
    else
    {
        //TRACE("dijkstra_completed = true bunny2\n");
        dijkstra_completed = true;
    }
    return currentIface;
}  // end NetGraph::DijkstraTraversal::GetNextInterface()

bool NetGraph::DijkstraTraversal::PrevHopIsValid(Interface& currentIface)
{
    ASSERT(start_iface != NULL);
    if (!traverse_nodes)
    {
        if (&currentIface == start_iface)
            return true;
    } 
    else 
    {
        if(currentIface.GetNode().Contains(*start_iface))
            return true;
    }
    const Cost* currCostPtr = GetCost(currentIface);
    if(currCostPtr == NULL)
        return false;
    Interface* prevHopIface = NULL;
    if(NULL != (prevHopIface = queue_visited.GetPrevHop(currentIface)))
    {
        const Cost* prevCostPtr = GetCost(*prevHopIface);
        if(prevCostPtr == NULL)
            return false;
        
        if(!traverse_nodes)
        {
            Link* linkPtr = prevHopIface->GetLinkTo(currentIface);
            if(linkPtr != NULL)
            {
                if (AllowLink(*prevHopIface, *linkPtr))
                {
                    Cost& sumCost = AccessCostTemp();
                    sumCost = linkPtr->GetCost();
                    sumCost += *prevCostPtr;
                    if(sumCost > *currCostPtr)
                    {
                        // cost has increased and is no longer valid
                        return false;
                    } 
                    else if (sumCost < *currCostPtr) 
                    {
                        // we could return false here but the link has only gotten "better" so should still be a valid prev hop just with the wrong value
                        return true;
                    } 
                    else 
                    {
                        return true;
                    }
                }
            } 
            else 
            {
                // no link exists from my previous hop, returning null
                return false;
            }
        } 
        else 
        {
            ManetNode::NeighborIterator nbIt(prevHopIface->GetNode());
            Link* linkPtr = NULL;
            while(NULL != (linkPtr = nbIt.GetNextNeighborLink()))
            {
                Interface* srcPtr = linkPtr->GetSrc();
                ASSERT(NULL != srcPtr);
                if (!AllowLink(*srcPtr, *linkPtr)) continue;
                Interface* dstPtr = linkPtr->GetDst();
                if(dstPtr!=NULL)
                {
                    if(currentIface.GetNode().Contains(*dstPtr))
                    {
                        Cost& sumCost = AccessCostTemp();
                        sumCost = linkPtr->GetCost();
                        sumCost += *prevCostPtr;
                        if(sumCost <= *currCostPtr)
                            return true;
                        if (sumCost == *currCostPtr) 
                            return true;
                    }
                }
            }
            return false;
        }   
    } 
    else 
    {
        // previous hop is null, returning false;
        return false;
    }
    return true;
} //end NetGraph::Interface* NetGraph::DijkstraTraversal::PreviousHopIsValid()

void NetGraph::DijkstraTraversal::Update(Interface& startIface)
{
    //TRACE("  calling update on interface %s\n",startIface.GetAddress().GetHostString());
    if (!dijkstra_completed)
    {
        //TRACE("bunny doing a full reset...\n");
        Reset();
        // Complete dijkstra traversal
        while (NULL != GetNextInterface());
        return;
    }
    if(!PrevHopIsValid(startIface))
    {
        //we might be able to find and update the "star cluster" for the next best paths but it might not be worth it
        //TRACE("  previous hop isn't valid....doing a full reset...\n");
        Reset();
        // Complete dijkstra traversal
        while (NULL != GetNextInterface());
        return;
    }
    ASSERT(queue_pending.IsEmpty());

    //set up the initial state for walking through the graph stub.
    in_update = true; 
    Interface* origStartIfacePtr = start_iface;
    start_iface=&startIface;
    if(startIface.IsInQueue(queue_visited))
    {
        //TRACE("  Interface was in the visited queue so setting up pending queue to update dijkstra state\n");
        if(traverse_nodes)
        {
            Node::InterfaceIterator ifaceIterator(startIface.GetNode());
            Interface* nextIface = NULL;
            while (NULL !=(nextIface = ifaceIterator.GetNextInterface()))
                queue_visited.TransferInterface(*nextIface, queue_pending);
        } 
        else 
        {
            queue_visited.TransferInterface(startIface, queue_pending);
        }
    }
    else
    {
        // Need to reset entire Dijkstra (as a new interface has been added and we don't know from where)
        // (TBD - maybe something smarter can be done here)
        // could be solved in a similar way to the star cluster as a single isolated branch/leaf is a subset of that problem
        //TRACE("  Interface to update isn't in the visited queue doing a full reset\n");
        reset_required = true;
    }
    //walk through and update the graph stub
    if(!reset_required)
    {
        //TRACE("  Going to walk through the graph\n");
    }
    Interface *nextIface = GetNextInterface();
    if(reset_required)
        nextIface = NULL;
    while (NULL != nextIface)
    {
        //TRACE("   %s is the next iface\n",nextIface->GetAddress().GetHostString());
        if(reset_required)
            nextIface = NULL;
        else
            nextIface = GetNextInterface();
    }
    //finished walking through stub check to make sure a reset isn't required
    //TRACE("  finishing up the state and checking to see if a reset is required\n");
    in_update = false;
    start_iface=origStartIfacePtr;
    if(reset_required)
    {
        //TRACE("  Needed a full reset\n");
        Reset();
        while (NULL != GetNextInterface());
    }
    //if (!dijkstra_completed)
    //    TRACE("Getting out without dijkstra being finished!\n");
}  // end NetGraph::DijkstraTraversal::Update()

// Call this to setup re-traverse of tree computed via Dijkstra
void NetGraph::DijkstraTraversal::Update(Interface& ifaceA, Interface& ifaceB)
{
    const NetGraph::Cost* aCostPtr = GetCost(ifaceA);
    const NetGraph::Cost* bCostPtr = GetCost(ifaceB);
    if((aCostPtr != NULL) && (bCostPtr != NULL))
    {
        if(*aCostPtr > *bCostPtr)
        {
            Link* linkPtr = ifaceB.GetLinkTo(ifaceA);
            if(NULL != linkPtr)
            {
                // there is a link so lets try and update B without doing a full update
                if (!AllowLink(ifaceB, *linkPtr))
                {
                    //the link isn't allowed and if the link was on the shortest path this will cause a full rest otherwise nothing will be updated
                    Update(ifaceA);
                } 
                else 
                {
                    Update(ifaceB);
                }
            } 
            else 
            {
                //the link doesn't exists and if the link was on shortest path this will cause a full reset otherwise nothing will be updated
                Update(ifaceA);
            }
        }
        else
        {
            //a's cost is less than b's 
            Link* linkPtr = ifaceA.GetLinkTo(ifaceB);
            if(NULL != linkPtr)
            {
                if (!AllowLink(ifaceA, *linkPtr))
                {
                    //the link isn't allowed and if the link was on the shortest path this will cause a full reset otherwise nothing will be updated
                    Update(ifaceB);
                } 
                else 
                {
                    //there is a link so lets try and update A without doing a full update
                    Update(ifaceA);
                }
            } 
            else 
            {
                //the link doesn't exists and if the link was on shortest path this will cause a full rest otherwise nothing will be updated
                Update(ifaceB);
            }
        }
    } 
    else if(aCostPtr != NULL) 
    {
        Update(ifaceA);
    } 
    else if (bCostPtr != NULL) 
    {
        Update(ifaceB);
    }
}

bool NetGraph::DijkstraTraversal::TreeWalkReset()
{
    // If Dijkstra was not completed, run full Dijkstra
    if (!dijkstra_completed)
    {
        //TRACE("In reset...\n");
        Reset();
        while (NULL != GetNextInterface());
    }
    ASSERT(queue_pending.IsEmpty());
    if (NULL != start_iface)
    {
        if (!queue_pending.Append(*start_iface))
        {
            PLOG(PL_ERROR, "NetGraph::DijkstraTraversal::TreeWalkReset() error: couldn't append start_iface\n");
            return false;
        }
    }
    trans_iface = NULL;
    current_level = 0;
    return true;
}  // end NetGraph::DijkstraTraversal::TreeWalkReset()

NetGraph::Interface* NetGraph::DijkstraTraversal::TreeWalkNext(unsigned int* level)
{
    Interface* currentIface =  queue_pending.RemoveHead();
    if (NULL != currentIface)
    {
        // Find selected links
        
        Link* nextLink;
        Link* firstLink = NULL;
        if(traverse_nodes)
        {
            NetGraph::Node::NeighborIterator linkIterator(currentIface->GetNode());
            while ((nextLink = linkIterator.GetNextNeighborLink()))
            {
                Interface* nextDst = nextLink->GetDst();
                ASSERT(NULL != nextDst);
                if (currentIface == GetPrevHop(*nextDst))
                {
                    if (NULL == firstLink) firstLink = nextLink;
                    queue_pending.Append(*nextDst);
                }
            }
        } 
        else 
        {
            AdjacencyIterator linkIterator(*currentIface);
            while ((nextLink = linkIterator.GetNextAdjacencyLink()))
            {
                Interface* nextDst = nextLink->GetDst();
                ASSERT(NULL != nextDst);
                if (currentIface == GetPrevHop(*nextDst))
                {
                    if (NULL == firstLink) firstLink = nextLink;
                    queue_pending.Append(*nextDst);
                }
            }
        }
        // Track depth as walk progresses ...
        if (NULL == trans_iface)
        {
            trans_iface = firstLink ? firstLink->GetDst() : NULL;
        }
        else if (trans_iface == currentIface)
        {
            trans_iface = firstLink ? firstLink->GetDst() : NULL; 
            current_level++;  
        }
        if (NULL != level) *level = current_level;
    }
    return currentIface;
}  // end NetGraph::DijkstraTraversal::TreeWalkNext()
