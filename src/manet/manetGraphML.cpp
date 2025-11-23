#include <manetGraphML.h>
#include <protoDebug.h>

ManetGraphMLParser::ManetGraphMLParser() : XMLName(NULL), indexes(0) 
{
    xmlInitParser();
}

ManetGraphMLParser::~ManetGraphMLParser() 
{
    xmlCleanupParser(); 
    oldindexkeylist.Empty();
    indexkeylist.Empty();
    namedkeylist.Destroy();
    
    attributelist.Destroy();
    if (NULL != XMLName) 
    {
        delete[] XMLName;
        XMLName = NULL;
    }
}

const char* ManetGraphMLParser::GetString(NetGraph::Interface& iface) const
{
    if(iface.GetDefaultAddress().IsValid())
    {
        return iface.GetDefaultAddress().GetHostString();
    }
    else
    {
        return iface.GetName();
    }
}

const char* ManetGraphMLParser::GetString(NetGraph::Node& node) const
{
    return GetString(*node.GetDefaultInterface());
}


bool ManetGraphMLParser::AttributeKey::Init(const char* theIndex,const char* theName,const char* theType,const char* theDomain, const char* theOldIndex,const char*theDefault)
{
    if((NULL != index) || (NULL != name) || (NULL != oldindex) || (Types::INVALID !=type) || (Domains::INVALID != domain) )
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Init: Error attempting to Init AttributeKey which has already called Init!\n");
        return false;
    }
    index = new char[strlen(theIndex)+1];
    name = new char[strlen(theName)+1];
    
    if((NULL == index) || (NULL == name))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Init: Error allocating space for index or name strings\n");
        return false;
    }
    if(NULL != theOldIndex)
    {
        oldindex = new char[strlen(theOldIndex)+1];
        if(NULL == oldindex)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Init: Error allocating space for oldindex string\n");
            return false;
        }
        strcpy(oldindex,theOldIndex);
    }
    strcpy(name,theName);
    strcpy(index,theIndex);
    if(!SetType(theType))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Init: Error setting the type to %s\n",theType);
        return false;
    }
    if(NULL != theDomain)
    {
        if(!SetDomain(theDomain))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Init: Error setting the domain to %s\n",theDomain);
            return false;
        }
    }
    if(NULL != theDefault)
    {
        if(!SetDefault(theDefault))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Init: Error setting the default to %s\n",theDefault);
            return false;
        }
    }
    return true;
}


bool ManetGraphMLParser::AttributeKey::Set(const char* theIndex,const char* theName,const char* theType,const char* theDomain, const char* theOldIndex,const char*theDefault)
{
    if(NULL != index)
        delete[] index;
    if(NULL != name)
        delete[] name;
    if(NULL !=oldindex)
        delete[] oldindex;
    index = new char[strlen(theIndex)+1];
    name = new char[strlen(theName)+1];
    
    if((NULL == index) || (NULL == name))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Set: Error allocating space for index or name strings\n");
        return false;
    }
    if(NULL != theOldIndex)
    {
        oldindex = new char[strlen(theOldIndex)+1];
        if(NULL == oldindex)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Set: Error allocating space for oldindex string\n");
            return false;
        }
        strcpy(oldindex,theOldIndex);
    }
    strcpy(name,theName);
    strcpy(index,theIndex);

    if(!SetType(theType))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Set: Error setting the type to %s\n",theType);
        return false;
    }
    if(NULL != theDomain)
    {
        if(!SetDomain(theDomain))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Set: Error setting the domain to %s\n",theDomain);
            return false;
        }
    }
    if(NULL != theDefault)
    {
        if(!SetDefault(theDefault))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::Set: Error setting the default to %s\n",theDefault);
            return false;
        }
    }
    return true;
}

