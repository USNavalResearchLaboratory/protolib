#include "protoXml.h"

#include <protoDebug.h>


bool ProtoXml::GetPropAsDouble(xmlNodePtr nodePtr, const char* attrName, double& value)
{
    xmlChar* attrValue = xmlGetProp(nodePtr, (xmlChar*)attrName);
    if (NULL == attrValue) 
    {
        PLOG(PL_WARN, "ProtoXml::GetPropAsDouble() error: no attr found!\n");
        return false;
    }
    bool result = true;
    if (1 != sscanf((const char*)attrValue, "%lf", &value))
    {
        PLOG(PL_WARN, "ProtoXml::GetPropAsDouble() error: invalid attr format!\n");
        result = false;
    }
    xmlFree(attrValue);
    return result;
}  // end ProtoXml::GetPropAsDouble()


bool ProtoXml::GetPropAsInt(xmlNodePtr nodePtr, const char* attrName, int& value)
{
    xmlChar* attrValue = xmlGetProp(nodePtr, (xmlChar*)attrName);
    if (NULL == attrValue) 
    {
        PLOG(PL_WARN, "ProtoXml::GetPropAsInt() error: no attr found!\n");
        return false;
    }
    bool result = true;
    if (1 != sscanf((const char*)attrValue, "%d", &value))
    {
        PLOG(PL_WARN, "ProtoXml::GetPropAsInt() error: invalid attr format!\n");
        result = false;
    }
    xmlFree(attrValue);
    return result;
}  // end ProtoXml::GetPropAsInt()

ProtoXml::IterFilterBase::Filter::Filter(const char* filterPath)
{
    filter_path[FILTER_PATH_MAX] = '\0';
    SetPath(filterPath);
}

void ProtoXml::IterFilterBase::Filter::SetPath(const char* filterPath)
{
    if (NULL != filterPath)
        strncpy(filter_path, filterPath, FILTER_PATH_MAX);
    else
        filter_path[0] = '\0';
}  // end ProtoXML::IterFilterBase::Filter::SetPath()

ProtoXml::IterFilterBase::IterFilterBase(const char* filterPath)
 : path_filter(filterPath)
{
    path_current[FILTER_PATH_MAX] = '\0';
    Reset();
}

void ProtoXml::IterFilterBase::SetFilter(const char* filterPath)
{
    path_filter_list.Destroy();
    path_filter.SetPath(filterPath);
}  // end ProtoXml::IterFilterBase::SetFilter()

bool ProtoXml::IterFilterBase::AddFilter(const char* filterPath)
{
    if (path_filter.IsSet())
    {
        Filter* filter = new Filter(filterPath);
        if (NULL == filter)
        {
            PLOG(PL_ERROR, "ProtoXml::IterFilterBase::AddFilter() new Filter error: %s\n", GetErrorString());
            return false;
        }
        path_filter_list.Insert(*filter);
    }
    else
    {
        path_filter.SetPath(filterPath);
    }
    return true;
}  // end ProtoXml::IterFilterBase::SetFilter()

void ProtoXml::IterFilterBase::Reset()
{
    path_current[0] = '\0';
    path_current_len = 0;
    path_depth = 0;
}  // end ProtoXml::IterFilterBase::Reset()

