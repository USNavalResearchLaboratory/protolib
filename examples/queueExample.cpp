// This example illustrates some use of the ProtoQueue classes
// that enable an "Item" to a member of multiple queues
// (lists, trees, etc) simultaneously _and_ be aware which
// queues in which it is included.  The ProtoQueue::Item
// also gracefully removes itself from all the queues it
// is included when it is deleted.  The item can be indexed
// or sorted in different ways in each queue as well.
// The queue classes can be subclassed and customized to
// provide different sorting behavior on different
// item attributes.

// We use a "ProductRecord" example class that has some
// different attributes (name, model number, cost) to 
// illustrate the use of the ProtoQueue classes.


#include "protoQueue.h"

#include <stdlib.h> // for rand()
#include <arpa/inet.h> // for htonl()


class ProductRecord : public ProtoQueue::Item
{
    public:
        ProductRecord(const char*  theName, 
                      unsigned int theModel,
                      double       theCost);
        
        const char* GetName() const
            {return name;}
        unsigned int GetModel() const
            {return model;}
        double GetCost() const
            {return cost;}
        
        enum {PRODUCT_NAME_MAX = 31};
        
        // These are used by our various queues to
        // access the attributes used for indexing
        const unsigned int* GetModelPtr() const
            {return &model;}
        const double* GetCostPtr() const
            {return &cost;}
        
    private:
        char            name[PRODUCT_NAME_MAX+1];
        unsigned int    model;
        double          cost;
        
};  // end class ProductRecord

ProductRecord::ProductRecord(const char*  theName, 
                             unsigned int theModel,
                             double       theCost)
 : model(theModel), cost(theCost)
{
    name[PRODUCT_NAME_MAX] = '\0';
    strncpy(name, theName, PRODUCT_NAME_MAX);
}

// I) The first here is a simple linked list of the products ...
class ProductList : public ProtoSimpleQueueTemplate<ProductRecord> {};

// II) The second is a queue indexed by product "name"
// NOTES:
// 1) A "ProtoIndexedQueue" derivative can only contain values
//    with unique keys/indexes (product "name" in this case)
//
// 2) The override of the "GetKey()" and "GetKeysize()" determine how
//    items in the "ProtoIndexedQueue" are indexed. (Here we set
//    the queue up to index by Product "name").
//
// 3) Note the "keysize" is in units of bits (_not_ bytes)
//
class ProductNameQueue : public ProtoIndexedQueueTemplate<ProductRecord>
{
    public:
        ProductNameQueue(bool usePool = false)
         : ProtoIndexedQueueTemplate<ProductRecord>(usePool) {}
        
        const char* GetKey(const Item& item) const
            {return static_cast<const ProductRecord&>(item).GetName();}
        // returns length of name in bits
        unsigned int GetKeysize(const Item& item) const
            {return (8*strlen(static_cast<const ProductRecord&>(item).GetName()));}
        
};  // end class ProductNameQueue

// III) The third is a queue sorted by product "model" number
// NOTES:
// 1) A "sorted queue" can have multiple entries with the same key
class ProductModelQueue : public ProtoSortedQueueTemplate<ProductRecord>
{
    public:
        ProductModelQueue(bool usePool = false)
         : ProtoSortedQueueTemplate<ProductRecord>(usePool) {}
        
        const char* GetKey(const Item& item) const
            {return ((const char*)static_cast<const ProductRecord&>(item).GetModelPtr());}
        // returns length of name in bits
        unsigned int GetKeysize(const Item& item) const
            {return (sizeof(unsigned int) << 3);}
        // For sorting "unsigned int", the following sorting criteria are used
        // native machine endian, useSignBit = false, useComplement2 = true
        // (the latter two are default for ProtoSortedQueue, so we just override one
        ProtoTree::Endian GetEndian() const
            {return ProtoTree::GetNativeEndian();}
        
};  // end class ProductModelQueue

// IV) The fourth is a queue sorted by product "cost"
// NOTES
// 1) Here we sort by "cost", a "double" floating point value, and we
//    set up the sorting accordingly.
class ProductCostQueue : public ProtoSortedQueueTemplate<ProductRecord>
{
    public:
        ProductCostQueue(bool usePool = false)
         : ProtoSortedQueueTemplate<ProductRecord>(usePool) {}
        
        const char* GetKey(const Item& item) const
            {return ((const char*)static_cast<const ProductRecord&>(item).GetCostPtr());}
        // returns length of name in bits
        unsigned int GetKeysize(const Item& item) const
            {return (sizeof(double) << 3);}
        // For sorting "double", the following sorting criteria are used
        // native machine endian, useSignBit = true, useComplement2 = false
        ProtoTree::Endian GetEndian() const
            {return ProtoTree::GetNativeEndian();}
        bool GetUseSignBit() const
            {return true;}
        bool GetUseComplement2() const
            {return false;}
        
};  // end class ProductCostQueue



