
// The purpose of this program is to illustrate use of the "NetGraphTemplate" 
// classes.  (Note the "NetGraphTemplate" allows different graph subclasses to 
// be created with different metric properties.  The "MyGraph" exaple here 
// assumes a non-negative "double" value metric for "link cost".

#include "manetGraph.h"
#include "protoSpace.h"  // we use this for its bounding box iteration
#include <protoDebug.h>
#include <protoDefs.h>

#include <stdio.h>   // for sprintf()
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>   // for "isprint()"
#include <math.h>    // for "sqrt()"

// "FastReader" is handy class I use for doing
//  buffered (fast) reading of an input file.
class FastReader
{
    public:
        enum Result {OK, ERROR_, DONE, TIMEOUT};
        FastReader(FILE* filePtr);
        FastReader::Result Read(char*           buffer,
                                unsigned int*   len,
                                double timeout = -1.0);
        FastReader::Result Readline(char*           buffer,
                                    unsigned int*   len,
                                    double          timeout = -1.0);

        bool Seek(int offset);

    private:
        enum {BUFSIZE = 2048};
        FILE*        file_ptr;
        char         savebuf[BUFSIZE];
        char*        saveptr;
        unsigned int savecount;
};  // end class FastReader

void Usage()
{
    fprintf(stderr, "Usage: graphExample <mobilityTraceFile>\n");
}

const unsigned int MAX_LINE = 256;

class MyNode; 

class MyInterface : public NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, MyInterface, ManetLink, MyNode>
{
    public:
        MyInterface(MyNode& theNode, const ProtoAddress& addr)
            : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, MyInterface, ManetLink, MyNode>(theNode, addr) {}
        MyInterface(MyNode& theNode)
            : NetGraph::InterfaceTemplate<NetGraph::SimpleCostDouble, MyInterface, ManetLink, MyNode>(theNode) {}
};  // end class MyInterface
        
class MyInterface;

class MyNode : public NetGraph::NodeTemplate<MyInterface>, public ProtoSpace::Node
{
    public:
        MyNode();
        ~MyNode();

        // _MUST_ call init to create a default interface
        bool Init(const ProtoAddress& addr);

        unsigned int GetId() const;

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

        void SetPosition(double xPos, double yPos)
        {
            ordinate[0] = xPos;
            ordinate[1] = yPos;
        }
        double GetOrdinateX() const
            {return ordinate[0];}
        double GetOrdinateY() const
            {return ordinate[1];}

        // ProtoSpace::Node required overrides
        unsigned int GetDimensions() const
            {return 2;}
        double GetOrdinate(unsigned int  index) const
            {return (ordinate[index]);}
        const double* GetOrdinatePtr() const
            {return ordinate;}

    private:
        UINT8   rtr_priority;
        bool    relay_status;
        bool    visited;
        double  ordinate[2];  // x,y coordinates

};  // end class MyNode


class MyGraph : public NetGraphTemplate<NetGraph::SimpleCostDouble, MyInterface, ManetLink, MyNode> {};


MyNode::MyNode()
 : rtr_priority(0)
{
    ordinate[0] = ordinate[1] = 0.0;
}

MyNode::~MyNode()
{
}

bool MyNode::Init(const ProtoAddress& addr)
{
    MyGraph::Interface* iface = new MyGraph::Interface(*this, addr);
    if (NULL != iface)
    {
        return AppendInterface(*iface);
    }
    else
    {
        PLOG(PL_ERROR, "MyNode::Init() new MyGraph::Interface() error: %s\n",
             GetErrorString());
        return false;
    }
}  // end MyNode::Init()

unsigned int MyNode::GetId() const
{
    MyGraph::Interface* iface = GetDefaultInterface();
    if (NULL != iface)
    {
        UINT8* addrPtr = (UINT8*)(iface->GetAddress().GetRawHostAddress());
        unsigned int id = addrPtr[2];
        id = (id << 8) + addrPtr[3];
        return id;
    }
    else
    {
        ASSERT(0);
        return ((unsigned int)-1);
    }
}  // end MyNode::GetId()

const double RANGE_MAX = 300.0;

inline double CalculateDistance(double x1, double y1, double x2,double y2)
{
    double dx = x1 - x2;
    double dy = y1 - y2;
    return sqrt(dx*dx + dy*dy);
}  // end CalculateDistance()

// This updates the "space" with data from the next time "epoch"
double ReadNextEpoch(FastReader& reader, ProtoSpace& space, MyGraph& graph);

