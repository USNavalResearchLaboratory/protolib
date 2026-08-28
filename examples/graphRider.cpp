
// GraphRider (gr) program
// This program loads a graph from an existing SDT file or GraphML file.  For SDT, it builds
// the graph by inserting Nodes corresponding to SDT nodes and connects the graph according
// to SDT "link" commands. Note that SDT scripts can dynamically update the graph state over
// time and hence this program updates the graph in "snapshots" between SDT "wait" commands.
// Some ECDS relay set selection algorithms are currently implemented here.  The SDT input
// is essentially passed through but modifies the coloring of SDT nodes to illustrate
// the relay set selection.

#include "manetGraph.h"
#include "manetGraphML.h"
#include "protoSpace.h"  // we use this for its bounding box iteration
#include "protoXml.h"
#include "protoLocation.h"
#include <protoDebug.h>
#include <protoDefs.h>
#include <protoString.h> // for ProtoTokenator

#include "protoCoverSet.h"

#include <stdio.h>   // for sprintf()
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>   // for "isprint()"
#include <math.h>    // for "sqrt()"

#include <array>
#include <string_view>
//#include <chrono>


class Node;  // predeclared so it can be passed to templated CdsInterface definition below

class CdsInterface;   // predeclared so it can be passed to templated CdsLink definition below

// The GraphAttribute class and its supporting subclasses are used when loading GraphML
// files to assign included graph/node/edge attributes to their corresponding CdsGraph component

class GraphAttribute : public ProtoTree::Item
{
    public:
        enum DataType {INVALID, INTEGER, DOUBLE, STRING, BOOLEAN};

        GraphAttribute();
        ~GraphAttribute();

        bool Init(const char* key, DataType type, const char* value);
        void Destroy();

        const char* GetName() const
            {return attr_name;}

        DataType GetType() const
            {return attr_type;}

        int GetInteger() const
            {return (INTEGER == attr_type) ? *((int*)attr_value) : std::numeric_limits<int>::min();}
        double GetDouble() const
            {return (DOUBLE == attr_type) ? *((double*)attr_value) : std::numeric_limits<double>::quiet_NaN();}
        const char* GetString() const
            {return (STRING == attr_type) ? (const char*)attr_value : NULL;}
        bool GetBoolean() const
            {return (INTEGER == attr_type) ? *((bool*)attr_value) : false;}

        class Info : public ProtoTree::Item
        {
            public:
                Info();
                ~Info();

                bool Init(const char* key, const char* name, DataType type);
                void Destroy();

                const char* GetKey() const
                    {return local_key;}
                unsigned int GetKeysize() const
                    {return ((NULL != local_key) ? strlen(local_key) << 3 : 0);}
                const char* GetName() const
                    {return attr_name;}
                DataType GetType() const
                    {return attr_type;}
            private:
                char*    local_key;
                char*    attr_name;
                DataType attr_type;

        };  // end class GraphAttribute::Info

        class InfoTable : public ProtoTreeTemplate<Info>
        {
            public:
                Info* FindInfo(const char* key)
                    {return ProtoTreeTemplate<Info>::Find(key, strlen(key) << 3);}
        };  // end class GraphAttribute::InfoTable

        static DataType GetDataType(char* name)
        {
            if (NULL == name) return DataType::INVALID;
            struct DataTypeEntry
            {
                const char* name;
                DataType    type;
            };
            static const std::array<DataTypeEntry, 4> DataTypeMap =
            {
                DataTypeEntry{"integer", DataType::INTEGER},
                DataTypeEntry{"double",  DataType::DOUBLE},
                DataTypeEntry{"string",  DataType::STRING},
                DataTypeEntry{"boolean",  DataType::BOOLEAN}
            };
            for (const DataTypeEntry& entry : DataTypeMap)
            {
                if (0 == strncmp(name, entry.name, strlen(name)))
                    return entry.type;
            }
            return DataType::INVALID;
        }

        class Table : public ProtoTreeTemplate<GraphAttribute>
        {
            public:
                GraphAttribute* AddAttribute(const char* name, DataType type, const char* value)
                {
                    GraphAttribute* attr = new GraphAttribute();
                    if ((NULL == attr) || (NULL == value) ||
                        !attr->Init(name, type, value))
                    {
                        PLOG(PL_ERROR, "GraphAttribute::Table::AddAttribute() new GraphAttribute error: %s\n", GetErrorString());
                        if (NULL != attr) delete attr;
                        return NULL;
                    }
                    Insert(*attr);
                    return attr;
                }
                void RemoveAttribute(const char* name)
                {
                    GraphAttribute* attr = GetAttribute(name);
                    if (NULL != attr)
                    {
                        Remove(*attr);
                        delete attr;
                    }
                }
                void RemoveAttribute(GraphAttribute& attr)
                    {Remove(attr);}

                GraphAttribute* GetAttribute(const char* name)
                    {return ProtoTreeTemplate<GraphAttribute>::Find(name, strlen(name) << 3);}

                int GetInteger(const char* name)
                {
                    GraphAttribute* attr = GetAttribute(name);
                    return (NULL != attr) ? attr->GetInteger() : std::numeric_limits<int>::min();
                }
                double GetDouble(const char* name)
                {
                    GraphAttribute* attr = GetAttribute(name);
                    return (NULL != attr) ? attr->GetDouble() : std::numeric_limits<double>::quiet_NaN();
                }
                const char* GetString(const char* name)
                {
                    GraphAttribute* attr = GetAttribute(name);
                    return (NULL != attr) ? attr->GetString() : NULL;
                }
                int GetBoolean(const char* name)
                {
                    GraphAttribute* attr = GetAttribute(name);
                    return (NULL != attr) ? attr->GetBoolean() : false;
                }

        };  // end class GraphAttribute::Table()

    private:
        const char* GetKey() const
            {return attr_name;}
        unsigned int GetKeysize() const
            {return ((NULL != attr_name) ? strlen(attr_name) << 3 : 0);}

        char*    attr_name;
        DataType attr_type;
        void*    attr_value;
};  // end class GraphAttribute

GraphAttribute::Info::Info()
  : local_key(NULL), attr_name(NULL), attr_type(INVALID)
{
}

GraphAttribute::Info::~Info()
{
    Destroy();
}

bool GraphAttribute::Info::Init(const char* key, const char* name, DataType type)
{
    Destroy();
    if ((NULL == key) || (NULL == name) || (DataType::INVALID == type))
    {
        PLOG(PL_ERROR, "GraphAttribute::Info::Init() error: invalid parameters\n");
        return false;
    }
    if ((NULL == (local_key = new char[strlen(key) + 1])) ||
        (NULL == (attr_name = new char[strlen(name) + 1])))
    {
        PLOG(PL_ERROR, "GraphAttribute::Info::Init() new attr key/name error: %s\n", GetErrorString());
        Destroy();
        return false;
    }
    strcpy(local_key, key);
    strcpy(attr_name, name);
    attr_type = type;
    return true;
}  // end GraphAttribute::Info::Init()

void GraphAttribute::Info::Destroy()
{
    if (NULL != local_key) delete[] local_key;
    local_key = NULL;
    if (NULL != attr_name) delete[] attr_name;
    attr_name = NULL;
    attr_type = DataType::INVALID;
}  // end GraphAttribute::Info::Destroy()


GraphAttribute::GraphAttribute()
  : attr_name(NULL), attr_type(INVALID), attr_value(NULL)
{
}
GraphAttribute::~GraphAttribute()
{
    Destroy();
}

