/**
* @file protoSpace.cpp
* 
* @brief This maintains a set of "Nodes" in n-dimensional Euclidean space.
*/
// Uncomment this to have SDT display bounding box iteration
//#define USE_SDT

#include "protoDebug.h"

#include "protoSpace.h"
#include <math.h>  // for "fabs()" 
#include <stdio.h> // for "printf()"
ProtoSpace::Node::Node()
{
}

ProtoSpace::Node::~Node()
{
}

ProtoSpace::Ordinate::Ordinate()
{
    memset(key, 0, sizeof(double)+sizeof(Node*));
}

ProtoSpace::Ordinate::~Ordinate()
{
}

ProtoSpace::ProtoSpace()
 : num_dimensions(0), ord_tree(NULL), node_count(0)
{
}

ProtoSpace::~ProtoSpace()
{
    if (NULL != ord_tree)
    {
        for (unsigned int i = 0; i < num_dimensions; i++)
            ord_tree[i].EmptyToPool(ord_pool);
        delete[] ord_tree;
        ord_tree = NULL;
    }
    ord_pool.Destroy();
}

void ProtoSpace::Empty()
{
    if (NULL != ord_tree)
    {
        for (unsigned int i = 0; i < num_dimensions; i++)
            ord_tree[i].EmptyToPool(ord_pool);
    }
}  // end ProtoSpace::Empty()


bool ProtoSpace::InsertNode(Node& node)
{
    if (0 == num_dimensions)
    {
        unsigned int numDimensions = node.GetDimensions();
        if (NULL == (ord_tree = new ProtoSortedTree[numDimensions]))
        {
            PLOG(PL_ERROR, "ProtoSpace::InsertNode() error: unable to allocate Ordinate tree array\n");
            return false;
        }
        num_dimensions = numDimensions;
    }
    else if (node.GetDimensions() != num_dimensions)
    {
        // (TBD) Allow space to be re-scoped if empty???
        PLOG(PL_ERROR, "ProtoSpace::InsertNode() error: Node dimensions does not match space!\n");
        return false;
    }
    
    // Get and Insert "Ordinate" entries for the node for each dimension
    for (unsigned int i = 0; i < num_dimensions; i++)
    {
        Ordinate* ord = static_cast<Ordinate*>(ord_pool.Get());
        if ((NULL == ord) && (NULL == (ord = new Ordinate)))
        {
            PLOG(PL_ERROR, "ProtoSpace::InsertNode() error: unable to allocate Ordinate\n");
            RemoveNode(node);
            return false;
        }
        ord->SetValue(node.GetOrdinate(i));
        ord->SetNode(&node);
        ord_tree[i].Insert(*ord);
    }
    node_count++;
    return true;
}  // end ProtoSpace::InsertNode();


bool ProtoSpace::RemoveNode(Node& node)
{
    if (0 == num_dimensions) return false;
    ASSERT(node.GetDimensions() == num_dimensions);
    bool result = false;
    for (unsigned int i = 0; i < num_dimensions; i++)
    {
        Ordinate tempOrd;
        tempOrd.SetNode(&node);
        tempOrd.SetValue(node.GetOrdinate(i));
        Ordinate* ord = static_cast<Ordinate*>(ord_tree[i].Find(tempOrd.GetKey(), tempOrd.GetKeysize()));
        if (NULL != ord)
        {
            ord_tree[i].Remove(*ord);
            ReturnOrdinateToPool(*ord);
            result = true;
        }
        else
        {
            //PLOG(PL_WARN, "ProtoSpace::RemoveNode() warning: no ordinate[%u] for node\n", i);
            continue;
        }
    }
    if (result) node_count--;
    return result;
}  // end ProtoSpace::RemoveNode()

bool ProtoSpace::ContainsNode(Node& node)
{
    if (0 == num_dimensions) return false;
    ASSERT(node.GetDimensions() == num_dimensions);
    bool result = true;
    for (unsigned int i = 0; i < num_dimensions; i++)
    {
        Ordinate tempOrd;
        tempOrd.SetNode(&node);
        tempOrd.SetValue(node.GetOrdinate(i));
        Ordinate* ord = static_cast<Ordinate*>(ord_tree[i].Find(tempOrd.GetKey(), tempOrd.GetKeysize()));
        if (NULL == ord)
        {
            //PLOG(PL_WARN, "ProtoSpace::RemoveNode() warning: no ordinate[%u] for node\n", i);
            return false;
        }
    }
    return result;
}  // end ProtoSpace::ContainsNode()


ProtoSpace::Iterator::Iterator(ProtoSpace& theSpace)
 : space(theSpace), orig(NULL), bbox_radius(0.0), x_factor(0.0),
   pos_it(NULL), neg_it(NULL)
{
}

ProtoSpace::Iterator::~Iterator()
{
    Destroy();
}

