
// GraphRider (gr) program
// This program loads a graph from an existing SDT file.  It builds the graph from the SDT
// file by inserting Nodes corresponding to SDT nodes and connects the graph according
// to SDT "link" commands. Note that SDT scripts can dynamically update the graph state over
// time and hence this program updates the graph in "snapshots" between SDT "wait" commands.
// The ECDS relay set selection algorithm is currently implemented here.  The SDT input
// is essentially passed through but modifies the coloring of SDT nodes to illustrate
// the relay set selection.

#include "manetGraph.h"
#include "manetGraphML.h"
#include "protoSpace.h"  // we use this for its bounding box iteration
#include <protoDebug.h>
#include <protoDefs.h>
#include <protoString.h> // for ProtoTokenator

#include <stdio.h>   // for sprintf()
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>   // for "isprint()"
#include <math.h>    // for "sqrt()"

//#include <chrono>


class Node;  // predeclared so it can be passed to templated CdsInterface definition below

class CdsInterface;   // predeclared so it can be passed to templated CdsLink definition below
           

// Define a ManetLink type that uses the "SimpleCostDouble" as its cost metric
class CdsLink : public NetGraph::LinkTemplate<NetGraph::SimpleCostDouble, CdsInterface>, public ProtoQueue::Item 
{
    public:
        class SimpleList : public ProtoSimpleQueueTemplate<CdsLink> {};
};

class CdsInterface : public NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node> 
{
    public:
        CdsInterface(Node& theNode) 
          : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node>(theNode),
            rtr_priority(0), relay_status(false), visited(false)
        {
        }    
        CdsInterface(Node& theNode, const ProtoAddress& addr) 
          : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node>(theNode, addr),
            rtr_priority(0), relay_status(false), visited(false)
        {
        }    
        virtual ~CdsInterface() {}
        
        UINT32 GetNodeId() const;
        
        void SetRtrPriority(UINT8 value)
            {rtr_priority = value;}
        UINT8 GetRtrPriority() const
            {return rtr_priority;}
        
        unsigned int GetDegree() const; //{return GetAdjacencyCount();}

        void SetRelayStatus(bool state)
            {relay_status = state;}
        bool GetRelayStatus() const
            {return relay_status;}

        void SetVisited(bool state)
            {visited = state;}
        bool WasVisited() const
            {return visited;}
            
    private:
        UINT8        rtr_priority;
        bool         relay_status;
        bool         visited;
};  // end class CdsInterface
   

class CdsGraph : public NetGraphTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node> {};
 

void Usage()
{
    fprintf(stderr, "Usage: gr input <fileName> [degree][link <name>,<priority>]\n");
}

const unsigned int MAX_LINE = 256;

class Node :public NetGraph::NodeTemplate<CdsInterface>, public ProtoQueue::Item
{
    public:
        Node();
        ~Node();

        // _MUST_ call init to create a default interface
        bool Init(UINT32 nodeId, const char* nodeName = NULL);
        
        CdsGraph::Interface* AddInterface(const char* ifaceName, unsigned int ifaceId)
        {
            ProtoAddress addr;
            iface_count += 1;
            if (0 != ifaceId)
                addr.SetEndIdentifier(ifaceId);
            else
                addr.SetEndIdentifier(node_id*1000+iface_count);
            CdsGraph::Interface* iface = new CdsGraph::Interface(*this, addr);
            iface->SetRtrPriority(1);
            if (NULL != iface)
            {
                iface->SetName(ifaceName);
                // Make first added interface the default interface
                if (AppendInterface(*iface, (1 == iface_count)))
                    return iface;
                else
                    return NULL;
            }
            else
            {
                PLOG(PL_ERROR, "Node::Init() new CdsGraph::Interface() error: %s\n",
                     GetErrorString());
                return NULL;
            }
        }       
        
        unsigned int GetInterfaceCount() const
            {return iface_count;}         

        unsigned int GetId() const
        {
            CdsGraph::Interface* iface = GetDefaultInterface();
            if (NULL != iface)
            {
                return (iface->GetAddress().GetEndIdentifier());
            }
            else
            {
                ASSERT(0);
                return ((unsigned int)-1);
            }
        }
        
        // We always give our nodes a default interface
        const ProtoAddress& GetAddress() const
            {return GetDefaultInterface()->GetAddress();}
        
        const char* GetName() const
            {return node_name;}

        void SetRtrPriority(UINT8 value)
            {rtr_priority = value;}
        UINT8 GetRtrPriority() const
            {return rtr_priority;}

        void SetRelayStatus(bool state)
            {relay_status = state;}
        bool GetRelayStatus() const
            {return relay_status;}

        void SetVisited(bool state)
            {visited = state;}
        bool WasVisited() const
            {return visited;}
        
        unsigned int GetDegree();  // counts each neighbor only once regardless of redundant links
        unsigned int GetTotalDegree();  // counts neighbors with redundant links multiple time 
        
        
        class Queue : public ProtoSimpleQueueTemplate<Node> {};
        
        class NeighborIterator : public NetGraph::NodeTemplate<CdsInterface>::NeighborIterator
        {
            public:
                NeighborIterator(Node& node) : NetGraph::NodeTemplate<CdsInterface>::NeighborIterator(node) {}
                virtual ~NeighborIterator() {visited_queue.Empty();}
                
                Node* GetNextNeighbor()
                {
                    Node* nextNeighbor = NULL;
                    CdsInterface* nextIface;
                    while (NULL != (nextIface = GetNextNeighborInterface()))
                    {
                        if ((NULL != nextIface) && !visited_queue.Contains(nextIface->GetNode()))
                        {
                            nextNeighbor = &(nextIface->GetNode());
                            visited_queue.Append(*nextNeighbor);
                            break;
                        }
                    }
                    return nextNeighbor;
                }
                
                void Reset()
                {
                    NetGraph::NodeTemplate<CdsInterface>::NeighborIterator::Reset();
                    visited_queue.Empty();
                }
            private:
                Queue   visited_queue;
        };  // end class Node::NeighborIterator
        
         // ProtoTree::Item overrides so nodes
        // can be cached by name
        const char* GetKey() const
            {return node_name;}   
        unsigned int GetKeysize() const
            {return node_name_bits;} 

    private:
        char*           node_name;
        unsigned int    node_name_bits;
        unsigned int    node_id;
        unsigned int    iface_count;
        UINT8           rtr_priority;
        bool            relay_status;
        bool            visited;

};  // end class Node

class NodeTree : public ProtoIndexedQueueTemplate<Node> 
{
    private:
        virtual const char* GetKey(const Item& item) const
            {return static_cast<const Node&>(item).GetKey();}
        virtual unsigned int GetKeysize(const Item& item) const
            {return static_cast<const Node&>(item).GetKeysize();}
};
typedef Node::Queue NodeQueue;

