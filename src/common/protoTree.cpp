
/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson of the Naval Research Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/
 /**
* @file protoTree.cpp
* 
* @brief This is a general purpose prefix-based C++ Patricia tree. 
* The code also provides an ability to iterate over items with a 
* common prefix of arbitrary bit length
*/
#include "protoTree.h"
#include "protoDebug.h"  // for PLOG()

#include <string.h>
#include <stdlib.h>  // for labs()

ProtoTree::Item::Item()
 : bit(0), parent((Item*)NULL), left((Item*)NULL), right((Item*)NULL)
{      
}

ProtoTree::Item::~Item()
{  
}

unsigned int ProtoTree::Item::GetDepth() const 
{
    unsigned int depth = 0;
    const Item* p = this;
    while (NULL != (p = p->parent)) depth++;
    return depth;
}  // end ProtoTree::Item::GetDepth()

ProtoTree::ItemPool::ItemPool()
 : head(NULL)
{
}

ProtoTree::ItemPool::~ItemPool()
{
    Destroy();
}

void ProtoTree::ItemPool::Destroy()
{
    Item* item;
    while ((item = Get())) delete item;
}  // end ProtoTree::ItemPool::Destroy()

ProtoTree::Item* ProtoTree::ItemPool::Get()
{
    Item* item = head;
    if (NULL != item) head = item->GetPoolNext();
    return item;
}  // end ProtoTree::ItemPool::Get()

void ProtoTree::ItemPool::Put(Item& item)
{
    item.SetPoolNext(head);
    head = &item;
}  // end ProtoTree::ItemPool::Put()

ProtoTree::ProtoTree()
 : root((Item*)NULL)
{
}

ProtoTree::~ProtoTree()
{
}

void ProtoTree::Empty()
{
    root = (ProtoTree::Item*)NULL;
    UpdateIterators(NULL, Iterator::EMPTY);
}  // end ProtoTree::Empty()

void ProtoTree::Destroy()
{
    while (NULL != root)
    {
        Item* item = root;
        Remove(*item);
        delete item;   
    }
}  // end ProtoTree::Destroy()

bool ProtoTree::PrefixIsEqual(const char*  key, 
                              unsigned int keysize,
                              const char*  prefix, 
                              unsigned int prefixSize,
                              Endian       keyEndian)
{
    if (prefixSize > keysize) return false;
    unsigned int fullByteCount = (prefixSize >> 3);
    unsigned int remBitCount = prefixSize & 0x07;
    if (ENDIAN_BIG == keyEndian)
        
    {
        // Compare any "remainder bits" of the "prefix" to the 
        // corresponding bits of the "key"
        // (we do this first to possibly avoid call to "memcmp()" below)
        if (0 != remBitCount)
        {
            char remBitMask = (unsigned char)0xff << (8 - remBitCount);
            // "remainder bits" are in last byte of big endian "prefix"
            if ((key[fullByteCount] & remBitMask) != (prefix[fullByteCount] & remBitMask))
                return false;
        }
    }
    else
    {
        // Adjust "key" ptr to point at its prefix portion
        key += (keysize >> 3);
        if (0 != (keysize &0x07)) key++;
        key -= fullByteCount;
        // Compare any "remainder bits" of the "prefix" to the 
        // corresponding bits of the "key"
        // (we do this first to possibly avoid call to "memcmp()" below)
        if (0 != remBitCount)
        {
            char remBitMask = 0xff << (8 - remBitCount);
            // "remainder bits" are in first byte of little endian "prefix"
            if ((key[0] & remBitMask) != (prefix[0] & remBitMask))
                return false;
        
            // Compare any full byte portion of the key / prefix
            if (0 != fullByteCount)
                return (0 == memcmp(key+1, prefix+1, fullByteCount));
            else
                return true;
        }
    }
    // Compare any full byte portion of the "prefix" 
    // to the corresponding "key" bytes        
    if (0 != fullByteCount)
        return (0 == memcmp(key, prefix, fullByteCount));
    else
        return true;
}  // end ProtoTree::PrefixIsEqual()

bool ProtoTree::KeysAreEqual(const char*  key1, 
                             const char*  key2, 
                             unsigned int keysize,
                             Endian       keyEndian)
{
    unsigned int fullByteCount = keysize >> 3;
    unsigned int remBitCount = keysize & 0x07;
    if (0 != remBitCount)
    {
        // Compare any "remainder bits" of the keys
        // (we do this first to possibly avoid call to "memcmp()" below)
        char remBitMask = 0xff << (8 - remBitCount);
        if (ENDIAN_BIG == keyEndian)
        {
            // "remainder bits" are in last byte of big endian keys
            if ((key1[fullByteCount] & remBitMask) != (key2[fullByteCount] & remBitMask))
                return false;
        }
        else
        {
            // "remainder bits" are in first byte of little endian keys
            if ((key1[0] & remBitMask) != (key2[0] & remBitMask))
                return false;
            // Compare any full byte portion of the keys
            if (0 != fullByteCount)
                return (0 == memcmp(key1+1, key2+1, fullByteCount));
            else
                return true;
        }
    }
    // Compare any full bytes of the keys
    if (0 != fullByteCount)
        return (0 == memcmp(key1, key2, fullByteCount));
    else
        return true;
}  // end ProtoTree::KeysAreEqual()

bool ProtoTree::ItemsAreEqual(const Item& item1, const Item& item2)
{
    unsigned int keysize = item1.GetKeysize();
    if (item2.GetKeysize() != keysize) return false;
    Endian keyEndian = item1.GetEndian();
    if (keyEndian != item2.GetEndian()) 
    {
        PLOG(PL_WARN, "ProtoTree::ItemsAreEqual() mis-matched key endian?!\n");
        ASSERT(0);
        return false;
    }
    return KeysAreEqual(item1.GetKey(), item2.GetKey(), keysize, keyEndian);   
}  // end ProtoTree::ItemsAreEqual()

bool ProtoTree::ItemIsEqual(const Item& item, const char* key, unsigned int keysize)
{
    if (item.GetKeysize() != keysize) return false;
    return KeysAreEqual(item.GetKey(), key, keysize, item.GetEndian());   
}  // end ProtoTree::ItemIsEqual()