bool ManetGraphMLParser::AttributeKey::SetType(const char* theType)
{
    if((!strcmp(theType,"bool")) || 
       (!strcmp(theType,"Bool")) ||
       (!strcmp(theType,"BOOL")) ||
       (!strcmp(theType,"boolean")) ||
       (!strcmp(theType,"Boolean")) ||
       (!strcmp(theType,"BOOLEAN"))) {
        type = Types::BOOL;
    } else if 
       ((!strcmp(theType,"int")) ||
        (!strcmp(theType,"Int")) ||
        (!strcmp(theType,"INT")) ||
        (!strcmp(theType,"integer")) ||
        (!strcmp(theType,"Integer")) ||
        (!strcmp(theType,"INTEGER"))) {
         type = Types::INT;
    } else if 
       ((!strcmp(theType,"long")) ||
        (!strcmp(theType,"Long")) ||
        (!strcmp(theType,"LONG"))) {
         type = Types::LONG;
    } else if 
       ((!strcmp(theType,"float")) ||
        (!strcmp(theType,"Float")) ||
        (!strcmp(theType,"FLOAT"))) {
         type = Types::FLOAT;
    } else if 
       ((!strcmp(theType,"double")) ||
        (!strcmp(theType,"Double")) ||
        (!strcmp(theType,"DOUBLE"))) {
         type = Types::DOUBLE;
    } else if 
       ((!strcmp(theType,"string")) ||
        (!strcmp(theType,"String")) ||
        (!strcmp(theType,"STRING"))) {
         type = Types::STRING;
    } else {
        type = Types::INVALID;
        return false;
    }
    return true;
}
bool
ManetGraphMLParser::AttributeKey::SetDomain(const char* theDomain)
{
    if((!strcmp(theDomain,"graph")) || 
       (!strcmp(theDomain,"Graph")) ||
       (!strcmp(theDomain,"GRAPH"))) {
        domain = Domains::GRAPH;
    } else if 
       ((!strcmp(theDomain,"node")) ||
        (!strcmp(theDomain,"Node")) ||
        (!strcmp(theDomain,"NODE"))) {
         domain = Domains::NODE;
    } else if 
       ((!strcmp(theDomain,"edge")) ||
        (!strcmp(theDomain,"Edge")) ||
        (!strcmp(theDomain,"EDGE"))) {
         domain = Domains::EDGE;
    } else if 
       ((!strcmp(theDomain,"all")) ||
        (!strcmp(theDomain,"All")) ||
        (!strcmp(theDomain,"ALL"))) {
         domain = Domains::ALL;
    } else {
        domain = Domains::INVALID;
        return false;
    }
    return true;
}
bool
ManetGraphMLParser::AttributeKey::SetDefault(const char* theDefault)
{
    if(NULL == theDefault)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::SetDefault: default value pointer is NULL!\n");
        return false;
    }
    if(NULL != defaultvalue)
    {
        //PLOG(PL_WARN,"ManetGraphMLParser::AttributeKey::SetDefault: default has already been set to \"%s\" and is being switched to \"%s\"\n",defaultvalue,theDefault);
        delete[] defaultvalue;
    }
    defaultvalue = new char[strlen(theDefault)+1];
    if(NULL == defaultvalue)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::SetDefault: error allocing space for the defaultvalue pointer!\n");
        return false;
    }
    if(!strcpy(defaultvalue,theDefault))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AttributeKey::SetDefault: Error copying the string!\n");
        delete[] defaultvalue;
        return false;
    }
    return true;
}
ManetGraphMLParser::AttributeKey::~AttributeKey()
{
    if(NULL != index)
    {
        delete[] index;
        index = NULL;
    }
    if(NULL != name)
    {
        delete[] name;
        name = NULL;
    }
    if(NULL != oldindex)
    {
        delete[] oldindex;
        oldindex = NULL;
    }
    if(NULL != defaultvalue)
    {
        delete[] defaultvalue;
        defaultvalue = NULL;
    }
}
bool
ManetGraphMLParser::SetXMLName(const char* theName)
{
    if(NULL == theName)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::SetXMLName() error setting NULL name!\n");
        return false;
    }
    if(NULL != XMLName)
    {
        delete[] XMLName;
    }
    XMLName = new char[strlen(theName)+1];
    if(NULL == XMLName)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::SetXMLName() error allocating space for name array!\n");
        return false;
    }
    strcpy(XMLName,theName);
    return true;
}
bool
ManetGraphMLParser::SetAttributeKey(const char* theName,const char* theType, const char* theDomain, const char* theOldKey,const char* theDefault)
{
    PLOG(PL_INFO,"ManetGraphMLParser::SetAttributeKey: Enter\n");
    //lets look to see if the name exists already
    AttributeKey* theKey;
    theKey = FindAttributeKey(theName);

    if(NULL == theKey)
    {
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttributeKey: didn't find the key\n");
        if(!AddAttributeKey(theName,theType,theDomain,theOldKey,theDefault))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttributeKey: error adding the key \"%s\"\n",theName);
            return false;
        }
        return true;
    } else {
        char tempIndex[20];
        strcpy(tempIndex,theKey->GetIndex()); //we need to copy it over because setting it will delete the old value.
        if(!theKey->Set(tempIndex,theName,theType,theDomain,theOldKey,theDefault))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttributeKey Error Init'ing the theKey with index \"%s\" and name \"%s\"\n",theKey->GetIndex(),theName);
            return false;
        }
        return true;
    }
    PLOG(PL_ERROR,"ManetGraphMLParser::SetAttributeKey: error got to the end of the function and shouldn't!\n");

    return false; //shouldn't get here
}

bool 
ManetGraphMLParser::SetAttribute(const char* theName, const char* theValue)
{
    PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(name=%s,theValue=%s)\n",theName,theValue);
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(Graph): Error finding index for attribute %s\n",theName);
        return false;
    }
    PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Node,name=%s,theValue=%s) found index %s\n",theName,theValue,theIndex);
    char theLookup[250];
    GetLookup(theLookup,250);
    ManetGraphMLParser::Attribute* theAttribute = attributelist.FindAttribute(theLookup,theIndex);