Node::Node()
 : node_name(NULL), node_name_bits(0), iface_count(0),
   rtr_priority(0)
{
}

Node::~Node()
{
}

bool Node::Init(UINT32 nodeId, const char* nodeName)
{
    if (NULL != nodeName)
    {
        if (NULL != node_name) delete[] node_name;
        size_t nameLen = strlen(nodeName) + 1;
        if (NULL == (node_name = new char[nameLen]))
        {
            PLOG(PL_ERROR, "Node::Init() new node_name error: %s\n",
                 GetErrorString());
            return false;
        }
        strcpy(node_name, nodeName);
        node_name_bits = nameLen << 3;
    }
    node_id = nodeId;
    return true;
}  // end Node::Init()

unsigned int Node::GetTotalDegree() 
{
    unsigned int count = 0;
    InterfaceIterator it(*this);
    CdsInterface* iface;
    while (NULL != (iface = it.GetNextInterface()))
        count += iface->GetAdjacencyCount();
    return count;
}  // end Node::GetTotalDegree() 

unsigned int Node::GetDegree() 
{
    unsigned int count = 0;
    NeighborIterator nit(*this);
    Node* neighbor;
    while (NULL != (neighbor = nit.GetNextNeighbor()))
    {
        count += 1;
    }
    return count;
}  // end Node::GetTotalDegree() 


UINT32 CdsInterface::GetNodeId() const
{
    return GetNode().GetId();
}  // end CdsInterface::GetNodeId()

unsigned int CdsInterface::GetDegree() const
{
    unsigned int result = GetAdjacencyCount();
    result += GetNode().GetInterfaceCount() - 1;
    return result;   
}  // end CdsInterface::GetDegree()         
            


// This class encapsulates most of the functionality of
// our "gr" (GraphRider) application.  I bothered to do
// this in case we want to re-use this elsewhere.

class GraphRider
{
    public:
        GraphRider();
        ~GraphRider();
        
        bool SetInputFile(const char* fileName)
            {return fast_reader.SetFile(fileName);}
        
        // This updates the "graph" with data from the next time "epoch"
        // from our SDT input file
        double ReadNextEpoch();
        
        CdsGraph& AccessGraph()
            {return graph;}
        
        NodeTree& AccessNodeTree()
            {return node_tree;}
        
        static int CalculateFullECDS(NodeTree&                                   nodeTree, 
                                     CdsGraph&                                   graph,
                                     bool                                        useDegree = false,
                                     ProtoGraph::Vertice::SortedList::ItemPool*  sortedVerticeItemPool = NULL);
        
        static int CalculateFullMECDS(NodeTree&                                  nodeTree, 
                                      CdsGraph&                                  graph,
                                      bool                                       useDegree,
                                      ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool);
        
        static double CalculateDensity(CdsGraph& graph);
        
        // For coloring links, etc    
        static const char* COLOR[8];
        
        // name indices into our COLOR array
        enum Color
        {
            GREEN,
            RED,
            BLUE,
            PURPLE,
            ORANGE,
            PINK,
            WHITE,
            GRAY
        };
        
        class LinkType : public ProtoTree::Item
        {
            public:
                LinkType(const char* name)
                 : rtr_priority(0), link_color(GRAY)
                {
                    unsigned int len = strlen(name);
                    if (len > 31) len = 31;
                    strncpy(link_name, name, len);
                    link_name[31] = '\0';
                    link_name_bits = len << 3;
                }
                
                const char* GetName() const
                    {return link_name;}

                void SetRtrPriority(UINT8 value)
                    {rtr_priority = value;}
                UINT8 GetRtrPriority() const
                    {return rtr_priority;}
                
                void SetColor(Color theColor)
                    {link_color = theColor;}
                const char* GetColorName() const
                    {return COLOR[link_color];}
            
            private:
                const char* GetKey() const {return link_name;}
                unsigned int GetKeysize() const {return link_name_bits;}
                
                char         link_name[32];
                unsigned int link_name_bits;
                unsigned int rtr_priority;
                Color        link_color;
                
        };  // end class GraphRider::LinkType
        
        class LinkTypeTable : public ProtoTreeTemplate<LinkType> {};
        
        LinkType* AddLinkType(const char* name);
        LinkType* GetLinkType(const char* name)
            {return link_table.FindString(name);}
        
    private:
        // "FastReader" is handy class I use for doing
        //  buffered (fast) reading of a usually text input file.
        class FastReader
        {
            public:
                enum Result {OK, ERROR_, DONE, TIMEOUT};
                FastReader(FILE* filePtr = NULL);
                ~FastReader();
                
                bool SetFile(const char* fileName)
                {
                    if (NULL != file_ptr) fclose(file_ptr);
                    return (NULL != (file_ptr = fopen(fileName, "r")));
                }
                
                Result Read(char*           buffer,
                            unsigned int*   len,
                            double timeout = -1.0);
                
                Result Readline(char*           buffer,
                                unsigned int*   len,
                                double          timeout = -1.0);

                bool Seek(int offset);

            private:
                enum {BUFSIZE = 2048};
                FILE*        file_ptr;
                char         savebuf[BUFSIZE];
                char*        saveptr;
                unsigned int savecount;
        };  // end class GraphRider::FastReader
       
        // member variables
        FastReader                                  fast_reader;
        CdsGraph                                    graph;  
        NodeTree                                    node_tree;
        UINT32                                      node_id_index;
        unsigned int                                iface_id_index;
        LinkTypeTable                               link_table;
        unsigned int                                link_color_index;
        double                                      next_epoch_time; 
        unsigned int                                input_line_num;
                        
};  // end class GraphRider

GraphRider::GraphRider()
 : node_id_index(0), iface_id_index(0), link_color_index(0), next_epoch_time(0.0), input_line_num(0)
{
}

GraphRider::~GraphRider()
{
    link_table.Destroy();
}

const char* GraphRider::COLOR[8] =
{
    "green",
    "red",
    "blue",
    "orange",
    "pink",
    "purple",
    "gray",
    "white",
};
    
static bool IsNumber(const char* name)
{
    for (int i = 0 ; i < strlen(name); i++)
    {
        if (!isdigit(name[i])) return false;
    }
    return true;
}  // end IsNumber() 
    
GraphRider::LinkType* GraphRider::AddLinkType(const char* name)
{
    LinkType* linkType = new LinkType(name);
    if (NULL == linkType)
    {
        PLOG(PL_ERROR, "GraphRider::AddLinkType() new LinkType error: %s\n", GetErrorString());
        return NULL;
    }
    if (IsNumber(name))
    {
        if (atoi(name))
            linkType->SetColor((Color)7);
        else
            linkType->SetColor((Color)6);
        //TRACE("Mapping link type %s to color %s\n", name, COLOR[7]);
    }
    else
    {
        linkType->SetColor((Color)link_color_index);
        TRACE("Mapping link type %s to color %s\n", name, COLOR[link_color_index]);
        link_color_index = (link_color_index + 1) % 8;
    }
    link_table.Insert(*linkType);
    return linkType;
}  // end GraphRider::AddLinkType()