bool GraphAttribute::Init(const char* name, DataType type, const char* value)
{
    Destroy();
    if (DataType::INVALID == type) return false;
    if ((NULL == name) || (NULL == value))
    {
        PLOG(PL_ERROR, "GraphAttribute::Init() error: invalid parameters!\n");
        return false;
    }
    if (NULL == (attr_name = new char[strlen(name) + 1]))
    {
        PLOG(PL_ERROR, "GraphAttribute::Init() new attr_name error: %s\n", GetErrorString());
        return false;
    }
    strcpy(attr_name, name);
    switch (type)
    {
        case DataType::INTEGER:
            if (NULL == (attr_value = new int))
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() new integer attr_value error: %s\n", GetErrorString());
                return false;
            }
            if (1 != sscanf(value, "%d", (int*)attr_value))
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() invalid integer value!\n");
                Destroy();
                return false;
            }
            break;
        case DataType::DOUBLE:
            if (NULL == (attr_value = new double))
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() new double attr_value error: %s\n", GetErrorString());
                Destroy();
                return false;
            }
            if (1 != sscanf(value, "%lf", (double*)attr_value))
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() invalid double value!\n");
                Destroy();
                return false;
            }
            break;
        case DataType::STRING:
            if (NULL == (attr_value = new char[strlen(value)+1]))
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() new string attr_value error: %s\n", GetErrorString());
                Destroy();
                return false;
            }
            strcpy((char*)attr_value, value);
            break;
        case DataType::BOOLEAN:
            if (NULL == (attr_value = new bool))
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() new boolean attr_value error: %s\n", GetErrorString());
                return false;
            }
            if (0 == strcmp("true", value))
            {
                *((bool*)attr_value) = true;
            }
            else if (0 == strcmp("false", value))
            {
                *((bool*)attr_value) = false;
            }
            else
            {
                PLOG(PL_ERROR, "GraphAttribute::Init() error: boolean attribute value '%s'\n", value);
                Destroy();
                return false;
            }
            break;

        default:
            // won't occur
            break;
    }
    attr_type = type;
    return true;
}  // end GraphAttribute::Init()

void GraphAttribute::Destroy()
{
    if (NULL != attr_name) delete[] attr_name;
    attr_name = NULL;
    if (NULL != attr_value)
    {
        switch (attr_type)
        {
            case DataType::INTEGER:
                delete (int*)attr_value;
                break;
            case DataType::DOUBLE:
                delete (double*)attr_value;
                break;
            case DataType::BOOLEAN:
                delete (bool*)attr_value;
                break;
            default:
                delete[] (char*)attr_value;
                break;
        }
        attr_value = NULL;
    }
    attr_type = DataType::INVALID;
}  // end GraphAttribute::Destroy()

// Define a ManetLink type that uses the "SimpleCostDouble" as its cost metric
class CdsLink : public NetGraph::LinkTemplate<NetGraph::SimpleCostDouble, CdsInterface>, public ProtoQueue::Item
{
    public:
        GraphAttribute* AddAttribute(const char* name, GraphAttribute::DataType type, const char* value)
            {return attr_table.AddAttribute(name, type, value);}
        void RemoveAttribute(const char* name)
            {attr_table.RemoveAttribute(name);}
        void RemoveAttribute(GraphAttribute& attr)
            {attr_table.RemoveAttribute(attr);}
        GraphAttribute* GetAttribute(const char* name)
            {return attr_table.GetAttribute(name);}
        GraphAttribute::Table& AccessAttributeTable()
            {return attr_table;}

        class SimpleList : public ProtoSimpleQueueTemplate<CdsLink> {};

        void Activate()
            {active = true;}
        void Deactivate()
            {active = false;}
        bool IsActivated() const
            {return active;}

    private:
        GraphAttribute::Table   attr_table;
        bool                    active;
};

class CdsInterface : public NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node>,
                     public ProtoQueue::Item
{
    public:
        CdsInterface(Node& theNode)
          : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node>(theNode),
            rtr_priority(0), relay_status(false), visited(false), user_index(-1)
        {
        }
        CdsInterface(Node& theNode, const ProtoAddress& addr)
          : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node>(theNode, addr),
            rtr_priority(0), relay_status(false), visited(false)
        {
        }
        virtual ~CdsInterface()
            {ProtoQueue::Item::Cleanup();}

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

        class Queue : public ProtoSimpleQueueTemplate<CdsInterface> {};

        void SetIndex(int index)
            {user_index = index;}
        int GetIndex() const
            {return user_index;}

    private:
        UINT8                   rtr_priority;
        bool                    relay_status;
        bool                    visited;
        int                     user_index;
};  // end class CdsInterface


class CdsGraph : public NetGraphTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node>
{
    public:
        GraphAttribute::Table& AccessAttributeTable()
            {return attr_table;}

        unsigned int activation_count;

    private:
        GraphAttribute::Table   attr_table;

};
//class CdsGraph : public ManetGraphMLTemplate<NetGraph::SimpleCostDouble, CdsInterface, CdsLink, Node> {};

void Usage()
{
    fprintf(stderr, "Usage: gr input <fileName> [ecdsm][mecds][degree][link <name>,<priority>]\n");
}

const unsigned int MAX_LINE = 256;

class Node :public NetGraph::NodeTemplate<CdsInterface>, public ProtoQueue::Item
{
    public:
        Node();
        ~Node();

        // _MUST_ call init to create a default interface
        bool Init(unsigned int nodeId, const char* nodeName = NULL);

        //bool AddInterface(Interface& iface, bool makeDefault = false)
        // {return ManetGraph::AddInterface(iface, makeDefault);}

