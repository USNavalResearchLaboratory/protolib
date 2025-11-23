
// The purpose of this program is to illustrate use of the "MyManetGraphML" class
// (Note the "NetGraphTemplate" allows different graph subclasses to be
//  created with different metric properties.  The "MyManetGraphML" assumes
//  a non-negative "double" value metric for "link cost".

#include "manetGraphML.h"
#include "protoSpace.h"  // we use this for its bounding box iteration
#include <protoDebug.h>
#include <protoDefs.h>

#include <stdio.h>   // for sprintf()
#include <string.h>
#include <stdlib.h>  // for rand(), srand()
#include <ctype.h>   // for "isprint()"
#include <math.h>    // for "sqrt()"

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>
//#define MY_ENCODING "ISO-8859-1"
#define MY_ENCODING "UTF-8"

class Commands
{
    public:
        enum FileType {NONE, GML};

        Commands() : inputFilePath(NULL),inputType(NONE),outputFilePath(NULL),outputType(NONE),doPathCalc(false),doDensityCalc(false),doECDSCalc(false) {}
        ~Commands() {delete inputFilePath; delete outputFilePath;}
        bool ProcessCommands(int argc,char* argv[]);

        char* inputFilePath;
        FileType inputType;
        char* outputFilePath;
        FileType outputType;
        bool doPathCalc;
        bool doDensityCalc;
        bool doECDSCalc;
        class ECDS
        {
            public:
                ECDS() : usingDensity(false) {}
                bool usingDensity;
        } ecds;
} theCommands;

class LocalAttributeKey : public ProtoSortedTree::Item
{
    public:
        bool Init(const char* theIndex,const char* theName)
        {
            index = new char[sizeof(theIndex)];
            strcpy(index,theIndex);
            name = new char[sizeof(theName)];
            strcpy(name,theName);
            if((NULL == index) || (NULL == name)) 
                return false;
            return true;
        }
        ~LocalAttributeKey()
        {
            delete[] index;
            delete[] name;
        }
        char* index;
        char* name;
    private:
        virtual const char* GetKey() const {return index;}
        virtual unsigned int GetKeysize() const {return strlen(index);}
};
typedef ProtoSortedTreeTemplate<LocalAttributeKey> KeyList;

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
    fprintf(stderr, "Usage: graphExample -ifile graphml <filename> -ofile graphml <filename> <-ecds <id|density>> <-density>\n");
}

const unsigned int MAX_LINE = 256;

class MyCost : public ManetGraph::Cost 
{
    public:
        MyCost() {};
        MyCost(double value) : ManetGraph::Cost(value) {};
};
class MyInterface;
class MyLink;
class MyNode: public NetGraph::NodeTemplate<MyInterface>, public ProtoSpace::Node
{
    public:
        MyNode();
        ~MyNode();

        void SetRtrPriority(int value)
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
        void SetNbrs(int theNbrs)
            {nbrs = theNbrs;}
        int GetNbrs() const
            {return nbrs;}
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
        int     rtr_priority;
        bool    relay_status;
        bool    visited;
        double  ordinate[2];  // x,y coordinates
        int     nbrs;
};  // end class Node
class MyInterface : public NetGraph::InterfaceTemplate<MyCost,MyInterface,MyLink,MyNode> 
{
    public:
        MyInterface(MyNode& theNode, const ProtoAddress& addr)
            : NetGraph::InterfaceTemplate<MyCost,MyInterface,MyLink,MyNode>(theNode,addr) {}
        MyInterface(MyNode& theNode)
            : NetGraph::InterfaceTemplate<MyCost,MyInterface,MyLink,MyNode>(theNode) {}
    private:
};

class MyLink : public NetGraph::LinkTemplate<MyCost,MyInterface> 
{
    public:
    private:
};

MyNode::MyNode()
 : rtr_priority(0)
{
    ordinate[0] = ordinate[1] = 0.0;
}

MyNode::~MyNode()
{
}
class MyManetGraphML : public ManetGraphMLTemplate<MyCost,MyInterface,MyLink,MyNode> 
{
    public :
        int GetId(MyNode &node) const;
        KeyList keys;
    protected :
        bool UpdateKeys(NetGraph& theGraph);
        bool UpdateNodeAttributes(NetGraph::Node& theNode);
        bool UpdateInterfaceAttributes(NetGraph::Interface& theInterface);
        bool UpdateLinkAttributes(NetGraph::Link& theLink);
    private :
        char* FindIndex(const char* theName);
};