bool ProtoTree::Bit(const char* key, unsigned int keysize, unsigned int index, Endian keyEndian)
{
    if (index < keysize)
    {
        unsigned int byteIndex = index >> 3;
        byteIndex = (ENDIAN_BIG == keyEndian) ? byteIndex : ((keysize - 1) >> 3) - byteIndex;
        return (0 != (key[byteIndex] & (0x80 >> (index & 0x07)))); 
    }
    else if (index < (keysize + (sizeof(keysize) << 3)))
    {
        index -= keysize;
        return (0 != (((char*)&keysize)[index >> 3] & (0x80 >> (index & 0x07))));
    }
    else
    {
        return false; 
    }
}  // end ProtoTree::Bit()
        
ProtoTree::Item* ProtoTree::GetFirstItem() const
{
    if (NULL != root)
    {
        if (root->left == root->right)
        {
            // 2-A) Only one node in this tree
            return root;
        }            
        else
        {
            // 2-B) Return left most node in this tree
            Item* x = (root->left == root) ? root->right : root;   
            while (x->left->parent == x) x = x->left;
            return (x->left);
        }
    }
    return NULL;
}  // end ProtoTree::GetFirstItem()

ProtoTree::Item* ProtoTree::GetLastItem() const
{
    if (NULL != root)
    {
        // 2) Follow root or left of root all the way to right to find
        //    the last item (lexically) in the tree
        Item* x = (root->right == root) ? root->left : root;
        Item* p;
        do
        {
            p = x;
            x = x->right;
        } while (p == x->parent);
        return x;
    }
    return NULL;
}  // end ProtoTree::GetLastItem()

bool ProtoTree::Insert(ProtoTree::Item& item)     
{
    if (NULL != root)
    {
        // 1) Find closest match to "item"
        const char* key = item.GetKey();
        unsigned int keysize = item.GetKeysize();
        Endian keyEndian = item.GetEndian();
        Item* x = root;
        Item* p;
        do
        {
            p = x;
            x = Bit(key, keysize, x->bit, keyEndian) ? x->right : x->left;
        } while (p == x->parent);
        
        // 2) Then, find index of first differing bit ("dBit")
        //    (also look out for exact match!)
        unsigned int dBit = 0;
        // A) Do byte-wise comparison to extent possible
        unsigned int keysizeMin, indexMax;
        if (keysize < x->GetKeysize()) 
        {
            keysizeMin = keysize;
            indexMax = x->GetKeysize() + (sizeof(unsigned int) << 3);
        }
        else
        {
            keysizeMin = x->GetKeysize();
            indexMax = keysize  + (sizeof(unsigned int) << 3);
        }
        const char* ptr1 = key;
        const char* ptr2 = x->GetKey();
        ASSERT(x->GetEndian() == keyEndian);
        if (ENDIAN_LITTLE == keyEndian)
        {
            ptr1 += ((keysize - 1) >> 3);
            ptr2 += ((x->GetKeysize() - 1) >> 3);
        }
        unsigned int fullByteBits = keysizeMin & ~0x07;
        while (dBit < fullByteBits)
        {
            if (*ptr1 != *ptr2)
            {
                // B) Do bit-wise comparison on differing byte
                unsigned char delta = *ptr1 ^ *ptr2;
                ASSERT(0 != delta);
                while (delta < 0x80)
                {
                    delta <<= 1;
                    dBit++;
                }
                break;
            }
            if (ENDIAN_BIG == keyEndian)
            {
                ptr1++;
                ptr2++;
            }
            else
            {
                ptr1--;
                ptr2--;
            } 
            dBit += 8;  
        }
        ASSERT(dBit <= fullByteBits);
        if (dBit == fullByteBits)
        {
            // C) Compare any remainder bit-by-bit
            for (; dBit < indexMax; dBit++)
            {
                if (Bit(key, keysize, dBit, keyEndian) != Bit(x->GetKey(), x->GetKeysize(), dBit, keyEndian))
                    break;
            }
            if (dBit == indexMax)
            {
                PLOG(PL_WARN, "ProtoTree::Insert() Equivalent item already in tree!\n");
                //ASSERT(0);
                return false;
            }
        }
        item.bit = dBit;

        // 3) Find "item" insertion point
        x = root;
        do
        {
            p = x;
            x = Bit(key, keysize, x->bit, keyEndian) ? x->right : x->left;
        } while ((x->bit < dBit) && (p == x->parent)); 


        // 4) Insert "item" into tree
        if (Bit(key, keysize, dBit, keyEndian))
        {
            ASSERT(NULL != x);
            item.left = x;
            item.right = &item;
        }
        else
        {
            item.left = &item;
            item.right = x;                 
        }
        item.parent = p;
        if (Bit(key, keysize, p->bit, keyEndian))
            p->right = &item;
        else 
            p->left = &item;
        if (p == x->parent) 
            x->parent = &item;
    }
    else
    {
        // tree is empty, so make "item" the tree root
        root = &item;
        item.parent = (Item*)NULL;
        item.left = item.right = &item;
        item.bit = 0;
    }
    // Note for ProtoTree, we call UpdateIterators() _after_
    // insertion/removal since we just reset the iterators
    UpdateIterators(&item, Iterator::INSERT);
    return true;
}  // end ProtoTree::Insert()

// Find node with backpointer to "item"
ProtoTree::Item* ProtoTree::FindPredecessor(ProtoTree::Item& item) const
{
    // Find terminal "q"  with backpointer to "item"   
    Item* x = &item;
    Item* q;
    const char* key = item.GetKey();
    unsigned int keysize = item.GetKeysize();
    Endian keyEndian = item.GetEndian();
    do
    {
        q = x;
        if (Bit(key, keysize, x->bit, keyEndian))
            x = x->right;
        else
            x = x->left;
    } while (x != &item);
    return q;
}  // end ProtoTree::FindPredecessor()

