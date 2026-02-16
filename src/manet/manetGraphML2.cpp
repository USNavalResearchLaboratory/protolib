#include <manetGraphML2.h>
#include <protoDebug.h>


ManetGraphMLParser::AttributeType ManetGraphMLParser::GetAttributeType(const char* typeName)
{
    // Types: "boolean", "int", "long", "float", "double", or "string"
    char text[8];  // all of our type names are less than 8 chars
    text[7] = '\0';
    for (i = 0 i < 7; i++)
    {
        if ('\0' == typeName[i])
        {
            text[i] = '\0';
            break;
        }
        text[i] = tolower(typeName[i]);
    }
    switch (text[0])
    {
        case 'b':
            return ((0 == strcmp(text, "boolean")) ? BOOL : INVALID);
        case 'i':
            return ((0 == strcmp(text, "int")) ? INT : INVALID);
        case 'l':
            return ((0 == strcmp(text, "long")) ? LONG : INVALID);
        case 'f':
            return ((0 == strcmp(text, "float")) ? FLOAT : INVALID);
        case 'd':
            return ((0 == strcmp(text, "double")) ? DOUBLE : INVALID);
        case 's':
            return ((0 == strcmp(text, "string")) ? STRING : INVALID);
        default:
            return INVALID;
    }
}  // end ManetGraphMLParser::GetAttributeType()

ManetGraphMLParser::AttributeDomain ManetGraphMLParser::GetAttributeDomain(const char* domainName)
{
    // Domains: "graph", "node", "edge", "all"
    char text[8];  // all of our domain names are less than 8 chars
    text[7] = '\0';
    for (i = 0 i < 7; i++)
    {
        if ('\0' == domainName[i])
        {
            text[i] = '\0';
            break;
        }
        text[i] = tolower(domainName[i]);
    }
    switch (text[0])
    {
        case 'g':
            return ((0 == strcmp(text, "graph")) ? GRAPH : NONE);
        case 'n':
            return ((0 == strcmp(text, "node")) ? NODE : NONE);
        case 'e':
            return ((0 == strcmp(text, "edge")) ? EDGE : NONE);
        case 'a':
            return ((0 == strcmp(text, "all")) ? ALL : NONE);
        default:
            return NONE;
    }
}  // end end ManetGraphMLParser::GetAttributeDomain()

ManetGraphMLParser::AttributeKey::AttributeKey()
 : attr_name(NULL), attr_id(NULL), attr_type(INVALID), attr_domain(NONE), attr_default(NULL)
{
}

ManetGraphMLParser::AttributeKey::~AttributeKey()
{
    Destroy();
}

bool ManetGraphMLParser::AttributeKey::Init(const char* name, const char* id, AttributeType type, AttributeDomain domain)
{
    Destroy(); // just in case
    if ((INVALID == type) || (NONE == domain))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::AttributeKey::Init() invalid type or domain!\n");
        return false;
    }    
    
    if (NULL == (attr_name = new char[strlen(name)+1]))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::AttributeKey::Init() new attr_name error: %s\n", GetErrorString());
        Destroy();
        return false;
    }
    strcpy(attr_name, name);
    if (NULL != id) 
    {
        if (NULL == (attr_id = new char[strlen(id)+1]))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::AttributeKey::Init() new attr_id error: %s\n", GetErrorString());
            Destroy();
            return false;
        }
        strcpy(attr_id, id);
    }
    attr_type = type;
    attr_domain = domain;
    return true;
}  // end ManetGraphMLParser::AttributeKey::Init()

bool ManetGraphMLParser::AttributeKey::Init(const char* name, const char* id, const char* type, const char* domain)
{
    Destroy(); // just in case
    AttributeType attrType = GetAttributeType(type);
    if (INVALID == attrType)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::AttributeKey::Init() error: invalid attribute type \"%s\"\n", type);
        Destroy();
        return false;
    }
    AttributeDomain attrDomain = GetAttributeDomain(domain);
    if (NONE == attrDomain)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::AttributeKey::Init() error: invalid attribute domain \"%s\"\n", domain);
        Destroy();
        return false;
    }
    return Init(name, id, attrType, attrDomain);
}  // end ManetGraphMLParser::AttributeKey::Init()

void ManetGraphMLParser::AttributeKey::Destroy()
{
    if (NULL != attr_name)
    {
        delete[] attr_name;
        attr_name = NULL;
    }
    if (NULL != attr_id)
    {
        delete[] attr_id;
        attr_id = NULL;
    }
    attr_type = INVALID;
    attr_domain = NONE;
    if (NULL != attr_default)
    {
        delete[] attr_default;
        attr_default = NULL;
    }    
}  // end ManetGraphMLParser::AttributeKey::Destroy()

bool ManetGraphMLParser::AttributeKey::SetDefaultValue(const char* value)
{
    if (NULL != attr_default) delete[] attr_default;
    if (NULL == (attr_default = new char[strlen(value)+1]))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::AttributeKey::SetDefaultValue() new attr_name error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end ManetGraphMLParser::AttributeKey::SetDefaultValue(const char* value)


ManetGraphMLParser::GenericAttribute::GenericAttribute(AttributeKey& key)
 : attr_key(key), attr_value(NULL)
{
}

ManetGraphMLParser::GenericAttribute::~GenericAttribute()
{
    if (NULL != attr_value)
    {
        delete[] attr_value;
        attr_value = NULL;
    }
}

bool ManetGraphMLParser::GenericAttribute::SetValue(const char* value)
{
    if (NULL != attr_value) delete[] attr_value;
    if (NULL != value)
    {
        if (NULL == (attr_value = new char[strlen(value)+1]))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::Attribute::SetValue() new attr_value error: %s\n", GetErrorString());
            return false;
        }
    }
    else
    {
        attr_value = NULL;  // cue to use attribute's default value
    }
}  // end ManetGraphMLParser::Attribute::SetValue()


