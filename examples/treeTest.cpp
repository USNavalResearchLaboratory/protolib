#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <protoTree.h>
#include <protoQueue.h>
#include <protoAddress.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

class StringItem : public ProtoSortedTree::Item
{
public:
    explicit StringItem(const std::string& s)
        : key(s)
    {
    }

    const std::string& GetString() const
    {
        return key;
    }

private:
    const char* GetKey() const override
    {
        return key.c_str();
    }

    unsigned int GetKeysize() const override
    {
        return static_cast<unsigned int>(8 * key.size());
    }

    std::string key;
};

class StringTree : public ProtoSortedTreeTemplate<StringItem>
{
public:
    explicit StringTree(bool uniqueItemsOnly = false)
        : ProtoSortedTreeTemplate<StringItem>()
    {
        (void)uniqueItemsOnly; // template wrapper does not expose ctor arg
    }
};

static std::string MakeRandomString(std::mt19937_64& rng,
                                    unsigned int minLen,
                                    unsigned int maxLen)
{
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    std::uniform_int_distribution<unsigned int> lenDist(minLen, maxLen);
    std::uniform_int_distribution<unsigned int> charDist(0, sizeof(alphabet) - 2);

    unsigned int len = lenDist(rng);
    std::string s;
    s.resize(len);
    for (unsigned int i = 0; i < len; ++i)
        s[i] = alphabet[charDist(rng)];
    return s;
}

struct BenchmarkResult
{
    std::size_t itemCount = 0;
    unsigned int minLen = 0;
    unsigned int maxLen = 0;
    std::uint64_t seed = 0;
    bool shuffleRemoval = false;
    double insertSeconds = 0.0;
    double removeSeconds = 0.0;
};

static BenchmarkResult BenchmarkProtoSortedTreeStrings(std::size_t itemCount,
                                                       unsigned int minLen,
                                                       unsigned int maxLen,
                                                       std::uint64_t seed,
                                                       bool shuffleRemoval)
{
    BenchmarkResult result;
    result.itemCount = itemCount;
    result.minLen = minLen;
    result.maxLen = maxLen;
    result.seed = seed;
    result.shuffleRemoval = shuffleRemoval;

    std::mt19937_64 rng(seed);
    StringTree tree;
    std::vector<StringItem*> items;
    items.reserve(itemCount);

    for (std::size_t i = 0; i < itemCount; ++i)
    {
        items.push_back(new StringItem(MakeRandomString(rng, minLen, maxLen)));
    }

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (!tree.Insert(*items[i]))
        {
            std::fprintf(stderr, "Insert failed at item %zu\n", i);
            std::exit(1);
        }
    }
    auto t1 = std::chrono::steady_clock::now();

    if (shuffleRemoval)
        std::shuffle(items.begin(), items.end(), rng);

    auto t2 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        tree.Remove(*items[i]);
        delete items[i];
    }
    auto t3 = std::chrono::steady_clock::now();

    result.insertSeconds =
        std::chrono::duration<double>(t1 - t0).count();
    result.removeSeconds =
        std::chrono::duration<double>(t3 - t2).count();

    return result;
}

#include <unordered_set>

class PlainStringItem : public ProtoTree::Item
{
public:
    explicit PlainStringItem(const std::string& s)
        : key(s)
    {
    }

    const std::string& GetString() const
    {
        return key;
    }

private:
    const char* GetKey() const override
    {
        return key.c_str();
    }

    unsigned int GetKeysize() const override
    {
        return static_cast<unsigned int>(8 * key.size());
    }

    std::string key;
};

class PlainStringTree : public ProtoTreeTemplate<PlainStringItem>
{
};

static BenchmarkResult BenchmarkProtoTreeStrings(std::size_t itemCount,
                                                 unsigned int minLen,
                                                 unsigned int maxLen,
                                                 std::uint64_t seed,
                                                 bool shuffleRemoval)
{
    BenchmarkResult result;
    result.itemCount = itemCount;
    result.minLen = minLen;
    result.maxLen = maxLen;
    result.seed = seed;
    result.shuffleRemoval = shuffleRemoval;

    std::mt19937_64 rng(seed);
    PlainStringTree tree;
    std::vector<PlainStringItem*> items;
    items.reserve(itemCount);

    // Pre-generate unique strings outside the timed section so duplicate
    // retries do not distort the insert benchmark.
    std::unordered_set<std::string> uniqueKeys;
    uniqueKeys.reserve(itemCount * 2);

    while (items.size() < itemCount)
    {
        std::string s = MakeRandomString(rng, minLen, maxLen);
        if (uniqueKeys.insert(s).second)
            items.push_back(new PlainStringItem(s));
    }

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (!tree.Insert(*items[i]))
        {
            std::fprintf(stderr, "ProtoTree insert failed at item %zu\n", i);
            std::exit(1);
        }
    }
    auto t1 = std::chrono::steady_clock::now();

    if (shuffleRemoval)
        std::shuffle(items.begin(), items.end(), rng);

    auto t2 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        tree.Remove(*items[i]);
        delete items[i];
    }
    auto t3 = std::chrono::steady_clock::now();

    result.insertSeconds =
        std::chrono::duration<double>(t1 - t0).count();
    result.removeSeconds =
        std::chrono::duration<double>(t3 - t2).count();

    return result;
}