void ProtoTree::Remove(ProtoTree::Item& item)
{
    ASSERT(0 != item.GetKeysize());
    if (((&item == item.left) || (&item == item.right)) && (NULL != item.parent))
    {
        // non-root "item" that has at least one self-pointer  
        // (a.k.a an "external entry"?)
        Item* orphan = (&item == item.left) ? item.right : item.left;
        if (item.parent->left == &item)
            item.parent->left = orphan;
        else 
            item.parent->right = orphan;
        if (orphan->bit > item.parent->bit)
            orphan->parent = item.parent;
    }
    else
    {
        // Root or "item" with no self-pointers 
        // (a.k.a an "internal entry"?)
        // 1) Find terminal "q"  with backpointer to "item"  
        const char* key = item.GetKey();
        unsigned int keysize = item.GetKeysize();
        Endian keyEndian = item.GetEndian();
        Item* x = &item;
        Item* q;
        do                      
        {     
            q = x;              
            if (Bit(key, keysize, x->bit, keyEndian))
                x = x->right;
            else                
                x = x->left;    
        } while (x != &item);
        
        if (NULL != q->parent)
        {
            // Non-root "q", so "q" is moved into the place of "item"
            Item* s = NULL;
            if (NULL == item.parent)
            {
                // There are always two nodes backpointing to "root"
                // (unless root is the only node in the tree)
                // "s" is the other of these besides "q"
                // (We need to find "s" before we mess with the tree)
                x = Bit(key, keysize, item.bit, keyEndian) ? item.left : item.right;
                do
                {
                    s = x;
                    if (Bit(key, keysize, x->bit, keyEndian))
                        x = x->right;
                    else
                        x = x->left;
                } while (x != &item);
            }
            
            // A) Set bit index of "q" to that of "item"
            q->bit = item.bit;
            
            // B) Fix the parent, left, and right node pointers to "q"
            //    (removes "q" from its current place in the tree)
            Item* parent = q->parent;
            Item* child = (q->left == &item) ? q->right : q->left;
            ASSERT(NULL != child);
            if (parent->left == q)
                parent->left = child;
            else
                parent->right = child;
            if (child->bit > parent->bit)
                child->parent = parent;
            
            // C) Fix the item's left->parent and right->parent node pointers to "item"
            //    (places "q" into the current place of the "item" in the tree)
            ASSERT(q != NULL);
            if (item.left->parent == &item)
                item.left->parent = q;
            if (item.right->parent == &item)
                item.right->parent = q;
            if (NULL != item.parent)
            {
                if (item.parent->left == &item)
                    item.parent->left = q;
                else 
                    item.parent->right = q;
            }
            else
            {
                // "item" was root node, so update the "s" node 
                //  backpointer to "q" instead of "item"
                ASSERT(s != NULL);
                ASSERT(s != &item);
                if (s->left == &item)
                    s->left = q;
                else
                    s->right = q;
                root = q;
            }
              
            // E) Finally, "q" gets the pointers of the "item" being removed
            //    (which now _may_ include a pointer to itself)
            if (NULL != item.parent)
                ASSERT((&item != item.left) && (&item != item.right));
            q->parent = item.parent;
            q->left = (item.left == &item) ? q : item.left;
            q->right = (item.right == &item) ? q : item.right;
        }
        else
        {
            // "root" is removed with none or one item left
            ASSERT(q == &item);
            Item* orphan = (q == q->left) ? q->right : q->left;
            if (q == orphan)
            {
                root = (Item*)NULL; 
            }
            else
            {
                root = orphan; 
                orphan->parent = NULL;
                if (orphan->left == q) 
                    orphan->left = orphan;
                else 
                    orphan->right = orphan;
                orphan->bit = 0;
            }
        }
    }   
    item.parent = item.left = item.right = (Item*)NULL;
    UpdateIterators(&item, Iterator::REMOVE);
}  // end ProtoTree::Remove()

ProtoTree::Item* ProtoTree::RemoveRoot()
{
    Item* item = root;
    if (NULL != item) Remove(*item);
    return item;
}  // end ProtoTree::RemoveRoot()


/**
 * Find item with exact match to key and keysize
 */
ProtoTree::Item* ProtoTree::Find(const char*  key, 
                                 unsigned int keysize) const
{
    Item* x = root;
    if (NULL != x)
    {
        Endian keyEndian = x->GetEndian();
        Item* p;
        do 
        { 
            p = x;
            x = Bit(key, keysize, x->bit, keyEndian) ? x->right : x->left;   
        } while (x->parent == p);
        return (ItemIsEqual(*x, key, keysize) ? x : NULL);
    }    
    else
    {
        return (Item*)NULL;
    }
}  // end ProtoTree::Find()

/**
 * Find item with "closest" match to key and keysize (biggest prefix match?)
 */
ProtoTree::Item* ProtoTree::FindClosestMatch(const char*  key, 
                                             unsigned int keysize) const
{
    Item* x = root;
    if (NULL != x)
    {
        Endian keyEndian = x->GetEndian();
        Item* p;
        do 
        { 
            p = x;
            x = Bit(key, keysize, x->bit, keyEndian) ? x->right : x->left;   
        } while ((x->parent == p) && (x->bit < keysize));
        return x;   
    }    
    else
    {
        return (Item*)NULL;
    }
}  // end ProtoTree::FindClosestMatch()

/**
 * Finds longest matching entry that is a prefix to "key"
 */
ProtoTree::Item* ProtoTree::FindPrefix(const char*  key, 
                                       unsigned int keysize) const
{
    // (TBD) Retest this code with new "size-agnostic" ProtoTree implementation
    Item* x = root;
    if (NULL != x)
    {
        Endian keyEndian = x->GetEndian();
        Item* p;
        do 
        { 
            p = x;
            x = Bit(key, keysize, x->bit, keyEndian) ? x->right : x->left; 
        } while ((x->parent == p) && (x->bit < keysize));
        if (PrefixIsEqual(key, keysize, x->GetKey(), x->GetKeysize(), keyEndian))           
            return x;
    }
    return NULL;
}  // end ProtoTree::FindPrefix()


/**
 * This finds prefix subtree root, (TBD) add find prefix 
 * subtree min, and find prefix subtree max methods
 * (e.g. for "min", first find subtree root, and roll left ???)
 */
ProtoTree::Item* ProtoTree::FindPrefixSubtree(const char*  prefix, 
                                              unsigned int prefixSize) const
{
    // (TBD) Retest this code more with new "size-agnostic" ProtoTree implementation
    Item* x = root;
    if (NULL != x)
    {
        Endian keyEndian = x->GetEndian();
        Item* p;
        do 
        { 
            p = x;
            x = Bit(prefix, prefixSize, x->bit, keyEndian) ? x->right : x->left; 
        } while ((x->parent == p) && (x->bit < prefixSize));
        if (PrefixIsEqual(x->GetKey(), x->GetKeysize(), prefix, prefixSize, keyEndian)) 
            return x;
    }
    return (Item*)NULL;
}  // end ProtoTree::FindPrefixSubtree()
                