// This updates the graph connectivity from the "space" and using "RANGE_MAX" comms range
bool UpdateGraphFromSpace(MyGraph&                                   graph,   
                          ProtoSpace&                                   space,   
                          ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool);

// This calculates an ECDS relay set given the "graph"
int CalculateFullECDS(MyGraph&                                   graph, 
                      MyGraph::Interface::SimpleList&            relayList, 
                      ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool);

// This calculates the nodal neighbor "density" of the given "graph"
double CalculateDensity(MyGraph& graph);


// Some "hacky" scaling factors to output a "GPS" version
// (some other code needs to be uncommented, etc to use this)
double X_MIN = -77.028633;
double X_MAX = -77.021267;
double X_SCALE = (X_MAX - X_MIN) / 1600.0;

double Y_MIN = 38.828533;
double Y_MAX = 38.822817;
double Y_SCALE = (Y_MAX - Y_MIN) / 600.0;


int main(int argc, char* argv[])
{

    // argv[1] is expected to be an ns-2 "mobility trace" file
    // (i.e. with "setdest" commands).  This program uses the
    // "destinations" given from "setdest" commands to as node
    // positions.  It does _not_ interpolate the "setdest"
    // locations itself.  We will eventually create a more useful
    // version of this program (likely under a different name, etc)
    // that can use our more recently developed node location/motion
    // XML schema (See the NRL Mobile Network Modeling Tools).
    if (argc < 2)
    {
        Usage();
        return -1;
    }

    // Create a MyGraph instance that we will populate
    // with nodes/interfaces from our "mobility trace file"
    MyGraph graph;

    // This is a pool of ProtoGraph::Vertice::SortedList::Items
    // that are used for temporary lists of MyGraph::Interfaces
    // as we adjust graph connectivity, etc.  Note that the use of an 
    // "external" item pool is _optional_ for the ProtoGraph/MyGraph
    // list classes, but can boost performance by reducing memory
    // alloc/deallocs when doing a lot of list manipulation.
    // If a list was inited with a "pool", then it is important to
    // keep the "pool" valid until after any associate "lists" are
    // destroyed as "pools" do _not_ keep track of which lists are
    // using them (yet!).
    ProtoGraph::Vertice::SortedList::ItemPool sortedVerticeItemPool;

    // We use the ProtoSpace to organize nodes by their position
    // and use the ProtoSpace bounding box iterator to efficiently
    // find adjacencies
    ProtoSpace space;

    // Open the trace file for reading
    FILE* infile = fopen(argv[1], "r");
    if (NULL == infile)
    {
        perror("graphExample: fopen error");
        return -1;
    }

    printf("bgbounds -100,-100,1500,500\n");

    // Uncomment this for the "GPS" version
    //printf("bgbounds -77.028633,38.828533,-77.021267,38.822817\n");


    double lastTime = -1.0;
    double nextTime;
    FastReader reader(infile);
    // "ReadNextEpoch" adds new "Nodes" to the "space" (and updates
    // the position of Nodes from prior epochs - the "graph" is provided
    // so that already existing "Nodes" can be found.)
    while (0.0 <= (nextTime = ReadNextEpoch(reader, space, graph)))
    {
        // Output "wait" command for SDT visualization, if applicable
        if (lastTime >= 0.0)
            printf("wait %f\n", (float)(1000.0 * (nextTime - lastTime)));
        lastTime = nextTime;
        
        
        // "UpdateGraphFromSpace()" adds/removes "links" between the Nodes depending
        // upon their spatial distance and the "RANGE_MAX" communications range
        // (TBD - pass the "comms range" as a parameter to this function?)
        TRACE("graphExample: Updating graph connections for time:%lf ..................................\n", nextTime);
        if (!UpdateGraphFromSpace(graph, space, &sortedVerticeItemPool))
        {
            PLOG(PL_ERROR, "graphExample: error updating graph ...\n");
            return -1;
        }
        TRACE("graphExample:  Performing E-CDS relay set selection for time:%lf ===================================\n", nextTime);

        
        // "CalculateFullECDS" implements the ECDS relay set selection
        // algorithm, marking selected nodes using the MyNode::SetRelayStatus() 
        // method.  It also copies the selected nodes into the "relayList"
        // that is passed to it.  (This is mainly for illustrative purposes
        // to demonstrate the utility of the MyGraph lists, iterators, etc
        MyGraph::Interface::SimpleList relayList;
        int relays = CalculateFullECDS(graph, relayList, &sortedVerticeItemPool);
        TRACE("relays is %d\n", relays);
        
        // Iterate over our relay set and color the relays "blue" and their
        // (non-relay) one-hop neighbors "green"
        // We also build up a "disconnectedList" by first putting all nodes
        // into it and then remove the relays and their one-hop neighbors.
        // The disconnected nodes remaining in the list are colored "red"
        
        
        // a) Initialize our "disconnectedList" with all ifaces in graph
        //    (note we _could_ have used the "sortedVerticeItemPool" here as
        //     well, but didn't to illustrate that it is optional.  We
        //     would perform better here if did use it, though.
        MyGraph::Interface::SortedList disconnectedList(&sortedVerticeItemPool);
        MyGraph::InterfaceIterator ifaceIterator(graph);
        MyGraph::Interface* iface;
        while (NULL != (iface = ifaceIterator.GetNextInterface()))
            disconnectedList.Insert(*iface);
        
        // b) Remove relays (and their neighbors) from the "disconnectedList"
        //    and output appropriate node colors
        MyGraph::Interface::SimpleList::Iterator relayIterator(relayList);
        MyGraph::Interface* relayIface;
        while (NULL != (relayIface = relayIterator.GetNextInterface()))
        {
            // It's a relay, so remove from disconnected list
            if (NULL != disconnectedList.FindInterface(relayIface->GetAddress()))
                disconnectedList.Remove(*relayIface);
            
            MyNode& relayNode = relayIface->GetNode();
            printf("node %d symbol circle,blue,3\n", relayNode.GetId());
            MyGraph::AdjacencyIterator neighborIterator(*relayIface);
            MyGraph::Interface* neighborIface;
            while (NULL != (neighborIface = neighborIterator.GetNextAdjacency()))
            {
                // It's a relay neighbor, so remove from disconnected list
                if (NULL != disconnectedList.FindInterface(neighborIface->GetAddress()))
                    disconnectedList.Remove(*neighborIface);
                MyNode& neighborNode = neighborIface->GetNode();
                if (!neighborNode.GetRelayStatus())
                    printf("node %d symbol circle,green,3\n", neighborNode.GetId());
            }  
        }
        
        // c) Color any nodes remaining in "disconnectedList" red
        MyGraph::Interface::SortedList::Iterator dcIterator(disconnectedList);
        MyGraph::Interface* disconnectedIface;
        while (NULL != (disconnectedIface = dcIterator.GetNextInterface()))
        {
            MyNode& disconnectedNode =disconnectedIface->GetNode();
            printf("node %d symbol circle,red,3\n", disconnectedNode.GetId());
        }   

        double nodedensity = 0;
        nodedensity = CalculateDensity(graph);
        TRACE("density is %f\n",nodedensity);

    }
    fprintf(stderr, "graphExample: Done.\n");
    return 0;
}  // end main()