class TestItem  : public ProtoTree::Item
{
    public:
        TestItem(UINT32 theValue) 
            : value(htonl(theValue)) {}
        ~TestItem() {}
        
        UINT32 GetValue() const
            {return ntohl(value);}
        
        const char* GetKey() const
            {return ((const char*)&value);}
        unsigned int GetKeysize() const
            {return (sizeof(UINT32) << 3);}
        
    private:
        UINT32 value;
};  // end class TestItem
    
class TestTree : public ProtoTreeTemplate<TestItem> {};

int main(int argc, char* argv[])
{
    
    //struct timeval currentTime;
    //ProtoSystemTime(currentTime);
    //srand(currentTime.tv_usec);
    
    
    TestTree testTree;
    
    int i = 0;
    for (i = i; i <= 20; i++)
    {
        TestItem* item = new TestItem((UINT32)rand());
        //TestItem* item = new TestItem((UINT32)i);
        if (!testTree.Insert(*item))
        {
            delete item;
            i--;
        }
    }
    
    TestItem* savedItem = NULL;
    TestItem* item;
    TestItem* pitem;
    TestTree::Iterator it(testTree, true);
    
    for (i = 1; i <= 10; i++)
    {
        pitem = it.PeekPrevItem();
        item = it.GetPrevItem();
        TRACE("item %d value %lu (pitem value %lu)\n", i, item->GetValue(), pitem->GetValue());
    }
    savedItem = item;
    for (i = 9; i >= 5; i--)
    {
        pitem = it.PeekNextItem();
        item = it.GetNextItem();
        TRACE("item %d value %lu (pitem value %lu)\n", i, item->GetValue(), pitem->GetValue());
    }
    for (i = 6; i <= 20; i++)
    {
        pitem = it.PeekPrevItem();
        item = it.GetPrevItem();
        TRACE("item %d value %lu (pitem value %lu)\n", i, item->GetValue(), pitem->GetValue());
    }
    
    
    TRACE("setting cursor to %lu\n", savedItem->GetValue());
    it.SetCursor(*savedItem);
            
    TestItem* prev = static_cast<TestItem*>(it.prev);
    TestItem* next = static_cast<TestItem*>(it.next);
    TestItem* curr = static_cast<TestItem*>(it.curr_hop);
    TRACE("iterator prev:%lu next:%lu curr:%lu\n",
            prev ? prev->GetValue() : 0, 
            next ? next->GetValue() : 0,
            curr ? curr->GetValue() : 0);
    
    
    TRACE("removing item %lu ...\n", savedItem->GetValue());
    testTree.Remove(*savedItem);
            
    prev = static_cast<TestItem*>(it.prev);
    next = static_cast<TestItem*>(it.next);
    curr = static_cast<TestItem*>(it.curr_hop);
    TRACE("iterator prev:%lu next:%lu curr:%lu\n",
            prev ? prev->GetValue() : 0, 
            next ? next->GetValue() : 0,
            curr ? curr->GetValue() : 0);
    
    
    /*
    it.GetNextItem();
    ProtoTree::Item* snext = it.prev;
    TRACE("\ngot one item\n");
    prev = static_cast<TestItem*>(it.prev);
    next = static_cast<TestItem*>(it.next);
    curr = static_cast<TestItem*>(it.curr_hop);
    TRACE("iterator prev:%lu next:%lu curr:%lu\n",
            prev ? prev->GetValue() : 0, 
            next ? next->GetValue() : 0,
            curr ? curr->GetValue() : 0);
    
    
    it.GetNextItem();
    TRACE("\ngot another item\n");
    prev = static_cast<TestItem*>(it.prev);
    next = static_cast<TestItem*>(it.next);
    curr = static_cast<TestItem*>(it.curr_hop);
    TRACE("iterator prev:%lu next:%lu curr:%lu\n",
            prev ? prev->GetValue() : 0, 
            next ? next->GetValue() : 0,
            curr ? curr->GetValue() : 0);
    
    //
    //it.prev = sprev;
    //it.next = snext;
    //it.reversed = true;
    //it.GetPrevItem();
    //it.reversed = true;
    it.GetPrevItem();
    
    //item = it.GetNextItem();
    
    //TRACE("next item = %lu\n", item->GetValue());
    TRACE("\nready to go ...\n");
    prev = static_cast<TestItem*>(it.prev);
    next = static_cast<TestItem*>(it.next);
    curr = static_cast<TestItem*>(it.curr_hop);
    TRACE("iterator prev:%lu next:%lu curr:%lu\n",
            prev ? prev->GetValue() : 0, 
            next ? next->GetValue() : 0,
            curr ? curr->GetValue() : 0);
    */
    for (i = 1; i <= 20; i++)
    {
        pitem = it.PeekPrevItem();
        item = it.GetPrevItem();
        if ((NULL == pitem) || (NULL == item)) break;
        TRACE("item %d value %lu (pitem value %lu)\n", i, item->GetValue(), pitem->GetValue());
    }
    TRACE("\n");
    
    i = 0;
    TestTree::SimpleIterator sit(testTree);
    while (NULL != (item = sit.GetNextItem()))
    {
        i++;
        //TRACE("stem %d value %lu\n", i, item->GetValue());
    }
    
    
    //return 0;
    
    
    // Instantiate our various queues
    ProductList productList;
    ProductNameQueue nameList;
    ProductModelQueue modelList;
    ProductCostQueue costList;
    
    TRACE("ProtoList::Item  size = %d bytes\n", sizeof(ProtoList::Item));
    TRACE("ProtoTree::Item  size = %d bytes\n", sizeof(ProtoTree::Item));
    TRACE("ProductList container size = %d bytes\n", sizeof(ProductList::Container));
    TRACE("ProductNameQueue container size = %d bytes\n", sizeof(ProductNameQueue::Container));
    TRACE("ProductCostQueue container size = %d bytes\n", sizeof(ProductCostQueue::Container));
    
    // Randomly generate some products
    for (int i = 0; i < 10 ; i++)
    {
        char name[9];
        for (int j = 0; j < 8; j++)
            name[j] = (rand() % 26) + 'a';
        name[8] = '\0';
        unsigned int model = (unsigned int)rand();
        double cost = 100.0 * ((double)rand()/(double) RAND_MAX);
        
        TRACE("creating product name:\"%s\" model:%u cost:%lf\n", name, model, cost);
        
        ProductRecord* product = new ProductRecord(name, model, cost);
        productList.Append(*product);
        nameList.Insert(*product);
        modelList.Insert(*product);
        ProductModelQueue::Iterator moderator(modelList);
        costList.Insert(*product);
    }
    
    ProductRecord* product;
    
    // Test iteration manipulation stuff
    ProductList::Iterator iterator(productList);
    product = productList.RemoveHead();
    if (NULL != product)
        TRACE("Removed head name:\"%s\" model:%u cost:%lf\n", 
                  product->GetName(), product->GetModel(), product->GetCost());
    while (NULL != (product = iterator.GetNextItem()))
    {
        TRACE("   name:\"%s\" model:%u cost:%lf\n", 
              product->GetName(), product->GetModel(), product->GetCost()); 
    } 
    product = productList.RemoveTail();
    if (NULL != product)
        TRACE("Removed tail name:\"%s\" model:%u cost:%lf\n", 
                  product->GetName(), product->GetModel(), product->GetCost()); 
    while (NULL != (product = iterator.GetPrevItem()))
    {
        TRACE("   name:\"%s\" model:%u cost:%lf\n", 
              product->GetName(), product->GetModel(), product->GetCost()); 
    } 
    //while (NULL != (product = iterator.GetNextItem()))
    //    delete product;
    
    //return 0;
    
    TRACE("\nProduct List Iteration:\n");   
    ProductList::Iterator listerator(productList);
    while (NULL != (product = listerator.GetNextItem()))
    {
        TRACE("   name:\"%s\" model:%u cost:%lf\n", 
              product->GetName(), product->GetModel(), product->GetCost()); 
    }  
    
    //nameList.Destroy();
    
    TRACE("\nProduct Name Iteration:\n");   
    ProductNameQueue::Iterator namerator(nameList);
    while (NULL != (product = namerator.GetNextItem()))
    {
        TRACE("   name:\"%s\" model:%u cost:%lf\n", 
              product->GetName(), product->GetModel(), product->GetCost()); 
    }  
    
    TRACE("\nProduct Model Iteration:\n");   
    ProductModelQueue::Iterator moderator(modelList);
    while (NULL != (product = moderator.GetNextItem()))
    {
        TRACE("   name:\"%s\" model:%u cost:%lf\n", 
              product->GetName(), product->GetModel(), product->GetCost()); 
    }
    
    TRACE("\nProduct Cost Iteration:\n");   
    ProductCostQueue::Iterator costerator(costList);
    while (NULL != (product = costerator.GetNextItem()))
    {
        TRACE("   name:\"%s\" model:%u cost:%lf\n", 
              product->GetName(), product->GetModel(), product->GetCost()); 
    }  
    
}  // end main()