        CdsGraph::Interface* AddNamedInterface(const char* ifaceName, unsigned int ifaceId)
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
                //if (AppendInterface(*iface, (1 == iface_count)))
                if (AddInterface(*iface, (1 == iface_count)))
                {
                    return iface;
                }
                else
                {
                    iface_count -= 1;
                    return NULL;
                }
            }
            else
            {
                iface_count -= 1;
                PLOG(PL_ERROR, "Node::Init() new CdsGraph::Interface() error: %s\n",
                     GetErrorString());
                return NULL;
            }
        }

        unsigned int GetInterfaceCount() const
            {return iface_count;}

        unsigned int GetId() const {return node_id;}
        /*
        {
            if (NULL != iface)
            {
                return (iface->GetAddress().GetEndIdentifier());
            }
            else
            {
                unsigned int fakeId;
                memcpy(&fakeId, (unsigned int*)this, sizeof(fakeId));
                return fakeId;
            }
        }
        */

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

        void SetEssential(bool state)
            {essential = state;}
        bool IsEssential() const
            {return essential;}

        void AddEssentialNeighbor(Node& nbr)
        {
            if (!essential_nbrs.Contains(nbr))
                essential_nbrs.Append(nbr);
        }
        void RemoveEssentialNeighbor(Node& nbr)
            {essential_nbrs.Remove(nbr);}
        void ClearEssentialNeighbors()
            {essential_nbrs.Empty();}
        bool IsEssentialNeighbor(Node& nbr)
            {return essential_nbrs.Contains(nbr);}

        unsigned int GetDegree();       // counts each neighbor only once regardless of redundant links
        unsigned int GetTotalDegree();  // counts neighbors with redundant links multiple time

        GraphAttribute* AddAttribute(const char* name, GraphAttribute::DataType type, const char* value)
            {return attr_table.AddAttribute(name, type, value);}
        void RemoveAttribute(const char* name)
            {attr_table.RemoveAttribute(name);}
        void RemoveAttribute(GraphAttribute& attr)
            {attr_table.RemoveAttribute(attr);}
        GraphAttribute* GetAttribute(const char* name)
            {return attr_table.GetAttribute(name);}
        GraphAttribute::Table& AccessAttributeTable()
            {return attr_table;}

        class Queue : public ProtoSimpleQueueTemplate<Node> {};

        Queue& AccessEssentialNeighborList()
            {return essential_nbrs;}

        class NeighborIterator : public NetGraph::NodeTemplate<CdsInterface>::NeighborIterator
        {
            public:
                NeighborIterator(Node& node)
                    : NetGraph::NodeTemplate<CdsInterface>::NeighborIterator(node), debug(false) {}
                virtual ~NeighborIterator() {visited_queue.Empty();}
                void SetDebug(bool state) {debug = state;}

                Node* GetNextNeighbor()
                {
                    Node* nextNeighbor = NULL;
                    CdsInterface* nextIface;
                    while (NULL != (nextIface = GetNextNeighborInterface()))
                    {
                        nextNeighbor = &(nextIface->GetNode());
                        if (debug) TRACE("      GetNextNeighbor(%s) nbr: %s visited: %d\n", nextNeighbor->GetName(), visited_queue.Contains(*nextNeighbor));
                        if (visited_queue.Contains(*nextNeighbor))
                        {
                            nextNeighbor = NULL;
                            continue;
                        }
                        else
                        {
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
                bool    debug;
        };  // end class Node::NeighborIterator

        void SetIndex(int index)
            {user_index = index;}
        int GetIndex() const
            {return user_index;}

         // ProtoTree::Item overrides so nodes
        // can be cached by name
        const char* GetKey() const
            {return node_name;}
        unsigned int GetKeysize() const
            {return node_name_bits;}

    private:
        char*                   node_name;
        unsigned int            node_name_bits;
        unsigned int            node_id;
        unsigned int            iface_count;
        UINT8                   rtr_priority;
        bool                    relay_status;
        Queue                   essential_nbrs;
        bool                    visited;
        bool                    essential;
        int                     user_index;
        GraphAttribute::Table   attr_table;

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
   rtr_priority(0), visited(false), essential(false),
   user_index(-1)
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
    if (0 == GetInterfaceCount()) return 0;
    unsigned int count = 0;
    NeighborIterator nit(*this);
    Node* neighbor;
    while (NULL != (neighbor = nit.GetNextNeighbor()))
    {
        count += 1;
    }
    return count;
}  // end Node::GetDegree()


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

        bool LoadGraphML(const char* filePath, CdsGraph& graph);

        CdsGraph& AccessGraph()
            {return graph;}

        NodeTree& AccessNodeTree()
            {return node_tree;}
        Node* FindNode(const char* nodeName)
            {return node_tree.Find(nodeName, (strlen(nodeName) + 1) << 3);}

        static int CalculateFullECDS(NodeTree&                                   nodeTree,
                                     CdsGraph&                                   graph,
                                     bool                                        useDegree = false,
                                     ProtoGraph::Vertice::SortedList::ItemPool*  sortedVerticeItemPool = NULL);

        static int CalculateFullMECDS(NodeTree&                                  nodeTree,
                                      CdsGraph&                                  graph,
                                      bool                                       useDegree,
                                      ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool);

        static int CalculateFullECDSM(NodeTree&                                  nodeTree,
                                      CdsGraph&                                  graph,
                                      bool                                       useDegree,
                                      ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool);

        static unsigned int ActivateEssentialInterfaces(Node& n0, Node::Queue& neighbors);
        static void ActivateEssentialInterfaces2(Node& n0, Node::Queue& neighbors);

        static double CalculateDensity(CdsGraph& graph);

        bool ValidateCDS();

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
        GraphAttribute::InfoTable                   graph_attr_info_table;
        GraphAttribute::InfoTable                   node_attr_info_table;
        GraphAttribute::InfoTable                   edge_attr_info_table;
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
        //TRACE("Mapping link type %s to color %s\n", name, COLOR[link_color_index]);
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
                unsigned int nodeId;
                // Is it in the form <nodeId>-XXX where <nodeId> is an integer id
                ProtoTokenator tk(nameString, '-');
                const char* idString = tk.GetNextItem();
                unsigned int id;
                if (1 != sscanf(idString, "%u", &id))
                    nodeId = node_id_index++;
                else
                    nodeId = id;
                // Create and insert new node into the "graph" and "node_tree"
                Node* node = new Node();
                if (!node->Init(nodeId, nameString))
                {
                    PLOG(PL_ERROR, "gr error: Node initialization failure!\n");
                    return -1.0;
                }
                //TRACE("Added node %s id: %u\n", nameString, nodeId);
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
            const char* ifaceName = (NULL != linkName) ? linkName : "default";

            TRACE("ifaceName : %s\n", ifaceName);

            LinkType* linkType = GetLinkType(ifaceName);
            if ((NULL == linkType) && (NULL == (linkType = AddLinkType(ifaceName))))
            {
                PLOG(PL_ERROR, "gr error: unable to add link type\n");
                return -1.0;
            }
            iface1 = node1->FindInterfaceByName(ifaceName);
            if (NULL == iface1)
                iface1 = node1->AddNamedInterface(ifaceName, (NULL != linkName) ? ++iface_id_index : 0);
            iface2 = node2->FindInterfaceByName(ifaceName);
            if (NULL == iface2)
                iface2 = node2->AddNamedInterface(ifaceName, (NULL != linkName) ? ++iface_id_index : 0);
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
                //TRACE("gr: connecting %s/%s->%s/%s\n", node1->GetName(), linkName, node2->GetName(), linkName);
                if (!graph.Connect(*iface1, *iface2, cost, true))
                    PLOG(PL_ERROR, "gr error: unable to connect interfaces in graph\n");
                //else
                //    TRACE("Added link %s/%s<->%s/%s\n", node1->GetName(), iface1->GetName(), node2->GetName(), iface2->GetName());
            }
            else
            {
                graph.Disconnect(*iface1, *iface2, true);
                TRACE("Removed link %s/%s<->%s/%s\n", node1->GetName(), iface1->GetName(), node2->GetName(), iface2->GetName());
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

void InsertByPriority(NodeQueue& priorityQ, Node& node)
{
    // Priority insertion
    NodeQueue::Iterator queuerator(priorityQ);
    Node* z;
    while (NULL != (z = queuerator.GetNextItem()))
    {
        if (ComparePriority(node.GetRtrPriority(), node.GetTotalDegree(), node.GetAddress(),
                            z->GetRtrPriority(), z->GetTotalDegree(), z->GetAddress()) > 0)
        {
            break;  // to insert y before z in priority Q
        }
    }
    if (NULL == z)
        priorityQ.Append(node);
    else
        priorityQ.Insert(node,*z);
}  // end InsertByPriority()

int GraphRider::CalculateFullECDSM(NodeTree&                                  nodeTree,
                                   CdsGraph&                                  graph,
                                   bool                                       useDegree,
                                   ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool)
{
    graph.activation_count = 0;
    bool activate = true;
    // Now that we have a fully updated "graph", perform Relay Set Selection
    // algorithm for each n0 in graph
    unsigned int numberOfRelays = 0;
    unsigned int nonRelayCount = 0;
    unsigned int multiRelayCount = 0;
    NodeTree::Iterator noderator(nodeTree);
    Node* n0;
    while (NULL != (n0 = noderator.GetNextItem()))
    {
        unsigned int degreeN0 = useDegree ? n0->GetTotalDegree() : 0;
        //TRACE("___________\nECDSM check for node %s priority:%d degree:%d id:%d...\n",
        //      n0->GetName(), n0->GetRtrPriority(), degree, n0->GetId());
        n0->ClearEssentialNeighbors();
        n0->SetRelayStatus(false);
        CdsInterface* iface;
        Node::InterfaceIterator ifacerator(*n0);
        while (NULL != (iface = ifacerator.GetNextInterface()))
                iface->SetRelayStatus(false);

        if (n0->GetTotalDegree() < 1)
        {
            // no neighbors, so ignore
            continue;
        }
        else if (n0->GetTotalDegree() < 2)
        {
            // Add sole neighbor to essential_neighbor set
            Node::NeighborIterator neighborator(*n0);
            Node* nbr = neighborator.GetNextNeighbor();
            ASSERT(NULL != nbr);
            nbr->SetEssential(true);
            n0->AddEssentialNeighbor(*nbr);
            NodeQueue N1;
            if (activate)
            {
                NodeQueue N1;
                N1.Append(*nbr);
                graph.activation_count += ActivateEssentialInterfaces(*n0, N1);
            }
            TRACE("   P0 Node %s essential ECDSM neighbors:\n   %s\n", n0->GetName(), nbr->GetName());
            continue; // leaf nodes are inherently non-essential
        }
        //TRACE("%s enter PHASE 1 ...\n", n0->GetName());
        // ECDS-M PHASE 1 - "Neighborhood extraction"
        NodeQueue N1;  // will be populated with one-hop neighbors of 'n0'
        Node::NeighborIterator neighborator0(*n0);
        Node* n1;
        //unsigned int N1size = 0;
        while (NULL != (n1 = neighborator0.GetNextNeighbor()))
        {
            //N1size += 1;
            n1->SetVisited(false);   // for use in BFS below
            n1->SetEssential(false); // for use in BFS below
            N1.Append(*n1);
        }
        NodeQueue N2; // will be populated with two-hop neighbors of 'n0'
        NodeQueue::Iterator iterN1(N1);
        while (NULL != (n1 = iterN1.GetNextItem()))
        {
            Node::NeighborIterator neighborator1(*n1);
            Node* n2;
            while (NULL != (n2 = neighborator1.GetNextNeighbor()))
            {
                if (n2 == n0) continue;
                if (N1.Contains(*n2)) continue;
                if (N2.Contains(*n2)) continue;
                n2->SetVisited(false);  // for use in BFS below
                N2.Append(*n2);
            }
        }
        /*
        if (N2.IsEmpty())
        {
            // IMPORTANT - for this to be valid, we need to check that
            // all of our one-hop neighbors can see each other directly.
            bool fullMesh = true;
            iterN1.Reset();
            while (NULL != (n1 = iterN1.GetNextItem()))
            {
                unsigned int count = 1;
                Node::NeighborIterator neighborator1(*n1);
                Node* n2;
                while (NULL != (n2 = neighborator1.GetNextNeighbor()))
                {
                    if (N1.Contains(*n2))
                        count += 1;
                }
                if (count != N1size)
                {
                    fullMesh = false;
                    break;
                }
            }
            if (fullMesh)
            {
                // No need for a relay if one-hop neighbors only
                // but need to cover all neighbors a non-relay
                iterN1.Reset();
                TRACE("   Node %s essential ECDSM neighbors:\n", n0->GetName());
                while (NULL != (n1 = iterN1.GetNextItem()))
                {
                    n1->SetEssential(true);
                    n0->AddEssentialNeighbor(*n1);
                    TRACE("      %s\n", n1->GetName());
                }
                if (activate) ActivateEssentialInterfaces(*n0);
                continue;
            }
        }
        */

        // ECDS-M PHASE 2 - "highest priority anchor selection"
        //TRACE("%s enter PHASE 2 ...\n", n0->GetName());
        Node* nMax = NULL;
        unsigned int degreeNmax = 0;
        iterN1.Reset();
        while (NULL != (n1 = iterN1.GetNextItem()))
        {
            unsigned int degreeN1 = useDegree ? n1->GetTotalDegree() : 0;
            if ((NULL == nMax) ||
                ComparePriority(n1->GetRtrPriority(), degreeN1, n1->GetAddress(),
                                nMax->GetRtrPriority(), degreeNmax, nMax->GetAddress()) > 0)
            {
                nMax = n1;
                degreeNmax = degreeN1;
            }
        }
        if (ComparePriority(n0->GetRtrPriority(), degreeN0, n0->GetAddress(),
                            nMax->GetRtrPriority(), degreeNmax, nMax->GetAddress()) >= 0)
        {
            n0->SetRelayStatus(true); // n0 has the highest local priority
            numberOfRelays += 1;
            // We're the big dog in the neighborhood, so mark all neighbors essential
            iterN1.Reset();
            TRACE("   P2 Node %s* essential ECDSM neighbors:\n", n0->GetName());
            while (NULL != (n1 = iterN1.GetNextItem()))
            {
                n1->SetEssential(true);
                n0->AddEssentialNeighbor(*n1);
                TRACE("      %s\n", n1->GetName());
            }
            if (activate)
                graph.activation_count += ActivateEssentialInterfaces(*n0, N1);
            continue;
        }
        // ECDS-M PHASE 3 - "high-priority BFS reachability check"
        //TRACE("%s enter PHASE 3 (nMax: %s) ...\n", n0->GetName(), nMax->GetName());
//#define PRIORITY_BFS 1
        NodeQueue neighborRelays;
        nMax->SetVisited(true);
        nMax->SetEssential(true);
        n0->AddEssentialNeighbor(*nMax);
        NodeQueue Q;
        Q.Append(*nMax);
        Node* x;
        while (NULL != (x = Q.RemoveHead()))
        {
            // filter neighbors: find valid next-hops from 'x'
            // We do a prioritized traversal to properly identify "essential neighbors"
            Node::NeighborIterator neigboratorx(*x);
            Node* y;
#ifdef PRIORITY_BFS
            NodeQueue priorityQ;
            while (NULL != (y = neigboratorx.GetNextNeighbor()))
            {
                // Priority insertion
                InsertByPriority(priorityQ, *y);
            }
            NodeQueue::Iterator pqiter(priorityQ);
            while (NULL != (y = pqiter.GetNextItem()))  // use this line for priority BFS
#else
            while (NULL != (y = neigboratorx.GetNextNeighbor()))
#endif  // if/elsePRIORITY_BFS
            {
                //TRACE("Next neighbor of %s is %s (degree:%d)was visited: %d...\n", x->GetName(), y->GetName(), y->GetTotalDegree(), y->WasVisited());
                if (!y->WasVisited() && (y != n0) &&
                    (N1.Contains(*y) || N2.Contains(*y)))
                {
                    // Note N2 -> N2 traversal is invalid since that
                    // falls outside the horizon of two-hop neighborhood information
                    // so validate that at least 'x' or 'y' is a one-hop neighbor
                    // Process as valid next-hop if valid
                    if (N1.Contains(*x))
                    {
                        //TRACE("    node %s covered by %s\n", y->GetName(), x->GetName());
                        x->SetEssential(true);
                        n0->AddEssentialNeighbor(*x);
                    }
                    else if (!N1.Contains(*y))
                    {
                        continue;
                    }
                    //TRACE("Next neighbor of %s is %s ...\n", x->GetName(), y->GetName());
                    y->SetVisited(true);
                    unsigned int degreeY = useDegree ? y->GetTotalDegree() : 0;
                    //TRACE("compare %s %d:%d:%d to %s %d:%d:%d ...\n",
                    //       y->GetName(), y->GetRtrPriority(), degreeY, y->GetId(),
                    //        n0->GetName(),n0->GetRtrPriority(), degreeN0, n0->GetId());
                    if (ComparePriority(y->GetRtrPriority(), degreeY, y->GetAddress(),
                                        n0->GetRtrPriority(), degreeN0, n0->GetAddress()) > 0)
                    {
                        //TRACE("    (enqueuing %s ...)\n", y->GetName());
#ifdef PRIORITY_BFS
                        InsertByPriority(Q, *y);
#else
                        Q.Append(*y);
#endif  // if/elsePRIORITY_BFS
                    }
                }
            }  // end while y in neighbors of x
        }  // end while Q not empty
        // ECDS-M PHASE 4 - final essentiality determination
        //TRACE("%s enter PHASE 4 ...\n", n0->GetName());
        iterN1.Reset();
        while (NULL != (n1 = iterN1.GetNextItem()))
        {
            //TRACE("   n1 : %s degree:%d visited: %d essential: %d\n", n1->GetName(), useDegree ? n1->GetTotalDegree() : 0, n1->WasVisited(), n1->IsEssential());
            if (!n1->WasVisited())
            {
                n1->SetEssential(true);
                n0->AddEssentialNeighbor(*n1);
                if (!n0->GetRelayStatus())
                {
                    //TRACE("   (enabling %s)\n", n0->GetName());
                    // There was at least one uncovered neighbor so n0 must activate
                    n0->SetRelayStatus(true);  // a neighbor was unreachable; n0 is essential
                }
                //break;
            }
        }  // end while (NULL != n1 ...)

        if (!n0->GetRelayStatus())
        {
            // Phase 5
            // For non-relay nodes, this identifies what neighboring relays it can
            // Note that it is always possible to identify at least one
            nonRelayCount += 1;
            //TRACE("Non-relay node %s ....\n", n0->GetName());
            // Identify as many neighboring relays as possible
            n0->ClearEssentialNeighbors();
            iterN1.Reset();
            while (NULL != (n1 = iterN1.GetNextItem()))
                n1->SetEssential(false);

            // The highest priority neighbor "nMax" is always a relay
            nMax->SetEssential(true);
            n0->AddEssentialNeighbor(*nMax);
            unsigned int ecount = 1;
            // The nMax of other one-hop neighbors that are also
            // a one-hop neighbor of n0 will self-elect as relays
            iterN1.Reset();
            while (NULL != (n1 = iterN1.GetNextItem()))
            {
                if (n1 == nMax) continue;  // already identified as neighboring relay
                Node* n1Max = n1;  // may end up being n1 in the end
                unsigned int n1MaxDegree = useDegree ? n1Max->GetTotalDegree() : 0;
                Node::NeighborIterator nbrIter(*n1);
                Node* n2;
                while (NULL != (n2 = nbrIter.GetNextNeighbor()))
                {
                    unsigned int n2Degree = useDegree ? n2->GetTotalDegree() : 0;
                    if ((ComparePriority(n2->GetRtrPriority(), n2Degree, n2->GetAddress(),
                                         n1Max->GetRtrPriority(), n1MaxDegree, n1Max->GetAddress()) > 0))
                    {
                        n1Max = n2;
                        n1MaxDegree = n2Degree;
                    }
                }
                if (N1.Contains(*n1Max) && !n0->IsEssentialNeighbor(*n1Max))
                {

                    n1Max->SetEssential(true);
                    n0->AddEssentialNeighbor(*n1Max);
                    ecount++;
                }
            }
            if (ecount > 1)
            {
                multiRelayCount += 1;
            }
        }  // if (!n0->GetRelayStatus())

        TRACE("   P4 Node %s%s essential ECDSM neighbors:\n", n0->GetName(), n0->GetRelayStatus() ? "*" : "");
        iterN1.Reset();
        while (NULL != (n1 = iterN1.GetNextItem()))
        {
            if (n1->IsEssential()) TRACE("      %s\n", n1->GetName());
        }

        // Essentiallity consistency check
        iterN1.Reset();
        while (NULL != (n1 = iterN1.GetNextItem()))
        {
            if (n1->IsEssential() &&!n0->IsEssentialNeighbor(*n1))
            {
                //TRACE("Node %s has marked neigbor %s not in essential neighbor list!\n",
                //        n0->GetName(), n1->GetName());
                ASSERT(0);
            }
            if (n0->IsEssentialNeighbor(*n1) && !n1->IsEssential())
            {
                //TRACE("Node %s has unmarked neigbor %s in essential neighbor list!\n",
                //        n0->GetName(), n1->GetName());
                ASSERT(0);
            }
        }

        if (activate)
            graph.activation_count += ActivateEssentialInterfaces(*n0, N1);
        if (n0->GetRelayStatus())
        {
            numberOfRelays += 1;
        }
    }  // end while (NULL != n0 ...)

    // Default dumb link activation - this currently just
    // activates all interfaces on relay nodes
    if (!activate)
    {
        // default 'dumb', full link activation
        noderator.Reset();
        while (NULL != (n0 = noderator.GetNextItem()))
        {
            bool relayStatus = n0->GetRelayStatus();
            CdsInterface* iface;
            Node::InterfaceIterator ifacerator(*n0);
            while (NULL != (iface = ifacerator.GetNextInterface()))
                iface->SetRelayStatus(relayStatus);
        }
    }
    //TRACE("enter VALIDATE 1\n");
    // VALIDATE that "essential neighbors" for non-relays
    // only contain relay nodes.
    noderator.Reset();
    while (NULL != (n0 = noderator.GetNextItem()))
    {
        if (NULL == n0->GetDefaultInterface()) continue;
        if (!n0->GetRelayStatus())
        {
            Node::Queue::Iterator iter(n0->AccessEssentialNeighborList());
            Node* n1;
            while (NULL != (n1 = iter.GetNextItem()))
            {
                if (!n1->GetRelayStatus() && (n1->GetTotalDegree() > 1))
                {
                    TRACE("ERROR - NODE %s identified non-relay essential neighbor: %s\n",
                                n0->GetName(), n1->GetName());
                    ASSERT(0);
                }
            }
        }
    }
    // VALIDATE the CDS connectivity
    noderator.Reset();
    while (NULL != (n0 = noderator.GetNextItem()))
    {
        // Because graphs may be fragmented, we need to first
        // count the number of reachable nodes starting from "n0"
        NodeQueue visitedList;
        visitedList.Append(*n0);
        n0->SetVisited(false);
        unsigned int reachableCount = 1;
        CdsInterface* startIface = n0->GetDefaultInterface();
        //ASSERT(NULL != startIface);
        if (NULL == startIface) continue;
        CdsGraph::SimpleTraversal bfs(graph, *startIface);
        unsigned int level;
        CdsInterface* iface;
        while (NULL != (iface = bfs.GetNextInterface(&level)))
        {
            Node& node = iface->GetNode();
            if (!visitedList.Contains(node))
            {
                visitedList.Append(node);
                node.SetVisited(false);  // will be used for CDS traversal
                reachableCount++;
            }
        }

        //TRACE("Node %s reachable count: %u\n", n0->GetName(), reachableCount);
        // Do a BFS traversal through via essential neighbors
        NodeQueue Q;
        Q.Append(*n0);
        n0->SetVisited(true);
        unsigned int ecdsmCount = 1;
        Node* node;
        //TRACE("VALIDATING %s subgraph:\n",  n0->GetName());
        while(NULL != (node = Q.RemoveHead()))
        {
            NodeQueue::Iterator iter(node->AccessEssentialNeighborList());
            Node* nbr;
            //TRACE("  via %s\n", node->GetName());
            // An essential relay must forward to all its essential neighbors
            while (NULL != (nbr = iter.GetNextItem()))
            {
                if (!nbr->WasVisited())
                {
                    //TRACE("    visiting: %s\n", nbr->GetName());
                    Q.Append(*nbr);
                    nbr->SetVisited(true);
                    ecdsmCount++;
                }
                else
                {
                    //TRACE("    (%s already visited)\n", nbr->GetName());
                }
                if (!node->GetRelayStatus())
                {
                    //TRACE("    non-relay break.\n");
                    break;  // non-relay only needs to forward to any one neighboring relay
                }
                // else relays forward to all essential neigbors
            }
        }
        //TRACE("Node %s reachable: %u ecdsm: %u\n", n0->GetName(), reachableCount, ecdsmCount);
        if (ecdsmCount != reachableCount)
        {
            TRACE("mismatched reachable/ecdsm count!\n");
            ASSERT(ecdsmCount == reachableCount);
        }
    }

    //TRACE("relayCount: %4u  nonRelayCount: %4u  activationCount: %4u multiRelayCount: %4u\n", 
    //        numberOfRelays, nonRelayCount, graph.activation_count, multiRelayCount);
    TRACE("relayCount: %4u activationCount: %4u\n", numberOfRelays, graph.activation_count);
    return numberOfRelays;
}  // end GraphRider::CalculateFullECDSM()

void GraphRider::ActivateEssentialInterfaces2(Node& n0, Node::Queue& N1)
{
    // This link activation algorithm iteratively activates interfaces
    // according to which interface covers the most essential neighbors
    bool addLink = true;
    if (!n0.GetRelayStatus()) addLink = false;  // comment this to activate non-relay ifaces
    // Activate interfaces to cover essential 1-hop neighbors
    // 0) "Deactivate" all links to init visualization state properly
    Node::InterfaceIterator ifacerator(n0);
    CdsInterface* iface0;
    while (NULL != (iface0 = ifacerator.GetNextInterface()))
    {
        iface0->SetRelayStatus(false);
        NetGraph::AdjacencyIterator adjacerator(*iface0);
        CdsInterface* iface1;
        while (NULL != (iface1 = static_cast<CdsInterface*>(adjacerator.GetNextAdjacency())))
        {
            CdsLink* link = iface0->GetLinkTo(*iface1);
            link->Deactivate();
        }
    }

    // 1) Count essential neighbors of node 'n0'
    unsigned int essentialCount = 0;
    Node::NeighborIterator niter1(n0);
    Node* n1;
    while (NULL != (n1 = niter1.GetNextNeighbor()))
    {
        if (n1->IsEssential())
            essentialCount += 1;
    }
    // 2) Iteratively activate interfaces to cover essential neighbors of node 'n0'
    while (essentialCount > 0)
    {
        // 2a) Find unactivated interface that has most uncovered essential neighbors
        CdsInterface* maxIface = NULL;
        unsigned maxCount = 0;
        ifacerator.Reset();
        while (NULL != (iface0 = ifacerator.GetNextInterface()))
        {
            if (iface0->GetRelayStatus()) continue;  // already activated
            unsigned int ifaceCount = 0;
            NetGraph::AdjacencyIterator adjacerator(*iface0);
            CdsInterface* iface1;
            while (NULL != (iface1 = static_cast<CdsInterface*>(adjacerator.GetNextAdjacency())))
            {
                Node& nbr = iface1->GetNode();
                if (nbr.IsEssential())
                    ifaceCount += 1;
            }
            if (ifaceCount > maxCount)
            {
                maxIface = iface0;
                maxCount = ifaceCount;
            }
        }
        // 2b) Activate 'maxIface' and (un)mark essential neighbors covered
        ASSERT(NULL != maxIface);
        if (addLink)
            maxIface->SetRelayStatus(true);
        NetGraph::AdjacencyIterator adjacerator(*maxIface);
        CdsInterface* iface1;
        while (NULL != (iface1 = static_cast<CdsInterface*>(adjacerator.GetNextAdjacency())))
        {
            CdsLink* link = maxIface->GetLinkTo(*iface1);
            ASSERT(NULL != link);
            Node& nbr = iface1->GetNode();
            if (nbr.IsEssential())
            {
                nbr.SetEssential(false);
                if (addLink)
                    link->Activate();
                else
                    link->Deactivate();
                essentialCount -= 1;
            }
            else
            {
                link->Deactivate();
            }
        }
        if (!n0.GetRelayStatus()) addLink = false;  // non-relays only need to cover one essential relay
    } // end while (essentialCount > 0)

}  // end GraphRider::ActivateEssentialInterfaces()

unsigned int GraphRider::ActivateEssentialInterfaces(Node& n0, Node::Queue& N1)
{
    bool addLink = true;
    if (!n0.GetRelayStatus()) addLink = false;  // comment this to activate non-relay ifaces

    // This uses the ProtoCoverSet::Solver() to determine the interface
    // activation to optimally "cover" the necessary essential neighbors
    // ProtoCoverSet::Matrix is use to represent the cover set problem
    // with the rows of the matrix corresponding to the "n0" interface
    // set and the columns of matrix corresponding to "n0" one-hop neighbors
    unsigned int ifaceCount = n0.GetInterfaceCount();
    unsigned int nbrCount = N1.GetCount();

    ProtoCoverSet::Matrix matrix;
    if (!matrix.Create(ifaceCount, nbrCount))
    {
        // handle allocation failure
        TRACE("GraphRider::ActivateEssentialInterfaces() error: unable to create solver matrix!\n", GetErrorString());
        ASSERT(0);
    }
    // Assign indices to neighbors (columns) and interface (rows) for
    // solver matrix indexing and set matrix row/column contexts
    Node::InterfaceIterator ifacerator(n0);
    CdsInterface* iface0;
    int index = 0;
    while (NULL != (iface0 = ifacerator.GetNextInterface()))
    {
        matrix.SetRowContext(index, iface0);
        iface0->SetIndex(index++);
        iface0->SetRelayStatus(false);
        NetGraph::AdjacencyIterator adjacerator(*iface0);
        CdsInterface* iface1;
        while (NULL != (iface1 = static_cast<CdsInterface*>(adjacerator.GetNextAdjacency())))
        {
            CdsLink* link = iface0->GetLinkTo(*iface1);
            link->Deactivate();
        }
    }
    Node::Queue::Iterator iterN1(N1);
    Node* n1;
    index = 0;
    while (NULL != (n1 = iterN1.GetNextItem()))
    {
        matrix.SetColumnContext(index, n1);
        n1->SetIndex(index++);
    }
    // Iterate over each interface and adjacencies to
    // set matrix entry values
    ifacerator.Reset();
    while (NULL != (iface0 = ifacerator.GetNextInterface()))
    {
        CdsGraph::AdjacencyIterator adjacerator(*iface0);
        CdsInterface* iface1;
        while (NULL != (iface1 = adjacerator.GetNextAdjacency()))
        {
            CdsLink* link = iface0->GetLinkTo(*iface1);
            GraphAttribute* attr1 = link->GetAttribute("bandwidth");
            GraphAttribute* attr2 = link->GetAttribute("etc");
            GraphAttribute* attr3 = link->GetAttribute("weight");
            double cost = 1.0;
            if (NULL != attr1)
            {
                cost = 1.0 / attr1->GetDouble();
                if (NULL != attr2)
                    cost *= attr2->GetDouble();
            }
            else if (NULL != attr2)
            {
                cost = attr2->GetDouble();
            }
            else if (NULL != attr3)
            {
                cost = attr3->GetDouble();
            }
            Node& nbr1 = iface1->GetNode();
            matrix.SetValue(iface0->GetIndex(), nbr1.GetIndex(), cost);
        }
    }
    ProtoCoverSet::Solver solver(matrix);
    ProtoCoverSet::Result result;
    unsigned int acount = 0;

    // EXACT_MIN_COST, EXACT_MIN_ROWS, GREEDY_MIN_COST, GREEDY_MIN_ROWS,
    if (!solver.Solve(ProtoCoverSet::EXACT_MIN_COST, result))
    {
        TRACE("GraphRider::ActivateEssentialInterfaces() solver.Solve() error: %s\n", GetErrorString());
        ASSERT(0);
        return 0;
    }
    else if (!result.feasible)
    {
        TRACE("GraphRider::ActivateEssentialInterfaces() error: no feasible result!\n");
        ASSERT(0);
        return 0;
    }
    else
    {
        // result contains the optimal solution.
        for (unsigned int row : result.rows)
        {
            CdsInterface* iface = (CdsInterface*)matrix.GetRowContext(row);
            ASSERT(NULL != iface);
            if (addLink)
                iface->SetRelayStatus(true);
            else
                iface->SetRelayStatus(false);
            NetGraph::AdjacencyIterator adjacerator(*iface);
            CdsInterface* iface1;
            while (NULL != (iface1 = static_cast<CdsInterface*>(adjacerator.GetNextAdjacency())))
            {
                CdsLink* link = iface->GetLinkTo(*iface1);
                ASSERT(NULL != link);
                Node& nbr = iface1->GetNode();
                if (nbr.IsEssential())
                {
                    if (addLink)
                    {
                        acount += 1;
                        link->Activate();
                    }
                    else
                    {
                        link->Deactivate();
                    }
                }
                else
                {
                    link->Deactivate();
                }
            }
            if (!n0.GetRelayStatus()) addLink = false;  // non-relays only need to cover one essential relay
        }
    }
    return acount;
}  // end GraphRider::ActivateEssentialInterfaces2()

/*
void GraphRider::ValidateCDS(NodeTree&                                  nodeTree,
                             CdsGraph&                                  graph)
{
    // Make sure that CDS reaches all nodes reachable from nodes
    NodeTree::Iterator noderator(nodeTree);
    Node* node;
    while (NULL != (node = noderator.GetNextItem()))
    {
        Node* currentNode = node;
        CdsInterface* ifacex = node->GetDefaultInterface();
        ASSERT(NULL != ifacex);
        CdsGraph::SimpleTraversal bfs(graph, *ifacex);
        unsigned int level;
        CdsInterface* ifacey;
        while (NULL != (ifacey = bfs.GetNextInterface(&level)))
        {
        }
    }
}  // end GraphRider::ValidateCDS()
*/

int GraphRider::CalculateFullMECDS(NodeTree&                                  nodeTree,
                                   CdsGraph&                                  graph,
                                   bool                                       useDegree,
                                   ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool)
{
    bool collapseNodes = false;
    bool activateEssentialOnly = false;  // 'true' doesn't work
    unsigned int K = 2;

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
        /*
        unsigned int level;
        CdsInterface* ifacey;
        while (NULL != (ifacey = bfs.GetNextInterface(&level)))
        {
            TRACE("nMECDS test bfs traversal from %s/%s to %s/%s at level %u...\n",
                  ifacex->GetNode().GetName(), ifacex->GetName(),
                  ifacey->GetNode().GetName(), ifacey->GetName(), level);
        }
        */

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
                TRACE("   MECDS disabling iface %s/%s degree %d per E-CDS step 2\n", node->GetName(), iface->GetName(), degree);
                iface->SetRelayStatus(false); // 'leaf' interface
                continue;
            }

            //TRACE("   MECDS Steps 3-6 iface: %s/%s (%d:%d:%d) ...\n", node->GetName(), iface->GetName(),
            ///        iface->GetRtrPriority(), degree, iface->GetAddress().GetEndIdentifier());

            if (!useDegree) degree = 0;
            // E-CDS Step 3, 5, and 6, mark one-hop and two-hop neighbors as unvisited.
            // (We also set 3-hop neighbors as visited to mark as out-of-bounds.)
            iface->SetVisited(false);
            UINT8 priority = iface->GetRtrPriority();
            CdsInterface* ifaceN1Max = NULL;
            UINT8 priorityN1Max;// = priority;
            unsigned int degreeN1Max;// = degree;
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

            TRACE("   MECDS STEP 4 compare %s/%s %d:%d:%d to %s/%s %d:%d:%d\n",
                  iface->GetNode().GetName(), iface->GetName(), priority, degree, iface->GetAddress().GetEndIdentifier(),
                  ifaceN1Max->GetNode().GetName(), ifaceN1Max->GetName(), priorityN1Max, degreeN1Max, ifaceN1Max->GetAddress().GetEndIdentifier());

            if (ComparePriority(priority, degree, iface->GetAddress(),
                                priorityN1Max, degreeN1Max, ifaceN1Max->GetAddress()) > 0)
            {
                TRACE("   MECDS ENABLING iface %s/%s degree %d per STEP 4\n", node->GetName(), iface->GetName(), degree);
                if (!node->GetRelayStatus())
                {
                    node->SetRelayStatus(true);
                    numberOfRelays += 1;
                }
                if (!activateEssentialOnly)
                    iface->SetRelayStatus(true);
                // else essential relays activated later ...
                continue;
            }
            //TRACE("   MECDS Step 7 (ifaceN1Max: %s/%s)...\n", ifaceN1Max->GetNode().GetName(), ifaceN1Max->GetName());
            // E-CDS Step 7
            CdsGraph::Interface::SimpleList Q;
            Q.Append(*ifaceN1Max);
            ifaceN1Max->SetVisited(true);
            ifaceN1Max->GetNode().SetVisited(true);
            // E-CDS Step 8
            //TRACE("   MECDS Step 8 ...\n");
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
                    n->GetNode().SetVisited(true);
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
            TRACE(" MECDS Step 9 ...\n");
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
                    if (!node->GetRelayStatus())
                    {
                        node->SetRelayStatus(true);
                        numberOfRelays += 1;
                    }
                    isRelay = true;
                    //if (activateEssentialOnly)
                    //    ActivateEssentialInterfaces(*node);
                    //else
                        iface->SetRelayStatus(true);  //  - old MECDS activated ifaces here instead of deferred link activation.
                    break;
                }
            }
            if (!isRelay)
            {
                TRACE("   MECDS Disabling iface %s/%s degree %d per step 9\n", node->GetName(), iface->GetName(), degree);
                iface->SetRelayStatus(false);
            }
        }  // end while (NULL != iface ...)

        //if (activateEssentialOnly && node->GetRelayStatus())
        //    ActivateEssentialInterfaces(*node);

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