bool ProtoXml::IterFilterBase::UpdateCurrentPath(int nodeDepth, const char* nodeName)
{
    if (nodeDepth == path_depth)
    {
        // Replace "path_current" tail with "/nodeName"
        char* ptr = strrchr(path_current, '/');
        if (NULL == ptr) ptr = path_current;
        size_t tailLen = strlen(ptr);
        size_t nameLen = strlen(nodeName);
        if ((path_current_len - tailLen + 1 + nameLen) > FILTER_PATH_MAX)
        {
            PLOG(PL_WARN, "ProtoXml::IterFilterBase::UpdatePath() error: XML path name exceeds filter maximum\n");
            return false;
        }
        *ptr++ = '/';
        strcpy(ptr, nodeName);
        path_current_len += nameLen + 1 - tailLen;
    }
    else if (nodeDepth > path_depth)
    {
        ASSERT(1 == (nodeDepth - path_depth));
        // Append path_current with '/' + "nodeName"
        size_t nameLen = strlen((const char*)nodeName);
        if ((path_current_len + 1 + nameLen) > FILTER_PATH_MAX)
        {
            PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() error: XML path name exceeds filter maximum\n");
            return false;
        }
        //ASSERT(0 != path_current_len);
        char* ptr = path_current + path_current_len;
        *ptr++ = '/';
        strcpy(ptr, (char*)nodeName);
        path_current_len += (1 + nameLen);
        path_depth++;
    }
    else // if (nodeDepth < path_depth)
    {
        //TRACE("path_depth:%d nodeDepth:%d\n", path_depth, nodeDepth);
        char* ptr = path_current;
        for (int i = 0; i <= nodeDepth; i++)
        {
            ptr = strchr(ptr, '/');
            ASSERT(NULL != ptr);
            ptr++;
        }
        size_t headLen = ptr - path_current;
        size_t nameLen = strlen((const char*)nodeName);
        if ((headLen + nameLen) > FILTER_PATH_MAX)
        {
            PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() error: XML path name exceeds filter maximum\n");
            return false;
        }
        strcpy(ptr, nodeName);
        path_current_len = headLen + nameLen;
        path_depth = nodeDepth;
    }
    //TRACE("path_current = %s (depth:%d)\n", path_current, path_depth);
    return true;
}  // end ProtoXml::IterFilterBase::UpdateCurrentPath()

bool ProtoXml::IterFilterBase::IsMatch()
{
    // Does "path_current" match any of our path filters
    if (!path_filter.IsSet())
        return true;
    else if (0 == strcmp(path_filter.GetPath(), path_current))
        return true;
    else if (NULL != path_filter_list.FindString(path_current))
        return true;
    else
        return false;
}  // end ProtoXml::IterFilterBase::IsMatch()

ProtoXml::IterParser::IterParser(const char* filterPath)
 : IterFilterBase(filterPath), reader_ptr(NULL), prev_node(NULL)
{
}

ProtoXml::IterParser::~IterParser()
{
    Close();
}

bool ProtoXml::IterParser::Open(const char* fileName, const char* filterPath)
{
    Close();  // just in case already open
    if (NULL == (reader_ptr = xmlNewTextReaderFilename(fileName)))
    {
        PLOG(PL_ERROR, "ProtoXml::IterParser::Open() xmlNewTextReaderFilename() error: %s\n",
                       GetErrorString());
        return false;
    }
    if (NULL != filterPath)
        IterFilterBase::SetFilter(filterPath);
    return true;
}  // end ProtoXml::IterParser::Open()

void ProtoXml::IterParser::Close()
{
    if (NULL != reader_ptr)
    {
        xmlFreeTextReader(reader_ptr);
        reader_ptr = NULL;
        prev_node = NULL;
        IterFilterBase::Reset();
    }
}  // end ProtoXml::IterParser::Close()

xmlNodePtr ProtoXml::IterParser::GetNext()
{
    //TRACE("enter ProtoXml::IterParser::GetNext() prev_node:%s\n", prev_node ? (const char*)prev_node->name : "(null)");
    int result;
    // If we didn't just return a "prev_node", then read the
    // very next sequential node w/ xmlTextReaderRead(), else 
    // skip entire "prev_node" sub-tree w/ xmlTextReaderNext()
    if (NULL == prev_node)
        result = xmlTextReaderRead(reader_ptr);
    else
        result = xmlTextReaderNext(reader_ptr);
    prev_node = NULL;
    switch (result)
    {
       case 0:
            return NULL;
       case -1:
            PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() xmlTextReaderRead error!\n");
            return NULL;
        default:
            break;
    }
    do
    {
        int nodeType = xmlTextReaderNodeType(reader_ptr);
        if (XML_READER_TYPE_ELEMENT != nodeType)
            continue;
        const char* nodeName = (const char*)xmlTextReaderConstLocalName(reader_ptr);
        if (NULL == nodeName)
        {
            PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() xmlTextReaderConstLocalName() error\n");
            return NULL;
        }
        int nodeDepth =  xmlTextReaderDepth(reader_ptr);
        if (nodeDepth < 0)
        {
            PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() xmlTextReaderDepth() error\n");
            return NULL;
        }
        
        if (!IterFilterBase::UpdateCurrentPath(nodeDepth, nodeName))
        {
            PLOG(PL_WARN, "ProtoXml::IterParser::GetNext() error: unable to update current path\n");
            continue;
        }
        if (!IterFilterBase::IsMatch())
            continue;  // no match, so continue
        prev_node = xmlTextReaderExpand(reader_ptr);
        if (NULL == prev_node)
            PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() xmlTextReaderExpand() error!\n");
        return prev_node;    
           
    } while ((result = xmlTextReaderRead(reader_ptr)) > 0);
    if (result < 0)
        PLOG(PL_ERROR, "ProtoXml::IterParser::GetNext() xmlTextReaderRead() error!\n");
    return NULL;
}  // end ProtoXml::IterParser::GetNext()