double ReadNextEpoch(FastReader& reader, ProtoSpace& space, MyGraph& graph)
{
    double lastTime = -1.0;
    // Read the "setdest" lines from the file, finding
    // new nodes and their x,y positions versus time

    // line format we want is '<sim> at <time> "<nodeName> setdest x y velocity"'
    bool reading = true;
    while (reading)
    {
        char buffer[MAX_LINE];
        unsigned int len = MAX_LINE;
        switch (reader.Readline(buffer, &len))
        {
            case FastReader::OK:
                break;
            case FastReader::ERROR_:
                PLOG(PL_ERROR, "graphExample: error reading file\n");
                return false;
            case FastReader::DONE:
                reading = false;
                break;
            case FastReader::TIMEOUT:
                return false; // should never occur for this program
        }

        // Is this a new "time"?
        char sim[32];
        double time;
        if (2 != sscanf(buffer, "%s at %lf", sim, &time))
            continue;  // go to next line

        if (lastTime < 0.0)
        {
            TRACE("setting lastTime to %lf\n", time);
            lastTime = time;
        }
        else if (time != lastTime)
        {
            // It's a new epoch, so seek backwards length of this line and return
            reader.Seek(-len);
            TRACE("returning lastTime %lf\n", lastTime);
            return lastTime;
        }

        // Find leading quote of "nodeName setdest x y velocity" command
        char* ptr = strchr(buffer, '\"');
        if (NULL == ptr)
            continue;  // go to next line
        else
            ptr++;
        char nodeName[32];
        double x, y, vel;
        if (4 != sscanf(ptr, "%s setdest %lf %lf %lf", nodeName, &x, &y, &vel))
            continue;

        // This rescales to our "gps" bounding box, but breaks our range
        // calculation ...
        /* 
        x = ((x + 100)*X_SCALE) + X_MIN;
        y = ((y + 100)*Y_SCALE) + Y_MIN;
        */

        // Find numeric portion of "nodeName"
        char nodeNamePrefix[32];
        unsigned int nodeId;
        if (2 != sscanf(nodeName, "%[^0-9]%u", nodeNamePrefix, &nodeId))
        {
            PLOG(PL_ERROR, "graphExample: nodeName \"%s\" has no numeric identifier content!\n", nodeName);
            return false;
        }

        // Make/Update an entry nodeID into ManetSpace which was just read in
        // Make an IPv4 addr from the "nodeId" for ManetSpace use

        UINT8 ipv4Addr[4];
        ipv4Addr[0] = 192;
        ipv4Addr[1] = 168;
        ipv4Addr[2] = nodeId / 256;
        ipv4Addr[3] = nodeId % 256;
        ProtoAddress addr;
        addr.SetRawHostAddress(ProtoAddress::IPv4, (char*)ipv4Addr, 4);
        MyNode* node = graph.FindNode(addr);

        if (NULL == node)
        {
            // New node, create and insert into "space"
            MyNode* node = new MyNode();
            if (!node->Init(addr))
            {
                PLOG(PL_ERROR, "graphExample: error initializing new node!\n");
                return -1;
            }
            node->SetPosition(x, y);
            graph.InsertNode(*node);
            if (!space.InsertNode(*node))
            {
                PLOG(PL_ERROR, "graphExample: error inserting node into space\n");
                return -1;
            }
            // Output new node position for SDT visualization
            printf("node %d position %f,%f symbol circle,red,3 label white\n", nodeId, x, y);
        }
        else
        {
            // Existing node, update position in "space", etc
            TRACE("Removing node %d from space ...\n", nodeId);
            space.RemoveNode(*node);
            node->SetPosition(x, y);
            space.InsertNode(*node);
            // Update node position for SDT visualization
            printf("node %d position %f,%f\n", nodeId, x, y);
        }
    }  // end while reading()

    TRACE("returning 2 lastTime %lf\n", lastTime);

    return lastTime;
}  // end ReadNextEpoch()