// returns the start time (in seconds) of the "epoch"
double GraphRider::ReadNextEpoch()
{
    double lastTime = next_epoch_time;
    // Read the SDT "node" from the file, finding new nodes and 
    // process SDT "link" and "unlink" commands to determine 
    // graph connectivity
    bool gotLine = false; // this is set to false if we find any useful content
    bool reading = true;
    while (reading)
    {
        char buffer[MAX_LINE];
        unsigned int len = MAX_LINE;
        switch (fast_reader.Readline(buffer, &len))
        {
            case GraphRider::FastReader::OK:
                break;
            case GraphRider::FastReader::ERROR_:
                PLOG(PL_ERROR, "gr: error reading file\n");
                return -1.0;
            case GraphRider::FastReader::DONE:
                reading = false;
                continue;
            case GraphRider::FastReader::TIMEOUT:
                return -1.0; // should never occur for this program
        }
        
        input_line_num++;
        gotLine = true;
        
        // pass the input line through to STDOUT (except "symbol" lines that "graphRider" will generate its own)
        if (NULL == strstr(buffer, "symbol"))
            printf("%s\n", buffer);

        // Is this line the start of a new "epoch"?
        double time;
        if (1 == sscanf(buffer, "wait %lf\n", &time))
        {
            // It's an SDT "wait" command ...
            next_epoch_time += time;
            return lastTime;
        }
        
        char nameString[256];
        if (1 == sscanf(buffer, "node %s", nameString))
        {
            // Do we know this node already?
            unsigned int nameBits = (unsigned int)(strlen(nameString)+1) << 3;
            Node* node = static_cast<Node*>(node_tree.Find(nameString, nameBits));
            if (NULL == node)
            {
                // It's a new node
                unsigned int nodeId = node_id_index++;
                // Create and insert new node into the "graph" and "node_tree"
                Node* node = new Node();
                if (!node->Init(nodeId, nameString))
                {
                    PLOG(PL_ERROR, "gr error: Node initialization failure!\n");
                    return -1.0;
                }
                graph.InsertNode(*node);
                node_tree.Insert(*node);
            }       
        }
        else
        {
            ProtoTokenator tk(buffer, ' ', true);
            const char* cmd = tk.GetNextItem();
            if (NULL == cmd) continue;
            bool link; 
            if (0 == strcmp(cmd, "link"))
                link = true;
            else if (0 == strcmp(cmd, "unlink"))
                link = false;
            else
                continue;
            const char* value = tk.GetNextItem();  // this should be the link or unlink command comma-delimited value
            if (NULL == value)
            {
                PLOG(PL_ERROR, "gr error: malformed \"%s\" command in input file at line %lu!\n", 
                               link ? "link" : "unlink", input_line_num);
                return -1.0;
            } 
            ProtoTokenator tk2(value, ',', true);
            const char* src = tk2.GetNextItem(true);  // detach to keep it
            const char* dst = tk2.GetNextItem();  
            if ((NULL == src) || (NULL == dst))
            {
                PLOG(PL_ERROR, "gr error: malformed \"%s\" command in input file at line %lu!\n", 
                               link ? "link" : "unlink", input_line_num);
                if (NULL != src) delete[] src;
                return -1.0;
            }
            Node* node1 = static_cast<Node*>(node_tree.Find(src, (strlen(src)+1) << 3));
            Node* node2 = static_cast<Node*>(node_tree.Find(dst, (strlen(dst)+1) << 3));
            delete[] src;  // not needed any further
            if ((NULL == node1) || (NULL == node2))
            {
                PLOG(PL_ERROR, "gr error: unknown nodes in \"%s\" command in input file at line %lu!\n", 
                               link ? "link" : "unlink", input_line_num);
                return -1.0;
            }
            const char* linkName = tk2.GetNextItem();
            CdsGraph::Interface* iface1;
            CdsGraph::Interface* iface2;
            if (NULL == linkName) linkName = "default";
            
            LinkType* linkType = GetLinkType(linkName);
            if ((NULL == linkType) && (NULL == (linkType = AddLinkType(linkName))))
            {
                PLOG(PL_ERROR, "gr error: unable to add link type\n");
                return -1.0;
            }            
            iface1 = node1->FindInterfaceByName(linkName);
            if (NULL == iface1) 
                iface1 = node1->AddInterface(linkName, ++iface_id_index);
            iface2 = node2->FindInterfaceByName(linkName);
            if (NULL == iface2) 
                iface2 = node2->AddInterface(linkName, ++iface_id_index);
            if ((NULL == iface1) || (NULL == iface2))
            {
                PLOG(PL_ERROR, "gr new interface error: %s\n", GetErrorString());
                return -1.0;
            }
            else if (NULL != linkType)
            {
                iface1->SetRtrPriority(linkType->GetRtrPriority());
                iface2->SetRtrPriority(linkType->GetRtrPriority());
            }
            // TBD - check to see if graph actually was changed?
            if (link)
            {
                CdsGraph::SimpleCostDouble cost(1.0);
                TRACE("gr: connecting %s/%s->%s/%s\n", node1->GetName(), linkName, node2->GetName(), linkName);
                if (!graph.Connect(*iface1, *iface2, cost, true))
                    PLOG(PL_ERROR, "gr error: unable to connect interfaces in graph\n");
            }
            else
            {
                graph.Disconnect(*iface1, *iface2, true);
            }
            node1->GetDegree();
            node2->GetDegree();
        }
    }  // end while reading()
    return (gotLine ? lastTime : -1.0);
}  // end ReadNextEpoch()

static int ComparePriority(unsigned int priority1, unsigned int degree1, const ProtoAddress& addr1,      
                           unsigned int priority2, unsigned int degree2, const ProtoAddress& addr2)     
{
    // returns 1 if p1 > p2, -1 if p1 < p2, and 0 if equal
    if (priority1 > priority2)
        return 1;
    else if (priority1 < priority2)
        return -1;
    else if (degree1 > degree2)
        return 1;
    else if (degree1 < degree2)
        return -1;
    else if (addr1 > addr2)
        return 1;
    else if (addr1 < addr2)
        return -1;
     else  // equal
     {
        return 0;
     }
}  // end  ComparePriority()
       