ProtoXml::IterFinder::IterFinder(xmlNodePtr rootElem, const char* filterPath)
 : IterFilterBase(filterPath), root_elem(rootElem), prev_elem(rootElem), iter_depth(0)
{
}

ProtoXml::IterFinder::~IterFinder()
{
}

void ProtoXml::IterFinder::Reset(const char* filterPath)
{
    prev_elem = root_elem;
    if (NULL != filterPath)
        SetFilter(filterPath);
    IterFilterBase::Reset();
}  // end ProtoXml::IterFinder::Reset()

// Depth-first traversal of sub-tree (below root_elem)
xmlNodePtr ProtoXml::IterFinder::GetNext()
{
    //TRACE("enter ProtoXml::IterFinder::GetNext() prev_elem:%s\n", prev_elem ? (const char*)prev_elem->name : "(null)");
    if(NULL == prev_elem)
    {
        return NULL;
    }
    else if (prev_elem != root_elem)
    {
        // We need to skip the subtree of the prev_elem
        // This involves moving right or up and right
        xmlNodePtr nextElem = prev_elem->next;
        while (NULL == nextElem)
        {
            prev_elem = prev_elem->parent; // move up
            iter_depth--;
            if (prev_elem != root_elem)
                nextElem = prev_elem->next; // move right
            else
                break; // we're done (returned to root_elem)
        }
        prev_elem = nextElem;
        // Check for match before descending
        if (NULL != nextElem)
        {
            if (!UpdateCurrentPath(iter_depth, (const char*)nextElem->name))
                PLOG(PL_WARN, "ProtoXml::IterFinder::GetNext() error: unable to update current path\n");
            else if (IsMatch()) 
                return nextElem; // found match, done for now
        }
    }    
    // Descend down or across tree
    while (NULL != prev_elem) 
    {
        // First, try to move down or the right of the tree
        xmlNodePtr nextElem = prev_elem->xmlChildrenNode;
        if (NULL == nextElem)
        {
            if (prev_elem != root_elem)
            {
                nextElem = prev_elem->next;  // no children, try next sibling
            }
            else
            {
                prev_elem = NULL; // done, root_elem had no children
                return NULL;
            }
        }
        else if (prev_elem != root_elem)
        {
            iter_depth++;
        }
        // Second, if needed, move back up the tree and to the right
        while (NULL == nextElem)
        {
            // No child or sibling, so move up a level 
            // and right to parent's next sibling
            prev_elem = prev_elem->parent;   // move up
            iter_depth--;
            if (prev_elem != root_elem)
                nextElem = prev_elem->next;  // move right
            else 
                break; // we're done (returned to root_elem)
        }
        prev_elem = nextElem;
        if (NULL != nextElem)
        {
            if (!UpdateCurrentPath(iter_depth, (const char*)nextElem->name))
            {
                
                PLOG(PL_WARN, "ProtoXml::IterFinder::GetNext() error: unable to update current path\n");
                continue;
            }
            if (IsMatch()) break; // found match, done for now
        }
    } 
    return prev_elem;
}  // end ProtoXml::IterFinder::GetNext()
