#ifndef _PROTO_SPACE
#define _PROTO_SPACE


#include "protoTree.h"
#include <sys/types.h>  // for BYTE_ORDER macro

/**
 * @class ProtoSpace
 *
 * @brief For now, this maintains a set of "Nodes" in n-dimensional
 *  space.  Note that the "space" is destroyed, the Nodes themselves
 *  are not destroyed.
 * (TBD - support n-dimensions)
 */
class ProtoSpace
{
    public:
        ProtoSpace();
        ~ProtoSpace();    
        
        unsigned int GetDimensions() const
            {return num_dimensions;}
            
        /**
         * @class Node
         *
         * @brief This base class let's us associate ordinate position 
         * information with any class that derives from it.
         */
        class Node
        {
            public:
                virtual ~Node();
            
                virtual unsigned int GetDimensions() const = 0;
                virtual double GetOrdinate(unsigned int dim) const = 0;
                
            protected:
                Node();
                
        };  // end class ProtoSpace::Node
    
        bool InsertNode(Node& node);
        bool RemoveNode(Node& node); // returns false if Node not in space
        bool ContainsNode(Node& node); // returns false if Node not in space
        void Empty();
        
        unsigned int GetNodeCount() const
            {return node_count;}

        class Ordinate : public ProtoSortedTree::Item
        {
            public:
                Ordinate();
                ~Ordinate();
                
                const char* GetKey() const 
                        {return key;}
                enum {KEYBYTES = (sizeof(double)+sizeof(Node*))};
                enum {KEYBITS = 8*KEYBYTES};
                
                // ProtoSortedTree::Item overrides
                unsigned int GetKeysize() const
                    {return KEYBITS;}
                bool UseSignBit() const
                    {return true;}
                bool UseComplement2() const
                    {return false;}
                
#if BYTE_ORDER == LITTLE_ENDIAN  
                ProtoTree::Endian GetEndian() const
                    {return ProtoTree::ENDIAN_LITTLE;}
                void SetNode(Node* theNode)
                    {memcpy(key, &theNode, sizeof(Node*));}
                Node* GetNode() const
                {
                    Node* node;
                    memcpy(&node, key, sizeof(Node*));
                    return node;
                }
                void SetValue(double theValue)
                    {memcpy(key+sizeof(Node*), &theValue, sizeof(double));}
                double GetValue() const
                {
                    double value;
                    memcpy(&value, key+sizeof(Node*), sizeof(double));
                    return value;
                }
#else               
                ProtoTree::Endian GetEndian() const
                    {return ProtoTree::ENDIAN_BIG;}
                void SetNode(Node* theNode)
                    {memcpy(key+sizeof(double), &theNode, sizeof(Node*));}
                Node* GetNode() const
                {
                    Node* node;
                    memcpy(&node, key+sizeof(double), sizeof(Node*));
                    return node;
                }
                void SetValue(double theValue)
                    {memcpy(key, &theValue, sizeof(double));}
                double GetValue() const
                {
                    double value;
                    memcpy(&value, key, sizeof(double));
                    return value;
                }
#endif  // end if/else (BYTE_ORDER == LITTLE_ENDIAN)                
                
            private:
                char   key[sizeof(double)+sizeof(Node*)];
        };  // end class ProtoSpace::Ordinate
        
        
        /**
         * @class Iterator
         *
         * @brief This class starts at a origin point in the "space" and 
         * iterates through the nodes from the closest node(s)
         * to the farthest
         */
        class Iterator
        {
            public:
                Iterator(ProtoSpace& theSpace);
                ~Iterator();
                
                bool Init(const double* originOrdinates = NULL);
                void Destroy();
                
                void Reset(const double* originOrdinates = NULL);
                
                Node* GetNextNode(double* distance = NULL);
                
            private:
                ProtoSpace& space;
                
                // This array contains the ordinates 
                // of the "origin"
                double*                      orig;
                double                       bbox_radius;
                double                       x_factor;
                // The iterations define an expanding
                // bounding n-dimensional sub-space.
                ProtoSortedTree::Iterator**  pos_it;
                ProtoSortedTree::Iterator**  neg_it;
                ProtoSortedTree              ord_tree;
                
        };  // end class Iterator()
        friend class Iterator;
        
            
    private:
        Ordinate* GetOrdinateFromPool() 
            {return static_cast<Ordinate*>(ord_pool.Get());}   
        void ReturnOrdinateToPool(Ordinate& ord)
            {ord_pool.Put(ord);} 
            
        unsigned int              num_dimensions;    
        ProtoSortedTree*          ord_tree;
        ProtoSortedTree::ItemPool ord_pool;
        unsigned                  node_count;
            
};  // end class ProtoSpace


#endif // _PROTO_SPACE