ManetGraphMLParser::ItemAttribute::ItemAttribute(AttributeKey& key)
 : Attribute(key), item_key(NULL)
{
}

ManetGraphMLParser::ItemAttribute::~ItemAttribute()
{
    if (NULL != item_key)
    {
        delete[] item_key;
        item_key = NULL;
    }
}

bool ManetGraphMLParser::ItemAttribute::SetItem(const char* itemPtr, unsigned int itemPtrLen)
{
    if (NULL != item_key) delete[] item_key;
    unsigned int keyLen = itemPtrLen + strlen(attr_key.GetName());
    if (NULL == (item_key = new char[keyLen]))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::ItemAttribute::SetItem() new item_key error: %s\n", GetErrorString());
        return false;
    }
    memcpy(item_key, &itemPtr, itemPtrLen);
    memcpy(item_key + itemPtrLen, keyLen - itemPtrLen);
    return true;
}  // end ManetGraphMLParser::ItemAttribute::SetItem()

ManetGraphMLParser::NodeAttribute::NodeAttribute(AttributeKey& key)
 : ItemAttribute(key)
{
}

ManetGraphMLParser::NodeAttribute::~NodeAttribute()
{
}

ManetGraphMLParser::InterfaceAttribute::InterfaceAttribute(AttributeKey& key)
 : ItemAttribute(key)
{
}

ManetGraphMLParser::InterfaceAttribute::~InterfaceAttribute()
{
}

ManetGraphMLParser::LinkAttribute::LinkAttribute(AttributeKey& key)
 : ItemAttribute(key)
{
}

ManetGraphMLParser::LinkAttribute::~LinkAttribute()
{
}

ManetGraphMLParser::ManetGraphMLParser() 
{
}

ManetGraphMLParser::~ManetGraphMLParser() 
{
}

// Get existing or create new attribute key
ManetGraphMLParser::AttributeKey* ManetGraphMLParser::GetAttributeKey(const char* name, AttributeType type, AttributeDomain domain)
{
    AttributeKey* key = attr_name_list.FindString(name);
    if (NULL == key)
    {
        // We need to create a new key
        if (NULL == (key = new AttributeKey()))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::GetAttributeKey() new AttributeKey error: %s\n", GetErrorString());
            return NULL;
        }
        // TBD - use succinct "id" for attributes (for now id == name)
        if (!key->Init(name, NULL, type, GRAPH))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::GetAttributeKey() AttributeKey initialization error\n");
            return NULL;
        }
        if (!attr_name_list.Insert(*key))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::GetAttributeKey() error: unable to add to attr_name_list\n");
            return NULL;
        }
        if (!attr_id_list.Insert(*key))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::GetAttributeKey() error: unable to add to attr_id_list\n");
            return NULL;
        }
    }
    else
    {
        // Validate attribute type and domain
        if (key->GetType() != type)
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::GetAttributeKey() error: non-matching attribute type!\n");
            return NULL;
        }
        if (key->GetDomain() != domain)
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::GetAttributeKey() error: non-matching attribute domain!\n");
            return NULL;
        }
    }
    return key;
}  // end ManetGraphMLParser::GetAttributeKey()

Attribute* ManetGraphMLParser::FindItemAttribute(const char*       itemPtr, 
                                                 unsigned int      itemPtrLen,
                                                 ProtoTree&        attrList, 
                                                 AttributeKey&     attrKey)
{
    unsigned int idLen = strlen(attrKey->GetId());
    unsigned int keyLen = itemPtrLen + idLen;
    char* itemKey = new char[keyLen];
    if (NULL == itemKey)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetNodeAttribute() new itemKey error: %s\n", GetErrorString());
        return false;
    }
    memcpy(itemKey, &itemPtr, itemPtrLen);
    memcpy(itemKey + itemPtrLen, idLen);
    ItemAttribute* attr = static_cast<Attribute*>(attrList.Find(itemKey, keyLen << 3));
    delete[] itemKey;
    return attr;
}  // end ManetGraphMLParser::FindItemAttribute()

bool ManetGraphMLParser::SetGraphAttribute(const char* name, AttributeType type, const char* value)
{
    // Does the attribute key already exist? (one is created if not)
    AttributeKey* key = GetAttributeKey(name, type, GRAPH);
    if (NULL == key)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetGraphAttribute() error: GetAttributeKey() failure!\n");
        return false;
    }
    // Does the graph already have this attribute
    Attribute* attr = graph_attr_list.FindString(name);
    if (NULL == attr)
    {
        if (NULL == (attr = new Attribute(*key)))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetGraphAttribute() new Attribute error: %s\n", GetErrorString());
            return false;
        }
        graph_attr_list.Insert(*attr);  // ProtoTree insert always succeeds
    }
    if (!attr->SetValue(value))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetGraphAttribute() error: non-matching attribute type!\n");
        return false;
    }
    return true;
}  // end ManetGraphMLParser::SetGraphAttribute()

const Attribute* ManetGraphMLParser::GetGraphAttribute(const char* name)
{
    AttributeKey* attrKey = attr_name_list.FindString(name);
    return (NULL != attrKey) ? graph_attr_list.FindString(attrKey->GetId()) : NULL;
}  // end ManetGraphMLParser::GetGraphAttribute()