ProtoTree::Iterator::Iterator(ProtoTree& theTree, bool reverse, ProtoTree::Item* cursor)
 : ProtoIterable::Iterator(theTree), prefix_size(0), prefix_item(NULL)
{
    if (NULL != cursor)
    {
        reversed = reverse;
        SetCursor(*cursor);
    }
    else
    {
        Reset(reverse);  // Reset() sets all to defaults
    }
}  

ProtoTree::Iterator::~Iterator()
{
}

void ProtoTree::Iterator::Reset(bool                reverse,
                                const char*         prefix,
                                unsigned int        prefixSize)
{
    ProtoTree* tree = static_cast<ProtoTree*>(iterable);
    
    prefix_size = 0;
    prefix_item = prev = next = curr_hop = (Item*)NULL;
    if ((NULL == tree) || (NULL == tree->root)) return;
    
    if (0 != prefixSize)
    {
        if (NULL == prefix) return;
        // Find root of subtree with matching prefix 
        // (TBD - there's a better way to find the min/max prefix matches via prefix00000 or prefix11111
        ProtoTree::Item* prefixItem = tree->FindPrefixSubtree(prefix, prefixSize);
        if (NULL == prefixItem) return;
        // Temporarily "Reset()" the iterator and "SetCursor()" to find
        reversed = reverse ? false : true;
        SetCursor(*prefixItem);
        Endian keyEndian = prefixItem->GetEndian();
        if (reverse)
        {
            // Find the maximum value with matching prefix.
            ProtoTree::Item* lastItem;
            while (NULL != (lastItem = GetNextItem()))
            {   
                if (!tree->PrefixIsEqual(lastItem->GetKey(), lastItem->GetKeysize(), prefix, prefixSize, keyEndian))
                    break;  // The "cursor" is set to after the last matching item
            }
            if (NULL == lastItem) Reset(reverse);
        }
        else
        {
            // Find the minimum value with matching prefix.
            ProtoTree::Item* firstItem;
            while (NULL != (firstItem = GetPrevItem()))
            {   
                if (!tree->PrefixIsEqual(firstItem->GetKey(), firstItem->GetKeysize(), prefix, prefixSize, keyEndian))
                    break;  // The "cursor" is set to before the first matching item
            }
            if (NULL == firstItem) Reset(reverse);
        }
        prefix_size = prefixSize;
        prefix_item = prefixItem;
        return;
    }
    
    if (reverse)
    {
        // This code is basically the same as ProtoTree::GetLastItem()
        if (NULL != tree->root)
        {
            // Follow left of root all the way to right to find
            // the very last item (lexically) in the tree
            Item* x = (tree->root->right == tree->root) ? tree->root->left : tree->root;
            Item* p;
            do
            {
               p = x;
               x = x->right;
            } while (p == x->parent);
            prev = x;
        }
        reversed = true;
    }    
    else
    {
        // This code is basically the same as ProtoTree::GetFirstItem()
        // (although the code to find the "curr_hop" for iteration is different)
        if (NULL != tree->root)
        {
            if (tree->root->left == tree->root->right)
            {
                // Only one entry in the tree
                next = tree->root;
                curr_hop = NULL;
            }
            else
            {
                // If root has a left side, go as far left as possible
                // to find the very first item (lexically) in the tree
                Item* x = (tree->root->left == tree->root) ? tree->root->right : tree->root;   
                while (x->left->parent == x) x = x->left;
                next = x->left;
                if (x->right->parent == x)
                {
                    // Branch right and go as far left as possible
                    x = x->right;
                    while (x->left->parent == x) x = x->left;
                }
                curr_hop = x;
            }
        }
        reversed = false;
    }
}  // end ProtoTree::Iterator::Reset()

void ProtoTree::Iterator::SetCursor(ProtoTree::Item& item)
{
    ProtoTree* tree = static_cast<ProtoTree*>(iterable);
    // Save prefix subtree info
    unsigned int prefixSize = prefix_size;
    ProtoTree::Item* prefixItem = prefix_item;
    prefix_size = 0;
    prefix_item = NULL;
    
    if ((NULL== tree) || (NULL == tree->root))
    {
        prev = next = curr_hop = NULL;
    }
    else if (tree->root->left == tree->root->right)
    {
        ASSERT(&item == tree->root);
        curr_hop = NULL;
        if (reversed)
        {
            prev = NULL;
            next = tree->root;
        }
        else
        {
            prev = tree->root;
            next = NULL;
        }
    }
    else if (reversed)
    {
        // Setting "cursor" for "reversed" iteration is easy.
        curr_hop = NULL;
        prev = &item;   
        GetPrevItem();  // note this sets "next"
    }
    else
    {
        // Setting "cursor" for forward iteration is a little more complicated.
        // Given an "item", we can find the "curr_hop" for the tree 
        // entry that lexically precedes the "item"
        // (We do a reverse iteration to find that preceding entry)
        reversed = true;
        prev = &item;
        GetPrevItem();  // note "GetPrevItem()" also sets "next" as needed
        if (NULL == GetPrevItem())
        {
            Reset(false);
            // Move forward one place so "cursor" is correct position
            GetNextItem();
        }
        else
        {
            // This finds the proper "curr_hop" that goes with
            // the entry previous of "item" ...
            if ((&item != tree->root) || (item.right != &item))
            {
                // Find the node's "predecessor" 
                // (has backpointer to "item"
                curr_hop = tree->FindPredecessor(item);
            }
            else
            {
                // Instead, use the other node with backpointer to root "item"
                ASSERT(&item == tree->root);
                const char* key = item.GetKey();
                unsigned int keysize = item.GetKeysize();
                Endian keyEndian = item.GetEndian();
                Item* s;
                Item* x = tree->Bit(key, keysize, item.bit, keyEndian) ? item.left : item.right;
                ASSERT((x == item.left) || (x == item.right));
                do
                {
                    s = x;
                    if (tree->Bit(key, keysize, x->bit, keyEndian))
                        x = x->right;
                    else
                        x = x->left;
                } while (x != &item);  
                curr_hop = s;       
            }
            // Move forward two places so "cursor" is correct position
            reversed = false;
            GetNextItem();
            GetNextItem();
        }
    }
    // Restore prefix subtree info
    if (0 != prefixSize)
    {
        prefix_item = prefixItem;
        prefix_size = prefixSize;
    }
}  // end ProtoTree::Iterator::SetCursor()

