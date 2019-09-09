
#define _PROTO_CHECK_IMPL
#include "protoCheck.h"

#include "protoDispatcher.h"  // for ProtoDispatcher::Mutex to provide thread safety

#include <exception>
#include <cstdlib>

#include <stdio.h>  // for fprintf()
#include <string.h> // for strncpy()
#include <map>      // for std::map<>

class ProtoCheckItem
{
    public:
        ProtoCheckItem();
        ProtoCheckItem(size_t size, const char* file, int line);
        ProtoCheckItem(const ProtoCheckItem& item);
        ~ProtoCheckItem();
        
        ProtoCheckItem& operator=(const ProtoCheckItem& item);
        
        size_t GetSize() const
            {return mem_size;}
        const char* GetFile() const
            {return file_name;}
        int GetLine() const
            {return line_num;}
        
        static class ProtoChecker* proto_checker;
                  
    private:
        size_t  mem_size;
        char    file_name[256];  // TBD PATH_MAX
        int     line_num;
        
};  // end class ProtoCheckItem()

typedef std::map<const void* , ProtoCheckItem> ProtoCheckMap;


class ProtoChecker
{
    public:
        ProtoChecker();
        ~ProtoChecker();
        
        void AddItem(void* ptr, size_t size, const char* file, int line)
        {
            ProtoDispatcher::Lock(check_map_mutex);
            check_map[ptr] = ProtoCheckItem(size, file, line);
            ProtoDispatcher::Unlock(check_map_mutex);
        }
        void DeleteItem(void* ptr)
        {
            //fprintf(stderr, "proto check deleting item %p\n", ptr);
            ProtoDispatcher::Lock(check_map_mutex);
            check_map.erase(ptr);
            ProtoDispatcher::Unlock(check_map_mutex);
        }
        
        void LogAllocations(FILE* filePtr);
        
    private:
        ProtoCheckMap           check_map;
        ProtoDispatcher::Mutex  check_map_mutex;
};


ProtoChecker* ProtoCheckItem::proto_checker = NULL;

ProtoChecker::ProtoChecker()
{
}

ProtoChecker::~ProtoChecker()
{
}

void ProtoChecker::LogAllocations(FILE* filePtr)
{
    ProtoDispatcher::Lock(check_map_mutex);
    for (ProtoCheckMap::iterator it=check_map.begin(); it!=check_map.end(); ++it)
    {
        const void* ptr = it->first;
        ProtoCheckItem& item = it->second;
        fprintf(filePtr, "ProtoCheck: alloc of %lu bytes for ptr %p from %s:%d\n",
                        (unsigned long)item.GetSize(), ptr, item.GetFile(), item.GetLine()); 
    }
    ProtoDispatcher::Unlock(check_map_mutex);
}  // end ProtoChecker::LogAllocations()

void ProtoCheckLogAllocations(FILE* filePtr)
{
    if (NULL != ProtoCheckItem::proto_checker)
        ProtoCheckItem::proto_checker->LogAllocations(filePtr);
}  // end ProtoCheckLogAllocations()

ProtoCheckItem::ProtoCheckItem()
 : mem_size(0), line_num(0)
{
    file_name[0] = '\0';
}

ProtoCheckItem::ProtoCheckItem(size_t size, const char* file, int line)
 : mem_size(size), line_num(line)
{
    file_name[255] = '\0';
    strncpy(file_name, file, 255);
}

ProtoCheckItem::ProtoCheckItem(const ProtoCheckItem& item)
 : mem_size(item.mem_size), line_num(item.line_num)
{
    file_name[255] = '\0';
    strncpy(file_name, item.file_name, 255); 
    
}

ProtoCheckItem& ProtoCheckItem::operator=(const ProtoCheckItem& item)
{
    mem_size = item.mem_size;
    strcpy(file_name, item.file_name); 
    line_num = item.line_num;
    return *this;
}

ProtoCheckItem::~ProtoCheckItem()
{
}


#ifdef USE_PROTO_CHECK

void* operator new(size_t size, const char* file, int line)
{
    void* p = malloc(size);
    if (0 == p)
        throw std::bad_alloc();
    if (NULL == ProtoCheckItem::proto_checker)
        ProtoCheckItem::proto_checker = new ProtoChecker();
    ProtoCheckItem::proto_checker->AddItem(p, size, file, line);
    return p;
}

void operator delete(void* p) throw()
{
    if (NULL != ProtoCheckItem::proto_checker)
        ProtoCheckItem::proto_checker->DeleteItem(p);
    free(p);
}

void* operator new[](size_t size, const char* file, int line)
{
    void* p = malloc(size);
    if (0 == p)
        throw std::bad_alloc();
    if (NULL == ProtoCheckItem::proto_checker)
        ProtoCheckItem::proto_checker = new ProtoChecker();
    ProtoCheckItem::proto_checker->AddItem(p, size, file, line);
    return p;
}

void operator delete[](void* p) throw()
{
    if (NULL != ProtoCheckItem::proto_checker)
        ProtoCheckItem::proto_checker->DeleteItem(p);
    free(p);
}

#endif // USE_PROTO_CHECK