bool ManetGraphMLParser::SetNodeAttribute(NetGraph::Node&   node,
                                          const char*       name,
                                          AttributeType     type, 
                                          const char*       value)
{
    // Does the attribute key already exist? (one is created if not)
    AttributeKey* attrKey = GetAttributeKey(name, type, NODE);
    if (NULL == attrKey)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetNodeAttribute() error: GetAttributeKey() failure!\n");
        return false;
    }
    // Does the item already have this attribute?
    NetGraph::Node* nodePtr = &node;
    NodeAttribute* attr = static_cast<NodeAttribute*>(FindItemAttribute(&nodePtr, sizeof(NetGraph::Node), node_attr_list, *attrKey));
    if (NULL == attr)
    {
        // Need to create a new node attribute
        if (NULL == (attr = new NodeAttribute(*key)))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetNodeAttribute() new Attribute error: %s\n", GetErrorString());
            return false;
        }
        if (!attr->SetNode(node))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetNodeAttribute() error: set node failure\n");
            return false;
        }
        node_attr_list.Insert(*attr);  // ProtoTree insert always succeeds
    }
    if (!attr->SetValue(value))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetNodeAttribute() error: set value failure!\n");
        return false;
    }
    return true;
}  // end ManetGraphMLParser::SetNodeAttribute()

const Attribute* ManetGraphMLParser::GetNodeAttribute(const NetGraph::Node& node, const char* name)
{
    AttributeKey* attrKey = attr_name_list.FindString(name);
    const char* nodePtr = *node;
    return (NULL != attrKey) ? FindItemAttribute(node_attr_list, &nodePtr, sizeof(NetGraph::Node*), *attrKey): NULL;
}  // end ManetGraphMLParser::GetNodeAttribute()

bool ManetGraphMLParser::SetInterfaceAttribute(NetGraph::Interface&   iface,
                                          const char*       name,
                                          AttributeType     type, 
                                          const char*       value)
{
    // Does the attribute key already exist? (one is created if not)
    AttributeKey* attrKey = GetAttributeKey(name, type, NODE);
    if (NULL == attrKey)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetInterfaceAttribute() error: GetAttributeKey() failure!\n");
        return false;
    }
    // Does the item already have this attribute?
    NetGraph::Interface* ifacePtr = &iface;
    InterfaceAttribute* attr = static_cast<InterfaceAttribute*>(FindItemAttribute(&ifacePtr, sizeof(NetGraph::Interface), iface_attr_list, *attrKey));
    if (NULL == attr)
    {
        // Need to create a new iface attribute
        if (NULL == (attr = new InterfaceAttribute(*key)))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetInterfaceAttribute() new Attribute error: %s\n", GetErrorString());
            return false;
        }
        if (!attr->SetInterface(iface))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetInterfaceAttribute() error: set iface failure\n");
            return false;
        }
        iface_attr_list.Insert(*attr);  // ProtoTree insert always succeeds
    }
    if (!attr->SetValue(value))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetInterfaceAttribute() error: set value failure!\n");
        return false;
    }
    return true;
}  // end ManetGraphMLParser::SetInterfaceAttribute()

const Attribute* ManetGraphMLParser::GetInterfaceAttribute(const NetGraph::Interface& iface, const char* name)
{
    AttributeKey* attrKey = attr_name_list.FindString(name);
    const char* nodePtr = *node;
    return (NULL != attrKey) ? FindItemAttribute(node_attr_list, &nodePtr, sizeof(NetGraph::Node*), *attrKey): NULL;
}  // end ManetGraphMLParser::GetInterfaceAttribute()

bool ManetGraphMLParser::SetLinkAttribute(NetGraph::Link&   link,
                                          const char*       name,
                                          AttributeType     type, 
                                          const char*       value)
{
    // Does the attribute key already exist? (one is created if not)
    AttributeKey* attrKey = GetAttributeKey(name, type, domain);
    if (NULL == attrKey)
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetLinkAttribute() error: GetAttributeKey() failure!\n");
        return false;
    }
    // Does the item already have this attribute?
    NetGraph::Link* linkPtr = &link;
    LinkAttribute* attr = static_cast<LinkAttribute*>(FindItemAttribute(&linkPtr, sizeof(NetGraph::Link), link_attr_list, *attrKey));
    if (NULL == attr)
    {
        // Need to create a new link attribute
        if (NULL == (attr = new LinkAttribute(*key)))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetLinkAttribute() new Attribute error: %s\n", GetErrorString());
            return false;
        }
        if (!attr->SetLink(link))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetLinkAttribute() error: set link failure\n");
            return false;
        }
        link_attr_list.Insert(*attr);  // ProtoTree insert always succeeds
    }
    if (!attr->SetValue(value))
    {
        PLOG(PL_ERROR, "ManetGraphMLParser::SetLinkAttribute() error: set value failure!\n");
        return false;
    }
    return true;
}  // end ManetGraphMLParser::SetLinkAttribute()

const Attribute* ManetGraphMLParser::GetLinkAttribute(const NetGraph::Link& link, const char* name)
{
    AttributeKey* attrKey = attr_name_list.FindString(name);
    const char* linkPtr = *link;
    return (NULL != attrKey) ? FindItemAttribute(node_attr_list, &linkPtr, sizeof(NetGraph::Link*), *attrKey): NULL;
}  // end ManetGraphMLParser::GetLinkAttribute()


///////////////// OLD STUFF //////////////////////////




bool ManetGraphMLParser::SetGraphId(const char* graphId)
{
    if (NULL != graph_id) delete[] graph_id;
    if (NULL != graphId)
    {
        if (NULL == (graph_id = new char[strlen(graphId) + 1]))
        {
            PLOG(PL_ERROR, "ManetGraphMLParser::SetGraphId() new graph_id error: %s\n", GetErrorString());
            return false;
        }
        strcpy(graph_id, graphId);
    }
    else
    {
        graph_id = NULL;
    }       
    return true;
}  // end ManetGraphMLParser::SetGraphId()