//this doesn't work it only finds one not all!
    //ManetGraphMLParser::Attribute* theAttribute = attributelist.Find(theLookup,strlen(theLookup)*8);
    if(NULL == theAttribute)
    {
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(name=%s,theValue=%s) making new one\n",theName,theValue);
        return AddAttribute(theName,theValue);
    } 
    else
    {
        char tempIndex[20];
        strcpy(tempIndex,theIndex); //we need to copy it over because setting it will delete the old value.
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(name=%s,theValue=%s) Updating old one with index=%s\n",theName,theValue,theIndex);
        if(!theAttribute->Set(theLookup,tempIndex,theValue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(): Error setting the attribute\n");
            return false;
        }
        return true;
    }
    return false; //should never get here
}

bool 
ManetGraphMLParser::SetAttribute(NetGraph::Node& node, const char* theName, const char* theValue)
{
    PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Node,name=%s,theValue=%s)\n",theName,theValue);
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(Node): Error finding index for attribute %s\n",theName);
        return false;
    }
    PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Node,name=%s,theValue=%s) found index %s\n",theName,theValue,theIndex);
    char theLookup[250];
    GetLookup(theLookup,250,node);
    ManetGraphMLParser::Attribute* theAttribute = attributelist.FindAttribute(theLookup,theIndex);
//this doesn't work it only finds one not all!
    //ManetGraphMLParser::Attribute* theAttribute = attributelist.Find(theLookup,strlen(theLookup)*8);
    if(NULL == theAttribute)
    {
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Node,name=%s,theValue=%s) making new one\n",theName,theValue);
        return AddAttribute(node,theName,theValue);
    } 
    else
    {
        char tempIndex[20];
        strcpy(tempIndex,theIndex); //we need to copy it over because setting it will delete the old value.
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Node,name=%s,theValue=%s) Updating old one with index=%s\n",theName,theValue,theIndex);
        if(!theAttribute->Set(theLookup,tempIndex,theValue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(Node): Error setting the attribute\n");
            return false;
        }
        return true;
    }
    return false; //should never get here
}
bool ManetGraphMLParser::SetAttribute(NetGraph::Link& link, const char* theName, const char* theValue)
{
    PLOG(PL_INFO,"ManetGraphMLParser::SetAttribute(Link): Enter\n");
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(Link): Error finding index for attribute %s\n",theName);
        return false;
    }
    char theLookup[250];
    GetLookup(theLookup,250,link);
    
    ManetGraphMLParser::Attribute* theAttribute = attributelist.FindAttribute(theLookup,theIndex);
    if(NULL == theAttribute)
    {
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Link): didn't find existing attribute calling AddAttribute with theName=\"%s\", theValue=\"%s\"\n",theName,theValue);
        if(!AddAttribute(link,theName,theValue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute: error on calling AddAttribute\n");
            return false;
        }
        return true;
    }
    else
    {
        PLOG(PL_DETAIL,"ManetGraphMLParser::SetAttribute(Link): found existing attribute Attribute.Set with theName=\"%s\", theValue=\"%s\"\n",theName,theValue);
        char tempIndex[20];
        strcpy(tempIndex,theIndex); //we need to copy it over because setting it will delete the old value.
        if(!theAttribute->Set(theLookup,tempIndex,theValue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(link): Error setting the attribute\n");
            return false;
        }
        return true;
    }
    PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(link): Error reaching bottom of function and it shouldn't!\n");
    return false; //should never get here
}

bool ManetGraphMLParser::SetAttribute(NetGraph::Interface& interface, const char* theName, const char* theValue)
{
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(Interface): Error finding index for attribute %s\n",theName);
        return false;
    }
    char theLookup[250];
    GetLookup(theLookup,250,interface);
    ManetGraphMLParser::Attribute* theAttribute = attributelist.FindAttribute(theLookup,theIndex);
//this doesn't work it only finds one not all!

    //ManetGraphMLParser::Attribute* theAttribute = attributelist.Find(theLookup,strlen(theLookup)*8);
    if(NULL == theAttribute)
    {
        return AddAttribute(interface,theName,theValue);
    }
    else
    {
        if(!theAttribute->Set(theLookup,theIndex,theValue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::SetAttribute(link): Error setting the attribute\n");
            return false;
        }
        return true;
    }
    return false; //should never get here
}