ProtoTree::Item* ProtoTree::Iterator::GetPrevItem()
{
    if (NULL != prev)
    {
        ProtoTree* tree = static_cast<ProtoTree*>(iterable);
        if (!reversed)
        {
            // This iterator has been moving forward
            // so we need to turn it around.
            reversed = true;
            // temporarily suspend prefix matching (if applicable) to allow turn-around
            unsigned savePrefixSize = prefix_size;
            prefix_size = 0;
            GetPrevItem();
            prefix_size = savePrefixSize;
	        if (NULL == prev) return NULL;
        }
        Item* item = prev;
	    Endian keyEndian = item->GetEndian();
        if (0 != prefix_size)
        {
            // Test "item" against our reference "prefix_item"
            if ((NULL == prefix_item) || 
                !tree->PrefixIsEqual(item->GetKey(), item->GetKeysize(), prefix_item->GetKey(), prefix_size, keyEndian))
            {
                prev = NULL;
                return NULL;
            }
        }
        
        Item* x;
        // Find node "q" with backpointer to "item"
        if ((NULL == item->parent) && (item->right == item))
            x = item->left;
        else
            x = item;
        Item* q;
        do                      
        {                       
            q = x;              
            if (tree->Bit(item->GetKey(), item->GetKeysize(), x->bit, keyEndian))
                x = x->right;
            else                
                x = x->left;    
        } while (x != prev); 
        
        if (q->right != item)
        {
            // Go up the tree
            do
            {
                x = q;
                q = q->parent;
            } while ((NULL != q) && (x == q->left));

            if ((NULL == q) || (NULL == q->parent))
            {
                if ((NULL == q) || (q->left == q))
                {
                    // We've bubbled completely up or
                    // root has no left side, so we're done
                    prev = NULL;
                }
                else 
                {
                    // We've iterated to root from the right side
                    // and root has a left side we should check out
                    // So, find the left-side predecessor to root "q"
                    Item* r = q;
                    x = q->left;
                    do                      
                    {                       
                        q = x;              
                        if (tree->Bit(r->GetKey(), r->GetKeysize(), x->bit, keyEndian))
                            x = x->right;
                        else                
                            x = x->left;    
                    } while (x != r); 
                    if (q->left != q)
                    {
                        // Go as far right of "q->left" as possible
                        q = q->left;
                        do
                        {
                            x = q;
                            q = q->right; 
                        } while (x == q->parent);
                    }
                    prev = q;
                }
                next = item;
                return item;
            }
        }  // end if (q->right != prev)
        
        if (q->left->parent != q)
        {
            if ((NULL == q->left->parent) &&
                (q->left->left != q->left) &&
                tree->Bit(q->GetKey(), q->GetKeysize(), 0, keyEndian))
            {
                // We've come from the right and there is a left of root
                // So, go as far right of the left of root "q->left" as possible
                x = q->left->left;
                do
                {
                    q = x;
                    x = x->right; 
                } while (q == x->parent);
                prev = x;
            }
            else
            {
                // Otherwise, this is the appropriate iterate
                prev = q->left;
            }
        }
        else
        {
            // Go as far right of "q->left" as possible
            x = q->left;
            do
            {
                q = x;
                x = x->right; 
            } while (q == x->parent);
            prev = x;
        }
        next = item;
        return item;
    }
    else
    {
        return NULL;
    } 
}  // end ProtoTree::Iterator::GetPrevItem()


ProtoTree::Item* ProtoTree::Iterator::PeekPrevItem()
{
    if (reversed)
    {
        return prev;
    }
    else
    {
        Item* prevItem = GetPrevItem();
        GetNextItem(); // puts mode/cursor back to right place
        return prevItem;
    }
}  // end ProtoTree::PeekPrevItem()

ProtoTree::Item* ProtoTree::Iterator::GetNextItem()
{
    if (NULL != next)
    {
        ProtoTree* tree = static_cast<ProtoTree*>(iterable);
	    if (reversed)
        {
	        // This iterator has been going backwards
            // so we need to turn it around
            reversed = false;
	        SetCursor(*next);
	        if (NULL == next) return NULL;  
        }
	    Item* item = next;
        Endian keyEndian = next->GetEndian();
        if (NULL == curr_hop)
        {
            next = NULL;
        }
        else
        {
	        Item* x = curr_hop;
            if (((x->left != next) && (x->left->parent != x)) ||
                (x->right->parent == x))
            {
                next = x->left;
                // Now, update "curr_hop" if applicable
                // First, check for root node visit
                if ((NULL == next->parent) &&
                    (tree->Bit(next->GetKey(), next->GetKeysize(), 0, keyEndian) != tree->Bit(x->GetKey(), x->GetKeysize(), 0, keyEndian)))
                {
                    if (x->right == x)
                    {
                        next = x;
                        curr_hop = NULL; 
                    }
                    else
                    {
                        // Branch right and go as far left as possible
                        x = x->right;
                        while (x->left->parent == x) x = x->left;
                        next = x->left;
                        if (x->right->parent == x)
                        {
                            // Branch right and go as far left as possible
                            x = x->right;
                            while (x->left->parent == x) x = x->left;
                        }
                        curr_hop = x;
                    }
                }
                else if (x->right->parent == x)
                {
                    // Branch right and go as far left as possible
                    x = x->right;
                    while (x->left->parent == x) x = x->left;
                    curr_hop = x;
                }
                // Otherwise, there was no change to "curr_hop" for now
            }
            else // if (curr_hop->right->parent != curr_hop)
            {
                // Right item is next in iteration
                next = x->right;
                // Now, update "curr_hop" if applicable
                // First, check for root node visit
                if ((NULL == next->parent) &&
                    (next->right != next)  &&
                    (tree->Bit(x->GetKey(), x->GetKeysize(), 0, keyEndian) != tree->Bit(next->GetKey(), next->GetKeysize(), 0, keyEndian)))   
                {
                    // Branch right and go as far left as possible
                    x = next->right;
                    while (x->left->parent == x) x = x->left;
                    next = x->left;
                    if (x->right->parent == x)
                    {
                        // Again, branch right and go as far left as possible
                        x = x->right;
                        while (x->left->parent == x) x = x->left;
                    }
                    curr_hop = x;
                }
                else
                {
                    // Go back up the tree
                    Item* p = x->parent;
                    while ((p != NULL) && (p->right == x))
                    {
                        x = p;
                        p = x->parent;
                    }
                    if (NULL != p)
                    {
                        if ((NULL == p->parent) && (p->right == p))
                        {
                            p = NULL;
                        }
                        else if (p->right->parent == p)
                        {
                            // Branch right and go as far left as possible
                            Item* x = p->right;
                            while (x->left->parent == x) x = x->left;
                            p = x;
                        }
                    }
                    curr_hop = p;
                }
            }
        }  // end if/else (NULL == curr_hop)
        if (0 != prefix_size)
        {
            // Test "item" against prefix of item last returned
            if ((NULL == prefix_item) || 
                !tree->PrefixIsEqual(item->GetKey(), item->GetKeysize(), prefix_item->GetKey(), prefix_size, keyEndian))
                return NULL;
        }
        prev = item;
        return item;
    }
    else
    {
        return NULL;
    }  // end if/else(NULL != next)
}  // end ProtoTree::Item* ProtoTree::Iterator::GetNextItem()