int MyManetGraphML::GetId(MyNode &node) const
{
    MyManetGraphML::Interface* iface = node.GetDefaultInterface();
    if (NULL != iface)
    {
        const char* addrPtr = GetString(*iface);
        int id=0;
        for(int i = 0;i<(int)strlen(addrPtr);i++)
        {
            (i % 2) ? id += ((int)addrPtr[i]) << 8 : id += addrPtr[i];
            
        }
        return id;
    }
    else
    {
        ASSERT(0);
        return ((int)-1);
    }
}
bool MyManetGraphML::UpdateKeys(NetGraph& theGraph)
{
    //buuny just use theCommands and add them!
    //AddAttribute(name,boolean|int|double|float|double|string,node|edge|port,NULL,default
    if(theCommands.doECDSCalc)
        AddAttributeKey("ecdsrelay","boolean","node",NULL,"false");
    if(theCommands.doDensityCalc)
        AddAttributeKey("prio","int","node");
    //do link attributes
    AddAttributeKey("weight","double","edge",NULL,"1.0");
    return true;
}
bool MyManetGraphML::UpdateNodeAttributes(NetGraph::Node& theNode)
{
    bool rv = true;
    if(theCommands.doECDSCalc)
    {
        if((static_cast<MyNode&>(theNode)).GetRelayStatus())
        {
            rv &= AddAttribute(theNode,"ecdsrelay","true");
        } else {
            rv &= AddAttribute(theNode,"ecdsrelay","false");
        }
    }
    if(theCommands.doDensityCalc)
    {
        int prio = (static_cast<MyNode&>(theNode)).GetRtrPriority();
        char prioStr[50];
        sprintf(prioStr,"%d",prio);
        rv &= AddAttribute(theNode,"prio",prioStr);
    }
    if(!rv)
    {
        PLOG(PL_ERROR,"MyManetGraphML::UpdateNodeAttributes: Error updating some of the attributes\n");
    }
    return rv;
}
bool MyManetGraphML::UpdateInterfaceAttributes(NetGraph::Interface& iface)
{
    bool rv = true;
    return rv;
}
/*bool MyManetGraphML::WriteNodeAttributes(xmlTextWriter* writerPtr, NetGraph::Node& theNode)
{
    int xmlreturn = 0;

    if(theCommands.doECDSCalc)
    {
        char *index = FindIndex("ecdsrelay");
        xmlreturn += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
        xmlreturn += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST index);
        if((static_cast<MyNode&>(theNode)).GetRelayStatus())
        {
            xmlreturn = xmlTextWriterWriteString(writerPtr, BAD_CAST "true");
        } else {
            xmlreturn = xmlTextWriterWriteString(writerPtr, BAD_CAST "false");
        }
        xmlreturn += xmlTextWriterEndElement(writerPtr);
    }
    if(theCommands.doDensityCalc)
    {
        int prio = (static_cast<MyNode&>(theNode)).GetRtrPriority();
        char prioStr[50];
        sprintf(prioStr,"%d",prio);
        char *index = FindIndex("nbrs");
        xmlreturn += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
        xmlreturn += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST index);
        xmlreturn = xmlTextWriterWriteString(writerPtr, BAD_CAST prioStr);
        xmlreturn += xmlTextWriterEndElement(writerPtr);
    }

    if(xmlreturn < 0) return false;
    return true;
}*/
/*bool MyManetGraphML::WriteInterfaceAttributes(xmlTextWriter* writerPtr, NetGraph::Interface& theInterface)
{
    int xmlreturn = 0;
    xmlreturn = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "nameExamplePort", BAD_CAST "valueExamplePort");
    if(xmlreturn < 0)
        return false;
    return true;
}*/
bool MyManetGraphML::UpdateLinkAttributes(NetGraph::Link& theLink)
{
    bool rv = true;
    char value[100];
    sprintf(value,"%f",(static_cast<MyLink&>(theLink)).GetCost().GetValue());
    rv &= AddAttribute(theLink,"weight",value);
    if(!rv)
    {
        PLOG(PL_ERROR,"MyManetGraphML::UpdateLinkAttributes: Error updating some of the attributes\n");
    }
    return rv;
}
/*bool MyManetGraphML::WriteLinkAttributes(xmlTextWriter* writerPtr, NetGraph::Link& theLink)
{
    int xmlreturn = 0;
    char* index = FindIndex("weight");
    char value[100];
    sprintf(value,"%f",(static_cast<MyLink&>(theLink)).GetCost().GetValue());
        xmlreturn += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
        xmlreturn += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST index);
        xmlreturn = xmlTextWriterWriteString(writerPtr, BAD_CAST value);
        xmlreturn += xmlTextWriterEndElement(writerPtr);
    if(xmlreturn < 0)
        return false;
    return true;
}*/
char *
MyManetGraphML::FindIndex(const char* theName)
{
    KeyList::Iterator kit(keys);
    LocalAttributeKey* keyPtr = kit.GetNextItem();
    char* index = NULL;
    while(keyPtr && (!index))
    {
        if(!strcmp(keyPtr->name,theName))
        {
            //printf("%s\n",keyPtr->index);
            index = keyPtr->index;
        }
        keyPtr=kit.GetNextItem();
    }
    return index;
}
const double RANGE_MAX = 300.0;

