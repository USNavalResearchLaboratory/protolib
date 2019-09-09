
// GraphRider (gr) program
// This program loads a graph from an existing SDT file.  It builds the graph from the SDT
// file by inserting Nodes corresponding to SDT nodes and connects the graph according
// to SDT "link" commands. Note that SDT scripts can dynamically update the graph state over
// time and hence this program updates the graph in "snapshots" between SDT "wait" commands.
// The ECDS relay set selection algorithm is currently implemented here.  The SDT input
// is essentially passed through but modifies the coloring of SDT nodes to illustrate
// the relay set selection.

#include "manetGraph.h"
#include "protoSpace.h"  // we use this for its bounding box iteration
#include <protoDebug.h>
#include <protoDefs.h>

#include <stdio.h>   // for sprintf()
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>   // for "isprint()"
#include <math.h>    // for "sqrt()"


void Usage()
{
    fprintf(stderr, "Usage: gr {ns|sdt} <mobilityTraceFile> range <commsRange> [degree]\n");
}

const unsigned int MAX_LINE = 256;

class Node : public ManetGraph::Node, public ProtoTree::Item
{
    public:
        Node();
        ~Node();

        // _MUST_ call init to create a default interface
        bool Init(UINT32 nodeId, const char* nodeName = NULL);

        unsigned int GetId() const
        {
            ManetGraph::Interface* iface = GetDefaultInterface();
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

    private:
        // ProtoTree::Item overrides so nodes
        // can be cached by name
        const char* GetKey() const
            {return node_name;}   
        unsigned int GetKeysize() const
            {return node_name_bits;} 
            
        char*           node_name;
        unsigned int    node_name_bits;
        UINT8           rtr_priority;
        bool            relay_status;
        bool            visited;

};  // end class Node

Node::Node()
 : node_name(NULL), node_name_bits(0), rtr_priority(0)
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
    
    ProtoAddress addr;
    addr.SetEndIdentifier(nodeId);
    ManetGraph::Interface* iface = new ManetGraph::Interface(*this, addr);
    if (NULL != iface)
    {
        return AppendInterface(*iface);
    }
    else
    {
        PLOG(PL_ERROR, "Node::Init() new ManetGraph::Interface() error: %s\n",
             GetErrorString());
        return false;
    }
}  // end Node::Init()


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
        
        ManetGraph& AccessGraph()
            {return graph;}
        
        static int CalculateFullECDS(ManetGraph&                                   graph, 
                                     ManetGraph::Interface::SimpleList&            relayList, 
                                     bool                                          useDegree = false,
                                     ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool = NULL);
        
        static double CalculateDensity(ManetGraph& graph);
        
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
        ManetGraph                                  graph;  
        ProtoTree                                   node_tree;
        UINT32                                      node_id_index;
        double                                      next_epoch_time; 
        unsigned int                                input_line_num;
                        
};  // end class GraphRider

GraphRider::GraphRider()
 : node_id_index(0), next_epoch_time(0.0), input_line_num(0)
{
}

GraphRider::~GraphRider()
{
}

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
                // Find numeric portion of "nodeName", if applicable, to use as an identifier
                unsigned int nodeId;
                bool nameHasNumber = (1 == sscanf(nameString, "%u", &nodeId));
                //ASSERT(nameHasNumber);
                char nodeNamePrefix[32];
                if (!nameHasNumber)
                    nameHasNumber = (2 == sscanf(nameString, "%[^0-9]%u", nodeNamePrefix, &nodeId));
                if (nameHasNumber)
                {
                    // Make sure it doesn't collide with existing node
                    ProtoAddress addr;
                    addr.SetEndIdentifier((UINT32)nodeId);
                    //  If collision, assign a unique identifier from our space
                    if (NULL != graph.FindInterface(addr))
                        nodeId = node_id_index++; 
                }
                else
                {
                    //  Assign an identifier from our space
                    //ASSERT(0);
                    nodeId = node_id_index++; 
                }
                // Adjust "node_id_index" if necessary to guarantee uniqueness
                // of assigned identifiers used when there is no embedded id
                if (nodeId >= node_id_index) 
                    node_id_index = nodeId + 1;

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
            bool link; 
            if (1 == sscanf(buffer, "link %s", nameString))
                link = true;
            else if (1 == sscanf(buffer, "unlink %s", nameString))
                link = false;
            else
                continue;
            char* nodeName1 = nameString;
            char* nodeName2 = strchr(nameString, ',');
            if (NULL == nodeName2)
            {
                PLOG(PL_ERROR, "gr error: malformed \"%s\" command in input file at line %lu!\n", 
                               link ? "link" : "unlink", input_line_num);
                return -1.0;
            }
            *nodeName2++ = '\0';
            char* endPtr = strchr(nodeName2, ',');
            if (NULL != endPtr) *endPtr = '\0';
            Node* node1 = static_cast<Node*>(node_tree.Find(nodeName1, (strlen(nodeName1)+1) << 3));
            Node* node2 = static_cast<Node*>(node_tree.Find(nodeName2, (strlen(nodeName2)+1) << 3));
            if ((NULL == node1) || (NULL == node2))
            {
                PLOG(PL_ERROR, "gr error: unknown nodes in \"%s\" command in input file at line %lu!\n", 
                               link ? "link" : "unlink", input_line_num);
                return -1.0;
            }
            ManetGraph::Interface* iface1 = node1->GetDefaultInterface();
            ManetGraph::Interface* iface2 = node2->GetDefaultInterface();
            // TBD - check to see if graph actually was changed?
            if (link)
            {
                ManetGraph::SimpleCostDouble cost(1.0);
                if (!graph.Connect(*iface1, *iface2, cost, true))
                    PLOG(PL_ERROR, "gr error: unable to connect interfaces in graph\n");
            }
            else
            {
                graph.Disconnect(*iface1, *iface2, true);
            }
        }
    }  // end while reading()
    return (gotLine ? lastTime : -1.0);
}  // end ReadNextEpoch()