ProtoTree::Item* ProtoTree::Iterator::PeekNextItem()
{
    if (reversed)
    {
        Item* nextItem = GetNextItem();
        GetPrevItem(); // puts mode/cursor back to right place
        return nextItem;
    }
    else
    {
        return next;
    }
}  // end ProtoTree::PeekNextItem()

void ProtoTree::Iterator::Update(ProtoIterable::Item* theItem, Action theAction)
{
    switch (theAction)
    {
        case INSERT:
        {
            // Save our current iterator state
            Item* oldPrev = prev;
            Item* oldNext = next;
            Item* oldPrefixItem = prefix_item;
            // First, find new prefix subtree if applicable/needed
            if (NULL != prefix_item)
            {
                Reset(reversed, prefix_item->GetKey(), prefix_size);
                ASSERT(NULL != prefix_item); // should have found old one at least
            }
            // Restore iterator cursor state
            if (reversed)
            {
                if (NULL != oldNext)
                    SetCursor(*oldNext);
                else if (NULL == prefix_item)
                    Reset(true);
            }
            else
            {
                if (NULL != oldPrev)
                    SetCursor(*oldPrev);
                else if (NULL == oldPrefixItem)
                    Reset(false);
            }
            break;
        }
        case REMOVE:
        {
            // NOTE - This doesn't work quite right for prefix iterators 
            // (mid-iteration removal of items can break comprehensive prefix iteration)
            // Save our current iterator state
            Item* oldPrev = prev;
            Item* oldNext = next;
            // First, find new prefix subtree if applicable/needed
            if (static_cast<Item*>(theItem) == prefix_item)
            {
                Reset(reversed, prefix_item->GetKey(), prefix_size);
                if (NULL == prefix_item)
                    break;  // no matching prefix subtree remains
            }
            // Restore cursor state with trick if cursor was item removed
            if (reversed)
            {
                // "next" is the cursor for reverse mode
                if (static_cast<Item*>(theItem) != oldNext)
                {
                    if (NULL == oldNext)
                    {
                        // tree is now empty?
                        //ASSERT(NULL == prefix_item);
                        if (NULL == prefix_item)
                            prev = next = NULL;
                        else
                            Reset(reversed, prefix_item->GetKey(), prefix_size);
                    }
                    else
                    {   
                        SetCursor(*oldNext);
                    }
                }
                else
                {
                    // This puts the iteration to a ambiguous
                    // state that allows subsequent calls to 
                    // either GetPrevItem() or GetNextItem() to
                    // work properly even though the "cursor" is
                    // wrong. (Note PeekNextItem() won't be correct)
                    if (NULL == oldPrev)
                    {
                        // tree is now empty?
                        //ASSERT(NULL == prefix_item);
                        if (NULL == prefix_item)
                            prev = next = NULL;
                        else
                            Reset(reversed, prefix_item->GetKey(), prefix_size);
                    }
                    else
                    {
                        if (NULL == prefix_item)
                        {
                            SetCursor(*oldPrev);
                            prev = oldPrev;
                        }
                        else
                        {
                            Reset(reversed, prefix_item->GetKey(), prefix_size);
                        }
                    }
                }
            }
            else
            {
                // "prev" is the cursor for normal mode
                if (static_cast<Item*>(theItem) != oldPrev)
                {
                    if (NULL == oldPrev)
                    {
                        // tree is now empty?   
                        //ASSERT(NULL == prefix_item);
                        if (NULL == prefix_item)
                            prev = next = NULL;
                        else
                            Reset(reversed, prefix_item->GetKey(), prefix_size);
                    }
                    else
                    {
                        SetCursor(*oldPrev);
                    }
                }
                else
                {
                    if (NULL == oldNext)
                    {
                        // tree is now empty?   
                        //ASSERT(NULL == prefix_item);
                        if (NULL == prefix_item)
                            prev = next = NULL;
                        else
                            Reset(reversed, prefix_item->GetKey(), prefix_size);
                    }
                    else
                    {
                        if (NULL == prefix_item)
                        {
                            SetCursor(*oldNext);
                            next = oldNext;
                        }
                        else
                        {
                            Reset(reversed, prefix_item->GetKey(), prefix_size);
                        }
                    }
                }
            }
            break;
        }
        case EMPTY:
            prev = next = prefix_item = NULL;
            prefix_size = 0;
            break;
        case APPEND:
        case PREPEND:
            ASSERT(0);
            break;
    }
}  // end ProtoTree::Iterator::Update()

ProtoTree::SimpleIterator::SimpleIterator(ProtoTree& theTree)
 : ProtoIterable::Iterator(theTree)
{
    Reset();
}

ProtoTree::SimpleIterator::~SimpleIterator()
{
}

void ProtoTree::SimpleIterator::Reset()
{
    ProtoTree* tree = static_cast<ProtoTree*>(iterable);
    if (NULL != tree)
    {
        Item* x = tree->root;
        if (NULL != x)
        {
            // Go left as far as possible
            while (x->left->parent == x)
                 x = x->left;
        }
        next = x;
    }
    else
    {
        next = NULL;
    }
}  // end ProtoTree::SimpleIterator::Reset()

