#include <stdio.h>
#include <protoTree.h>
#include <protoQueue.h>


class IndexedItem : public ProtoSortedTree::Item
{
    public:
        IndexedItem(const char* const ptr) : string(ptr) {}
    
        const char* GetKey() const
            {return string;}
        unsigned int GetKeysize() const
            {return strlen(string) * 8;}
        
        //ProtoTree::Endian GetEndian() const {return ProtoTree::GetNativeEndian();}
        
        const char* const string;
};
    
class Index : public ProtoSortedTreeTemplate<IndexedItem>
{
    
    const char* GetKey(const Item& item) const
        {return static_cast<const IndexedItem&>(item).GetKey();}
    unsigned int GetKeysize(const Item& item) const
        {return static_cast<const IndexedItem&>(item).GetKeysize();}
    
    
    //ProtoTree::Endian GetEndian() const {return ProtoTree::GetNativeEndian();}
};


const char* const strings[] = 
{
    "adamson, brian",
    "adamson, nancy",
    "adamson, connor",
    "batson, amy",
    "batson, randy",
    "adams, steve",
    NULL  
};

int main(int argc, char* argv[])
{
    
    Index index;
    
    const char* const*  ptr = strings;
    
    int i = 0;
    IndexedItem* itemx;
    
    while (NULL != *ptr)
    {
        IndexedItem* item = new IndexedItem(*ptr++);
        index.Insert(*item);
        if (++i == 1) itemx = item;
    }
        
    
    Index::Iterator iterator(index, true);//, itemx);//->GetKey(), item15->GetKeysize());
    
    //index.Remove(*itemx);
    
    iterator.SetCursor(itemx);
    
    //iterator.Reset(true, itemx->string, 8); 
    
    //iterator.Reset();//, item15->GetKey(), (sizeof(unsigned int) << 3) - 0);
    IndexedItem* item;
    while (NULL != (item = iterator.GetNextItem()))
    {
        printf("got item %s\n", item->string);
        //index.Remove(*item);
    }
            
}