bool ProtoSpace::Iterator::Init(const double* originOrdinates)
{
    Destroy();
    // Allocate and set origin ordinates for iteration
    unsigned int dim = space.GetDimensions();
    if (NULL == (orig = new double[dim]))
    {
        PLOG(PL_ERROR, "ProtoSpace::Iterator::Init() error: unable to allocate 'orig' array: %s\n",
                       GetErrorString());
        return false;
    }
    if (NULL != originOrdinates)
        memcpy(orig, originOrdinates, dim*sizeof(double));
    else
        memset(orig, 0, dim*sizeof(double));
    
    // Allocate and init positive ordinate iterators
    if (NULL == (pos_it = new ProtoSortedTree::Iterator*[dim]))
    {
        PLOG(PL_ERROR, "ProtoSpace::Iterator::Init() error: unable to allocate 'pos_it' array: %s\n",
                       GetErrorString());
        Destroy();
        return false;
    }
    memset(pos_it, 0, dim*sizeof(ProtoSortedTree::Iterator*));
    Ordinate tempOrd;
    for (unsigned int i = 0; i < dim; i++)
    {
        tempOrd.SetValue(orig[i]);
        if (NULL == (pos_it[i] = new ProtoSortedTree::Iterator(space.ord_tree[i], 
                                                               false, 
                                                               tempOrd.GetKey(),
                                                               Ordinate::KEYBITS)))
        {
            Destroy();
            return false;
        }
    }
    
    // Allocate and init negative ordinate iterators
    if (NULL == (neg_it = new ProtoSortedTree::Iterator*[dim]))
    {
        PLOG(PL_ERROR, "ProtoSpace::Iterator::Init() error: unable to allocate 'pos_it' array: %s\n",
                       GetErrorString());
        Destroy();
        return false;
    }
    memset(neg_it, 0, dim*sizeof(ProtoSortedTree::Iterator*));
    for (unsigned int i = 0; i < dim; i++)
    {
        tempOrd.SetValue(orig[i]);
        if (NULL == (neg_it[i] = new ProtoSortedTree::Iterator(space.ord_tree[i], 
                                                               false, 
                                                               tempOrd.GetKey(),
                                                               Ordinate::KEYBITS)))
        {
            Destroy();
            return false;
        }
        neg_it[i]->Reverse();
    }

#ifdef USE_SDT
    // Create bounding box for SDT visualization
    printf("node ul position %f,%f label off\n", originOrdinates[0], originOrdinates[1]);
    printf("node ur position %f,%f label off\n", originOrdinates[0], originOrdinates[1]);
    printf("node ll position %f,%f label off\n", originOrdinates[0], originOrdinates[1]);
    printf("node lr position %f,%f label off\n", originOrdinates[0], originOrdinates[1]);
    printf("link ul,ur,blue,2\n");
    printf("link ul,ll,blue,2\n");
    printf("link ur,lr,blue,2\n");
    printf("link ll,lr,blue,2\n");
#endif // USE_SDT
        
    bbox_radius = 0.0;
    x_factor = sqrt((double)dim);
    
    return true;
}  // end ProtoSpace::Iterator::Init()

void ProtoSpace::Iterator::Destroy()
{
    // empty our queue
    Ordinate* nextOrd;
    while (NULL != (nextOrd = static_cast<Ordinate*>(ord_tree.RemoveHead())))
        space.ReturnOrdinateToPool(*nextOrd);
    if (NULL != orig)
    {
        delete[] orig;
        orig = NULL;
    }
    unsigned int dim = space.GetDimensions();
    if (NULL != pos_it)
    {
        for (unsigned int i = 0; i < dim; i++)
        {
            if (NULL != pos_it[i]) delete pos_it[i];
        }
        delete[] pos_it;
        pos_it = NULL;
    }
    if (NULL != neg_it)
    {
        for (unsigned int i = 0; i < dim; i++)
        {
            if (NULL != neg_it[i]) delete neg_it[i];
        }
        delete[] neg_it;
        neg_it = NULL;
    }
    if (NULL != orig)
    {
        delete[] orig;
        orig = NULL;
    }    
}  // end ProtoSpace::Iterator::Destroy()


void ProtoSpace::Iterator::Reset(const double* originOrdinates)
{
    unsigned int dim = space.GetDimensions();
    ASSERT(NULL != orig);
    if (NULL != originOrdinates)
        memcpy(orig, originOrdinates, dim*sizeof(double));
    Ordinate tempOrd;
    for (unsigned int i = 0; i < dim; i++)
    {
        tempOrd.SetValue(orig[i]);
        pos_it[i]->Reset(false, tempOrd.GetKey(), Ordinate::KEYBITS);   
        neg_it[i]->Reset(false, tempOrd.GetKey(), Ordinate::KEYBITS);   
        neg_it[i]->Reverse(); 
    }
    // empty our queue
    Ordinate* nextOrd;
    while (NULL != (nextOrd = static_cast<Ordinate*>(ord_tree.RemoveHead())))
        space.ReturnOrdinateToPool(*nextOrd);
}  // end ProtoSpace::Iterator::Reset()

/**
 * Return the Node of those discovered by our expanding
 * bounding box that meets these criteria
 *    1) Closest location to <orig_x, orig_y>, _and_
 *    2) Falls within the current bounding box
 */  