bool
ManetGraphMLParser::AddAttributeKey(const char* theName,const char* theType, const char* theDomain, const char* theOldKey,const char* theDefault)
{
    //lets look to see if the name exists already
    PLOG(PL_INFO,"ManetGraphMLParser::AddAttributeKey(): Enter\n");
    if(NULL != FindAttributeIndex(theName))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttributeKey has been called with attribute name \"%s\" which already exists!\n",theName);
        return false;
    }
    PLOG(PL_DETAIL,"ManetGraphMLParser::AddAttributeKey() making new key\n");
    AttributeKey* newKey = new AttributeKey;
    if(NULL == newKey)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttributeKey Error allocating a new AttributeKey\n");
        return false;
    }
    char theIndex[20];//this will work as we are only using d%d as our indexes.
    sprintf(theIndex,"d%d",indexes++);
    PLOG(PL_DETAIL,"ManetGraphMLParser::AddAttributeKey() made key initing with \"%s\"\n",theIndex);
    if(!newKey->Init(theIndex,theName,theType,theDomain,theOldKey,theDefault))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttributeKey Error Init'ing the newKey with index \"d%d\" and name \"%s\"\n");
        delete newKey;
        indexes--;
        return false;
    }
    indexkeylist.Insert(*newKey);
    namedkeylist.Insert(*newKey);
    if(NULL != theOldKey)
        oldindexkeylist.Insert(*newKey);
    PLOG(PL_DETAIL,"ManetGraphMLParser::AddAttributeKey() return true\n");
    return true;
}

bool ManetGraphMLParser::AddAttribute(const char* theName, const char* theValue)
{
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(): Error finding index for attribute %s\n",theName);
        return false;
    }
    Attribute* newAttribute = new Attribute();
    if(NULL == newAttribute)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(): Error allocating attribute\n");
        return false;
    }
     
    char theLookup[250];    //TBD
    GetLookup(theLookup,250);
    if(!newAttribute->Init(theLookup,theIndex,theValue))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Node): Error init the attribute\n");
        return false;
    }
    if(!attributelist.Insert(*newAttribute))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(): Error inserting the attribute\n");
        return false;
    }
    DMSG(7,"ManetGraphMLParser::AddAttribute(%s,%s) added successfully\n",theName,theValue); 
    return true;
}

bool ManetGraphMLParser::AddAttribute(NetGraph::Node& node, const char* theName, const char* theValue)
{
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Node): Error finding index for attribute %s\n",theName);
        return false;
    }
    Attribute* newAttribute = new Attribute();
    if(NULL == newAttribute)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Node): Error allocating attribute\n");
        return false;
    }
     
    char theLookup[250];    //TBD
    GetLookup(theLookup,250,node);
    if(!newAttribute->Init(theLookup,theIndex,theValue))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Node): Error init the attribute\n");
        return false;
    }
    if(!attributelist.Insert(*newAttribute))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Node): Error inserting the attribute\n");
        return false;
    }
    DMSG(7,"ManetGraphMLParser::AddAttribute(node,%s,%s) added successfully\n",theName,theValue); 
    return true;
}
bool ManetGraphMLParser::AddAttribute(NetGraph::Link& link, const char* theName, const char* theValue)
{
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Link): Error finding index for attribute %s\n",theName);
        return false;
    }
    Attribute* newAttribute = new Attribute();
    if(NULL == newAttribute)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Link): Error allocating attribute\n");
        return false;
    }
    char theLookup[250];
    GetLookup(theLookup,250,link);

    if(!newAttribute->Init(theLookup,theIndex,theValue))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Link): Error initing the attribute\n");
        return false;
    }
    if(!attributelist.Insert(*newAttribute))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Link): Error inserting the attribute\n");
        return false;
    }
    return true; 
}
bool ManetGraphMLParser::AddAttribute(NetGraph::Interface& interface, const char* theName, const char* theValue)
{
    const char* theIndex = FindAttributeIndex(theName);
    if(NULL == theIndex)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(interface): Error finding index for attribute %s\n",theName);
        return false;
    }
    Attribute* newAttribute = new Attribute();
    if(NULL == newAttribute)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Link): Error allocating attribute\n");
        return false;
    }
    char theLookup[250];

    GetLookup(theLookup,250,interface);    
    if(!newAttribute->Init(theLookup,theIndex,theValue))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Interface): Error init the attribute\n");
        return false;
    }
    if(!attributelist.Insert(*newAttribute))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::AddAttribute(Interface): Error inserting the attribute\n");
        return false;
    }
    return true;
}

bool ManetGraphMLParser::GetLookup(char* theLookup,unsigned int maxlen)
{
    sprintf(theLookup,"thisGraph");//don't name interfaces/links/nodes thisGraph!
    return true;
}