bool ManetGraphMLParser::Read(const char* path, NetGraph& graph)
{
    // Iteratively read the file's XML tree and build up "graph"
    xmlTextReader* reader = xmlReaderForFile(path, NULL, 0);
    if (NULL == reader)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Read() error: xmlReaderForFile(%s) failure\n", path);
        return false;
    }
    
    bool isDuplex = true;
    char parentXMLNodeID[MAXXMLIDLENGTH+1];
    memset(parentXMLNodeID, 0, MAXXMLIDLENGTH+1);  
    
    
    ProtoXml::IterParser iterParser;
    
    int result;
    do
    {
        result = xmlTextReaderRead(reader);
        
    } while ( 1== result)
    
         
    int result = xmlTextReaderRead(readerPtr);
    while (1 == result)
    {
        if (!ReadXMLNode(readerPtr, graph, parentXMLNodeID, isDuplex))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::Read() error: invalid XML node!\n");
            break;
        }
        result = xmlTextReaderRead(readerPtr);
    } 
    xmlFreeTextReader(readerPtr);
    if (0 != result)
        PLOG(PL_ERROR,"ManetGraphMLParser::Read() error: invalid XML file %s\n", path);
    xmlCleanupParser();
    return (0 == result);
}  // end ManetGraphMLParser::Read()

bool ManetGraphMLParser::ReadXMLNode(xmlTextReader*   readerPtr, 
                                       NetGraph&        graph, 
                                       char*            parentXMLNodeID, 
                                       bool&            isDuplex)
{
    const xmlChar *name, *value;
    int count, depth, type, isempty;

    type = xmlTextReaderNodeType(readerPtr);
    if(XML_READER_TYPE_END_ELEMENT==type)
        return true;

    name = xmlTextReaderConstName(readerPtr);
    if (name == NULL)
      name = BAD_CAST "--";
    value = xmlTextReaderConstValue(readerPtr);
    depth = xmlTextReaderDepth(readerPtr);
    isempty = xmlTextReaderIsEmptyElement(readerPtr); 
    count = xmlTextReaderAttributeCount(readerPtr);
    //printf("processsing depth=%d, type=%d, name=%s, isempty=%d, value=%s, attributes=%d\n",depth,type,name,isempty,value,count);
    if(!strcmp("graph",(const char*)name))
    {
        //printf("processing graph node\n");
        xmlChar* graphId;
        graphId = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"id");
        if(NULL == graphId)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): Error missing graph id attribute!\n");
            return false;
        }
        SetXMLName((const char*)graphId);
        xmlChar* graphEdgeType;
        graphEdgeType = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"edgedefault");
        if(!strcmp("directed",(const char*)graphEdgeType))
        {
            isDuplex = false;
        }
        else
        {
            isDuplex = true;
        }
        //printf("exiting graph node id = \"%s\" is duplex is %s\n",graphId,isDuplex ? "true" : "false");
    }
    else if(!strcmp("node",(const char*)name))
    {
        xmlChar* nodeId = NULL;
        while (xmlTextReaderMoveToNextAttribute(readerPtr)>0)
        {

            //printf("found attribute %s with name %s\n",xmlTextReaderName(readerPtr),xmlTextReaderConstValue(readerPtr));
            if(!strcmp((const char*)xmlTextReaderConstName(readerPtr),"id"))
            {
                nodeId = xmlTextReaderValue(readerPtr);
            }
        }
        if(NULL == nodeId)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): Error missing node id attribute!\n");
            return false;
        }
        if(strlen((const char*)nodeId) > MAXXMLIDLENGTH)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode: Error the node id value of \"%s\" is too large\n",nodeId);
            return false;
        }
        strcpy(parentXMLNodeID,(const char*)nodeId);
        
        NetGraph::Interface* interface;
        ProtoAddress addr;
        addr.ResolveFromString((const char*)nodeId);
        if(addr.IsValid())
        {
            interface = graph.FindInterface(addr);
        } 
        else 
        {
            interface = graph.FindInterfaceByName((const char*)nodeId);
        }
        if (NULL == interface)
        {
            //Create new node and associated interface
            NetGraph::Node* node = CreateNode();
            
            if(NULL == node)
            {
                PLOG(PL_ERROR, "NetGraph::ProcessXMLNode: error Creating new node!\n");
                return -1;
            }    
            if(addr.IsValid())
            {
                interface = CreateInterface(*node,addr);
            } 
            else 
            {
                interface = CreateInterface(*node);
                interface->SetName((const char*)nodeId);
            }
            //node->AddInterface(*interface,true);
            AddInterfaceToNode(*node,*interface,true);
            //graph.InsertInterface(*interface);
            AddInterfaceToGraph(graph,*interface);
        }
        else
        {
            //existing node so update anything we might have on it
        }
        //printf("exiting node node\n");
    }
    else if(!strcmp("port",(const char*)name))
    {
        //printf("processing port node\n");
        xmlChar* portname;
        portname = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"name");
        NetGraph::Interface* portinterface;
        portinterface = graph.FindInterfaceByString((const char*)portname);

        ProtoAddress portAddr;
        portAddr.ResolveFromString((const char*)portname);
        if (NULL == portinterface)
        {
            //Find the parent node and associated with this interface
            NetGraph::Interface* interface;
            interface = graph.FindInterfaceByString(parentXMLNodeID);
            ProtoAddress addr;
            addr.ResolveFromString(parentXMLNodeID);
            if(NULL == interface)
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode: Error finding the parent \"%s\" interface in the graph for port \"%s\"!\n",parentXMLNodeID,portname);
                return false;
            }
            
            NetGraph::Node& node = interface->GetNode();
            //NetGraph::Node *node = &interface->GetNode();
            if(portAddr.IsValid())
            {
                portinterface = new NetGraph::Interface(node,portAddr);
            } 
            else 
            {
                portinterface = CreateInterface(node);
                portinterface->SetName((const char*)portname);
            }
            //node->SetPosition(x, y);
            //node.AddInterface(*portinterface,false);
            AddInterfaceToNode(node,*portinterface,false);
            //graph.InsertInterface(*portinterface);
            AddInterfaceToGraph(graph,*interface);
            //printf(" just added port \"%s\" to node \"%s\"\n",portname,parentXMLNodeID);
        }
        else
        {
            //portname exists already so we just need to update any info associated with it
        }
        //printf("exiting port node\n");