// "relayList" is filled with the selected relays
int GraphRider::CalculateFullECDS(ManetGraph&                                   graph, 
                                  ManetGraph::Interface::SimpleList&            relayList, 
                                  bool                                          useDegree,
                                  ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool)
{
    // Now that we have a fully updated "graph", perform Relay Set Selection
    // algorithm for each node in graph
    int numberOfRelays=0;
    ManetGraph::InterfaceIterator it(graph);
    ManetGraph::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        Node& node = static_cast<Node&>(iface->GetNode());
        UINT8 priority = useDegree ? iface->GetAdjacencyCount() : node.GetRtrPriority();
        
        TRACE("ECDS check for node %s priority:%d:%d...\n", node.GetName(), priority, node.GetId());
        
        // E-CDS Steps 1,2
        if (iface->GetAdjacencyCount() < 2)
        {
            node.SetRelayStatus(false);
            continue;
        }

        // E-CDS Step 3,4,5, & 6
        // TBD - use an "unvisitedList" that first caches all 1-hop neighbors and later removes them
        //       as they are visited?  This would eliminate the need for the "SetVisited()" method ...
        //       This would be a little more elegant, but a little more CPU/memory ...???
        ManetGraph::AdjacencyIterator iteratorN1(*iface);
        ManetGraph::Interface* ifaceN1Max = NULL;
        bool isRelay = true;  // this will be set to "false" if a test fails
        ManetGraph::Interface* ifaceN1;
        while (NULL != (ifaceN1 = iteratorN1.GetNextAdjacency()))
        {
            Node& nodeN1 = static_cast<Node&>(ifaceN1->GetNode());
            nodeN1.SetVisited(false);  // init for later path search
            // Save n1_max needed for E-CDS Step 6
            UINT8 priorityN1 = useDegree ? ifaceN1->GetAdjacencyCount() : nodeN1.GetRtrPriority();
            if (NULL == ifaceN1Max)
            {
                ifaceN1Max = ifaceN1;
            }
            else
            {
                Node& nodeN1Max = static_cast<Node&>(ifaceN1Max->GetNode());
                UINT8 priorityN1Max = useDegree ? ifaceN1Max->GetAdjacencyCount() : nodeN1Max.GetRtrPriority();
                if ((priorityN1 > priorityN1Max) ||
                    ((priorityN1 == priorityN1Max) &&
                     (nodeN1.GetId() > nodeN1Max.GetId())))
                {
                    ifaceN1Max = ifaceN1;
                }
            }

            if ((priorityN1 > priority) ||
                ((priorityN1 == priority) &&
                 (nodeN1.GetId() > node.GetId())))
            {
                isRelay = false;  // failed test of E-CDS Step 4
            }
            //if (isRelay) break;
            //bool saveRelay = isRelay;
          
            /*
            // Check 2-hop neighbors (other neighbors of N1)
            ManetGraph::AdjacencyIterator iteratorN2(*ifaceN1);
            ManetGraph::Interface* ifaceN2;
            while (NULL != (ifaceN2 = iteratorN2.GetNextAdjacency()))
            {
                if (ifaceN2 == iface) continue;  // was "iface" itself
                if (iface->HasEdgeTo(*ifaceN2)) continue;  // was 1-hop neighbor of "iface"
                Node& nodeN2 = static_cast<Node&>(ifaceN2->GetNode());
                nodeN2.SetVisited(false);  // init for later path search
                UINT8 priorityN2 = useDegree ? ifaceN2->GetAdjacencyCount() : nodeN2.GetRtrPriority();
                
                
                /*
                if (NULL == ifaceN1Max)
                {
                    ifaceN1Max = ifaceN2;
                }
                else
                {
                    Node& nodeN1Max = static_cast<Node&>(ifaceN1Max->GetNode());
                    UINT8 priorityN1Max = useDegree ? ifaceN1Max->GetAdjacencyCount() : nodeN1Max.GetRtrPriority();
                    if ((priorityN2 > priorityN1Max) ||
                        ((priorityN2 == priorityN1Max) &&
                         (nodeN2.GetId() > nodeN1Max.GetId())))
                    {
                        ifaceN1Max = ifaceN2;
                    }
                }
                *
                
                
                
                if ((priorityN2 > priority) ||
                    ((priorityN2 == priority) &&
                     (nodeN2.GetId() > node.GetId())))
                {
                    isRelay = false;  // failed test of E-CDS Step 4
                }
            }  // end while (NULL != ifaceN2)
            */
            //if (saveRelay) isRelay = true;
        }  // end (while (NULL != ifaceN1)
        if (isRelay)
        {
            // Passed test of E-CDS Step 4
            node.SetRelayStatus(true);
            numberOfRelays++;
            relayList.Append(*iface);
            TRACE("   Adding node %s to relay list\n", node.GetName());
            continue;
        }
        
        //TRACE("   doing path search for node %s ...\n", node.GetName());

        // E-CDS Step 7,8 - init path search 'Q'
        
        // TBD - implement path length limit on path search (and eventually "k-depth" relay set selection)
        ASSERT(NULL != ifaceN1Max);
        ManetGraph::Interface::SortedList Q(sortedVerticeItemPool);
        if (!Q.Append(*ifaceN1Max))
        {
            PLOG(PL_ERROR, "gr: error adding 'ifaceN1Max' to 'Q'\n");
            return -1;
        }
        static_cast<Node&>(ifaceN1Max->GetNode()).SetVisited(true);
        // E-CDS Step 8 - perform path search
        ManetGraph::Interface* x;
        while (NULL != (x = Q.RemoveHead()))
        {
            Node& nodeNx = static_cast<Node&>(x->GetNode());
            UINT8 priorityNx = useDegree ? x->GetAdjacencyCount() : nodeNx.GetRtrPriority();
            bool xIsInN1 = x->HasEdgeTo(*iface); // true if 'x' is 1-hop neighbor of "n0"?
            ManetGraph::AdjacencyIterator iteratorX(*x);
            ManetGraph::Interface* n;
            while (NULL != (n = iteratorX.GetNextAdjacency()))
            {
                if (n == iface) continue;  // was n == iface == "n0"
                bool nIsInN1 = n->HasEdgeTo(*iface); // true if 'n' is 1-hop neighbor of "n0"
                if (!xIsInN1 && !nIsInN1) continue;  // link not in 2-hop neighborhood of "n0"
                Node& nodeNn = static_cast<Node&>(n->GetNode());
                if (!nodeNn.WasVisited())
                {
                    // E-CDS Step 8a - mark node as "visited"
                    nodeNn.SetVisited(true);
                    // E-CDS Step 8b - check if RtrPri(n) > RtrPri(n0)
                    UINT8 priorityNn = useDegree ? n->GetAdjacencyCount() : nodeNn.GetRtrPriority();
                    if (nIsInN1)
                        TRACE("      visited node %s/%d:%d via node %s/%d:%d\n", nodeNn.GetName(), priorityNn, nodeNn.GetId(), nodeNx.GetName(), priorityNx, nodeNx.GetId());
                    if ((priorityNn > priority) ||
                        ((priorityNn == priority) &&
                         (nodeNn.GetId() > node.GetId())))
                    {
                        if (!Q.Append(*n))
                        {
                            PLOG(PL_ERROR, "gr: error adding 'n' to 'Q'\n");
                            return -1;
                        }
                    }
                }
            }  // end while (NULL != n)
        }  // end while (NULL != x)
        // E-CDS Step 9
        TRACE("   doing step 9 for node %s ...\n", node.GetName());
        bool relayStatus = false;
        iteratorN1.Reset();
        while (NULL != (ifaceN1 = iteratorN1.GetNextAdjacency()))
        {
            if (!static_cast<Node&>(ifaceN1->GetNode()).WasVisited())
            {
                relayStatus = true;
                numberOfRelays++;
                break;
            }
        }
        
        if (relayStatus)
        {
            TRACE("   Adding node %s to relay list\n", node.GetName());
            relayList.Append(*iface);
        }
        else
        {
            TRACE("   PATH SEARCH SUCCEEDED. no need to be a relay\n");
        }
        node.SetRelayStatus(relayStatus);

    }  // while (NULL != node)  (relay set selection)
    return numberOfRelays;
}  // end GraphRider::CalculateFullECDS()