int GraphRider::CalculateFullMECDS(NodeTree&                                   nodeTree, 
                                    CdsGraph&                                  graph,
                                    bool                                       useDegree,
                                    ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool)
{
    bool collapseNodes = false;
    unsigned int K = 2;
        ;
    
    // Now that we have a fully updated "graph", perform Relay Set Selection
    // algorithm for each node in graph
    int numberOfRelays = 0;
    NodeTree::Iterator noderator(nodeTree);
    Node* node;
    while (NULL != (node = noderator.GetNextItem()))
    {
        
        CdsInterface* ifacex = node->GetDefaultInterface();
        ASSERT(NULL != ifacex);
        CdsGraph::SimpleTraversal bfs(graph, *ifacex, true, collapseNodes);
        unsigned int level;
        CdsInterface* ifacey;
        while (NULL != (ifacey = bfs.GetNextInterface(&level)))
        {
            TRACE("Test bfs traversal from %s/%s to %s/%s at level %u...\n", 
                  ifacex->GetNode().GetName(), ifacex->GetName(), 
                  ifacey->GetNode().GetName(), ifacey->GetName(), level);
        }
        
        node->SetRelayStatus(false);
        unsigned int degree = node->GetDegree();
        TRACE("\nMECDS check for node %s priority:%d degree:%d id:%d...\n", 
              node->GetName(), node->GetRtrPriority(), degree, node->GetId());
        
        // First, do the E-CDS algorithm Step 1,2 for the "node" to see 
        // if it is a leaf node. (This is needed so that the altered
        // E-CDS Step 1,2 below for each iface works properly)
        CdsInterface* iface;
        Node::InterfaceIterator it(*node);
        if (degree < 2)
        {
            // We are a leaf node, so none of our interfaces will be relays
            while (NULL != (iface = it.GetNextInterface()))
                iface->SetRelayStatus(false);
            continue;
        } 
        if (!useDegree) degree = 0;
        
        // E-CDS Steps 3-9 are "compressed" for a "node" because, by definition, it has
        // a larger router priority than any of its interfaces.  So, we defer marking
        // the node as a relay.  If _any_ of its interfaces are selected as relay, then
        // the node gets marked as a relay, too.  So, we init the node's relay
        // status to "false", knowing it will be marked as needed later.
        
        // Now do E-CDS for each of the candidate relay node's interfaces separately
        it.Reset();
        while (NULL != (iface = it.GetNextInterface()))
        {
            TRACE("   MECDS check for iface: %s/%s (%d:%d:%d) ...\n", node->GetName(), iface->GetName(),
                    iface->GetRtrPriority(), degree, iface->GetAddress().GetEndIdentifier());
            degree = iface->GetDegree();
            //degree = iface->GetNode().GetTotalDegree();
            
            //TRACE("   MECDS Steps 1-2 iface: %s/%s (%d:%d:%d) ...\n", node->GetName(), iface->GetName(),
            //        iface->GetRtrPriority(), degree, iface->GetAddress().GetEndIdentifier());
            
            // E-CDS Step 1,2
            if (0 == degree)
            {
                TRACE("   MECDS isabling iface %s/%s degree %d per E-CDS step 2\n", node->GetName(), iface->GetName(), degree);
                iface->SetRelayStatus(false); // 'leaf' interface
                continue;
            }
            
            //TRACE("   MECDS Steps 3-6 iface: %s/%s (%d:%d:%d) ...\n", node->GetName(), iface->GetName(),
            ///        iface->GetRtrPriority(), degree, iface->GetAddress().GetEndIdentifier());
            
            if (!useDegree) degree = 0;
            // E-CDS Step 3, 5, and 6, mark one-hop and two-hop neighbors as unvisited.
            // (Ee set 3-hop neighbors as visited to mark as out-of-bounds.)
            iface->SetVisited(false);
            UINT8 priority = iface->GetRtrPriority();
            CdsInterface* ifaceN1Max = NULL;
            UINT8 priorityN1Max = priority;
            unsigned int degreeN1Max = degree;
            CdsGraph::SimpleTraversal bfs(graph, *iface, true, collapseNodes);
            CdsInterface* ifaceN1;
            unsigned int level;
            while (NULL != (ifaceN1 = bfs.GetNextInterface(&level)))
            {
                //TRACE("      bfs to %s/%s at level %u\n", ifaceN1->GetNode().GetName(), ifaceN1->GetName(), level);
                if (ifaceN1 == iface) continue;
                if (level < K)
                {
                    ifaceN1->SetVisited(false);
                    UINT8 priorityN1 = ifaceN1->GetRtrPriority();
                    unsigned int degreeN1 = useDegree ? ifaceN1->GetDegree() : 0;
                    //unsigned int degreeN1 = useDegree ? ifaceN1->GetNode().GetTotalDegree() : 0;
                    if (NULL == ifaceN1Max)
                    {
                        priorityN1Max  = priorityN1;
                        degreeN1Max = degreeN1;
                        ifaceN1Max = ifaceN1;
                    }
                    else 
                    {
                        /*TRACE("         compare %s/%s %d:%d:%d to %s/%s %d:%d:%d\n",
                              ifaceN1->GetNode().GetName(), ifaceN1->GetName(), priorityN1, degreeN1, ifaceN1->GetAddress().GetEndIdentifier(),
                              ifaceN1Max->GetNode().GetName(), ifaceN1Max->GetName(), priorityN1Max, degreeN1Max, ifaceN1Max->GetAddress().GetEndIdentifier());
                        */
                        if (ComparePriority(priorityN1, degreeN1, ifaceN1->GetAddress(),
                                            priorityN1Max, degreeN1Max, ifaceN1Max->GetAddress()) > 0)
                        {
                            priorityN1Max  = priorityN1;
                            degreeN1Max = degreeN1;
                            ifaceN1Max = ifaceN1;
                        }
                    }
                }
                else if (level < (K+1))
                {
                    ifaceN1->SetVisited(false);
                }
                else if ((K+1) == level)
                {
                    ifaceN1->SetVisited(true);
                }
                else
                {
                    break; // we're done
                }
            }
            
            TRACE("      MECDS %s/%s n1_max: %s/%s (%d:%d:%d)\n", node->GetName(), iface->GetName(), ifaceN1Max->GetNode().GetName(), ifaceN1Max->GetName(),
                         priorityN1Max, degreeN1Max, ifaceN1Max->GetAddress().GetEndIdentifier());
            // E-CDS Step 4 decision
            //if (ifaceN1Max == iface)
            
            /*TRACE("   MECDS Step 4 compare %s/%s %d:%d:%d to %s/%s %d:%d:%d\n",
                  iface->GetNode().GetName(), iface->GetName(), priority, degree, iface->GetAddress().GetEndIdentifier(),
                  ifaceN1Max->GetNode().GetName(), ifaceN1Max->GetName(), degreeN1Max, degree, ifaceN1Max->GetAddress().GetEndIdentifier());
            */
            if (ComparePriority(priority, degree, iface->GetAddress(),
                                priorityN1Max, degreeN1Max, ifaceN1Max->GetAddress()) > 0)
            {
                TRACE("   MECDS ENABLING iface %s/%s degree %d per step 4\n", node->GetName(), iface->GetName(), degree);
                iface->SetRelayStatus(true);
                node->SetRelayStatus(true);
                numberOfRelays += 1;
                continue;
            }
            //TRACE("   MCDS Step 7 (ifaceN1Max: %s/%s)...\n", ifaceN1Max->GetNode().GetName(), ifaceN1Max->GetName());
            // E-CDS Step 7
            CdsGraph::Interface::SimpleList Q;
            Q.Append(*ifaceN1Max);
            ifaceN1Max->SetVisited(true);
            // E-CDS Step 8
            //TRACE("   MCDS Step 8 ...\n");
            CdsInterface* x;
            while (NULL != (x = Q.RemoveHead()))
            {
                //TRACE("      evaluating %s/%s ...\n", x->GetNode().GetName(), x->GetName());
                CdsGraph::SimpleTraversal bfs1(graph, *x, true, collapseNodes);
                CdsInterface* n;
                unsigned int level;
                while (NULL != (n = bfs1.GetNextInterface(&level)))
                {
                    if (n == x) continue;
                    //TRACE("        checking %s/%s level %u ...\n", n->GetNode().GetName(), n->GetName(), level);
                    if (level >= K) break;
                    if (n == iface) continue;
                    if (n->WasVisited()) 
                    {
                        //TRACE("         already visited %s/%s ...\n", n->GetNode().GetName(), n->GetName());
                        continue; 
                    }
                    
                    n->SetVisited(true);
                    UINT8 priorityN = n->GetRtrPriority();
                    unsigned int degreeN = useDegree ? n->GetDegree() : 0;
                    //unsigned int degreeN = useDegree ? n->GetNode().GetTotalDegree() : 0;
                    
                    /*TRACE("         VISITED %s/%s %d:%d:%d and comparing to %s/%s %d:%d:%d\n",
                           n->GetNode().GetName(), n->GetName(), priorityN, degreeN, n->GetAddress().GetEndIdentifier(),
                           iface->GetNode().GetName(), iface->GetName(), priority, degree, iface->GetAddress().GetEndIdentifier());
                    */
                    if (ComparePriority(priorityN, degreeN, n->GetAddress(),
                                        priority, degree, iface->GetAddress()) > 0)
                    {
                        Q.Append(*n);
                    }
                }
                
                /*CdsGraph::AdjacencyIterator adjacerator(*x);
                CdsInterface* n;
                while (NULL != (n = adjacerator.GetNextAdjacency()))
                {
                    if (n == iface) continue;
                    if (n->WasVisited()) continue;
                    n->SetVisited(true);
                    UINT8 priorityN = n->GetRtrPriority();
                    unsigned int degreeN = useDegree ? n->GetDegree() : 0;
                    TRACE("  compare %s/%s %d:%d:%d to %s/%s %d:%d:%d\n",
                            n->GetNode().GetName(), n->GetName(), priorityN, degreeN, n->GetAddress().GetEndIdentifier(),
                            iface->GetNode().GetName(), iface->GetName(), priority, degree, iface->GetAddress().GetEndIdentifier());
                    if (ComparePriority(priorityN, degreeN, n->GetAddress(),
                                        priority, degree, iface->GetAddress()) > 0)
                    {
                        Q.Append(*n);
                    }
                }*/
            }
            
            // E-CDS Step 9
            ///TRACE("   MCDS Step 9 ...\n");
            bfs.Reset();
            bool isRelay = false;
            while (NULL != (ifaceN1 = bfs.GetNextInterface(&level)))
            {
                if (ifaceN1 == iface) continue;
                if (level >= K) break;
                //if ((ifaceN1->GetDegree() > 0) && !ifaceN1->WasVisited())
                if (!ifaceN1->WasVisited())
                {
                    //TRACE("Interface %s/%s was NOT visited\n", ifaceN1->GetNode().GetName(), ifaceN1->GetName());
                    TRACE("   MECDS ENABLING iface %s/%s degree %d per step 9\n", node->GetName(), iface->GetName(), degree);
                    iface->SetRelayStatus(true);
                    node->SetRelayStatus(true);
                    numberOfRelays += 1;
                    isRelay = true;
                    break;
                }
            }
            if (!isRelay)
            {
                TRACE("   MECDS Disabling iface %s/%s degree %d per step 9\n", node->GetName(), iface->GetName(), degree);
                iface->SetRelayStatus(false);
            }
        }  // end while (NULL != iface ...)
    }  // end while (NULL != node ...)
    return numberOfRelays;
}  // end GraphRider::CalculateFullMECDS()
        