//////////////////////////////
/// GraphML loader

bool GraphRider::LoadGraphML(const char* filePath, CdsGraph& graph)
{
    ProtoXml::IterParser parser;
    // 1) iterate through file finding attribute keys and cache them...
    if (!parser.Open(filePath, "/graphml/key"))
    {
        PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to open GraphML file\n");
        return false;
    }
    xmlNodePtr nextKey;
    while (NULL != (nextKey = parser.GetNext()))
    {
        xmlChar* domain = xmlGetProp(nextKey, (xmlChar*)"for");
        GraphAttribute::InfoTable* infoTable = NULL;
        if (NULL == domain)
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() warning: GraphML <key> with no 'for' property?!\n");
            xmlFree(domain);
            continue;
        }
        else if (0 == strcmp("graph", (char*)domain))
        {
            infoTable = &graph_attr_info_table;
        }
        else if (0 == strcmp("node", (char*)domain))
        {
            infoTable = &node_attr_info_table;
        }
        else if (0 == strcmp("edge", (char*)domain))
        {
            infoTable = &edge_attr_info_table;
        }
        xmlChar* attrKey = xmlGetProp(nextKey, (xmlChar*)"id");
        xmlChar* attrName = xmlGetProp(nextKey, (xmlChar*)"attr.name");
        xmlChar* attrType = xmlGetProp(nextKey, (xmlChar*)"attr.type");
        GraphAttribute::DataType dataType = GraphAttribute::GetDataType((char*)attrType);
        GraphAttribute::Info* attrInfo = new GraphAttribute::Info();
        if ((NULL == attrInfo) || !attrInfo->Init((char*)attrKey, (char*)attrName, dataType))
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to create new GraphAttribute::Info entry\n");
            xmlFree(domain);
            xmlFree(attrKey);
            xmlFree(attrName);
            xmlFree(attrType);
            return false;
        }
        TRACE("inserting '%s' attr key id:%s name:%s ...\n", domain, attrKey, attrName);
        xmlFree(domain);
        xmlFree(attrKey);
        xmlFree(attrName);
        xmlFree(attrType);
        infoTable->Insert(*attrInfo);
    }

    // TBD - get /graphml/graph/data (and graph edgedefault property

    // 3) iterate through file finding nodes ...
    if (!parser.Open(filePath, "/graphml/graph/node"))
    {
        PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to open GraphML file\n");
        return false;
    }
    xmlNodePtr nextNode = parser.GetNext();
    if (NULL == nextNode)
    {
        PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to find any graphml/graph/node elements!\n");
        parser.Close();
        return false;
    }
    UINT32 nodeId = 0;
    do
    {
        // TBD - add allocation checks here
        xmlChar* nodeName = xmlGetProp(nextNode, (xmlChar*)"id");
        unsigned int id;
        if (1 == sscanf((const char*)nodeName, "n%u", &id))  // for nodes named "n1", "n2", etc
            nodeId = id;
        ASSERT(NULL == FindNode((const char*)nodeName));
        Node* node = new Node();
        if (!node->Init(nodeId++, (const char*)nodeName))
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: Node initialization failure!\n");
            return false;
        }
        xmlFree(nodeName);
        node_tree.Insert(*node);
        // 2a - get any node data (attributes)
        ProtoXml::IterFinder finder(nextNode, "/data");
        xmlNodePtr data;
        while (NULL != (data = finder.GetNext()))
        {
            xmlChar* key  = xmlGetProp(data, (xmlChar*)"key");
            GraphAttribute::Info* info = node_attr_info_table.FindInfo((char*)key);
            if (NULL == info)
            {
                PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unknown data key '%s'\n", key);
                xmlFree(key);
                continue;
            }
            xmlChar* value = xmlNodeGetContent(data);
            if (NULL == node->AddAttribute(info->GetName(), info->GetType(), (char*)value))
            {
                PLOG(PL_ERROR, "GraphRider::LoadGraphML() new GraphAttribute error: %s\n", GetErrorString());
            }
            xmlFree(value);

        }
    } while (NULL != (nextNode = parser.GetNext()));
    parser.Close();
    // 3) Iterate through file finding edges ...
    if (!parser.Open(filePath, "/graphml/graph/edge"))
    {
        PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to open GraphML file\n");
        return false;
    }
    xmlNodePtr nextEdge = parser.GetNext();
    if (NULL ==  nextEdge)
    {
        PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to find any graphml/graph/edge elements!\n");
        parser.Close();
        return false;
    }
    do
    {
        xmlChar* srcName = xmlGetProp(nextEdge, (xmlChar*)"source");
        Node* srcNode = FindNode((const char*)srcName);
        if (NULL == srcNode)
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: invalid edge source \"%s\"\n", srcName);
            xmlFree(srcName);
            parser.Close();
            return false;
        }
        xmlFree(srcName);
        xmlChar* dstName = xmlGetProp(nextEdge, (xmlChar*)"target");
        Node* dstNode = FindNode((const char*)dstName);
        if (NULL == dstNode)
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: invalid edge target \"%s\"\n", (const char*)dstName);
            xmlFree(dstName);
            parser.Close();
            return false;
        }
        xmlFree(dstName);
        // First, check to see if edge source/target "port" is used to identify interface/link type, else use edge "id"
        xmlChar* linkName = xmlGetProp(nextEdge, (xmlChar*)"sourceport");
        if (NULL == linkName)
            linkName = xmlGetProp(nextEdge, (xmlChar*)"id");
        LinkType* linkType = GetLinkType((const char*)linkName);
        if ((NULL == linkType) && (NULL == (linkType = AddLinkType((const char*)linkName))))
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to add link type\n");
            xmlFree(linkName);
            parser.Close();
            return false;
        }
        CdsInterface* srcIface = srcNode->FindInterfaceByName((const char*)linkName);
        if (NULL == srcIface)
            srcIface = srcNode->AddNamedInterface((const char*)linkName, ++iface_id_index);
        CdsInterface* dstIface = dstNode->FindInterfaceByName((const char*)linkName);
        if (NULL == dstIface)
            dstIface = dstNode->AddNamedInterface((const char*)linkName, ++iface_id_index);
        xmlFree(linkName);
        // TBD - make sure it doesn't already exist?
        //TRACE("connecting %s/%s -> %s/%s\n", srcNode->GetName(), srcIface->GetName(), dstNode->GetName(), dstIface->GetName());
        if (!graph.Connect(*srcIface, *dstIface, 1.0, true))
        {
            PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unable to connect interfaces in graph\n");
            parser.Close();
            return false;
        }
        CdsLink* fwdLink = srcIface->GetLinkTo(*dstIface);
        CdsLink* revLink = dstIface->GetLinkTo(*srcIface);
        // 3a - get any edge data (attributes)
        ProtoXml::IterFinder finder(nextEdge, "/data");
        xmlNodePtr data;
        while (NULL != (data = finder.GetNext()))
        {
            xmlChar* key  = xmlGetProp(data, (xmlChar*)"key");
            GraphAttribute::Info* info = edge_attr_info_table.FindInfo((char*)key);
            if (NULL == info)
            {
                PLOG(PL_ERROR, "GraphRider::LoadGraphML() error: unknown data key '%s'\n", key);
                xmlFree(key);
                continue;
            }
            xmlChar* value = xmlNodeGetContent(data);
            if (NULL != fwdLink)
                fwdLink->AddAttribute(info->GetName(), info->GetType(), (char*)value);
            if (NULL != revLink)
                revLink->AddAttribute(info->GetName(), info->GetType(), (char*)value);
            xmlFree(value);
        }

    } while (NULL != (nextEdge = parser.GetNext()));
    return true;
}  // end GraphRider::LoadGraphML()

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
    bool ecdsm = false;
    bool ecds = false;
    bool essentialEdges = false;
    //SetDebugLevel(12);

    ProtoLocation origin(38.802301,-69.240741, 0.0, ProtoLocation::GPS);

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
        else if (0 == strncmp(argv[i], "ecdsm", len))
        {
            ecdsm = true;
        }
        else if (0 == strncmp(argv[i], "fecds", len))
        {
            ecds = true;
        }
        else if (0 == strncmp(argv[i], "essential", len))
        {
            essentialEdges = true;
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

    if (false)
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
        if (graphML)
        {
            if (!graphRider.LoadGraphML(inputFile, graph))
            {
                fprintf(stderr, "gr error: unable to parse GraphML input file!\n");
                exit(0);
                //return -1;
            }
            // Iterate through nodes and output locations if x/y attributes are available
            NodeTree::Iterator nodeIterator(nodeTree);
            Node* node;
            while (NULL != (node = nodeIterator.GetNextItem()))
            {
                GraphAttribute* attr = node->GetAttribute("x");
                if (NULL != attr)
                {
                    // note converting meters to km here?
                    double x = 1000.0 * attr->GetDouble();
                    double y = 1000.0 * node->AccessAttributeTable().GetDouble("y");
                    double z = 0.0;
                    TRACE("node %s cart %lf, %lf, %lf\n", node->GetName(), x, y, z);
                    ProtoLocation loc(x, y, z,  ProtoLocation::CART);
                    loc.ConvertTo(ProtoLocation::GPS, true, &origin);
                    printf("node %s symbol circle,red,3 ", node->GetName());
                    printf("pos %lf,%lf,%lf\n", loc.GetLon(), loc.GetLat(), loc.GetAlt());
                }
                else
                {
                    //TRACE("node %s has no X attr\n", node->GetName());
                    //GraphAttribute::Table::Iterator iter(node->AccessAttributeTable());
                    //GraphAttribute* attr;
                    //while (NULL != (attr = iter.GetNextItem()))
                    //    TRACE("node %s has attr '%s'\n", node->GetName(), attr->GetName());
                }
            }
        }
        else
        {

            if (!graphRider.SetInputFile(inputFile))
            {
                perror("gr error: unable to open input file");
                return -1;
            }
            if (graphRider.ReadNextEpoch() < 0.0)
            {
                perror("gr warning: empty input file");
                return -1;
            }
            /*
            CdsGraph::InterfaceIterator ifaceIterator(graph);
            // Get first iface to init Dijkstra
            CdsGraph::Interface* iface = ifaceIterator.GetNextInterface();
            CdsGraph::DijkstraTraversal dijkstra(graph, iface->GetNode(), iface);
            ifaceIterator.Reset();
            //std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
            while (NULL != (iface = ifaceIterator.GetNextInterface()))
            {
                dijkstra.Reset(iface);
                //std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
                CdsGraph::Interface* dface;
                while (NULL != (dface = dijkstra.GetNextInterface()))
                {
                    const CdsGraph::Cost* cost = dijkstra.GetCost(*dface);
                    if (NULL != cost)
                         TRACE("   dijkstra iterated to iface \"%s\" cost:%lf\n", dface->GetName(), cost->GetValue());
                }
                //std::chrono::high_resolution_clock::time_point t3 = std::chrono::high_resolution_clock::now();
                //unsigned int duration = (unsigned int)(std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count());
                //TRACE("   duration: %u\n", duration);
            }
            */

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


        do
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
            if (ecdsm)
            {
                //TRACE("CALLING ECDSM ...\n");
                GraphRider::CalculateFullECDSM(nodeTree, graph, useDegree, &sortedVerticeItemPool);
            }
            else if (mecds)
            {
                TRACE("CALLING MECDS ...\n");
                GraphRider::CalculateFullMECDS(nodeTree, graph, useDegree, &sortedVerticeItemPool);
            }
            else if (ecds)
            {
                TRACE("CALLING ECDS ...\n");
                GraphRider::CalculateFullECDS(nodeTree, graph, useDegree, &sortedVerticeItemPool);
            }

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
                        Node& node2 = iface2->GetNode();
                        bool essentialLink;// = essentialEdges ? node->IsEssentialNeighbor(node2) : iface1->GetRelayStatus();
                        //essentialLink |= essentialEdges ? node2.IsEssentialNeighbor(*node) : iface2->GetRelayStatus();
                        CdsLink* fwdLink = iface1->GetLinkTo(*iface2);
                        CdsLink* revLink = iface2->GetLinkTo(*iface1);
                        if (essentialEdges)
                        {
                            essentialLink =  iface1->GetRelayStatus() &&
                                             node->GetRelayStatus() &&
                                             node->IsEssentialNeighbor(node2);
                            essentialLink |= iface2->GetRelayStatus() &&
                                             node2.GetRelayStatus() &&
                                             node2.IsEssentialNeighbor(*node);
                            //essentialLink |= iface2->GetRelayStatus() &&
                            //                 node2.IsEssentialNeighbor(*node);
                            essentialLink = fwdLink->IsActivated()  || revLink->IsActivated();
                        }
                        else
                        {
                            essentialLink = iface1->GetRelayStatus() || iface2->GetRelayStatus();
                        }


                        if (essentialLink)
                        {
                            // sdt link usage:  link node1,node2,name line color,thickness,opacity,stipple
                            printf("link %s,%s,%s line %s,4,x,0\n", iface1->GetNode().GetName(), iface2->GetNode().GetName(), iface1->GetName(), linkColor);
                            if (disconnectedList.Contains(node2))
                                disconnectedList.Remove(node2);
                        }
                        else
                        {
                            // Make sure non-essential links are set as dashed (stippled) line
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
        }  while (!graphML && graphRider.ReadNextEpoch() >= 0.0);
    }
    //fprintf(stderr, "gr: Done.\n");
    return 0;
}  // end main()