bool UpdateGraphFromSpace(MyGraph& graph, ProtoSpace& space, ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool)
{
    MyGraph::InterfaceIterator it(graph);
    MyGraph::Interface* iface;
    ;
    unsigned int count = 0;
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();

        const ProtoAddress& nodeAddr = iface->GetAddress();

        // Cache list of node's current neighbors into
        // a list we can retrieve from as we determine
        // our "within range" nodes.
        MyGraph::Interface::SortedList nbrList(sortedVerticeItemPool);
        MyGraph::AdjacencyIterator nbrIterator(*iface);
        MyGraph::Interface* nbrIface;
        while (NULL != (nbrIface = nbrIterator.GetNextAdjacency()))
        {
            if (!nbrList.Insert(*nbrIface))
            {
                PLOG(PL_ERROR, "graphExample: error adding neighbor to temp list\n");
                return false;
            }
        }

        // Iterate through our ProtoSpace from this node's location,
        // looking for neighbor nodes within RANGE_MAX distance
        ProtoSpace::Iterator sit(space);
        if (!sit.Init(node.GetOrdinatePtr()))
        {
            PLOG(PL_ERROR, "graphExample: error initializing space iterator\n");
            return false;
        }

        MyNode* nbr;
        count = 0;
        double lastDist = -1.0;
        double distance;
        while (NULL != (nbr = static_cast<MyNode*>(sit.GetNextNode(&distance))))
        {
            count++;

            /*
            TRACE("Distance from %s to ", node.GetDefaultAddress().GetHostString());
            TRACE("%s is %lf meters (xrange:%lf yrange:%lf) (count:%d)\n",
            nbr->GetDefaultAddress().GetHostString(), distance,
            fabs(x - nbr->GetOrdinateX()), fabs(y - nbr->GetOrdinateY()), count);
            */

            ASSERT(lastDist <= distance);
            lastDist = distance;

            if (nbr == &node)
            {
                TRACE("count:%d - %s found self\n", count, nodeAddr.GetHostString());
                continue;
            }

            if (distance <= RANGE_MAX)
            {
                // Is this "nbr" already connected as a neighbor in our graph?
                MyGraph::Interface* defaultIface = nbr->GetDefaultInterface();
                ASSERT(NULL != defaultIface);
                const ProtoAddress& nbrAddr = defaultIface->GetAddress();
                nbrIface = nbrList.FindInterface(nbrAddr);
                if (NULL == nbrIface)
                {
                    TRACE("count:%d - connecting %s to ", count, nodeAddr.GetHostString());
                    TRACE("%s\n", nbrAddr.GetHostString());
                    MyGraph::SimpleCostDouble cost(1.0);
                    if (!graph.Connect(*iface, *defaultIface, cost, true))
                    {
                        PLOG(PL_ERROR, "graphExample error: unable to connect interfaces in graph\n");
                        return false;
                    }
                    printf("link %u,%u,green\n", node.GetId(), nbr->GetId());
                }
                else
                {
                    TRACE("count:%d - %s already neighbor of ", count, nodeAddr.GetHostString());
                    TRACE("%s\n", nbrAddr.GetHostString());
                    nbrList.Remove(*nbrIface);
                }
            }
            else
            {
                // Disconnect any former neighbors that were no longer in range
                if (!nbrList.IsEmpty())
                {
                    MyGraph::Interface::SortedList::Iterator listIterator(nbrList);
                    while (NULL != (nbrIface = listIterator.GetNextInterface()))
                    {
                        graph.Disconnect(*iface, *nbrIface, true);
                        TRACE("count:%d - %s no longer neighbor of ", count, nodeAddr.GetHostString());
                        TRACE("%s\n", nbrIface->GetAddress().GetHostString());
                        printf("unlink %u,%u\n", node.GetId(), nbrIface->GetNode().GetId());
                    }
                }
                break;
            }
        }  // end while (NULL != nbr)
    }  // end while (NULL != node)  (graph update)
    return true;
}  // end UpdateGraphFromSpace()