bool ManetGraphMLParser::GetLookup(char* theLookup,unsigned int maxlen,NetGraph::Node& node)
{
    if(strlen(GetString(node))>maxlen)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::GetLookup(node) node string is longer than max leng\n");
        return false;
    }
    sprintf(theLookup,"node:%s",GetString(node));
    return true;
}
bool ManetGraphMLParser::GetLookup(char* theLookup,unsigned int maxlen,NetGraph::Link& link)
{
    char* sourceName;
    char* sourcePortName;
    char* targetName;
    char* targetPortName;
    sourceName = new char[strlen(GetString(link.GetSrc()->GetNode()))+1];
    strcpy(sourceName,GetString(link.GetSrc()->GetNode()));
    sourcePortName = new char[strlen(GetString(*link.GetSrc()))+1];
    strcpy(sourcePortName,GetString(*link.GetSrc()));
    targetName = new char[strlen(GetString(link.GetDst()->GetNode()))+1];
    strcpy(targetName,GetString(link.GetDst()->GetNode()));
    targetPortName = new char[strlen(GetString(*link.GetDst()))+1];
    strcpy(targetPortName,GetString(*link.GetDst()));
    if(strlen(sourceName)+strlen(sourcePortName)+strlen(targetName)+strlen(targetPortName)+21>maxlen)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::GetLookup(link) link string is longer than max length\n");
        delete[] sourceName;
        delete[] sourcePortName;
        delete[] targetName;
        delete[] targetPortName;
        return false;
    }
    sprintf(theLookup,"edge:source:%s:%s:dest:%s:%s",sourceName,sourcePortName,targetName,targetPortName);

    delete[] sourceName;
    delete[] sourcePortName;
    delete[] targetName;
    delete[] targetPortName;
     
    return true;
}
bool ManetGraphMLParser::GetLookup(char* theLookup,unsigned int maxlen,NetGraph::Interface& interface)
{
    char *portName;
    portName = new char[strlen(GetString(interface))+1];
    strcpy(portName,GetString(interface));
    if(strlen(portName)+strlen(GetString(interface.GetNode()))+12>maxlen)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::GetLookup(interface) interface string is longer than max length\n");
        delete[] portName;
        return false;
    }
    sprintf(theLookup,"node:%s:port:%s",GetString(interface.GetNode()),portName);
    delete[] portName;
    return true;
}

const char* ManetGraphMLParser::FindAttributeIndex(const char* theName)
{
    PLOG(PL_DETAIL,"ManetGraphMLParser::FindAttributeIndex(%s): Enter\n",theName);
    AttributeKey* theKey = namedkeylist.Find(theName,strlen(theName)*8);
    //PLOG(PL_DETAIL,"ManetGraphMLParser::FindAttributeIndex(): after the find call\n");
    if(NULL == theKey)
    {
        PLOG(PL_INFO,"ManetGraphMLParser::FindAttributeIndex(): didn't exist returning NULL\n");
        return NULL;
    }
    //PLOG(PL_DETAIL,"ManetGraphMLParser::FindAttributeIndex(): Found a key returning its index\n");
    return theKey->GetIndex();
}
ManetGraphMLParser::AttributeKey* ManetGraphMLParser::FindAttributeKey(const char* theName)
{
    return namedkeylist.Find(theName,strlen(theName)*8);
} 
ManetGraphMLParser::AttributeKey* ManetGraphMLParser::FindAttributeKeyByOldIndex(const char* theOldIndex)
{
    return oldindexkeylist.Find(theOldIndex,strlen(theOldIndex)*8);
}

