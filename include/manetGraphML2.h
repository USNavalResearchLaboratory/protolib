#ifndef _MANET_GRAPHML
#define _MANET_GRAPHML
#define MAXXMLIDLENGTH 255

#include <manetGraph.h>
#include <protoQueue.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>

#define MY_GRAPHML_ENCODING "UTF-8"

// NOTES
// Basic GraphML parsing to instantiate a ProtoGraph NetGraph is supported
// 
// Current Limitations:
// 1) The NetGraph::Node could be extended here to have a list of child graphs to allow support of 
//    nested graphs per the GraphML specifications, but that has not yet been implemented.
// 2) Handling of "default" attributes can be made more efficient.  Currently, graph elements are
//    given Attribute instances for those in the key that have default values specified.  This could
//    be made more efficient by having the "GetAttribute()" and AttributeIterator code search the
//    attribute key list for any defaults rather than each element having its own instances of default 
//    attributes
// 3) Node ports are supported but nested ports and nested graphs are not yet supported


class ManetGraphMLParser
{
    public:
        virtual ~ManetGraphMLParser();
    
        // TBD - should this public attribute interface stuff be moved to a base ManetGraphML class that the Parser inherits?
        enum AttributeType {INVALID = 0, BOOL, INT, LONG, FLOAT, DOUBLE, STRING};
        enum AttributeDomain {NONE = 0, GRAPH, NODE, EDGE, ALL};
        
        static AttributeType GetAttributeType(const char* text);
        static AttributeDomain GetAttributeDomain(const char* text);
        
        class Attribute
        {
            public:
                const char* GetName() const = 0;
                const char* GetValue() const = 0;
        };  // end class ManetGraphMLParser::Attribute
        
        // Attribute set / get methods
        bool SetGraphAttribute(const char* name, const char* value);
        bool SetNodeAttribute(const NetGraph::Node& node, const char* name, AttributeType type, const char* value);
        bool SetInterfaceAttribute(const NetGraph::Interface& iface, const char* name, AttributeType type, const char* value);
        bool SetLinkAttribute(const NetGraph::Link& link, const char* name, AttributeType type, const char* value);
        
        const Attribute* GetGraphAttribute(const char* name);
        const Attribute* GetNodeAttribute(const NetGraph::Node& node, const char* name);
        const Attribute* GetInterfaceAttribute(const NetGraph::Interface& iface, const char* name);
        const Attribute* GetLinkAttribute(const NetGraph::Link& link, const char* name);
        
        // TBD - GraphAttributeIterator, NodeAttributeIterator, InterfaceAttributeIterator, and LinkAttributeIterator
        
        bool Read(const char* path, NetGraph& graph);   // load graph from GraphML file
        
        bool Write(NetGraph& graph, const char* path);  // make GraphML file from graph
        
        
    protected:           
        class AttributeKey : public ProtoQueue::Item
        {
            public:
                AttributeKey();
                bool Init(const char* name, const char* id, AttributeType type, AttributeDomain domain);
                bool Init(const char* name, const char* id, const char* type, const char* domain);
                ~AttributeKey();
                
                bool SetDefaultValue(const char* value);
                const char& GetDefaultValue() const
                    {return attr_default;}
                
                const char* GetName() const
                    {return attr_name;}
                const char* GetId() const
                    {return ((NULL != attr_id) ? attr_id : attr_name);}
                AttributeType GetType() const
                    {return attr_type;}
                AttributeDomain GetDomain() const
                    {return attr_domain;}
                
            private:
                char*   attr_name;
                char*   attr_id;  // if NULL, then id == name
                Type    attr_type
                Domain  attr_domain;
                char*   attr_default;
                
        };  // end class ManetGraphMLParser::AttributeKey     
        
        class AttributeNameList : public ProtoIndexedQueue<AttributeKey>
        {
            private:
                const char* GetKey(const ProtoQueue::Item& item) const
                    {return static_cast<const AttributeKey&>(item).GetName();}
                unsigned int GetKeysize(const ProtoQueue::Item& item) const
                    {return 8*strlen(static_cast<const AttributeKey&>(item).GetName());}
        };  // end class ManetGraphMLParser::AttributeNameList
                
        class AttributeIdList : public ProtoIndexedQueue<AttributeKey>
        {
            private:
                const char* GetKey(const ProtoQueue::Item& item) const
                    {return static_cast<const AttributeKey&>(item).GetId();}
                unsigned int GetKeysize(const ProtoQueue::Item& item) const
                    {return 8*strlen(static_cast<const AttributeKey&>(item).GetId());}
                    
        };  // end class ManetGraphMLParser::AttributeNameList
        
              
        class GenericAttribute : public Attribute, public ProtoTree::Item
        {
            public:
                virtual ~GenericAttribute();
            
                const char* GetName() const
                    {return attr_key.GetName();}
                
                const char* GetValue() const
                    {return ((NULL != attr_value) ? attr_value : attr_key.GetDefaultValue());}
            