// "relayList" is filled with the selected relays
int CalculateFullECDS(MyGraph&                                   graph, 
                      MyGraph::Interface::SimpleList&            relayList, 
                      ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool)
{
    // Now that we have a fully updated "graph", perform Relay Set Selection
    // algorithm for each node in graph
    int numberOfRelays=0;
    MyGraph::InterfaceIterator it(graph);
    MyGraph::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();
        UINT8 priority = node.GetRtrPriority();

        // E-CDS Steps 1,2
        TRACE("   node:%u adjacencyCount:%u\n", node.GetId(), iface->GetAdjacencyCount());

        if (iface->GetAdjacencyCount() < 2)
        {
            node.SetRelayStatus(false);
            continue;
        }

        // E-CDS Step 3,4,5, & 6
        // "nbrList" will cache the "node" 1-hop neighbors for "visited" elimination with the path search
        MyGraph::AdjacencyIterator iteratorN1(*iface);
        MyGraph::Interface* ifaceN1Max = NULL;
        bool isRelay = true;  // this will be set to "false" if a test fails
        MyGraph::Interface* ifaceN1;
        while (NULL != (ifaceN1 = iteratorN1.GetNextAdjacency()))
        {
            MyNode& nodeN1 = ifaceN1->GetNode();
            nodeN1.SetVisited(false);  // init for later path search
            // Save n1_max needed for E-CDS Step 6
            UINT8 priorityN1 = nodeN1.GetRtrPriority();
            if (NULL == ifaceN1Max)
            {
                ifaceN1Max = ifaceN1;
            }
            else
            {
                MyNode& nodeN1Max = ifaceN1Max->GetNode();
                UINT8 priorityN1Max = nodeN1Max.GetRtrPriority();
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
            // Check 2-hop neighbors (other neighbors of N1)
            MyGraph::AdjacencyIterator iteratorN2(*ifaceN1);
            MyGraph::Interface* ifaceN2;
            while (NULL != (ifaceN2 = iteratorN2.GetNextAdjacency()))
            {
                if (ifaceN2 == iface) continue;  // was "iface" itself
                if (iface->HasEdgeTo(*ifaceN2)) continue;  // was 1-hop neighbor of "iface"
                MyNode& nodeN2 = ifaceN2->GetNode();
                nodeN2.SetVisited(false);  // init for later path search
                UINT8 priorityN2 = nodeN2.GetRtrPriority();
                if ((priorityN2 > priority) ||
                    ((priorityN2 == priority) &&
                     (nodeN2.GetId() > node.GetId())))
                {
                    isRelay = false;  // failed test of E-CDS Step 4
                }
            }  // end while (NULL != ifaceN2)
        }  // end (while (NULL != ifaceN1)
        if (isRelay)
        {
            // Passed test of E-CDS Step 4
            node.SetRelayStatus(true);
            numberOfRelays++;
            TRACE("node %u is relay by rule of step 4\n", node.GetId());
            relayList.Append(*iface);
            continue;
        }

        // E-CDS Step 7,8 - init path search 'Q'
        ASSERT(NULL != ifaceN1Max);
        MyGraph::Interface::SortedList Q(sortedVerticeItemPool);
        if (!Q.Append(*ifaceN1Max))
        {
            PLOG(PL_ERROR, "graphExample: error adding 'ifaceN1Max' to 'Q'\n");
            return -1;
        }
        ifaceN1Max->GetNode().SetVisited(true);
        // E-CDS Step 8 - perform path search
        TRACE("   e-cds path search starting\n");
        MyGraph::Interface* x;
        while (NULL != (x = Q.RemoveHead()))
        {
            bool xIsInN1 = x->HasEdgeTo(*iface);
            MyGraph::AdjacencyIterator iteratorX(*x);
            MyGraph::Interface* n;
            while (NULL != (n = iteratorX.GetNextAdjacency()))
            {
                if (n == iface) continue;  // was "iface" aka "n0"
                bool nIsInN1 = n->HasEdgeTo(*iface);
                if (!xIsInN1 && !nIsInN1) continue; // link not in "n0" 2-hop neighborhood
                MyNode& nodeNn = n->GetNode();
                if (!nodeNn.WasVisited())
                {
                    // E-CDS Step 8a - mark node as "visited"
                    nodeNn.SetVisited(true);
                    // E-CDS Step 8b - check if RtrPri(n) > RtrPri(n0)
                    UINT8 priorityNn = nodeNn.GetRtrPriority();
                    if ((priorityNn > priority) ||
                            ((priorityNn == priority) &&
                             (nodeNn.GetId() > node.GetId())))
                    {
                        if (!Q.Append(*n))
                        {
                            PLOG(PL_ERROR, "graphExample: error adding 'n' to 'Q'\n");
                            return -1;
                        }
                    }
                }
            }  // end while (NULL != n)
        }  // end while (NULL != x)
        TRACE("   e-cds path search completed\n");
        // E-CDS Step 9
        bool relayStatus = false;
        iteratorN1.Reset();
        while (NULL != (ifaceN1 = iteratorN1.GetNextAdjacency()))
        {
            if (!ifaceN1->GetNode().WasVisited())
            {
                relayStatus = true;
                numberOfRelays++;
                break;
            }
        }
        if (relayStatus)
        {
            TRACE("node %u is relay after path search\n", node.GetId());
            relayList.Append(*iface);
        }
        node.SetRelayStatus(relayStatus);

    }  // while (NULL != node)  (relay set selection)
    return numberOfRelays;
}  // end CalculateFullECDS()