// "relayList" is filled with the selected relays
int GraphRider::CalculateFullECDS(NodeTree&                                   nodeTree, 
                                  CdsGraph&                                   graph,
                                  bool                                        useDegree,
                                  ProtoGraph::Vertice::SortedList::ItemPool*  sortedVerticeItemPool)
{
    bool collapseNodes = false;
    // Now that we have a fully updated "graph", perform Relay Set Selection
    // algorithm for each node in graph
    int numberOfRelays = 0;
    NodeTree::Iterator noderator(nodeTree);
    Node* node;
    while (NULL != (node = noderator.GetNextItem()))
    {
        node->SetVisited(false);
        UINT8 priority = node->GetRtrPriority();
        unsigned int degree =  node->GetDegree();
        TRACE("ECDS check for node %s priority:%d:%d degree:%d...\n", node->GetName(), priority, node->GetId(), degree);
        // E-CDS Steps 1,2
        if (degree < 2)
        {
            // We are a leaf node
            TRACE("   ECDS disabling node %s as relay per Step 2\n", node->GetName());
            node->SetRelayStatus(false);
            Node::InterfaceIterator it(*node);
            CdsInterface* iface;
            while (NULL != (iface = it.GetNextInterface()))
                iface->SetRelayStatus(false);
            continue;
        } 
        if (!useDegree) degree = 0;
        
        
        //TRACE("   ECDS Steps 3-6 for node %s ...\n", node->GetName());
        // E-CDS Step 3 is implicit as we already have the info in our graph
        
        // E-CDS Steps 4 (Find nodeN1Max) and 5 (mark N1 and N2 as unvisited)
        Node* nodeN1Max = NULL;
        UINT8 priorityN1Max = 0;
        unsigned int degreeN1Max = 0;
        
        CdsGraph::SimpleTraversal bfs(graph, *node->GetDefaultInterface(), true, collapseNodes);
        CdsInterface* ifaceN1;
        unsigned int level;
        Node::Queue visited;
        while (NULL != (ifaceN1 = bfs.GetNextInterface(&level)))
        {
            Node& nodeN1 = ifaceN1->GetNode();
            //TRACE("   to %s/%s at level %u\n", nodeN1.GetName(), ifaceN1->GetName(), level);
            if (&nodeN1 == node) continue;
            if (visited.Contains(nodeN1))
                continue;
            else
                visited.Append(nodeN1);
            if (level < 2)
            {
                nodeN1.SetVisited(false);
                UINT8 priorityN1 = nodeN1.GetRtrPriority();
                unsigned int degreeN1 = useDegree ? nodeN1.GetDegree() : 0;
                if (NULL == nodeN1Max)
                {
                    priorityN1Max  = priorityN1;
                    degreeN1Max = degreeN1;
                    nodeN1Max = &nodeN1;
                }
                else 
                {
                    /*TRACE("  compare %s/%s %d:%d:%d to %s/%s %d:%d:%d\n",
                            nodeN1.GetName(), nodeN1.GetDefaultInterface()->GetName(), priorityN1, degreeN1, nodeN1.GetAddress().GetEndIdentifier(),
                            nodeN1Max->GetName(), nodeN1Max->GetDefaultInterface()->GetName(), priorityN1Max, degreeN1Max, nodeN1Max->GetAddress().GetEndIdentifier());
                    */
                    if (ComparePriority(priorityN1, degreeN1, nodeN1.GetAddress(),
                                        priorityN1Max, degreeN1Max, nodeN1Max->GetAddress()) > 0)
                    {
                        priorityN1Max  = priorityN1;
                        degreeN1Max = degreeN1;
                        nodeN1Max = &nodeN1;
                        //TRACE("  update max to %p\n", nodeN1Max);
                    }
                }
            }
            else if (level < 3)
            {
                nodeN1.SetVisited(false);
            }
            else if (3 == level)
            {
                nodeN1.SetVisited(true);  // Marking 3-hop neighbors as "visited" established order of our 2-hop neighborhood
            }
            else
            {
                break; // we're done
            }
        }  // end while bfs
        visited.Empty();
        
        if (NULL == nodeN1Max)
            continue;
        
        TRACE("   ECDS %s n1_max: %s (%d:%d:%d)\n", node->GetName(), nodeN1Max->GetName(), priorityN1Max, degreeN1Max, nodeN1Max->GetAddress().GetEndIdentifier());
        
        
        if (ComparePriority(priority, degree, node->GetAddress(),
                            priorityN1Max, degreeN1Max, nodeN1Max->GetAddress()) > 0)
        {
            //TRACE("node:%p max:%p\n", node, nodeN1Max);
            TRACE("   ECDS ENABLING node %s as relay per Step 4 p:%d/%d pmax:%s/%d/%d \n", 
                  node->GetName(), priority, degree, 
                  nodeN1Max->GetName(), priorityN1Max, degreeN1Max);
            node->SetRelayStatus(true);  // // selected per E-CDS Step 4
            Node::InterfaceIterator it(*node);
            CdsInterface* iface;
            while (NULL != (iface = it.GetNextInterface()))
                iface->SetRelayStatus(true);
            numberOfRelays++;
            continue;
        }
        // E-CDS Step 7
        ASSERT(NULL != nodeN1Max);
        NodeQueue Q;
        nodeN1Max->SetVisited(true);
        Q.Append(*nodeN1Max);
        Node* x;
        // E-CDS Step 8
        while (NULL != (x = Q.RemoveHead()))
        {
            //bool xIsInN1 = x->IsSymmetricNeighbor(*node); // true if 'x' is 1-hop neighbor of "n0"
            Node::NeighborIterator nit(*x);
            Node* n;
            while (NULL != (n = nit.GetNextNeighbor()))
            {
                if (n == node) continue;
                if (!n->WasVisited())
                {
                    n->SetVisited(true);
                    UINT8 priorityN = n->GetRtrPriority();
                    unsigned int degreeN = useDegree ? n->GetDegree() : 0;
                    if (ComparePriority(priorityN, degreeN, n->GetAddress(),
                                        priority, degree, node->GetAddress()) > 0)
                    {
                        Q.Append(*n);
                    }   
                }
            }
        }   
        // E-CDS Step 9
        node->SetRelayStatus(false);
        Node::NeighborIterator nit(*node);  
        Node* nodeN1;  
        bool isRelay = false;  
        while (NULL != (nodeN1 = nit.GetNextNeighbor()))
        {
            if (!nodeN1->WasVisited())
            {
                TRACE("   ECDS ENABLING node %s as relay per Step 9\n", node->GetName());
                isRelay = true;
                node->SetRelayStatus(true);
                Node::InterfaceIterator it(*node);
                CdsInterface* iface;
                while (NULL != (iface = it.GetNextInterface()))
                    iface->SetRelayStatus(true);
                numberOfRelays++;
                break;
            }
        }  
        if (!isRelay)
        {
            TRACE("   ECDS disabling node %s as relay per Step 9\n", node->GetName());
            node->SetRelayStatus(false);
            Node::InterfaceIterator it(*node);
            CdsInterface* iface;
            while (NULL != (iface = it.GetNextInterface()))
                iface->SetRelayStatus(false);
        }
         
    }  // end while (NULL != node ...)
    return numberOfRelays;
}  // end GraphRider::CalculateFullECDS()