double GraphRider::CalculateDensity(ManetGraph& graph)
{
    int neighborCount = 0;
    int nodeCount = 0;
    ManetGraph::InterfaceIterator it(graph);
    ManetGraph::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        ManetGraph::AdjacencyIterator iteratorN1(*iface);
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
    bool gotInput = false;
    bool useDegree = false;
    // Parse the command line
    int i = 1;
    while (i < argc)
    {
        size_t len = strlen(argv[i]);
        if (0 == strncmp(argv[i], "input", len))
        {
            if (++i == argc)
            {
                fprintf(stderr, "gr error: missing \"input\" argument!\n");
                Usage();
                return -1;
            }
            if (!graphRider.SetInputFile(argv[i]))
            {
                perror("gr error: unable to open input file");
                return -1;
            }
            gotInput = true;
        }
        else if (0 == strncmp(argv[i], "degree", len))
        {
            useDegree = true;
        }
        else
        {
            fprintf(stderr, "gr error: invalid command: %s\n", argv[i]);
            Usage();
            return -1;
        }
        i++;
    }
    
    if (!gotInput)
    {
        fprintf(stderr, "gr error: no input file specified!\n");
        Usage();
        return -1;
    }
    
    ManetGraph& graph = graphRider.AccessGraph();
    // This is a pool of ProtoGraph::Vertice::SortedList::Items
    // that are used for temporary lists of ManetGraph::Interfaces
    // for various graph manipulations, etc.  Note that the use of an 
    // "external" item pool is _optional_ for the ProtoGraph/ManetGraph
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
        
        // a) Initialize our "disconnectedList" with all ifaces in graph
        ManetGraph::Interface::SortedList disconnectedList(&sortedVerticeItemPool);
        ManetGraph::InterfaceIterator ifaceIterator(graph);
        ManetGraph::Interface* iface;
        while (NULL != (iface = ifaceIterator.GetNextInterface()))
            disconnectedList.Insert(*iface);
        
        
        // "CalculateFullECDS" implements the ECDS relay set selection
        // algorithm, marking selected nodes using the Node::SetRelayStatus() 
        // method.  It also copies the selected nodes into the "relayList"
        // that is passed to it.  (This is mainly for illustrative purposes
        // to demonstrate the utility of the ManetGraph lists, iterators, etc
        ManetGraph::Interface::SimpleList relayList;
        GraphRider::CalculateFullECDS(graph, relayList, useDegree, &sortedVerticeItemPool);
        
        // Iterate over our relay set and color the relays "blue" and their
        // (non-relay) one-hop neighbors "green"
        
        
        // b) Remove relays (and their neighbors) from the "disconnectedList"
        //    and output appropriate node colors
        ManetGraph::Interface::SimpleList::Iterator relayIterator(relayList);
        ManetGraph::Interface* relayIface;
        //TRACE("coloring ECDS relays blue ...\n");
        while (NULL != (relayIface = relayIterator.GetNextInterface()))
        {
            // It's a relay, so remove from disconnected list
            if (NULL != disconnectedList.FindInterface(relayIface->GetAddress()))
                disconnectedList.Remove(*relayIface);
            
            Node& relayNode = static_cast<Node&>(relayIface->GetNode());
            TRACE("iterated to node %s status %d in the relay list ...\n", relayNode.GetName(), relayNode.GetRelayStatus());
            
            printf("node %s symbol circle,blue,3\n", relayNode.GetName());
            ManetGraph::AdjacencyIterator neighborIterator(*relayIface);
            ManetGraph::Interface* neighborIface;
            while (NULL != (neighborIface = neighborIterator.GetNextAdjacency()))
            {
                // It's a relay neighbor, so remove from disconnected list
                if (NULL != disconnectedList.FindInterface(neighborIface->GetAddress()))
                    disconnectedList.Remove(*neighborIface);
                Node& neighborNode = static_cast<Node&>(neighborIface->GetNode());
                if (!neighborNode.GetRelayStatus())
                    printf("node %s symbol circle,green,3\n", neighborNode.GetName());
            }  
        }
        
        // c) Color any nodes remaining in "disconnectedList" red (orphans and orphan pairs)
        ManetGraph::Interface::SortedList::Iterator dcIterator(disconnectedList);
        ManetGraph::Interface* disconnectedIface;
        while (NULL != (disconnectedIface = dcIterator.GetNextInterface()))
        {
            Node& disconnectedNode = static_cast<Node&>(disconnectedIface->GetNode());
            printf("node %s symbol circle,red,3\n", disconnectedNode.GetName());
        }   

        //double nodeDensity = GraphRider::CalculateDensity(graph);
    }
    fprintf(stderr, "gr: Done.\n");
    return 0;
}  // end main()