inline double CalculateDistance(double x1, double y1, double x2,double y2)
{
    double dx = x1 - x2;
    double dy = y1 - y2;
    return sqrt(dx*dx + dy*dy);
}  // end CalculateDistance()

// This updates the graph connectivity from the "space" and using "RANGE_MAX" comms range
bool UpdateGraphFromSpace(MyManetGraphML&                                   graph,   
                          ProtoSpace&                                   space,   
                          ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool);

// This calculates an ECDS relay set given the "graph"
int CalculateFullECDS(MyManetGraphML&                                   graph, 
                      MyManetGraphML::Interface::SimpleList&            relayList, 
                      ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool);
void CalculateRouterPriority(MyManetGraphML& graph,bool usingDensity);
// This calculates the # of paths from 1 to 10 in the graph
int CalculatePaths(MyManetGraphML& graph);
int CountPaths(MyManetGraphML::Interface* source, MyManetGraphML::Interface* dest,int depth,int* patharray,int maxdepth); //recursive function depth first path search
// This calculates the nodal neighbor "density" of the given "graph"
double CalculateDensity(MyManetGraphML& graph);


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
    if(!theCommands.ProcessCommands(argc-1,argv+1))
    {
        Usage();
        return -1;
    }
    // Create a MyManetGraphML instance that we will populate
    // with nodes/interfaces from our "mobility trace file"
    MyManetGraphML graph;

    if(Commands::GML == theCommands.inputType)
    {
        printf("reading file\"%s\"\n",theCommands.inputFilePath);
        graph.Read(theCommands.inputFilePath);
    }
    // This is a pool of ProtoGraph::Vertice::SortedList::Items
    // that are used for temporary lists of MyManetGraphML::Interfaces
    // as we adjust graph connectivity, etc.  Note that the use of an 
    // "external" item pool is _optional_ for the ProtoGraph/MyManetGraphML
    // list classes, but can boost performance by reducing memory
    // alloc/deallocs when doing a lot of list manipulation.
    // If a list was inited with a "pool", then it is important to
    // keep the "pool" valid until after any associate "lists" are
    // destroyed as "pools" do _not_ keep track of which lists are
    // using them (yet!).
//TRACE("graphXMLExample: before verticeitempool decleration\n");
    ProtoGraph::Vertice::SortedList::ItemPool sortedVerticeItemPool;