/*    PLOG(PL_ERROR,"nodeid=%s, depth=%d, type=%d, name=%s, isEmpty=%d, hasValue=%d",
        parentXMLNodeID,
        xmlTextReaderDepth(readerPtr),
        xmlTextReaderNodeType(readerPtr),
        name,
        xmlTextReaderIsEmptyElement(readerPtr),
        xmlTextReaderHasValue(readerPtr));
        //printf("is a port\n");*/
    }
    else if(!strcmp("edge",(const char*)name))
    {
        //printf("processing edge node\n");
        //check to see if this edge is a port type first
        xmlChar *targetPortName, *sourcePortName;
        targetPortName = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"targetport");
        sourcePortName = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"sourceport");
        if((NULL != targetPortName) && (NULL != sourcePortName))
        {
            //find the target port interface
            NetGraph::Interface* targetPortInterface;
            NetGraph::Interface* sourcePortInterface;
            targetPortInterface = graph.FindInterfaceByString((const char*)targetPortName);
            sourcePortInterface = graph.FindInterfaceByString((const char*)sourcePortName);
            if((NULL == sourcePortInterface) || (NULL == targetPortInterface))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode: Error finding the source interface \"%s\" or target interface \"%s\" to create an edge!\n",sourcePortName,targetPortName);
                return false;
            }
            NetGraph::Cost* mycost = CreateCost(1.0);
            if(!Connect(*sourcePortInterface,*targetPortInterface,*mycost,isDuplex))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode: Error connecting the source interface \"%s\" or target interface \"%s\" to create an edge!\n",sourcePortName,targetPortName);
                return false;
            }
            //printf(" connected interface ports %s to %s\n",sourcePortName,targetPortName);
        }
        else //isn't a port type just a normal source target type
        {
            xmlChar *targetName, *sourceName;
            targetName = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"target");
            sourceName = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"source");
            //find the target interface
            NetGraph::Interface* targetInterface;
            NetGraph::Interface* sourceInterface;
            sourceInterface = graph.FindInterfaceByString((const char*)sourceName);
            targetInterface = graph.FindInterfaceByString((const char*)targetName);
            if((NULL == sourceInterface) || (NULL == targetInterface))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode: Error finding the source interface \"%s\" or target interface \"%s\" to create an edge!\n",sourceName,targetName);
                return false;
            }
            NetGraph::Cost* mycost = CreateCost(1.0);
            if(!Connect(*sourceInterface,*targetInterface,*mycost,isDuplex))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode: Error connecting the source interface \"%s\" or target interface \"%s\" to create an edge!\n",sourceName,targetName);
                return false;
            }
            //printf(" connected interface %s to %s\n",sourceName, targetName);
        }
        //printf("exiting edge node\n");
    }
    else if(!strcmp("key",(const char*)name))
    {
        xmlChar* oldkey = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"id");
        xmlChar* type = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"attr.type");
        xmlChar* name = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"attr.name");
        xmlChar* domain = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"for");

        xmlChar* defaultvalue = NULL;
        xmlNode* myXMLNode = xmlTextReaderExpand(readerPtr);
        xmlNode* childXMLNode = myXMLNode->children;
        while(NULL != childXMLNode)
        {
            if(!strcmp((char*)childXMLNode->name,"default"))
            {
                if(NULL == childXMLNode->children)
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): %s has an empty default value!\n",name);
                    return false;
                }
                defaultvalue = childXMLNode->children->content;
            }
            childXMLNode = childXMLNode->next;
        }
        //printf("reading in attribute name=%s key=%s defaultvalue=%s\n",name,oldkey,defaultvalue);
        if(!AddAttributeKey((const char*)name,(const char*)type,(const char*)domain,(const char*)oldkey,(const char*)defaultvalue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): Error adding attribute (name=%s,oldkey=%s)\n",name,oldkey);
            return false;
        }
    }
    else if(!strcmp("data",(const char*)name))
    {
        char newlookup[250];//this really should be dynamic and checks on size should take place TBD
        xmlChar* oldIndex;
        const char* newIndex;
        xmlChar* newValue;
        xmlNode* myxmlnode = xmlTextReaderExpand(readerPtr);

        if(!strcmp((const char*)myxmlnode->parent->name,"node"))
        {
            sprintf(newlookup,"node:%s",parentXMLNodeID);
        } else if(!strcmp((const char*)myxmlnode->parent->name,"port")) {
            xmlAttr* tempattribute = myxmlnode->parent->properties;
            const xmlChar *portName = NULL;
            while(NULL != tempattribute) 
            {
                if(!strcmp((const char*)tempattribute->name,"name")) 
                    portName = tempattribute->children->content;
                tempattribute= tempattribute->next;
            }
            sprintf(newlookup,"node:%s:port:%s",parentXMLNodeID,portName);
            
        } else if(!strcmp((const char*)myxmlnode->parent->name,"edge")) {
            xmlAttr* tempattribute = myxmlnode->parent->properties;
            const xmlChar *targetPortName(NULL), *sourcePortName(NULL), *targetName(NULL), *sourceName(NULL);
            while(NULL != tempattribute)
            {
                if(!strcmp((const char*)tempattribute->name,"sourceport")) 
                    sourcePortName = tempattribute->children->content;
                if(!strcmp((const char*)tempattribute->name,"targetport")) 
                    targetPortName = tempattribute->children->content;
                if(!strcmp((const char*)tempattribute->name,"source")) 
                    sourceName = tempattribute->children->content;
                if(!strcmp((const char*)tempattribute->name,"target")) 
                    targetName = tempattribute->children->content;
                tempattribute= tempattribute->next;
            }
            if((NULL == targetName) || (NULL == sourceName))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): Error finding target/source for link data\n");
                return false;
            }
            sprintf(newlookup,"edge:source:%s:%s:dest:%s:%s",sourceName,sourcePortName,targetName,targetPortName);
        } else {
            PLOG(PL_WARN,"ManetGraphMLParser::ReadXMLNode(): Ignoring data for unknown node type\n");
        }
        oldIndex = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"key");
        AttributeKey* tempakey = FindAttributeKeyByOldIndex((const char*)oldIndex);
        if(NULL == tempakey)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): found data with key type %s but it wasn't listed at one of the keys\n",oldIndex);
            return false;
        } else {
            newIndex = tempakey->GetIndex();
            //printf("%s\n",newIndex);
        }
        if(!xmlTextReaderRead(readerPtr))
            return false;
        newValue = xmlTextReaderValue(readerPtr);
        //printf("found data for \"%s\" with key %s with value %s\n",newlookup,oldIndex,newValue);
        
        Attribute* newAttribute = new Attribute();
        newAttribute->Init(newlookup,(const char*)newIndex,(const char*)newValue);
        //newAttribute->Init(newlookup,newIndex,(const char*)newValue);
        attributelist.Insert(*newAttribute);
    }
    else
    {
        //ignorning xml node
        //printf("ignoring xml node %s\n",name);
    }
    return true;

}  // end ManetGraphMLParser::ReadXMLNode()