ProtoSpace::Node* ProtoSpace::Iterator::GetNextNode(double* distance)
{
    unsigned int dim = space.GetDimensions();
    
    while(1)
    {
        Ordinate* nextOrd = static_cast<Ordinate*>(ord_tree.GetHead());
        if (NULL != nextOrd)
        {
            Node* nextNode = nextOrd->GetNode();
            ASSERT(NULL != nextNode);
            bool inBox = true;
            if (bbox_radius >= 0.0)
            {
            
                // Is the "nextNode" within the bbox*x_factor
                // (Note "x_factor = radius * sqrt(num_dimensions))
                double xRadius = bbox_radius / x_factor;
                for (unsigned int i = 0; i < dim; i++)
                {
                    if ((nextNode->GetOrdinate(i) <= (orig[i] - xRadius)) ||
                        (nextNode->GetOrdinate(i) >= (orig[i] + xRadius)))
                    {
                        inBox = false;
                        break;
                    }
                }
            }
            if (inBox)
            {
                ord_tree.RemoveHead();
                if (NULL != distance) 
                {
                    *distance = sqrt(nextOrd->GetValue());
                    //TRACE("ProtoSpace returning node01x distance %lf\n", *distance);
                }
                space.ReturnOrdinateToPool(*nextOrd);
                return nextNode;
            }
            
        }
        else if (bbox_radius < 0.0)
        {
            return NULL;  // finished
        }
        
        
        // A) Find the ordinate with smallest delta from origin
        //    to set "radius" of current bounding space
        double deltaMin = -1.0;
        unsigned int index = 0;  // which dimensional iterator was closest
        bool neg = false;        // Was it "pos_it" or "neg_it"
        for (unsigned int direction = 0; direction <= 1; direction++)
        {
            for (unsigned int i = 0 ; i < dim; i++)
            {
                Ordinate* ord = (0 == direction) ?
                                    static_cast<Ordinate*>(neg_it[i]->PeekPrevItem()) :
                                    static_cast<Ordinate*>(pos_it[i]->PeekNextItem());
                if (NULL != ord)
                {
                    Node* node = ord->GetNode();
                    ASSERT(NULL != node);
                    double delta = fabs(ord->GetValue() - orig[i]);
                    if ((delta < deltaMin) || (deltaMin < 0.0))
                    {
                        deltaMin = delta;
                        index = i;
                        neg = (0 == direction);
                    }
                }
            }
        }
        bbox_radius = deltaMin;
        if (deltaMin < 0.0) continue; 
        
        // Output new bounding box for SDT visualization
#ifdef USE_SDT
        printf("node ul position %f,%f\n", orig[0] - deltaMin, orig[1] - deltaMin);
        printf("node ur position %f,%f\n", orig[0] + deltaMin, orig[1] - deltaMin);
        printf("node ll position %f,%f\n", orig[0] - deltaMin, orig[1] + deltaMin);
        printf("node lr position %f,%f\n", orig[0] + deltaMin, orig[1] + deltaMin);
#endif // USE_SDT
                
        Node* node = neg ? static_cast<Ordinate*>(neg_it[index]->PeekPrevItem())->GetNode() :
                           static_cast<Ordinate*>(pos_it[index]->PeekNextItem())->GetNode();

        // B) Is the closest node on this axis within our current bounding box?
        bool inBox = true;
        for (unsigned int i = 0; i < dim; i++)
        {
            double ordDelta = fabs(orig[i] - node->GetOrdinate(i));
            if (ordDelta > deltaMin)
            {
                inBox = false;
                break;
            }
        }
        
        // C) Enqueue any nodes that lie _within_ the current bounding space
        if (inBox)
        {
            // a) calculate distance
            double distPartial = 0.0;
            for (unsigned j = 0; j < dim; j++)
            {
                double delta = node->GetOrdinate(j) - orig[j];
                distPartial += delta*delta;
            }
            
           
                     
            // b) get and init Ordinate 
            Ordinate* ord = space.GetOrdinateFromPool();
            if ((NULL == ord) && (NULL == (ord = new Ordinate)))
            {
                PLOG(PL_ERROR, "ProtoSpace::Iterator::GetNextNode() error: unable to allocate Ordinate: %s\n",
                        GetErrorString());
                return NULL;
            }
            ord->SetNode(node);
            ord->SetValue(distPartial);
            
            // c) if not already enqueued, enqueue it
            if (NULL == ord_tree.Find(ord->GetKey(), Ordinate::KEYBITS))
                ord_tree.Insert(*ord);
            else
                space.ReturnOrdinateToPool(*ord);
        }
        
        // D) Consume the applicable ordinate, expanding our bounding box
        if (neg)
            neg_it[index]->GetPrevItem();
        else
            pos_it[index]->GetNextItem();

#ifdef USE_SDT        
        printf("wait 1\n");
#endif // USE_SDT
                
    }  // end while(1)
  
}  // end ProtoSpace::Iterator::GetNextNode()