//TRACE("graphXMLExample: after verticeitempool decleration\n");
    // We use the ProtoSpace to organize nodes by their position
    // and use the ProtoSpace bounding box iterator to efficiently
    // find adjacencies
    ProtoSpace space;

    if(theCommands.doPathCalc)
    {
        int paths = CalculatePaths(graph);
        TRACE("paths = %d\n",paths);
    }
    MyManetGraphML::Interface::SortedList disconnectedList(&sortedVerticeItemPool);
    MyManetGraphML::InterfaceIterator ifaceIterator(graph);
    MyManetGraphML::Interface::SimpleList relayList;
    
    //int relays = CalculateFullECDS(graph, relayList, &sortedVerticeItemPool);
    //int relays = CalculateFullECDS(graph, relayList, NULL);
    if(theCommands.doECDSCalc)
    {
        //TRACE("graphExample:  Performing E-CDS relay set selection ===================================\n");

        
        // "CalculateFullECDS" implements the ECDS relay set selection
        // algorithm, marking selected nodes using the Node::SetRelayStatus() 
        // method.  It also copies the selected nodes into the "relayList"
        // that is passed to it.  (This is mainly for illustrative purposes
        // to demonstrate the utility of the MyManetGraphML lists, iterators, etc
        //theCommands.ecds.usingDensity ? TRACE("true\n") : TRACE("false\n");
        CalculateRouterPriority(graph,theCommands.ecds.usingDensity);
        MyManetGraphML::Interface::SimpleList relayList;
        //CalculateRouterPriority(graph);
        int relays = CalculateFullECDS(graph, relayList, &sortedVerticeItemPool);
        TRACE("graphXMLExample:: E-CDS relays is %d\n",relays);
        
        // Iterate over our relay set and color the relays "blue" and their
        // (non-relay) one-hop neighbors "green"
        // We also build up a "disconnectedList" by first putting all nodes
        // into it and then remove the relays and their one-hop neighbors.
        // The disconnected nodes remaining in the list are colored "red"
            
        
        // a) Initialize our "disconnectedList" with all ifaces in graph
        //    (note we _could_ have used the "sortedVerticeItemPool" here as
        //     well, but didn't to illustrate that it is optional.  We
        //     would perform better here if did use it, though.
        MyManetGraphML::Interface::SortedList disconnectedList(&sortedVerticeItemPool);
        MyManetGraphML::InterfaceIterator ifaceIterator(graph);
        MyManetGraphML::Interface* iface;
        while (NULL != (iface = ifaceIterator.GetNextInterface()))
        {
            //TRACE("Example:: getting key size of %s\n",graph.GetString(*iface));
            //TRACE("Example:: the iface pointer = %p, the iterator pointer = %p\n",iface,&ifaceIterator);
            //iface->GetKeysize();
            //TRACE("Example:: it crashes here with key size %d\n",iface->GetKeysize());
            disconnectedList.Insert(*iface);
//            TRACE("inserting %s\n",graph.GetString(*iface));
        }
        // b) Remove relays (and their neighbors) from the "disconnectedList"
        //    and output appropriate node colors
        MyManetGraphML::Interface::SimpleList::Iterator relayIterator(relayList);
        MyManetGraphML::Interface* relayIface;
        while (NULL != (relayIface = relayIterator.GetNextInterface()))
        {
            // It's a relay, so remove from disconnected list
            if(relayIface->GetAddress().IsValid())
            {
                if (NULL != disconnectedList.FindInterface(relayIface->GetAddress()))
                    disconnectedList.Remove(*relayIface);
            } else {
                if (NULL != disconnectedList.FindInterfaceByName(relayIface->GetName()))
                    disconnectedList.Remove(*relayIface);
            }
            //MyNode& relayNode = relayIface->GetNode();
            //printf("node %s symbol circle,blue,3\n", graph.GetString(relayNode));
            MyManetGraphML::AdjacencyIterator neighborIterator(*relayIface);
            MyManetGraphML::Interface* neighborIface;
            while (NULL != (neighborIface = neighborIterator.GetNextAdjacency()))
            {
                // It's a relay neighbor, so remove from disconnected list
                if(neighborIface->GetAddress().IsValid())
                {
                    //printf("  bunny found %s",neighborIface->GetAddress().GetHostString());
                    //printf("aka %s\n",neighborIface->GetName());
                    if (NULL != disconnectedList.FindInterface(neighborIface->GetAddress()))
                        disconnectedList.Remove(*neighborIface);
                } else {
                    //printf("  bunny looking for%s\n",neighborIface->GetName());
                    if (NULL != disconnectedList.FindInterfaceByName(neighborIface->GetName()))
                        disconnectedList.Remove(*neighborIface);
                }
                MyNode& neighborNode = neighborIface->GetNode();
                if (!neighborNode.GetRelayStatus())
                    ;//printf("node %s symbol circle,green,3\n", graph.GetString(neighborNode));
            }  
        }
        
        //TRACE("bunny 2 relays is %d\n",relays);
        // c) Color any nodes remaining in "disconnectedList" red
        MyManetGraphML::Interface::SortedList::Iterator dcIterator(disconnectedList);
        MyManetGraphML::Interface* disconnectedIface;
        while (NULL != (disconnectedIface = dcIterator.GetNextInterface()))
        {
            MyNode& disconnectedNode = disconnectedIface->GetNode();
            printf("node %s symbol circle,red,3\n", graph.GetString(disconnectedNode));
        }   
//        TRACE("bunny 3 relays is %d\n",relays);
    }
    if(theCommands.doDensityCalc)
    {
        double nodedensity = 0;
        //TRACE("doing density calc...\n");
        nodedensity = CalculateDensity(graph);
        TRACE("density is %f\n",nodedensity);
        //fprintf(ofile,"%f,%d\n",nodedensity,paths);
    }
    if(Commands::GML == theCommands.outputType)
    {
        graph.Write(theCommands.outputFilePath);
    }
    fprintf(stderr, "GraphXMLExample: Done.\n");
    return 0;
}  // end main()