ProtoTree::Item* ProtoTree::SimpleIterator::GetNextItem()
{
    Item* i = next;
    if (NULL != i)
    {
        if (i->right->parent == i)
        {
            // Go right one, then left as far as possible
            Item* y = i->right;
            while (y->left->parent == y) y = y->left;
            if (y != i)
            {
                next = y;
                return i;
            }
        }
        Item* x = i;
        Item* y = i->parent;
        while ((NULL != y) && (y->right == x))
        {
            x = y;
            y = y->parent;
        }
        next = y;
    }
    return i;
}  // end ProtoTree::SimpleIterator::GetNextItem()

void ProtoTree::SimpleIterator::Update(ProtoIterable::Item* /*theItem*/, Action /*theAction*/)
{
    // For ProtoTree::SimpleIterator, really the only "sane" thing to do when the associated
    // tree is modified is to "Reset()" the iterator since the logical tree structure
    // is potentially heavily affected by any change.  This is heavy-handed so it's 
    // actually more efficient to do a "GetRoot(), Remove()" loop until GetRoot() returns NULL
    // (see ProtoTree::Destroy()) instead of using this SimpleIterator.  Where it _is_
    // useful is in the ProtoIndexedQueue::Empty() method where it empties the tree without invoking
    // any of the Item virtual methods (i.e. GetKey(), etc).  The SimpleIterator is also useful
    // as a little lighter weight (than the regular ProtoTree::Iterator) where the order of
    // iteration doesn't matter and mid-iteration Item insertion/removal isn't done.
    Reset();
}  // end ProtoTree::SimpleIterator::Update()


ProtoSortedTree::ProtoSortedTree(bool uniqueItemsOnly)
 : unique_items_only(uniqueItemsOnly), positive_min(NULL)
{
}

ProtoSortedTree::~ProtoSortedTree()
{
}

bool ProtoSortedTree::Insert(Item& item)
{
    ASSERT(0 != item.GetKeysize());
    // Is an item with this key already in tree?
    const char* key = item.GetKey();
    unsigned int keysize = item.GetKeysize();
    ProtoTree::Endian keyEndian = item.GetEndian();
    Item* match = Find(key, keysize);
    
    if (NULL == match)
    {
        // Insert the item into our "item_tree"
        item_tree.Insert(item);
        // Now find the place to thread this new item into our linked list
        ProtoTree::Iterator iterator(item_tree, true, &item);
        match = static_cast<Item*>(iterator.PeekPrevItem());
        if (NULL == match)
        {
            if (item_list.IsEmpty())
            {
                // nothing in the list yet
                item_list.Append(item);
                if (item.UseSignBit())
                {
                    bool itemSign = item_tree.Bit(key, keysize, 0, keyEndian);
                    if (!itemSign) positive_min = &item;
                }
            }
            else
            {
                // this item lexically precedes the anything in the tree
                bool useSignBit = item.UseSignBit();
                ASSERT(useSignBit == GetHead()->UseSignBit());
                if (useSignBit)
                {
                    bool itemSign = item_tree.Bit(key, keysize, 0, keyEndian);
                    if (itemSign)
                    {
                        bool useComplement2 = item.UseComplement2();
                        ASSERT(useComplement2 == GetHead()->UseComplement2());
                        // A signed "item" with no lexical predecessor
                        if (useComplement2)
                        {
                            // is smallest negative number in a tree that
                            // has no positive numbers yet.
                            // So this goes to the "head" automatically!
                            item_list.Prepend(item);
                        }
                        else
                        {
                            // is biggest negative number in a tree that
                            // has no positive numbers yet.
                            // So this goes to the "tail" automatically!
                            item_list.Append(item);
                        }
                        // Note: (itemSign && !headSign) is impossible here
                    }
                    else 
                    {
                        Item* head = GetHead();
                        bool headSign = item_tree.Bit(head->GetKey(), head->GetKeysize(), 0, keyEndian);
                        if (headSign)
                        {
                            // (!itemSign && headSign)
                            // An unsigned (positive value) "item" with no lexical predecessor
                            // is the smallest _positive_ number in the tree
                            // so insert this before our prior "positive_min" or at tail
                            if (NULL != positive_min)
                            {
                                item_list.Insert(item, *positive_min);
                            }
                            else
                            {
                                // first positive value, put at tail
                                item_list.Append(item);
                            }
                            positive_min = &item;
                        }
                        else
                        {
                            // (!itemSign && !headSign)
                            // Both positive, and "item" < "head"
                            // (no negative numbers in the tree?)
                            // So, insert "item" before "head"
                            item_list.Prepend(item);
                            positive_min = &item;
                        }
                    }
                }
                else
                {
                    // this item lexically precedes the "head"
                    item_list.Prepend(item);
                }  // end if/else (useSignBit)
            }  // end if/else item_list.IsEmpty()
        }
        else
        {
            // this "item" lexically succeeds the "match"
            bool useSignBit = item.UseSignBit();
            ASSERT(useSignBit == match->UseSignBit());
            if (useSignBit)
            {
                bool itemSign = item_tree.Bit(key, keysize, 0, keyEndian);
                if (!itemSign)
                {
                    // (!itemSign && !matchSign)
                    // Both positive, match < item
                    // Insert "item" just after "match"
                    Item* next = static_cast<Item*>(item_list.GetNextItem(*match));
                    if (NULL != next)
                        item_list.Insert(item, *next);
                    else
                        item_list.Append(item);
                    ASSERT(!item_tree.Bit(match->GetKey(), match->GetKeysize(), 0, keyEndian));
                    // Note (!itemSign && matchSign) can't happen
                    // here since signed "match" (negative value) 
                    // _must_ lexically succeed unsigned "item"
                }
                else 
                {
                    bool useComplement2 = item.UseComplement2();
                    ASSERT(useComplement2 == match->UseComplement2());
                    bool matchSign = item_tree.Bit(match->GetKey(), match->GetKeysize(), 0, keyEndian);
                    if (matchSign)
                    {
                        // (itemSign && matchSign)
                        if (useComplement2)
                        {
                            // Both negative, "match" < "item"
                            // Insert "item" just after "match"
                            Item* next = static_cast<Item*>(item_list.GetNextItem(*match));
                            if (NULL != next)
                                item_list.Insert(item, *next);
                            else
                                item_list.Append(item);
                        }
                        else
                        {
                            // Both negative, "item" < "match"
                            // Insert "item" before first equivalent "match"
                            // note "prev" here lexically _succeeds_ this
                            // "item" that was inserted into tree above, so:
                            
                            ProtoTree::Iterator iterator(item_tree, false, &item);
                            Item* prev = static_cast<Item*>(iterator.PeekNextItem());
                            
                            /* (Old "while()" loop approach to find prev, in-tree item)
                            Item* prev = match->GetPrev();
                            while ((NULL != prev) && !prev->IsInTree())
                            {
                                match = prev;
                                prev = prev->GetPrev();
                            }
                            */
                            if (NULL != prev)
                            {
                                Item* next = static_cast<Item*>(item_list.GetNextItem(*prev));
                                ASSERT(NULL != next);
                                item_list.Insert(item, *next);
                            }
                            else
                            {
                                item_list.Prepend(item);
                            }
                        }
                    }
                    else
                    {
                        // (itemSign && !matchSign)
                        if (useComplement2)
                        {
                            // Smallest negative number, put at list head
                            item_list.Prepend(item);
                        }
                        else
                        {
                            // Greatest negative number, put before "positive_min"
                            ASSERT(NULL != positive_min);
                            item_list.Insert(item, *positive_min);
                        }
                    }
                }
            }
            else
            {
                // Insert the item just after the close "match" from "item_tree"
                Item* next = static_cast<Item*>(item_list.GetNextItem(*match));
                if (NULL != next)
                    item_list.Insert(item, *next);
                else
                    item_list.Append(item);
            }  // end if/else (useSignBit)
        }
    }
    else if (match != &item)
    {
        if (unique_items_only) return false;
        // Insert the item just before the exact "match" from "item_tree"
        item_list.Insert(item, *match);
        item.left = NULL;  // denotes item is _not_ in tree (in linked list only)
        bool useSignBit = item.UseSignBit();
        ASSERT(useSignBit == match->UseSignBit());
        if (useSignBit && (match == positive_min))
            positive_min = &item;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoSortedTree::Insert() warning: item already in tree!\n");
    }
    return true;
}  // end  ProtoSortedTree::Insert()