bool
ManetGraphMLParser::Attribute::Set(const char* theLookupvalue, const char* theIndex,const char* theValue)
{
    if(NULL!=lookupvalue)
    {
        if(strcmp(theLookupvalue,lookupvalue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::Attribute::Set: Error attempting to Set Attribute with new lookupvalue! new!=old \"%s\"!=\"%s\"\n",theLookupvalue,lookupvalue);
            return false;
        }
        delete[] lookupvalue;
        lookupvalue = NULL;
    }
    if(NULL != index)
    {
        delete[] index; 
        index = NULL;
    }
    if(NULL != value)
    {
        delete[] value;
        value = NULL;
    }
    //PLOG(PL_INFO,"ManetGraphMLParser::Attribute::Set: before calling Init\n");
    if(!Init(theLookupvalue,theIndex,theValue))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Attribute::Set: Error calling init with new lookup value %s\n",theLookupvalue);
        return false;
    }
    //PLOG(PL_INFO,"ManetGraphMLParser::Attribute::Set: returning true %s\n",theLookupvalue);
    return true;
}
bool
ManetGraphMLParser::Attribute::Init(const char* theLookupvalue, const char* theIndex,const char* theValue)
{
    if((NULL!=lookupvalue) || (NULL != index) || (NULL != value))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Attribute::Init: Error attempting to Init Attribute which has already called Init!\n");
        return false;
    }
    lookupvalue = new char[strlen(theLookupvalue)+1];
    index = new char[strlen(theIndex)+1];
    value = new char[strlen(theValue)+1];
    if((NULL == lookupvalue) || (NULL == index) || (NULL == value))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Attribute::Init: Error allocating space for lookup/index/value strings\n");
        return false;
    }
    strcpy(lookupvalue,theLookupvalue);
    strcpy(value,theValue);
    strcpy(index,theIndex);
    return true;
}
ManetGraphMLParser::Attribute::~Attribute()
{
    delete[] lookupvalue;
    delete[] index;
    delete[] value;
}
bool ManetGraphMLParser::ReadXMLNode(xmlTextReader*   readerPtr, 
                                       NetGraph&        graph, 
                                       char*            parentXMLNodeID, 
                                       bool&            isDuplex)
{
    //const xmlChar *name, *value;
    //int count, depth, type, isempty;
    const xmlChar *name;
    int type;
    type = xmlTextReaderNodeType(readerPtr);
    if(XML_READER_TYPE_END_ELEMENT==type)
        return true;

    name = xmlTextReaderConstName(readerPtr);
    if (name == NULL)
      name = BAD_CAST "--";
    //value = xmlTextReaderConstValue(readerPtr);
    //depth = xmlTextReaderDepth(readerPtr);
    //isempty = xmlTextReaderIsEmptyElement(readerPtr); 
    //count = xmlTextReaderAttributeCount(readerPtr);
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
        xmlFree(graphId);
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
        xmlFree(graphEdgeType);
        //printf("exiting graph node id = \"%s\" is duplex is %s\n",graphId,isDuplex ? "true" : "false");
    }
    else if(!strcmp("node",(const char*)name))
    {
        xmlChar* nodeId = NULL;
        while (xmlTextReaderMoveToNextAttribute(readerPtr)>0)
        {
            // TRACE("   found attribute %s with name %s\n",xmlTextReaderName(readerPtr),xmlTextReaderConstValue(readerPtr));
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
        memset(parentXMLNodeID, 0, MAXXMLIDLENGTH+1);       
        strcpy(parentXMLNodeID,(const char*)nodeId);
        
        NetGraph::Interface* interface;
        ProtoAddress addr;
        addr.ConvertFromString((const char*)nodeId);
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
            AddNodeToGraph(graph,*node);//this is a hook to let derived graph classes know about added nodes
            xmlFree(nodeId);
        }
        else
        {
            //existing node so update anything we might have on it
        }
        //TRACE("   exiting node node\n");
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
            delete mycost;  //should we have a virtual override on this too? TBD
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
            delete mycost;  //should we have a virtual override on this too? TBD
            xmlFree(sourceName);
            xmlFree(targetName);
            //printf(" connected interface %s to %s\n",sourceName, targetName);
        }
        //printf("exiting edge node\n");
    }
    else if(!strcmp("key",(const char*)name))
    {
        xmlChar* oldkey = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"id");
        xmlChar* attr_type = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"attr.type");
        xmlChar* attr_name = xmlTextReaderGetAttribute(readerPtr,(xmlChar*)"attr.name");
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
                    PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): %s has an empty default value!\n",attr_name);
                    return false;
                }
                defaultvalue = childXMLNode->children->content;
            }
            childXMLNode = childXMLNode->next;
        }
        //printf("reading in attribute name=%s key=%s defaultvalue=%s\n",name,oldkey,defaultvalue);
        if(!AddAttributeKey((const char*)attr_name,(const char*)attr_type,(const char*)domain,(const char*)oldkey,(const char*)defaultvalue))
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::ReadXMLNode(): Error adding attribute (name=%s,oldkey=%s)\n",attr_name,oldkey);
            return false;
        }
        xmlFree(oldkey);
        xmlFree(attr_type);
        xmlFree(attr_name);
        xmlFree(domain);
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
            if(NULL == sourcePortName)
            {
                sourcePortName = sourceName;
            }
            if(NULL == targetPortName)
            {
                targetPortName = targetName;
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
            //printf("newIndex =%s\n",newIndex);
        }
        if(!xmlTextReaderRead(readerPtr))
            return false;
        newValue = xmlTextReaderValue(readerPtr);
        //printf("found data for \"%s\" with key %s with value %s\n",newlookup,oldIndex,newValue);
        
        Attribute* newAttribute = new Attribute();
        newAttribute->Init(newlookup,(const char*)newIndex,(const char*)newValue);
        //newAttribute->Init(newlookup,newIndex,(const char*)newValue);
        attributelist.Insert(*newAttribute);
        xmlFree(oldIndex);
        xmlFree(newValue);
    }
    else
    {
        //ignorning xml node
        //TRACE("   ignoring xml node %s\n", name);
    }
    //TRACE("exit ManetGraphMLParser::ReadXMLNode()\n");
    return true;

}  // end ManetGraphMLParser::ReadXMLNode()


bool ManetGraphMLParser::Read(const char* path, NetGraph& graph)
{
    // Iteratively read the file's XML tree and build up "graph"
    //xmlTextReader* readerPtr = xmlReaderForFile(path, NULL, 1);
    xmlTextReader* readerPtr = xmlReaderForFile(path, "", 0);
    if (NULL == readerPtr)
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Read() xmlReaderForFile(%s) error: %s\n",path, GetErrorString());
        return false;
    }
    
    bool isDuplex = true;
    char parentXMLNodeID[MAXXMLIDLENGTH+1];
    memset(parentXMLNodeID, 0, MAXXMLIDLENGTH+1);       
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
    //xmlCleanupParser();  //this doesn't do what it should!!! it only frees up global state memory and should only be called upon destruction!
    return (0 == result);
}  // end ManetGraphMLParser::Read()