bool ManetGraphMLParser::Write(NetGraph& graph, const char* path)
{
    PLOG(PL_INFO,"ManetGraphMLParser::Write: Enter!\n");
/* Create a new XmlWriter for DOM, with no compression. */
    int returnvalue;
    xmlTextWriter* writerPtr;
    xmlDoc* docPtr;

    writerPtr = xmlNewTextWriterDoc(&docPtr, 0);
    if (writerPtr == NULL) {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlwriterPtrDoc: Error creating the xml writer\n");
        return false;
    }

    /* Start the docPtrument with the xml default for the version,
     * encoding UTF-8 and the default for the standalone
     * declarao*/
    returnvalue = xmlTextWriterStartDocument(writerPtr, NULL, MY_GRAPHML_ENCODING, NULL);
    if (returnvalue < 0) {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlwriterPtrDoc: Error at xmlTextWriterStartDocument\n");
        return false;;
    }

    /* Start an element named "graphml". Since thist is the first
     * element, this will be the root element of the docPtrument. */
    returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "graphml");
    if (returnvalue < 0) {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlwriterPtrDoc: Error at xmlTextWriterStartElement\n");
        return false;
    }
    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "xmlns", BAD_CAST "http://graphml.graphdrawing.org/xmlns");
    if (returnvalue < 0) {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlWriterDoc: Error at xmlTextWriterWriteAttribute\n");
        return false;
    } 
    if(!UpdateKeys(graph))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlWriterDoc: Error updating key elements in the header\n");
        return false;
    }
    if(!WriteLocalKeys(writerPtr))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlWriterDoc: Error at writing key elements in header\n");
        return false;
    }
    
    /* We are done with the header so now we go through the actual graph and add each node and edge */
    /* We are adding each node */
    returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "graph");
    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error starting XML graph element\n"); return false;}
    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "id",BAD_CAST XMLName);
    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error setting XML graph attribute id\n"); return false;}
    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "edgedefault",BAD_CAST "directed");
    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error setting graph attribute directed\n"); return false;}

    NetGraph::InterfaceIterator it(graph);
    NetGraph::Interface* iface;
    //adding the nodes to the xml
    while (NULL != (iface = it.GetNextInterface()))
    {
        //check to see if this is a default "node" interface
        //if(!iface->IsPort())
        if(iface == iface->GetNode().GetDefaultInterface())
        {
            //Node& node = static_cast<Node&>(iface->GetNode());
            returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "node");
            if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding XML node\n"); return false;}
            if(iface->GetAddress().IsValid())
            {
                returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "id", BAD_CAST iface->GetAddress().GetHostString());
                //printf("writing node %s\n",iface->GetAddress().GetHostString());
            } else {
                returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "id", BAD_CAST iface->GetName());
                //printf("writing node %s\n",iface->GetName());
            }
            if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding setting node id\n"); return false;}
            
            //update the node attributes using the virtual function
            if(!UpdateNodeAttributes(iface->GetNode()))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error updating the node attributes\n");
                return false;
            } 
            //call the local function write the attributes out
            if(!WriteLocalNodeAttributes(writerPtr,iface->GetNode()))
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error writing local node attributes\n");
                return false;
            }
            //call the virtual function to allow the node to fill in any attributes if there
/*            if(!WriteNodeAttributes(writerPtr,iface->GetNode())) //this shouldn't really have to be called if we add/update the attributes correctly TBD
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error writing node attributes\n");
                return false;
            }*/

            //we need to iterate through the any "port" interfaces which we may have
            NetGraph::Node *theNode = &iface->GetNode();
            NetGraph::Node::InterfaceIterator nodeIt(*theNode);
            NetGraph::Interface* portIface;
            while (NULL != (portIface = static_cast<NetGraph::Interface*>(nodeIt.GetNextInterface())))
            {
                //we only want ports
                if(!(portIface == portIface->GetNode().GetDefaultInterface()))
//                if(portIface->IsPort())
                {
                    returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "port");
                    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding XML node\n"); return false;}
                    if(portIface->GetAddress().IsValid())
                    {
                        returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "name", BAD_CAST portIface->GetAddress().GetHostString());
                        //printf("writing interface %s\n",iface->GetAddress().GetHostString());
                    } else {
                        returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "name", BAD_CAST portIface->GetName());
                        //printf("writing node %s\n",iface->GetName());
                    } 
                    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding setting node id\n"); return false;}
                    //update attributes to the port/interface using the virutal function
                    if(!UpdateInterfaceAttributes(*portIface))
                    {
                        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error updating the interface attributes\n");
                        return false;
                    } 
                    //write the attributes to the port/interface
                    if(!WriteLocalInterfaceAttributes(writerPtr,*portIface))
                    {
                        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error writing local interface attributes\n");
                        return false;
                    }
                    //write any attributes to the port/interface using the virutal function