bool
Commands::ProcessCommands(int argc,char* argv[])
{
    for(int i=0;NULL !=argv[i];i++)
    {
        if(!strcmp(argv[i],"-ifile"))
        {
            i++;
            if(!strcmp(argv[i],"graphml"))
            {
                i++;
                inputType = GML;
                if(NULL == argv[i])
                {
                    PLOG(PL_ERROR,"Commands::ProcessCommands Error no input file path\n");
                    return false;
                }
                inputFilePath = new char[strlen(argv[i])];
                if(NULL == inputFilePath)
                {
                    PLOG(PL_ERROR,"Commands::ProcessCommands Error allocating space for input file path\n");
                    return false;
                }
                memset(inputFilePath,0,strlen(argv[i]));
                strcpy(inputFilePath,argv[i]);
            } else {
                PLOG(PL_ERROR,"Commands::ProcessCommands Error unknown input file format \"%s\"\n",argv[i]);
                return false;
            }
        } else if (!strcmp(argv[i],"-ofile")) {
            i++;
            if(!strcmp(argv[i],"graphml"))
            {
                i++;
                outputType = GML;
                if(NULL == argv[i])
                {
                    PLOG(PL_ERROR,"Commands::ProcessCommands Error no output file path\n");
                    return false;
                }
                outputFilePath = new char[strlen(argv[i])];
                if(NULL == outputFilePath)
                {
                    PLOG(PL_ERROR,"Commands::ProcessCommands Error allocating space for output file path\n");
                    return false;
                }
                memset(outputFilePath,0,strlen(argv[i]));
                strcpy(outputFilePath,argv[i]);
            } else {
                PLOG(PL_ERROR,"Commands::ProcessCommands Error unknown output file format \"%s\"\n",argv[i]);
                return false;
            }
        } else if (!strcmp(argv[i],"-ecds")) {
            doECDSCalc = true;
            i++;
            if(NULL != argv[i]){
                if(!strcmp(argv[i],"id")) {
                    ecds.usingDensity = false;                
                } else if (!strcmp(argv[i],"density")) {
                    ecds.usingDensity = true;
                } else {
                    i--;
                }
            } else {
                i--;
            }
        } else if (!strcmp(argv[i],"-density")) {
            doDensityCalc = true;
        } else {
            PLOG(PL_ERROR,"Commands::ProcessCommands Error Unknown command \"%s\"\n",argv[i]);
            return false;
        }
    }
    return true;
}
bool UpdateGraphFromSpace(MyManetGraphML& graph, ProtoSpace& space, ProtoGraph::Vertice::SortedList::ItemPool* sortedVerticeItemPool)
{
    MyManetGraphML::InterfaceIterator it(graph);
    MyManetGraphML::Interface* iface;
    ;
    unsigned int count = 0;
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();

        //const ProtoAddress& nodeAddr = iface->GetAddress();

        // Cache list of node's current neighbors into
        // a list we can retrieve from as we determine
        // our "within range" nodes.
        MyManetGraphML::Interface::SortedList nbrList(sortedVerticeItemPool);
        MyManetGraphML::AdjacencyIterator nbrIterator(*iface);
        MyManetGraphML::Interface* nbrIface;
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
                //TRACE("count:%d - %s found self\n", count, nodeAddr.GetHostString());
                continue;
            }

            if (distance <= RANGE_MAX)
            {
                // Is this "nbr" already connected as a neighbor in our graph?
                MyManetGraphML::Interface* defaultIface = nbr->GetDefaultInterface();
                ASSERT(NULL != defaultIface);
                const ProtoAddress& nbrAddr = defaultIface->GetAddress();
                nbrIface = nbrList.FindInterface(nbrAddr);
                if (NULL == nbrIface)
                {
                    //TRACE("count:%d - connecting %s to ", count, nodeAddr.GetHostString());
                    //TRACE("%s\n", nbrAddr.GetHostString());
                    MyManetGraphML::SimpleCostDouble cost(1.0);
                    if (!graph.Connect(*iface, *defaultIface, cost, true))
                    {
                        PLOG(PL_ERROR, "graphExample error: unable to connect interfaces in graph\n");
                        return false;
                    }
                    printf("link %s,%s,green\n", graph.GetString(node), graph.GetString(*nbr));
                }
                else
                {
                    //TRACE("count:%d - %s already neighbor of ", count, nodeAddr.GetHostString());
                    //TRACE("%s\n", nbrAddr.GetHostString());
                    nbrList.Remove(*nbrIface);
                }
            }
            else
            {
                // Disconnect any former neighbors that were no longer in range
                if (!nbrList.IsEmpty())
                {
                    MyManetGraphML::Interface::SortedList::Iterator listIterator(nbrList);
                    while (NULL != (nbrIface = listIterator.GetNextInterface()))
                    {
                        graph.Disconnect(*iface, *nbrIface, true);
                        //TRACE("count:%d - %s no longer neighbor of ", count, nodeAddr.GetHostString());
                        //TRACE("%s\n", nbrIface->GetAddress().GetHostString());
                        //printf("unlink %u,%u\n", node.GetId(), static_cast<Node&>(nbrIface->GetNode()).GetId());
                    }
                }
                break;
            }
        }  // end while (NULL != nbr)
    }  // end while (NULL != node)  (graph update)
    return true;
}  // end UpdateGraphFromSpace()