double CalculateDensity(MyGraph& graph)
{
    int neighborCount = 0;
    int nodeCount = 0;
    MyGraph::InterfaceIterator it(graph);
    MyGraph::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyGraph::AdjacencyIterator iteratorN1(*iface);
        nodeCount++;
        while (NULL != (iteratorN1.GetNextAdjacency()))
            neighborCount++;
    }
    return ((double)neighborCount)/((double)nodeCount);
}  // end CalculateDensity()


//////////////////////////////////////////////////////////////////////////////////////////
// "FastReader" implementation
FastReader::FastReader(FILE* filePtr)
        : file_ptr(filePtr), savecount(0)
{
}

FastReader::Result FastReader::Read(char*           buffer,
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
#ifndef WIN32 // no real-time TRPR for WIN32 yet
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
                        perror("trpr: FastReader::Read() select() error");
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
            // This check skips NULLs that have been read on some
            // use of trpr via tail from an NFS mounted file
            if (!isprint(*savebuf) &&
                    ('\t' != *savebuf) &&
                    ('\n' != *savebuf) &&
                    ('\r' != *savebuf))
                continue;
        }
        if (result)
        {
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
            if (*len)
                return OK;  // we read at least something
            else
                return DONE; // we read nothing
        }
    }  // end while(want)
    return OK;
}  // end FastReader::Read()

// An OK text readline() routine (reads what will fit into buffer incl. NULL termination)
// if *len is unchanged on return, it means the line is bigger than the buffer and
// requires multiple reads
FastReader::Result FastReader::Readline(char*         buffer,
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
}  // end FastReader::Readline()

bool FastReader::Seek(int offset)
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
}  // end FastReader::Seek()