/*                    if(!WriteInterfaceAttributes(writerPtr,*portIface))//this needn't be called if we add/update the attributes in the local list TBD
                    {
                        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error writing interface attributes\n");
                        return false;
                    }*/
                    returnvalue = xmlTextWriterEndElement(writerPtr);
                    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error ending node element\n"); return false;}
                }
            }
            //close up the node node element
            returnvalue = xmlTextWriterEndElement(writerPtr);
            if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error ending node element\n"); return false;}
        }
    }
    //adding the directional links to the xml
    it.Reset();
    while (NULL != (iface = it.GetNextInterface()))
    {
        //if(!iface->IsPort())//not a port node so we can just print it normally
        if(iface == iface->GetNode().GetDefaultInterface())
        {
            NetGraph::AdjacencyIterator iteratorN1(*iface);
            NetGraph::Interface* nbrIface;
            while (NULL != (nbrIface = iteratorN1.GetNextAdjacency()))
            {
                returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "edge");
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding edge\n"); return false;}
                if(iface->GetAddress().IsValid()) {
                    //printf("writing connection %s ->",iface->GetAddress().GetHostString());
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST iface->GetAddress().GetHostString());
                } else { 
                    //printf("writing connection %s ->",iface->GetName());
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST iface->GetName());
                }
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); return false;}
                if(nbrIface->GetAddress().IsValid()) {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrIface->GetAddress().GetHostString());
                    ////printf("%s\n",nbrIface->GetAddress().GetHostString());
                } else {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrIface->GetName());
                    ////printf("%s\n",nbrIface->GetName());
                }
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); return false;}
                NetGraph::Link* link = iface->GetLinkTo(*nbrIface);
                if(NULL == link)
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error getting link which should always exist\n");
                    return false;
                }
                //update the link attributes using the virtual function
                if(!UpdateLinkAttributes(*link))
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error updating the link attributes\n");
                    return false;
                } 
                //actually write the attributes out
                if(!WriteLocalLinkAttributes(writerPtr,*link))
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error writing link attributes\n");
                    return false;
                }
                returnvalue = xmlTextWriterEndElement(writerPtr);
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error ending node element\n"); return false;}
            } 
        }
        else //it is a port interface so we need to find the "node" interface
        {
            //lets first find the "src" node interface
            NetGraph::Interface* nodeIface = iface->GetNode().GetDefaultInterface();
            if(!(nodeIface == nodeIface->GetNode().GetDefaultInterface()))
//            if(nodeIface->IsPort())
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::Write Error the default interface was not the \"node\" interface for the src.  You should iterate over them all and find the right one\n");
                return false;
            }
            
            NetGraph::AdjacencyIterator iteratorN1(*iface);
            NetGraph::Interface* nbrIface, *nbrNodeIface;
            while (NULL != (nbrIface = iteratorN1.GetNextAdjacency()))
            {
                //lets find the "nbr" node interface
                nbrNodeIface = static_cast<NetGraph::Interface*>(nbrIface->GetNode().GetDefaultInterface());
                //if(nbrNodeIface->IsPort())
                if(!(nbrNodeIface == nbrNodeIface->GetNode().GetDefaultInterface()))
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error the default interface was not the \"node\" interface for the nbr.  You should iterate over them all and find the right one\n");
                    return false;
                }
                returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "edge");
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding edge\n"); return false;}

                if(nodeIface->GetAddress().IsValid()) {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST nodeIface->GetAddress().GetHostString());
                } else { 
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST nodeIface->GetName());
                }
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); return false;}
                if(nbrNodeIface->GetAddress().IsValid()) {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrNodeIface->GetAddress().GetHostString());
                } else {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrNodeIface->GetName());
                }
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); return false;}
                if(iface->GetAddress().IsValid()) {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "sourceport", BAD_CAST iface->GetAddress().GetHostString());
                } else { 
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "sourceport", BAD_CAST iface->GetName());
                }
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); return false;}
                if(nbrIface->GetAddress().IsValid()) {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "targetport", BAD_CAST nbrIface->GetAddress().GetHostString());
                } else {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "targetport", BAD_CAST nbrIface->GetName());
                }
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); return false;}

                NetGraph::Link* link = iface->GetLinkTo(*nbrIface);
                if(NULL == link)
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error getting link which should always exist\n");
                    return false;
                }
                //update the link attributes using the virtual function
                if(!UpdateLinkAttributes(*link))
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error updating the link attributes\n");
                    return false;
                }
                //actually write the attributes out 
                if(!WriteLocalLinkAttributes(writerPtr,*link))
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error writing link attributes\n");
                    return false;
                }
                returnvalue = xmlTextWriterEndElement(writerPtr);
                if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write Error ending node element\n"); return false;}
            } 
        }
    }
 
    returnvalue = xmlTextWriterEndDocument(writerPtr);
    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write:testXmlwriterPtrDoc: Error at xmlTextWriterEndDocument\n"); return false;}

    xmlFreeTextWriter(writerPtr);

    xmlSaveFormatFileEnc(path, docPtr, MY_GRAPHML_ENCODING,1);

    xmlFreeDoc(docPtr);
 
    return true;
}  // end ManetGraphMLParser::Write()