int CalculatePaths(MyManetGraphML& graph)
{
    TRACE("in calculate paths\n");
    MyManetGraphML::InterfaceIterator it(graph);
    MyManetGraphML::Interface* isource;
    MyManetGraphML::Interface* idest;
    isource = it.GetNextInterface();
    idest = it.GetNextInterface();
    
    MyManetGraphML::Node node;
    it.Reset();
    MyManetGraphML::Interface* interface;
    while (NULL != (interface = it.GetNextInterface()))
    {
        MyNode& node = interface->GetNode();
        node.SetVisited(false);
        //TRACE(" visiting %s\n",graph.GetString(node));
    }
     
    MyNode& node1 = isource->GetNode();
    //int nodeid1 = node1.GetId();
    MyNode& node2 = idest->GetNode();
    //int nodeid2 = node2.GetId();
    TRACE("calculating paths from %s to",graph.GetString(node1));
    TRACE(" %s\n",graph.GetString(node2));
    int maxdepth = 20;
    int* patharray = (int*) malloc (maxdepth*4);
    memset(patharray,0,maxdepth*4);
    int paths = CountPaths(isource,idest,0,patharray,maxdepth);
    for(int i=0;i<maxdepth;i++)
    {
        TRACE("#paths at depth %d:    %d\n",i,patharray[i]);
    }
    return paths;
}
int CountPaths(MyManetGraphML::Interface* isource,MyManetGraphML::Interface* idest,int depth,int* patharray,int maxdepth)
{
    char spaces[255];
    memset(spaces,' ',255);
    memset(spaces+depth,0,1);
    if(depth> maxdepth) exit(0);
    //Node& node1 = static_cast<Node&>(isource->GetNode());
    //int nodeid1 = node1.GetId();
    //Node& node2 = static_cast<Node&>(idest->GetNode());
    //int nodeid2 = node2.GetId();
    //TRACE("%svisiting %d\n",spaces,nodeid1);
    
    if(isource==idest)//we are at the end!
    {
        //TRACE("%sat the end!\n");
        patharray[depth]+=1;
        return 1;
    }
    int paths = 0;
    MyNode& node = isource->GetNode();
    node.SetVisited(true);
    //iterate over adjancys and recursivly call this funciton on neighbor which haven't yet been visited
    MyManetGraphML::AdjacencyIterator it(*isource);
    MyManetGraphML::Interface* inbr;
    while(NULL != (inbr = it.GetNextAdjacency()))
    {
        MyNode& nbrnode = inbr->GetNode();
        //TRACE("%s%p is nbrnode\n",spaces,&nbrnode);
        if(!nbrnode.WasVisited())
        {
            //TRACE("%sgoing to visit %d from %d\n",spaces,nbrnode.GetId(),nodeid1);
            int ccount = CountPaths(inbr,idest,depth+1,patharray,maxdepth);
            paths+= ccount;
            //TRACE("%s%d is paths\n",spaces,paths);
        }
        else
        {
            //TRACE("%s%d was visited\n",spaces,nbrnode.GetId());
        }
    }
    node.SetVisited(false);
    //TRACE("CountPaths returning\n");
    return paths;
}