            protected:
                friend class ManetGraphMLParser;
                GenericAttribute(AttributeKey& key);
                
                const char* GetId() const
                    {return attr_key.GetId();}
            
                bool SetValue(const char* value);
                virtual ~Attribute();
                
                // ProtoTree::Item required overrides
                const char* GetKey() const
                    {return attr_key.GetId();}   
                unsigned int GetKeysize() const
                    {return (strlen(attr_key.GetId() << 3);}
                    
            private:
                AttributeKey&   attr_key;
                char*           attr_value;        
              
        };  // end class ManetGraphMLParser::Attribute
            
        ManetGraphMLParser();    
    
        // Get existing or create new attribute key 
        AttributeKey* GetAttributeKey(const char* name, AttributeType type, AttributeDomain domain);  
    
        class GenericAttributeList : public ProtoTreeTemplate<GenericAttribute) {};
    
        // These custom Attribute subclasses allow us to organize things for rapid access
        // (The ItemAttribute class here is a base class for the others)
        class ItemAttributeBase : public GenericAttribute
        {
            public:
                virtual ~ItemAttributeBase();
            
            protected: 
                ItemAttributeBase(AttributeKey& key);
                
                bool SetItemKey(const char* itemPtr, unsigned int itemPtrLen);
              
                const char* GetKey() const
                    {return item_key;}
                virtual unsigned int GetKeysize() const = 0;
                
            private
                char*   item_key;  // concatenation of graph item pointer and attribute id
        };  // end class ManetGraphMLParser::ItemAttributeBase
        
        template <class ITEM_TYPE>
        class ItemAttribute : public ItemAttributeBase
        {
            public:
                virtual ~ItemAttribute();
                bool SetItem(const ITEM_TYPE& item)
                {
                    const ITEM_PTR* itemPtr = &item;
                    return SetItemKey(&itemPtr, sizeof(ITEM_TYPE*));
                }
                
            protected:
                ItemAttribute(AttributeKey& key);
                
                unsigned int GetKeysize() const
                    {return ((sizeof(ITEM_TYPE*) + strlen(attr_key.GetId())) << 3);}
            
        };  // end class ManetGraphMLParser::ItemAttribute
        
        
        // Helper method to find graph item attributes from a given attribute list (ProtoTree)
        Attribute* FindItemAttribute(ProtoTree& attrList, const char* itemPtr, unsigned int itemPtrLen, AttributeKey& attrKey);
        
        class NodeAttribute : public ItemAttribute<NetGraph::Node>
            {public: NodeAttribute(AttributeKey& key) : ItemAttribute(key) {}};
        class InterfaceAttribute : public ItemAttribute<NetGraph::Interface> 
            {public: InterfaceAttribute(AttributeKey& key) : ItemAttribute(key) {}};
        class LinkAttribute : public ItemAttribute<NetGraph::Link> 
            {public: LinkAttribute(AttributeKey& key) : ItemAttribute(key) {}};
        class NodeAttributeList : public ProtoTreeTemplate<NodeAttribute> {};
        class InterfaceAttributeList : public ProtoTreeTemplate<InterfaceAttribute> {};
        class LinkAttributeList : public ProtoTreeTemplate<LinkAttribute> {};
        
        
        /////// OLD STUFF ////
        bool SetGraphID(const char* name);

        ManetGraphMLParser();

    private:
        // Attribute management members
        AttributeNameList        attr_name_list;
        AttributeIdList         attr_id_list;
        AttributeList           graph_attr_list;
        NodeAttributeList       node_attr_list;
        InterfaceAttributeList  iface_attr_list;
        LinkAttributeList       link_attr_list;
        
        char *XMLName;

        const char* FindAttributeIndex(const char* theName);
        AttributeKey* FindAttributeKey(const char* theName);
        
        
//        virtual bool WriteKeys(xmlTextWriter* writerPtr, NetGraph& graph) = 0;
        virtual bool UpdateKeys(NetGraph& graph) = 0; //this will replace the above as all keys will be stored locally
        bool WriteLocalKeys(xmlTextWriter* writerPtr);

        virtual NetGraph::Node* CreateNode() = 0;
//        virtual bool WriteNodeAttributes(xmlTextWriter* writerPtr,NetGraph::Node& theNode) = 0;
        virtual bool UpdateNodeAttributes(NetGraph::Node& theNode) = 0; //this will replace the above so keys will be stored locally
        bool WriteLocalNodeAttributes(xmlTextWriter* writerPtr,NetGraph::Node& theNode);