double GraphRider::CalculateDensity(CdsGraph& graph)
{
    int neighborCount = 0;
    int nodeCount = 0;
    CdsGraph::InterfaceIterator it(graph);
    CdsGraph::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        CdsGraph::AdjacencyIterator iteratorN1(*iface);
        nodeCount++;
        while (NULL != (iteratorN1.GetNextAdjacency()))
            neighborCount++;
    }
    return ((double)neighborCount)/((double)nodeCount);
}  // end GraphRider::CalculateDensity()

//////////////////////////////////////////////////////////////////////////////////////////
// "GraphRider::FastReader" implementation
GraphRider::FastReader::FastReader(FILE* filePtr)
 : file_ptr(filePtr), savecount(0)
{
}

GraphRider::FastReader::~FastReader()
{
    if (NULL != file_ptr)
    {
        fclose(file_ptr);
        file_ptr = NULL;
    }
}

GraphRider::FastReader::Result GraphRider::FastReader::Read(char*           buffer,
                                                            unsigned int*   len,
                                                            double          timeout)
{
    unsigned int want = *len;
    if (savecount)
    {
        unsigned int ncopy = MIN(want, savecount);
        memcpy(buffer, saveptr, ncopy);
        savecount -= ncopy;
        saveptr += ncopy;
        buffer += ncopy;
        want -= ncopy;
    }
    while (want)
    {
        unsigned int result;
#ifndef WIN32 // no real-time input for WIN32 yet
        if (timeout >= 0.0)
        {
            int fd = fileno(file_ptr);
            fd_set input;
            FD_ZERO(&input);
            struct timeval t;
            t.tv_sec = (unsigned long)timeout;
            t.tv_usec = (unsigned long)((1.0e+06 * (timeout - (double)t.tv_sec)) + 0.5);
            FD_SET(fd, &input);
            int status = select(fd+1, &input, NULL, NULL, &t);
            switch (status)
            {
                case -1:
                    if (EINTR != errno)
                    {
                        perror("trpr: GraphRider::FastReader::Read() select() error");
                        return ERROR_;
                    }
                    else
                    {
                        continue;
                    }
                    break;

                case 0:
                    return TIMEOUT;

                default:
                    result = fread(savebuf, sizeof(char), 1, file_ptr);
                    break;
            }
        }
        else
#endif // !WIN32
        {
            // Perform buffered read when there is no "timeout"
            result = fread(savebuf, sizeof(char), BUFSIZE, file_ptr);
        }
        if (result)
        {
            
            // This check skips NULLs that have been read on some
            // use of trpr via tail from an NFS mounted file
            if (!isprint(*savebuf) &&
                    ('\t' != *savebuf) &&
                    ('\n' != *savebuf) &&
                    ('\r' != *savebuf))
                continue;
            unsigned int ncopy= MIN(want, result);
            memcpy(buffer, savebuf, ncopy);
            savecount = result - ncopy;
            saveptr = savebuf + ncopy;
            buffer += ncopy;
            want -= ncopy;
        }
        else  // end-of-file
        {
#ifndef WIN32
            if (ferror(file_ptr))
            {
                if (EINTR == errno) continue;
            }
#endif // !WIN32
            *len -= want;
            if (0 != *len)
                return OK;  // we read at least something
            else
                return DONE; // we read nothing
        }
    }  // end while(want)
    return OK;
}  // end GraphRider::FastReader::Read()