ManetGraphMLParser::Attribute* 
ManetGraphMLParser::AttributeList::FindAttribute(const char *theLookup,const char* theIndex)
{
    AttributeList::Iterator it(*this,false,theLookup,strlen(theLookup)*8);
    Attribute* attr(NULL);
    attr = it.GetNextItem();
    while(NULL !=attr)
    {
        if(strcmp(attr->GetLookup(),theLookup))
        {
            attr = NULL;
            //we didn't find the entry
        } else {
            if(strcmp(attr->GetIndex(),theIndex))
            {
                //this attribute does not have the index we are looking for
                attr = it.GetNextItem();
            } else {
                return attr;
            }
        }
    }
    return attr;
}
bool
ManetGraphMLParser::WriteLocalKeys(xmlTextWriter* writerPtr)
{
    int rv = 0;
    IndexKeylist::Iterator it(indexkeylist);
    AttributeKey* key(NULL);
    while(NULL != (key = it.GetNextItem()))
    {
        rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "key");
        rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "id",BAD_CAST key->GetIndex());
        rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.name",BAD_CAST key->GetName());
        switch (key->GetType())
        {
          case AttributeKey::Types::BOOL:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.type",BAD_CAST "boolean");
            break;
          case AttributeKey::Types::INT:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.type",BAD_CAST "int");
            break;
          case AttributeKey::Types::LONG:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.type",BAD_CAST "long");
            break;
          case AttributeKey::Types::FLOAT:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.type",BAD_CAST "float");
            break;
          case AttributeKey::Types::DOUBLE:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.type",BAD_CAST "double");
            break;
          case AttributeKey::Types::STRING:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "attr.type",BAD_CAST "string");
            break;
          default:
            PLOG(PL_ERROR,"ManetGraphMLParser::WriteLocalKeys: type is invalid!\n");
            return false;
        }
        switch (key->GetDomain())
        {
          case AttributeKey::Domains::GRAPH:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "for",BAD_CAST "graph");
            break;
          case AttributeKey::Domains::NODE:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "for",BAD_CAST "node");
            break;
          case AttributeKey::Domains::EDGE:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "for",BAD_CAST "edge");
            break;
          case AttributeKey::Domains::ALL:
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "for",BAD_CAST "all");
            break;
          default:
            break;
        }
        if (NULL != key->GetDefault())
        {
            rv += xmlTextWriterWriteElement(writerPtr,BAD_CAST "default", BAD_CAST key->GetDefault());
        }
        rv += xmlTextWriterEndElement(writerPtr);
    }
    if(rv < 0)
        return false;
    return true;
}
bool 
ManetGraphMLParser::WriteLocalNodeAttributes(xmlTextWriter* writerPtr,NetGraph::Node& theNode)
{
    bool rv = true;
    char key[255];//this should be dynamic or checks added TBD
    sprintf(key,"node:%s",GetString(theNode));
    AttributeList::Iterator it(attributelist,false,key,strlen(key)*8);
    Attribute* attr(NULL);
    //iterate over items which have the matching keys
    attr = it.GetNextItem();
    while(NULL != attr)
    {
        if(strcmp(attr->GetLookup(),key))
        {
            attr = NULL;
            //PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalNodeAttributes():mykey=\"%s\",lookup=\"%s\",key=\"%s\",value=\"%s\"\n",key,attr->GetLookup(),attr->GetIndex(),attr->GetValue());
            //attr = it.GetNextItem();
        } else {
            rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST attr->GetIndex());
            rv += xmlTextWriterWriteString(writerPtr, BAD_CAST attr->GetValue());
            rv += xmlTextWriterEndElement(writerPtr);
            PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalNodeAttributes():key=\"%s\",value=\"%s\"\n",attr->GetIndex(),attr->GetValue());
            attr = it.GetNextItem();
        }
    }
    return rv;
}
bool 
ManetGraphMLParser::WriteLocalInterfaceAttributes(xmlTextWriter* writerPtr,NetGraph::Interface& theInterface)
{
    bool rv = true;
    char key[255];//this should be dynamic or checks added TBD
    sprintf(key,"node:%s",GetString(theInterface.GetNode()));
    sprintf(key,"%s:port:%s",key,GetString(theInterface));
    AttributeList::Iterator it(attributelist,false,key,strlen(key)*8);
    Attribute* attr(NULL);
    attr = it.GetNextItem();
    while(NULL != attr)
    {
        if(strcmp(attr->GetLookup(),key))
        {
            attr = NULL;
        } else {
            rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST attr->GetIndex());
            rv += xmlTextWriterWriteString(writerPtr, BAD_CAST attr->GetValue());
            rv += xmlTextWriterEndElement(writerPtr);
            attr = it.GetNextItem();
        }
    }
    return rv;
}
bool 
ManetGraphMLParser::WriteLocalLinkAttributes(xmlTextWriter* writerPtr,NetGraph::Link& theLink)
{
    bool rv = true;
    char key[255];//this should be dynamic or checkes added TBD
    sprintf(key,"edge:source:%s",GetString(theLink.GetSrc()->GetNode()));
    sprintf(key,"%s:%s",key,GetString(*theLink.GetSrc()));
    sprintf(key,"%s:dest:%s",key,GetString(theLink.GetDst()->GetNode()));
    sprintf(key,"%s:%s",key,GetString(*theLink.GetDst()));
    //printf("%s\n",key);
    
    AttributeList::Iterator it(attributelist,false,key,strlen(key)*8);
    Attribute* attr(NULL);
    attr = it.GetNextItem();
    while(NULL != attr)
    {
        if(strcmp(attr->GetLookup(),key))
        {
            attr = NULL;
        } else {
            rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST attr->GetIndex());
            rv += xmlTextWriterWriteString(writerPtr, BAD_CAST attr->GetValue());
            rv += xmlTextWriterEndElement(writerPtr);
            attr = it.GetNextItem();
        }
    }
    return rv;
}