// This used to test ProtoSortedTree support for basic lexical ordering (e.g., strings)
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

// This tests/demos the use of a tree where the ProtoSortedTree items are indexed
// by a "double" floating point value
class FloatingItem : public ProtoSortedTree::Item
{
    public:
        FloatingItem(double value) : item_key((0.0 == value) ? 0.0 : value) {}
        double GetValue() const {return item_key;}

        const char* GetKey() const
            {return (char*)&item_key;}
        unsigned int GetKeysize() const
            {return (sizeof(double) << 3);}
    private:
        // These configure the key interpretation to properly sort "double" type key values
        virtual bool UseSignBit() const {return true;}
        virtual bool UseComplement2() const {return false;}
        virtual ProtoTree::Endian GetEndian() const {return ProtoTree::GetNativeEndian();}

        double  item_key;
};  // end class FloatingItem

class FloatingTree : public ProtoSortedTreeTemplate<FloatingItem> {};


// This tests/demos the use of a tree where the ProtoSortedTree items are indexed
// by a "int" floating point value
class IntegerItem : public ProtoSortedTree::Item
{
    public:
        IntegerItem(int value) : item_key(value) {}
        int GetValue() const {return item_key;}

    private:
        const char* GetKey() const
            {return (char*)&item_key;}
        unsigned int GetKeysize() const
            {return (sizeof(int) << 3);}
        // These configure the key interpretation to properly sort "int" type key values
        virtual bool UseSignBit() const {return true;}
        virtual bool UseComplement2() const {return true;}
        virtual ProtoTree::Endian GetEndian() const {return ProtoTree::GetNativeEndian();}

        int  item_key;
};  // end class IntegerItem

class IntegerTree : public ProtoSortedTreeTemplate<IntegerItem> {};


class AddressItem : public ProtoTree::Item
{
    public:
        AddressItem(const char* addrString)
        {
            addr.ConvertFromString(addrString);
            memset(addr_key, 0, 16);
            memcpy(addr_key, addr.GetRawHostAddress(), addr.GetLength());
            addr_keysize = 8*addr.GetLength();
        }
        void SetKeysize(unsigned int size) {addr_keysize = size;}
        const ProtoAddress& GetAddress() const {return addr;}
        bool IsValid() const {return addr.IsValid();}

    private:
        const char* GetKey() const
            {return addr_key;}
        unsigned int GetKeysize() const
            {return addr_keysize;}
        ProtoAddress addr;
        char addr_key[16];
        unsigned int addr_keysize;
};  // end class AddressItem

class AddressTree : public ProtoTreeTemplate<AddressItem>
{
    public:
        AddressItem* FindPrefixSubtree(const char*  prefix,
                                        unsigned int prefixSize) const
        {
            return static_cast<AddressItem*>(ProtoTree::FindPrefixSubtree(prefix, prefixSize));
        }
};

double MakeRandomFloat(std::mt19937_64& rng,
                       double minVal,
                       double maxVal)
{
    std::uniform_real_distribution<double> randDouble(minVal, maxVal);
    return randDouble(rng);
}

static BenchmarkResult BenchmarkProtoSortedTreeFloats(std::size_t itemCount,
                                                      unsigned int minLen,
                                                      unsigned int maxLen,
                                                      std::uint64_t seed,
                                                      bool shuffleRemoval)
{
    BenchmarkResult result;
    result.itemCount = itemCount;
    result.minLen = minLen;
    result.maxLen = maxLen;
    result.seed = seed;
    result.shuffleRemoval = shuffleRemoval;

    std::mt19937_64 rng(seed);
    FloatingTree tree;
    std::vector<FloatingItem*> items;
    items.reserve(itemCount);

    for (std::size_t i = 0; i < itemCount; ++i)
    {
        items.push_back(new FloatingItem(MakeRandomFloat(rng, -1000.0, 1000.0)));
    }

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (!tree.Insert(*items[i]))
        {
            std::fprintf(stderr, "Insert failed at item %zu\n", i);
            std::exit(1);
        }
    }
    auto t1 = std::chrono::steady_clock::now();

    if (shuffleRemoval)
        std::shuffle(items.begin(), items.end(), rng);

    auto t2 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        tree.Remove(*items[i]);
        delete items[i];
    }
    auto t3 = std::chrono::steady_clock::now();

    result.insertSeconds =
        std::chrono::duration<double>(t1 - t0).count();
    result.removeSeconds =
        std::chrono::duration<double>(t3 - t2).count();

    return result;
}