// An OK text readline() routine (reads what will fit into buffer incl. NULL termination)
// if *len is unchanged on return, it means the line is bigger than the buffer and
// requires multiple reads
GraphRider::FastReader::Result GraphRider::FastReader::Readline(char*         buffer,
                                                                    unsigned int* len,
                                                                    double        timeout)
{
    unsigned int count = 0;
    unsigned int length = *len;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int one = 1;
        switch (Read(ptr, &one, timeout))
        {
            case OK:
                if (('\n' == *ptr) || ('\r' == *ptr))
                {
                    *ptr = '\0';
                    *len = count;
                    return OK;
                }
                count++;
                ptr++;
                break;

            case TIMEOUT:
                // On timeout, save any partial line collected
                if (count)
                {
                    savecount = MIN(count, BUFSIZE);
                    if (count < BUFSIZE)
                    {
                        memcpy(savebuf, buffer, count);
                        savecount = count;
                        saveptr = savebuf;
                        *len = 0;
                    }
                    else
                    {
                        memcpy(savebuf, buffer+count-BUFSIZE, BUFSIZE);
                        savecount = BUFSIZE;
                        saveptr = savebuf;
                        *len = count - BUFSIZE;
                    }
                }
                return TIMEOUT;

            case ERROR_:
                return ERROR_;

            case DONE:
                return DONE;
        }
    }
    // We've filled up the buffer provided with no end-of-line
    return ERROR_;
}  // end GraphRider::FastReader::Readline()

bool GraphRider::FastReader::Seek(int offset)
{
    bool result = true;
    if (offset < 0)
    {
        if (0 != savecount)
        {
            int avail = saveptr - savebuf;
            if (avail >= abs(offset))
            {
                savecount += abs(offset);
                saveptr -= abs(offset);
                offset = 0;
            }
            else
            {
                offset -= savecount;
                savecount = 0;
            }
        }
        if (0 != offset)
            result = (0 == fseek(file_ptr, offset, SEEK_CUR));
    }
    else if (offset > 0)
    {
        if ((unsigned int)offset < savecount)
        {
            savecount -= offset;
            saveptr += offset;
        }
        else
        {
            if ((unsigned int)offset > savecount)
            {
                result = (0 == fseek(file_ptr, offset - savecount, SEEK_CUR));
            }
            savecount = 0;
        }
    }
    return result;
}  // end GraphRider::FastReader::Seek()