void ProtoSortedTree::Prepend(Item& item)
{
    item_list.Prepend(item);
}  // end ProtoSortedTree::Prepend()

void ProtoSortedTree::Append(Item& item)
{
    item_list.Append(item);
}  // end ProtoSortedTree::Append()

void ProtoSortedTree::Remove(Item& item)
{
    // 1) Save some state and remove from linked list
    Item* prev = static_cast<Item*>(item_list.GetPrevItem(item));
    if (&item == positive_min) 
        positive_min = static_cast<Item*>(item_list.GetNextItem(item));
    item_list.Remove(item);
    
    // 2) Remove from ProtoTree, if applicable
    if (item.IsInTree())
    {
        // Remove "item" from the tree and mark as removed (item.left = NULL)
        item_tree.Remove(item);
        item.left = NULL;
        // Does "prev" needs to take the place of "item" in the ProtoTree?
        if ((NULL != prev) && !prev->IsInTree())
            item_tree.Insert(*prev);
    }
}  // end ProtoSortedTree::Remove()

void ProtoSortedTree::Empty()
{
    if (!IsEmpty())
    {
        item_tree.Empty();
        item_list.Empty();
        positive_min = NULL;      
    } 
}  // end ProtoSortedTree::Empty()

void ProtoSortedTree::EmptyToPool(ItemPool& itemPool)
{
    if (!IsEmpty())
    {
        item_tree.Empty();
        item_list.EmptyToPool(itemPool);
        positive_min = NULL;      
    } 
}  // end ProtoSortedTree::EmptyToPool()

void ProtoSortedTree::Destroy()
{
    if (!IsEmpty())
    {
        item_tree.Empty();
        item_list.Destroy();
        positive_min = NULL;      
    }
}  // end ProtoSortedTree::Destroy()

ProtoSortedTree::Item::Item()
{
}

ProtoSortedTree::Item::~Item()
{
}

ProtoSortedTree::Iterator::Iterator(ProtoSortedTree&    theTree, 
                                    bool                reverse, 
                                    const char*         keyMin, 
                                    unsigned int        keysize)
 : tree(theTree), list_iterator(theTree.item_list, reverse)
{
    Reset(reverse, keyMin, keysize);
}

ProtoSortedTree::Iterator::~Iterator()
{
}

void ProtoSortedTree::Iterator::Reset(bool reverse, const char* keyMin, unsigned int keysize)
{
    list_iterator.Reset(reverse); // put the iterator in the right direction
    if ((NULL != keyMin) && list_iterator.IsValid() && !tree.IsEmpty())
    {
        // refine if a "keyMin" start point was provided
        // (note for "reverse" == true, "keyMin" is really a "keyMax"
        Item* match = tree.Find(keyMin, keysize); 
        if (NULL == match)
        {
            // There was no exact match to "keyMin", so look for next item (or prev if reverse == true)
            TempItem tmpItem(keyMin, keysize, tree.GetHead()->GetEndian());
            tree.item_tree.Insert(tmpItem);
            ProtoTree::Iterator iterator(tree.item_tree, reverse, &tmpItem);
            match = reverse ? static_cast<Item*>(iterator.PeekPrevItem()) : 
                              static_cast<Item*>(iterator.PeekNextItem());
            tree.item_tree.Remove(tmpItem);  // it's done its job, so bye-bye
        }
        if ((NULL != match) && !reverse)
        {
            // Make sure we are positioned on _first_ item of equal valued items
            ProtoTree::Iterator iterator(tree.item_tree, true, match);
            Item* prev = static_cast<Item*>(iterator.PeekPrevItem());
            if (NULL == prev)
                match = tree.item_list.GetHead();    
            else
                match = static_cast<Item*>(tree.item_list.GetNextItem(*prev));
        }
        list_iterator.SetCursor(match);
    }
}  // end ProtoSortedTree::Iterator::Reset()

                
ProtoSortedTree::Iterator::TempItem::TempItem(const char* theKey, unsigned int theKeysize, ProtoTree::Endian keyEndian)
 : key(theKey), keysize(theKeysize), key_endian(keyEndian)
{
}

ProtoSortedTree::Iterator::TempItem::~TempItem()
{
}