int main(int argc, char* argv[])
{

    std::size_t itemCount = 1000000;
    unsigned int minLen = 8;
    unsigned int maxLen = 24;
    std::uint64_t seed = 0x12345678ULL;
    bool shuffleRemoval = false;

    if (argc > 1) itemCount = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    if (argc > 2) minLen = static_cast<unsigned int>(std::strtoul(argv[2], nullptr, 10));
    if (argc > 3) maxLen = static_cast<unsigned int>(std::strtoul(argv[3], nullptr, 10));
    if (argc > 4) seed = static_cast<std::uint64_t>(std::strtoull(argv[4], nullptr, 10));
    if (argc > 5) shuffleRemoval = (0 != std::atoi(argv[5]));

    if ((0 == minLen) || (minLen > maxLen))
    {
        std::fprintf(stderr, "Invalid min/max string length\n");
        return 1;
    }

    // SOME BASIC FUNCTION TESTS/DEMOS

    // Test/demo prefix tree operation
    const char* const addrs[] =
    {
        "192.168.1.11",
        "192.168.1.10",
        "192.168.1.0",
        "192.168.2.2",
        "192.168.3.3",
        "192.168.1.100",
        NULL
    };
    int count = 0;
    AddressTree addrTree;
    const char* const*  aptr = addrs;
    while (NULL != *aptr)
    {
        count++;
        printf("adding address %s\n", *aptr);
        AddressItem* addrItem = new AddressItem(*aptr++);
        if (3 == count)
        {
            addrItem->SetKeysize(24);
        }
        else if (5 == count)
        {
            addrItem->SetKeysize(16);
        }
        assert(addrItem->IsValid());
        addrTree.Insert(*addrItem);
    }

    ProtoAddress addr("192.168.2.99");
    //AddressItem* match = addrTree.FindClosestMatch(addr.GetRawHostAddress(), 8*addr.GetLength());
    AddressItem* match = addrTree.FindPrefix(addr.GetRawHostAddress(), 8*addr.GetLength());
    //AddressItem* match = addrTree.FindPrefixSubtree(addr.GetRawHostAddress(), 24);
    if (NULL != match)
    {
        printf("Best match is %s\n", match->GetAddress().GetHostString());
    }
    else
    {
        printf("No match to %s\n", addr.GetHostString());
    }


    // Test/demo lexical sorted items (e.g., strings)
    const char* const strings[] =
    {
        "smithson, bob",
        "smithson, jane",
        "smithson, john",
        "jones, brandy",
        "jones, tony",
        "smith, steve",
        NULL
    };
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
    index.Remove(*itemx);
    //iterator.SetCursor(itemx);
    iterator.Reset(true, itemx->string, 8*strlen(itemx->string));
    //iterator.Reset();//, item15->GetKey(), (sizeof(unsigned int) << 3) - 0);
    IndexedItem* item;
    //while (NULL != (item = iterator.GetNextItem()))
    while (NULL != (item = iterator.GetPrevItem()))
    {
        printf("got item %s\n", item->string);
        //index.Remove(*item);
    }


    // Test/demo floating point sorting
    FloatingTree floatTree;
    // Fill with random numbers
    double RANGE_MIN = -100.0;
    double RANGE_MAX = 100.0;
    srand((unsigned int)time(NULL));  // seed RNG
    FloatingItem* fitemx = NULL;
    for (int i = 0; i < 10; i++)
    {
        double value;
        if (0 == i)
            value = 0.0;
        else if (5 == i)
            value = -0.0;
        else
            value = ((double)rand() / RAND_MAX) * (RANGE_MAX - RANGE_MIN) -  (RANGE_MAX - RANGE_MIN)/ 2.0;
        FloatingItem* fitem = new FloatingItem(value);
        floatTree.Insert(*fitem);
        if (8 == i) fitemx = fitem;
    }
    FloatingTree::Iterator fiterator(floatTree);

    floatTree.Remove(*fitemx);
    //iterator.SetCursor(itemx);
    fiterator.Reset(true, fitemx->GetKey(), fitemx->GetKeysize());

    FloatingItem* fitem;
    while (NULL != (fitem = fiterator.GetNextItem()))
    //while (NULL != (fitem = fiterator.GetPrevItem()))
    {
        printf("got FloatingItem %f\n", fitem->GetValue());
        //index.Remove(*item);
    }


    // Test/demo Integer point sorting
    IntegerTree integerTree;
    // Fill with random numbers
    int NUM_MIN = -100;
    int NUM_MAX = 100;
    for (int i = 0; i < 10; i++)
    {
        int value;
        if (0 == i)
            value = 0;
        //else if (5 == i)
        //   value = INT_MAX;
        else
            value = (int)rand() % (NUM_MAX - NUM_MIN + 1) -  (NUM_MAX - NUM_MIN)/ 2;
        IntegerItem* intem = new IntegerItem(value);
        integerTree.Insert(*intem);
        //IntegerItem* intem2 = new IntegerItem(value);
        //integerTree.Insert(*intem2);
    }
    IntegerTree::Iterator interator(integerTree);
    IntegerItem* intem;
    while (NULL != (intem = interator.GetNextItem()))
    //while (NULL != (item = iterator.GetPrevItem()))
    {
        printf("got IntegerItem %d\n", intem->GetValue());
        //index.Remove(*item);
    }
    
    return 0;

    // BENCHMARK INSERT / REMOVAL
     BenchmarkResult result = BenchmarkProtoSortedTreeStrings(itemCount,
                                                             minLen,
                                                             maxLen,
                                                             seed,
                                                             shuffleRemoval);

    std::printf("ProtoSortedTree string benchmark\n");
    std::printf("  itemCount      : %zu\n", result.itemCount);
    std::printf("  minLen         : %u\n", result.minLen);
    std::printf("  maxLen         : %u\n", result.maxLen);
    std::printf("  seed           : %llu\n",
                static_cast<unsigned long long>(result.seed));
    std::printf("  shuffleRemoval : %s\n",
                result.shuffleRemoval ? "true" : "false");
    std::printf("  insertSeconds  : %.9f\n", result.insertSeconds);
    std::printf("  removeSeconds  : %.9f\n", result.removeSeconds);
    std::printf("  insertRate     : %.3f ops/sec\n",
                result.itemCount / result.insertSeconds);
    std::printf("  removeRate     : %.3f ops/sec\n",
                result.itemCount / result.removeSeconds);

    result = BenchmarkProtoTreeStrings(itemCount,
                                       minLen,
                                       maxLen,
                                       seed,
                                       shuffleRemoval);

    std::printf("ProtoTree string benchmark\n");
    std::printf("  itemCount      : %zu\n", result.itemCount);
    std::printf("  minLen         : %u\n", result.minLen);
    std::printf("  maxLen         : %u\n", result.maxLen);
    std::printf("  seed           : %llu\n",
                static_cast<unsigned long long>(result.seed));
    std::printf("  shuffleRemoval : %s\n",
                result.shuffleRemoval ? "true" : "false");
    std::printf("  insertSeconds  : %.9f\n", result.insertSeconds);
    std::printf("  removeSeconds  : %.9f\n", result.removeSeconds);
    std::printf("  insertRate     : %.3f ops/sec\n",
                result.itemCount / result.insertSeconds);
    std::printf("  removeRate     : %.3f ops/sec\n",
                result.itemCount / result.removeSeconds);

    result = BenchmarkProtoSortedTreeFloats(itemCount,
                                            minLen,
                                            maxLen,
                                            seed,
                                            shuffleRemoval);

    std::printf("ProtoTree double benchmark\n");
    std::printf("  itemCount      : %zu\n", result.itemCount);
    std::printf("  minLen         : %u\n", result.minLen);
    std::printf("  maxLen         : %u\n", result.maxLen);
    std::printf("  seed           : %llu\n",
                static_cast<unsigned long long>(result.seed));
    std::printf("  shuffleRemoval : %s\n",
                result.shuffleRemoval ? "true" : "false");
    std::printf("  insertSeconds  : %.9f\n", result.insertSeconds);
    std::printf("  removeSeconds  : %.9f\n", result.removeSeconds);
    std::printf("  insertRate     : %.3f ops/sec\n",
                result.itemCount / result.insertSeconds);
    std::printf("  removeRate     : %.3f ops/sec\n",
                result.itemCount / result.removeSeconds);

}  // end main()