bool ManetGraphMLParser::Write(NetGraph& graph, const char* path, char* buffer, unsigned int* len_ptr)
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
    if(!WriteLocalAttributes(writerPtr))
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write::testXmlWriterDoc: Error at writing graph attributes in header\n");
        return false;
    }
    
    /* We are done with the header so now we go through the actual graph and add each node and edge */
    /* We are adding each node */
    returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "graph");
    if (returnvalue < 0) 
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error starting XML graph element\n"); 
        return false;
    }
    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "id",BAD_CAST XMLName);
    if (returnvalue < 0) 
    {
        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error setting XML graph attribute id\n"); 
        return false;
    }
    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "edgedefault",BAD_CAST "directed");
    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error setting graph attribute directed\n"); return false;}

    NetGraph::InterfaceIterator it(graph);
    NetGraph::Interface* iface;
    //adding the nodes to the xml
    while (NULL != (iface = it.GetNextInterface()))
    {
        //check to see if this is a default "node" interface
        //if(!iface->IsPort())
        if (iface == iface->GetNode().GetDefaultInterface())
        {
            //Node& node = static_cast<Node&>(iface->GetNode());
            returnvalue = xmlTextWriterStartElement(writerPtr, BAD_CAST "node");
            if (returnvalue < 0) 
            {
               PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding XML node\n"); 
               return false;
            }
            if(iface->GetAddress().IsValid())
            {
                returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "id", BAD_CAST iface->GetAddress().GetHostString());
                //printf("writing node %s\n",iface->GetAddress().GetHostString());
            }
            else 
            {
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
                    if (returnvalue < 0) 
                    {
                        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding XML node\n"); 
                        return false;
                    }
                    if(portIface->GetAddress().IsValid())
                    {
                        returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "name", BAD_CAST portIface->GetAddress().GetHostString());
                        //printf("writing interface %s\n",iface->GetAddress().GetHostString());
                    } 
                    else 
                    {
                        returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "name", BAD_CAST portIface->GetName());
                        //printf("writing node %s\n",iface->GetName());
                    } 
                    if (returnvalue < 0) 
                    {
                         PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error adding setting node id\n"); 
                         return false;
                    }
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
                    if (returnvalue < 0) 
                    {
                        PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error ending node element\n"); 
                        return false;
                    }
                }
            }
            //close up the node node element
            returnvalue = xmlTextWriterEndElement(writerPtr);
            if (returnvalue < 0) 
            {
                PLOG(PL_ERROR,"ManetGraphMLParser::Write: Error ending node element\n"); 
                return false;
            }
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
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding edge\n"); return false;
                }
                if(iface->GetAddress().IsValid()) 
                {
                    //printf("writing connection %s ->",iface->GetAddress().GetHostString());
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST iface->GetAddress().GetHostString());
                }else 
                { 
                    //printf("writing connection %s ->",iface->GetName());
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST iface->GetName());
                }
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); 
                    return false;
                }
                if(nbrIface->GetAddress().IsValid()) {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrIface->GetAddress().GetHostString());
                    ////printf("%s\n",nbrIface->GetAddress().GetHostString());
                } 
                else 
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrIface->GetName());
                    ////printf("%s\n",nbrIface->GetName());
                }
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); 
                    return false;
                }
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
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error ending node element\n"); 
                    return false;
                }
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
                if (returnvalue < 0) 
                { 
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding edge\n"); 
                    return false;
                }
                if(nodeIface->GetAddress().IsValid()) 
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST nodeIface->GetAddress().GetHostString());
                } 
                else 
                { 
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "source", BAD_CAST nodeIface->GetName());
                }
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); 
                    return false;
                }
                if(nbrNodeIface->GetAddress().IsValid()) 
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrNodeIface->GetAddress().GetHostString());
                } 
                else 
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "target", BAD_CAST nbrNodeIface->GetName());
                }
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); 
                    return false;
                }
                if(iface->GetAddress().IsValid()) 
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "sourceport", BAD_CAST iface->GetAddress().GetHostString());
                } 
                else 
                { 
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "sourceport", BAD_CAST iface->GetName());
                }
                if (returnvalue < 0)
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); 
                    return false;
                }
                if(nbrIface->GetAddress().IsValid())
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "targetport", BAD_CAST nbrIface->GetAddress().GetHostString());
                }
                else 
                {
                    returnvalue = xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "targetport", BAD_CAST nbrIface->GetName());
                }
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error adding setting source attribute\n"); 
                    return false;
                }

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
                if (returnvalue < 0) 
                {
                    PLOG(PL_ERROR,"ManetGraphMLParser::Write Error ending node element\n"); 
                    return false;
                }
            } 
        }
    }
 
    returnvalue = xmlTextWriterEndDocument(writerPtr);
    if (returnvalue < 0) { PLOG(PL_ERROR,"ManetGraphMLParser::Write:testXmlwriterPtrDoc: Error at xmlTextWriterEndDocument\n"); return false;}

    xmlFreeTextWriter(writerPtr);

    if(NULL != path)
    {
        xmlSaveFormatFileEnc(path, docPtr, MY_GRAPHML_ENCODING,1);
    }
    if(NULL != buffer && NULL != len_ptr)
    {
        //xmlOutputBuffer xmlbuff;
        //xmlSaveFormatFileTo(&xmlbuff,docPtr,MY_GRAPHML_ENCODING,1);
        //if (xmlbuff.written > (int)*len_ptr){
        //    DMSG(0,"bunny in buffer section\n");
        
        xmlChar* tempout;
        int size;
        xmlDocDumpFormatMemoryEnc(docPtr,&tempout,&size,MY_GRAPHML_ENCODING,1);
        if(size > (int)*len_ptr)
        {
            PLOG(PL_ERROR,"ManetGraphMLParser::Write: size of document %d is larger than the allocated space of %d\n",size, (int)*len_ptr);
            return false;
        }
        *len_ptr = size;
        memcpy(buffer,tempout,size);
        xmlFree(tempout);
        xmlCleanupParser();
    }
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
        } 
        else 
        {
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

bool ManetGraphMLParser::WriteLocalKeys(xmlTextWriter* writerPtr)
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


bool  ManetGraphMLParser::WriteLocalNodeAttributes(xmlTextWriter* writerPtr,NetGraph::Node& theNode)
{
    PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalNodeAttributes: Enter\n");
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
            //PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalNodeAttributes():mykey=\"%s\",lookup=\"%s\",key=\"%s\",value=\"%s\"\n",key,attr->GetLookup(),attr->GetIndex(),attr->GetValue());
            attr = NULL;
            //attr = it.GetNextItem();
        } 
        else 
        {
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

bool  ManetGraphMLParser::WriteLocalAttributes(xmlTextWriter* writerPtr)
{
    PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalAttributes: Enter\n");
    bool rv = true;
    char key[255];//this should be dynamic or checks added TBD
    sprintf(key,"thisGraph");
    AttributeList::Iterator it(attributelist,false,key,strlen(key)*8);
    //AttributeList::Iterator it(attributelist);
    
    Attribute* attr(NULL);
    
    //iterate over items which have the matching keys
    attr = it.GetNextItem();
    while(NULL != attr)
    {
        if(strcmp(attr->GetLookup(),key))
        {
            //PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalAttributes():mykey=\"%s\",lookup=\"%s\",key=\"%s\",value=\"%s\"\n",key,attr->GetLookup(),attr->GetIndex(),attr->GetValue());
            attr = NULL;
            //attr = it.GetNextItem();
        } 
        else 
        {
            rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST attr->GetIndex());
            rv += xmlTextWriterWriteString(writerPtr, BAD_CAST attr->GetValue());
            rv += xmlTextWriterEndElement(writerPtr);
            PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalAttributes():key=\"%s\",value=\"%s\"\n",attr->GetIndex(),attr->GetValue());
            attr = it.GetNextItem();
        }
    }
    return rv;
}