        virtual NetGraph::Interface* CreateInterface(NetGraph::Node& node) = 0;
        virtual NetGraph::Interface* CreateInterface(NetGraph::Node& node,ProtoAddress& addr) = 0;
        virtual bool InsertInterface(NetGraph::Interface& theIface) = 0;
        virtual bool AddInterfaceToNode(NetGraph::Node& theNode,NetGraph::Interface& theIface,bool makeDefault = false) = 0;
        virtual bool AddInterfaceToGraph(NetGraph& theGraph,NetGraph::Interface& theIface) = 0;
        //virtual bool InsertInterface(NetGraph::Interface& theIface) = 0;
  //      virtual bool WriteInterfaceAttributes(xmlTextWriter* writerPtr,NetGraph::Interface& theInterface) = 0;
        virtual bool UpdateInterfaceAttributes(NetGraph::Interface& theInterface) = 0; //this will replace the above so keys will be stored locally
        bool WriteLocalInterfaceAttributes(xmlTextWriter* writerPtr,NetGraph::Interface& theInterface);

        virtual NetGraph::Cost* CreateCost(double value) = 0;

        virtual bool Connect(NetGraph::Interface& iface1, NetGraph::Interface& iface2, NetGraph::Cost& cost, bool isDuplex) = 0;
//        virtual bool WriteLinkAttributes(xmlTextWriter* writerPtr,NetGraph::Link& theLink) = 0;
        virtual bool UpdateLinkAttributes(NetGraph::Link& theLink) = 0; //this will replace the above so keys will be stored locally
        bool WriteLocalLinkAttributes(xmlTextWriter* writerPtr,NetGraph::Link& theLInk); 
      
        bool ReadXMLNode(xmlTextReader*   readerPtr, 
                         NetGraph&        graph, 
                         char*            parentXMLNodeID, 
                         bool&            isDuplex);
};  // end class ManetGraphMLParser

template <class COST_TYPE = ManetGraph::Cost, class IFACE_TYPE = ManetGraph::Interface, class LINK_TYPE = ManetGraph::Link,class NODE_TYPE = ManetGraph::Node>
class ManetGraphMLTemplate : public ManetGraphMLParser, public NetGraphTemplate<COST_TYPE, IFACE_TYPE, LINK_TYPE, NODE_TYPE>
{
    public:
        ManetGraphMLTemplate() {}
        virtual ~ManetGraphMLTemplate() {}
                
        bool Read(const char* path)   // load graph from GraphML file
            {return ManetGraphMLParser::Read(path, *this);}
            
        bool Write(const char* path)  // make GraphML file from graph
            {return ManetGraphMLParser::Write(*this, path);}
        bool Connect(NetGraph::Interface& iface1,NetGraph::Interface& iface2,NetGraph::Cost& theCost,bool isDuplex)
            {return NetGraph::Connect(iface1,iface2,theCost,isDuplex);}
        virtual bool InsertInterface(NetGraph::Interface& theIface)
            {return NetGraph::InsertInterface(theIface);}
    protected:
  //      virtual bool WriteKeys(xmlTextWriter* writerPtr, NetGraph& theGraph)
  //          {return true;}
        virtual bool UpdateKeys(NetGraph& theGraph)
            {return true;}
  /*      virtual bool WriteNodeAttributes(xmlTextWriter* writerPtr, NetGraph::Node& theNode)
            {return true;}
        virtual bool WriteInterfaceAttributes(xmlTextWriter* writerPtr, NetGraph::Interface& theInterface)
            {return true;}
        virtual bool WriteLinkAttributes(xmlTextWriter* writerPtr, NetGraph::Link& theLInk)
            {return true;}*/
        virtual bool UpdateNodeAttributes(NetGraph::Node& theNode)
            {return true;}
        virtual bool UpdateInterfaceAttributes(NetGraph::Interface& theInterface)
            {return true;}
        virtual bool UpdateLinkAttributes(NetGraph::Link& theLInk)
            {return true;}
    private:
        class NetGraph::Node* CreateNode()
            {return static_cast<NetGraph::Node*>(new NODE_TYPE());}
        class NetGraph::Interface* CreateInterface(NetGraph::Node& node)
            {return static_cast<NetGraph::Interface*>(new IFACE_TYPE(static_cast<NODE_TYPE&>(node)));}
        class NetGraph::Interface* CreateInterface(NetGraph::Node& node,ProtoAddress& addr)
            {return static_cast<NetGraph::Interface*>(new IFACE_TYPE(static_cast<NODE_TYPE&>(node),addr));}
        class NetGraph::Cost* CreateCost(double value)
            {return static_cast<NetGraph::Cost*>(new COST_TYPE(value));}
        class NetGraph::Cost* CreateCost()
            {return static_cast<NetGraph::Cost*>(new COST_TYPE());}
        virtual bool AddInterfaceToNode(NetGraph::Node& theNode,NetGraph::Interface& theIface,bool makeDefault)
            {return (static_cast<NODE_TYPE&>(theNode)).AddInterface(theIface,makeDefault);}
        virtual bool AddInterfaceToGraph(NetGraph& theGraph,NetGraph::Interface& theIface)
            {return (static_cast<ManetGraphMLTemplate&>(theGraph)).InsertInterface(theIface);}
};  // end class ManetGraphMLParser    


//class ManetGraphML : public ManetGraphMLTemplate<> {};

#endif // _MANET_GRAPHML