double CalculateDensity(MyManetGraphML& graph)
{
    int neighborCount = 0;
    int nodeCount = 0;
    MyManetGraphML::InterfaceIterator it(graph);
    MyManetGraphML::Interface* iface;
    it.Reset();
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();
        node.SetNbrs(0.0);
        node.SetVisited(false);
    }
    it.Reset();
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();
        int test = node.GetNbrs()+iface->GetAdjacencyCount();
        node.SetNbrs(test);
        //node.SetNbrs((node.GetNbrs())+(iface->GetAdjacencyCount()));
        neighborCount+=iface->GetAdjacencyCount();
        if(!node.WasVisited())
        {
            node.SetVisited(true);
            nodeCount++;
        }
        //TRACE("CalculateRouterPriority: setting adj to %d\n",iface->GetAdjacencyCount());
    }
    return ((double)neighborCount)/((double)nodeCount);
}  // end CalculateDensity()

void CalculateRouterPriority(MyManetGraphML& graph,bool usingDensity)
{
    if(usingDensity)
       CalculateDensity(graph);
    MyManetGraphML::InterfaceIterator it(graph);
    MyManetGraphML::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();
        if(usingDensity)
        {
            node.SetRtrPriority(node.GetNbrs());
        }
        else
        {
            node.SetRtrPriority(0.0);
        }
        //TRACE("CalculateRouterPriority: setting adj to %d\n",iface->GetAdjacencyCount());
    }
}
// "relayList" is filled with the selected relays
int CalculateFullECDS(MyManetGraphML&                                   graph, 
                      MyManetGraphML::Interface::SimpleList&            relayList, 
                      ProtoGraph::Vertice::SortedList::ItemPool*    sortedVerticeItemPool)
{
    // Now that we have a fully updated "graph", perform Relay Set Selection
    // algorithm for each node in graph
    int numberOfRelays=0;
    MyManetGraphML::InterfaceIterator it(graph);
    MyManetGraphML::Interface* iface;
    while (NULL != (iface = it.GetNextInterface()))
    {
        MyNode& node = iface->GetNode();
        UINT8 priority = node.GetRtrPriority();

        // E-CDS Steps 1,2
        //TRACE("   node:%s: node:%u adjacencyCount:%u\n", graph.GetString(node),graph.GetId(node), iface->GetAdjacencyCount());
        if (iface->GetAdjacencyCount() < 2)
        {
            node.SetRelayStatus(false);
            continue;
        }

        // E-CDS Step 3,4,5, & 6
        // "nbrList" will cache the "node" 1-hop neighbors for "visited" elimination with the path search
        MyManetGraphML::AdjacencyIterator iteratorN1(*iface);
        MyManetGraphML::Interface* ifaceN1Max = NULL;
        bool isRelay = true;  // this will be set to "false" if a test fails
        MyManetGraphML::Interface* ifaceN1;
        while (NULL != (ifaceN1 = iteratorN1.GetNextAdjacency()))
        {
            //TRACE("    visiting adj%s: adjacencyCount %u\n",graph.GetString(*ifaceN1),ifaceN1->GetAdjacencyCount());
            MyNode& nodeN1 = ifaceN1->GetNode();
//this is the place where it breaks
            nodeN1.SetVisited(false);  // init for later path search
            // Save n1_max needed for E-CDS Step 6
            UINT8 priorityN1 = nodeN1.GetRtrPriority();
            if (NULL == ifaceN1Max)
            {
                ifaceN1Max = ifaceN1;
  //              TRACE("    %s is being set as the max with prio and id %u/%u\n",graph.GetString(*ifaceN1),priorityN1,graph.GetId(nodeN1));
            }
            else
            {
                MyNode& nodeN1Max = ifaceN1Max->GetNode();
                UINT8 priorityN1Max = nodeN1Max.GetRtrPriority();
//TRACE("    priorityN1:%u/%u,priorityN1Max:%u/%u\n",priorityN1,graph.GetId(nodeN1),priorityN1Max,graph.GetId(nodeN1Max));
                if ((priorityN1 > priorityN1Max) ||
                    ((priorityN1 == priorityN1Max) &&
                     (graph.GetId(nodeN1)>=graph.GetId(nodeN1Max))))
                {
//                    TRACE("    %s is being set as the max with %u prio and %u id\n",graph.GetString(*ifaceN1),priorityN1,graph.GetId(nodeN1));
                    ifaceN1Max = ifaceN1;
                }
            }
            if(true == isRelay)
            {
                if ((priorityN1 > priority) ||
                    ((priorityN1 == priority) && (graph.GetId(nodeN1)>graph.GetId(node))))
                {
                    isRelay = false;  // failed test of E-CDS Step 4
   //                 TRACE("    %s is not the LARGEST\n",graph.GetString(node));
                }
            }
            // Check 2-hop neighbors (other neighbors of N1)
            MyManetGraphML::AdjacencyIterator iteratorN2(*ifaceN1);
            //TRACE("graphExmaple:CaluculateECDS iteratorN2 pointer is %p\n",&iteratorN2);
            MyManetGraphML::Interface* ifaceN2;
            while (NULL != (ifaceN2 = iteratorN2.GetNextAdjacency()))
            {
                //TRACE("     visiting 2adj%s\n",graph.GetString(*ifaceN2));
                if (ifaceN2 == iface) continue;  // was "iface" itself
                if (iface->HasEdgeTo(*ifaceN2)) continue;  // was 1-hop neighbor of "iface"
                MyNode& nodeN2 = ifaceN2->GetNode();
                nodeN2.SetVisited(false);  // init for later path search
                UINT8 priorityN2 = nodeN2.GetRtrPriority();
                if ((priorityN2 > priority) ||
                    ((priorityN2 == priority) &&
                     (graph.GetId(nodeN2)>graph.GetId(node))))
                {
                    isRelay = false;  // failed test of E-CDS Step 4
                }
            }  // end while (NULL != ifaceN2)
            //TRACE("     killing the iterator %s %p is the node\n",graph.GetString(node),&node);
        }  // end (while (NULL != ifaceN1)
        if (isRelay)
        {
            // Passed test of E-CDS Step 4
            node.SetRelayStatus(true);
            numberOfRelays++;
            //TRACE("node %s is relay by rule of step 4 %p\n", graph.GetString(node),&node);
            relayList.Append(*iface);
            continue;
        }

        // E-CDS Step 7,8 - init path search 'Q'
        ASSERT(NULL != ifaceN1Max);
        MyManetGraphML::Interface::SortedList Q(sortedVerticeItemPool);
        if (!Q.Append(*ifaceN1Max))
        {
            PLOG(PL_ERROR, "graphExample: error adding 'ifaceN1Max' to 'Q'\n");
            return -1;
        } else {
            //TRACE("   added %s to the Q\n",graph.GetString(*ifaceN1Max));
        }
        ifaceN1Max->GetNode().SetVisited(true);
        // E-CDS Step 8 - perform path search
        //TRACE("   e-cds path search starting\n");
        MyManetGraphML::Interface* x;
        while (NULL != (x = Q.RemoveHead()))
        {
            //TRACE("   popping %s from the Q\n",graph.GetString(*x));   
            bool xIsInN1 = x->HasEdgeTo(*iface);
            MyManetGraphML::AdjacencyIterator iteratorX(*x);
            MyManetGraphML::Interface* n;
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
                             (graph.GetId(nodeNn)>graph.GetId(node))))
                    {
                        if (!Q.Append(*n))
                        {
                            PLOG(PL_ERROR, "graphExample: error adding 'n' to 'Q'\n");
                            return -1;
                        } else {
                            //TRACE("   adding %s to the Q\n",graph.GetString(*n));
                        }
                    }
                }
            }  // end while (NULL != n)
        }  // end while (NULL != x)
        //TRACE("   e-cds path search completed\n");
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
            //TRACE("node %u is relay after path search\n", graph.GetString(node));
            relayList.Append(*iface);
        }
        node.SetRelayStatus(relayStatus);

    }  // while (NULL != node)  (relay set selection)
    return numberOfRelays;
}  // end CalculateFullECDS()



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