bool ManetGraphMLParser::WriteLocalInterfaceAttributes(xmlTextWriter* writerPtr,NetGraph::Interface& theInterface)
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
        } 
        else 
        {
            rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST attr->GetIndex());
            rv += xmlTextWriterWriteString(writerPtr, BAD_CAST attr->GetValue());
            rv += xmlTextWriterEndElement(writerPtr);
            attr = it.GetNextItem();
        }
    }
    return rv;
}

bool ManetGraphMLParser::WriteLocalLinkAttributes(xmlTextWriter* writerPtr,NetGraph::Link& theLink)
{
    PLOG(PL_DETAIL,"ManetGraphMLParser::WriteLocalLinkAttributes()\n");
    bool rv = true;
    char key[255];//this should be dynamic or checkes added TBD
    sprintf(key,"edge:source:%s",GetString(theLink.GetSrc()->GetNode()));
    sprintf(key,"%s:%s",key,GetString(*theLink.GetSrc()));
    sprintf(key,"%s:dest:%s",key,GetString(theLink.GetDst()->GetNode()));
    sprintf(key,"%s:%s",key,GetString(*theLink.GetDst()));
    
    AttributeList::Iterator it(attributelist,false,key,strlen(key)*8);
    Attribute* attr(NULL);
    attr = it.GetNextItem();
    while(NULL != attr)
    {
        if(strcmp(attr->GetLookup(),key))
        {
            attr = NULL;
        } 
        else 
        {
            rv += xmlTextWriterStartElement(writerPtr, BAD_CAST "data");
            rv += xmlTextWriterWriteAttribute(writerPtr, BAD_CAST "key",BAD_CAST attr->GetIndex());
            rv += xmlTextWriterWriteString(writerPtr, BAD_CAST attr->GetValue());
            rv += xmlTextWriterEndElement(writerPtr);
            attr = it.GetNextItem();
        }
    }
    return rv;
}