int main(int argc, char* argv[])
{
    GraphRider graphRider;
    const char* inputFile = NULL;
    bool useDegree = false;
    bool graphML = false;
    
    bool mecds = false;
    
    // Parse the command line
    int i = 1;
    while (i < argc)
    {
        size_t len = strlen(argv[i]);
        if (0 == strncmp(argv[i], "input", len))
        {
            if (++i >= argc)
            {
                fprintf(stderr, "gr error: missing \"input\" argument!\n");
                Usage();
                return -1;
            }
            inputFile = argv[i];
        }
        else if (0 == strncmp(argv[i], "degree", len))
        {
            useDegree = true;
        }
        else if (0 == strncmp(argv[i], "mecds", len))
        {
            mecds = true;
        }
        else if (0 == strncmp(argv[i], "link", len))
        {
            // link <name>,<priority>
            if (++i >= argc)
            {
                fprintf(stderr, "gr error: missing \"link\" argument!\n");
                Usage();
                return -1;
            }
            ProtoTokenator tk(argv[i], ',');
            const char* item = tk.GetNextItem();
            GraphRider::LinkType* linkType = graphRider.GetLinkType(item);
            if ((NULL == linkType) && (NULL == (linkType = graphRider.AddLinkType(item))))
            {
                fprintf(stderr, "gr new LinkTpye error: %s\n", GetErrorString());
                return -1;
            }
            item = tk.GetNextItem();
            if (NULL == item)
            {
                
                fprintf(stderr, "gr error: missing \"link priority\" argument!\n");
                Usage();
                return -1;
            }
            unsigned int priority;
            if (1 != sscanf(item, "%u", &priority))
            {
                fprintf(stderr, "gr error: invalid \"link priority\" argument!\n");
                Usage();
                return -1;
            }
            linkType->SetRtrPriority(priority);
        }
        else
        {
            fprintf(stderr, "gr error: invalid command: %s\n", argv[i]);
            Usage();
            return -1;
        }
        i++;
    }
    
    if (NULL == inputFile)
    {
        fprintf(stderr, "gr error: no input file specified!\n");
        Usage();
        return -1;
    }
    
    size_t len = strlen(inputFile);
    if ((len > 2) && (0 == strcmp("ml", inputFile+len-2)))
        graphML = true; 
    
    if (graphML)
    {
        
        ManetGraphML graph;
        if (!graph.Read(inputFile))
        {
            fprintf(stderr, "gr error: unable to parse GraphML input file!\n");
            return -1;
        }
        printf("gr: graphML loaded ...\n");
        ManetGraphML::InterfaceIterator ifaceIterator(graph);
        // Get first iface to init Dijkstra
        ManetGraphML::Interface* iface = ifaceIterator.GetNextInterface();
        ManetGraphML::DijkstraTraversal dijkstra(graph, iface->GetNode(), iface);        
        ifaceIterator.Reset();
        //std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        while (NULL != (iface = ifaceIterator.GetNextInterface()))
        {
            dijkstra.Reset(iface);
            //std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
            ManetGraphML::Interface* dface;
            while (NULL != (dface = dijkstra.GetNextInterface()))
            {
                const ManetGraphML::Cost* cost = dijkstra.GetCost(*dface);
                if (NULL != cost)
                     TRACE("   dijkstra iterated to iface \"%s\" cost:%lf\n", dface->GetName(), cost->GetValue());
            }
            //std::chrono::high_resolution_clock::time_point t3 = std::chrono::high_resolution_clock::now();
            //unsigned int duration = (unsigned int)(std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count());
            //TRACE("   duration: %u\n", duration);
        }
        //std::chrono::high_resolution_clock::time_point t4 = std::chrono::high_resolution_clock::now();
        //unsigned int duration = (unsigned int)(std::chrono::duration_cast<std::chrono::microseconds>(t4 - t1).count());
        //TRACE("total duration: %u\n", duration);
    }
    else 
    {
        NodeTree& nodeTree = graphRider.AccessNodeTree();
        CdsGraph& graph = graphRider.AccessGraph();
        if (!graphRider.SetInputFile(inputFile))
        {
            perror("gr error: unable to open input file");
            return -1;
        }
        // This is a pool of ProtoGraph::Vertice::SortedList::Items
        // that are used for temporary lists of CdsGraph::Interfaces
        // for various graph manipulations, etc.  Note that the use of an 
        // "external" item pool is _optional_ for the ProtoGraph/CdsGraph
        // list classes, but can boost performance by reducing memory
        // alloc/deallocs when doing a lot of list manipulation.
        // If a list was inited with a "pool", then it is important to
        // keep the "pool" valid until after any associate "lists" are
        // destroyed as "pools" do _not_ keep track of which lists are
        // using them (yet!).
        ProtoGraph::Vertice::SortedList::ItemPool sortedVerticeItemPool;

        while (graphRider.ReadNextEpoch() >= 0.0)
        {
            // We also build up a "disconnectedList" by first putting all nodes
            // into it and then remove the relays and their one-hop neighbors.
            // The disconnected nodes remaining in the list are colored "red"

            // a) Initialize our "disconnectedList" with all nodes in graph
            NodeQueue disconnectedList;
            NodeTree::Iterator nodeIterator(nodeTree);
            Node* node;
            while (NULL != (node = nodeIterator.GetNextItem()))
                disconnectedList.Append(*node);
            
            // "CalculateFullECDS" implements the ECDS relay set selection
            // algorithm, marking selected nodes using the Node::SetRelayStatus() 
            // method.
            if (mecds)
                GraphRider::CalculateFullMECDS(nodeTree, graph, useDegree, &sortedVerticeItemPool);
            else
                GraphRider::CalculateFullECDS(nodeTree, graph, useDegree, &sortedVerticeItemPool);
            
            // Iterate over our nodeTree and color the relays "purple" and their
            // (non-relay) one-hop neighbors "green" removing them from the
            // disconnectedList
            nodeIterator.Reset();
            while (NULL != (node = nodeIterator.GetNextItem()))
            {
                if (node->GetRelayStatus())
                {
                    printf("node %s symbol circle,purple,3\n", node->GetName());
                    if (disconnectedList.Contains(*node))
                        disconnectedList.Remove(*node);
                }
                else
                {
                    printf("node %s symbol circle,green,3\n", node->GetName());
                }
                // Color links from relays to other nodes thick and solid and links among non-relay nodes their link colors with skinny stipple
                CdsInterface* iface1;
                Node::InterfaceIterator it(*node);
                while (NULL != (iface1 = it.GetNextInterface()))
                {
                    CdsGraph::AdjacencyIterator adjacerator(*iface1);
                    CdsInterface* iface2;
                    while (NULL != (iface2 = adjacerator.GetNextAdjacency()))
                    {
                        GraphRider::LinkType* linkType = graphRider.GetLinkType(iface1->GetName());
                        ASSERT(NULL != linkType);
                        const char* linkColor = linkType->GetColorName();
                        //TRACE("iterating %s/%s->%s/%s ...\n", iface1->GetNode().GetName(), iface1->GetName(), iface2->GetNode().GetName(), iface2->GetName());
                        if (iface1->GetRelayStatus() || iface2->GetRelayStatus())
                        {
                            if (iface1->GetRelayStatus()) ASSERT(iface1->GetNode().GetRelayStatus());
                            if (iface2->GetRelayStatus()) ASSERT(iface2->GetNode().GetRelayStatus());
                            // sdt link usage:  link node1,node2,name line color,thickness,opacity,stipple
                            printf("link %s,%s,%s line %s,4,x,0\n", iface1->GetNode().GetName(), iface2->GetNode().GetName(), iface1->GetName(), linkColor);
                            Node& node2 = iface2->GetNode();
                            if (disconnectedList.Contains(node2))
                                disconnectedList.Remove(node2);
                        }
                        else
                        {
                            // Make sure non-relay link colors are returned to non-purple state.  
                            printf("link %s,%s,%s line %s,2,x,2,3855\n", iface1->GetNode().GetName(), iface2->GetNode().GetName(), iface1->GetName(), linkColor);
                        }
                    }
                }
            }
            // Color any nodes remaining in "disconnectedList" red (orphans and orphan pairs)  
            // Make sure link colors are returned to non-purple state.          
            NodeQueue::Iterator dcIterator(disconnectedList);
            while (NULL != (node = dcIterator.GetNextItem()))
            {
                disconnectedList.Remove(*node);
                printf("node %s symbol circle,red,3\n", node->GetName());
                CdsInterface* iface1;
                Node::InterfaceIterator it(*node);
                while (NULL != (iface1 = it.GetNextInterface()))
                {
                    CdsGraph::AdjacencyIterator adjacerator(*iface1);
                    CdsInterface* iface2;
                    while (NULL != (iface2 = adjacerator.GetNextAdjacency()))
                    {
                        GraphRider::LinkType* linkType = graphRider.GetLinkType(iface1->GetName());
                        ASSERT(NULL != linkType);
                        const char* linkColor = linkType->GetColorName();
                        printf("link %s,%s,%s line %s,2,x,2,3855\n", iface1->GetNode().GetName(), iface2->GetNode().GetName(), iface1->GetName(), linkColor);
                    }
                }
            }
            //double nodeDensity = GraphRider::CalculateDensity(graph);
        }  // end while ReadNextEpoch()
    }  // end if/else graphML
    fprintf(stderr, "gr: Done.\n");
    return 0;
}  // end main()

