
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
        
        void MarkLogged() 
            {mark_logged = true;}  
        void UnmarkLogged()
            {mark_logged = false;}   
        bool WasLogged() const
            {return mark_logged;} 
        void MarkDeleted()
            {mark_deleted = true;} 
        bool WasDeleted() const
            {return mark_deleted;}
        
        void SetDeletionInfo(const char* file, int line)
        {
            if (NULL != file)
                strncpy(deletion_file, file, 255);
            else
                deletion_file[0] = '\0';
            deletion_line = line;
        }
        const char* GetDeletionFile() const
            {return deletion_file;}
        int GetDeletionLine() const
            {return deletion_line;}
        
        // These are provided to avoid new / delete within locks
        static ProtoCheckItem* Create(size_t size, const char* file, int line)
        {
            ProtoCheckItem* item = (ProtoCheckItem*)calloc(1, sizeof(ProtoCheckItem));
            item->mem_size = size;
            item->file_name[255] = '\0';
            item->line_num = line;
            item->mark_logged = false;
            item->mark_deleted = false;
            item->deletion_file[0] = item->deletion_file[255] = '\0';
            item->deletion_line = 0;
            return item;
        }
        static void Destroy(ProtoCheckItem* item)
            {free(item);}
                  
    private:
        size_t  mem_size;
        char    file_name[256];  // TBD PATH_MAX
        int     line_num;
        bool    mark_logged;
        bool    mark_deleted;
        char    deletion_file[256];
        int     deletion_line;
        
};  // end class ProtoCheckItem()

typedef std::map<const void* , ProtoCheckItem> ProtoCheckMap;

class ProtoChecker
{
    public:
        ProtoChecker();
        ~ProtoChecker();
        
        bool GetMapLock() const
            {return map_is_locked;}
        
        void AddItem(void* ptr, size_t size, const char* file, int line)
        {
            ProtoDispatcher::Lock(check_map_mutex);
            if (map_is_locked)
            {
                // ignore reentrant new/delete from ourselves
                ProtoDispatcher::Unlock(check_map_mutex);
                return;
            }
            map_is_locked = true;
            //ProtoCheckItem* item = ProtoCheckItem::Create(size, file, line);
            check_map[ptr] = ProtoCheckItem(size, file, line);
            map_is_locked = false;
            ProtoDispatcher::Unlock(check_map_mutex);
        } 
        
        void CacheInfo(const char* file, int line)
        {
            // Used to cache info for upcoming delete call
            ProtoDispatcher::Lock(check_map_mutex);
            if (cache_lock)
            {
                ProtoDispatcher::Unlock(check_map_mutex);
                return;
            }
            cache_lock = true;  // will reset in corresponding DeleteItem() call
            if (NULL != cached_file)
                free(cached_file);
            if (NULL != (cached_file = (char*)malloc((strlen(file)+1)*sizeof(char))))
                strcpy(cached_file, file);
            cached_line = line;
            //ProtoDispatcher::Unlock(check_map_mutex);
        }
        void ClearCacheInfo()
        {
            cached_file[0] = '\0';
            cached_line = 0;
        }
        const char* GetCachedFile() const
            {return cached_file;}
        int GetCachedLine() const
            {return cached_line;}
                
        void DeleteItem(void* ptr)
        {
            ProtoDispatcher::Lock(check_map_mutex);
            if (map_is_locked)
            {
                // ignore reentrant new/delete from ourselves
                ProtoDispatcher::Unlock(check_map_mutex);
                return;
            }
            map_is_locked = true;
            ProtoCheckMap::iterator it = check_map.find(ptr);
            const char* file = NULL;
            int line = 0;
            if (cache_lock)
            {
                file = GetCachedFile();
                line = GetCachedLine();
            }
            char text[512];    
            if (it != check_map.end())
            {
                //ProtoCheckItem::Destroy(it->second);
                /*sprintf(text, "found item %p to delete %s %d (allocated at %s %d)", 
                        ptr, file, line, it->second.GetFile(), it->second.GetLine());
                perror(text);
                it->second.SetDeletionInfo(GetCachedFile(), GetCachedLine());
                it->second.MarkDeleted();*/
                check_map.erase(ptr);
            }
            else
            {
                sprintf(text, "couldn't find item %p to delete %s %d", ptr, file, line);
                perror(text);
            }
            if (cache_lock)
            {
                //ClearCacheInfo();
                cache_lock = false;
                ProtoDispatcher::Unlock(check_map_mutex);
            }
            //check_map.erase(ptr);
            map_is_locked = false;
            ProtoDispatcher::Unlock(check_map_mutex);
        }
        
        void Reset()
        {
            ProtoDispatcher::Lock(check_map_mutex);
            if (map_is_locked)
            {
                // ignore reentrant new/delete from ourselves
                ProtoDispatcher::Unlock(check_map_mutex);
                return;
            }
            map_is_locked = true;
            check_map.clear();
            map_is_locked = false;
            ProtoDispatcher::Unlock(check_map_mutex);
        }
        void LogAllocations(FILE* filePtr);
        
    private:
        ProtoCheckMap           check_map;
        ProtoDispatcher::Mutex  check_map_mutex;
        bool                    map_is_locked; 
        bool                    cache_lock;
        char*                   cached_file;
        int                     cached_line;
};

ProtoChecker::ProtoChecker()
 : map_is_locked(false), cache_lock(false), cached_file(NULL), cached_line(0)
{
#ifdef UNIX
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&check_map_mutex, &attr);
#else
    ProtoDispatcher::Init(check_map_mutex);  // Windows critical sections are recursive
#endif // UNIX / WIN32
}

ProtoChecker::~ProtoChecker()
{
    TRACE("enter ProtoChecker destructor ...\n");
    check_map.clear();
    ProtoDispatcher::Destroy(check_map_mutex);
    TRACE("exit ProtoChecker destructor.n");
}

void ProtoChecker::LogAllocations(FILE* filePtr)
{
    ProtoDispatcher::Lock(check_map_mutex);
    for (ProtoCheckMap::iterator it=check_map.begin(); it!=check_map.end(); ++it)
    {
        const void* ptr = it->first;
        ProtoCheckItem& item = it->second;
        if (!item.WasLogged() || item.WasDeleted())
        {
            const char* action;
            const char* file;
            int line;
            if (item.WasDeleted())
            {
                action = "deleted";
                file = item.GetDeletionFile();
                line = item.GetDeletionLine();
            }
            else
            {
                action = "allocated";
                file = item.GetFile();
                line = item.GetLine();
            }
            int result  = fprintf(filePtr, "ProtoCheck: %s %lu bytes for ptr %p from %s:%d\n",
                                  action, (unsigned long)item.GetSize(), ptr, file, line); 
            if (result < 0)
            {
                char buffer[1024];
                sprintf(buffer, "ProtoCheck: %s %lu bytes for ptr %p from %s:%d",
                        action, (unsigned long)item.GetSize(), ptr, file, line); 
                perror(buffer); 
            }  
            //item.MarkLogged();
            if (item.WasDeleted())
            {
                check_map.erase(ptr);
            }
        }      
    }
    ProtoDispatcher::Unlock(check_map_mutex);
}  // end ProtoChecker::LogAllocations()

      
ProtoCheckItem::ProtoCheckItem()
 : mem_size(0), line_num(0), mark_logged(false), 
   mark_deleted(false), deletion_line(0)
{
    file_name[0] = file_name[255] = '\0';
    deletion_file[0] = deletion_file[255] = '\0';
}

ProtoCheckItem::ProtoCheckItem(size_t size, const char* file, int line)
 : mem_size(size), line_num(line), mark_logged(false), 
   mark_deleted(false), deletion_line(0)
{
    file_name[255] = '\0';
    strncpy(file_name, file, 255);
    deletion_file[0] = deletion_file[255] = '\0';
}

ProtoCheckItem::ProtoCheckItem(const ProtoCheckItem& item)
 : mem_size(item.mem_size), line_num(item.line_num), mark_logged(item.mark_logged),
   mark_deleted(item.mark_deleted), deletion_line(item.deletion_line)
{
    file_name[255] = '\0';
    strncpy(file_name, item.file_name, 255); 
    deletion_file[255] = '\0';
    strncpy(deletion_file, item.deletion_file, 255);
    
}

ProtoCheckItem& ProtoCheckItem::operator=(const ProtoCheckItem& item)
{
    mem_size = item.mem_size;
    strcpy(file_name, item.file_name); 
    line_num = item.line_num;
    mark_logged = item.mark_logged;
    mark_deleted = item.mark_deleted;
    strcpy(deletion_file, item.deletion_file);
    return *this;
}

ProtoCheckItem::~ProtoCheckItem()
{
}

#ifdef USE_PROTO_CHECK

static ProtoChecker proto_checker;

void ProtoCheckLogAllocations(FILE* filePtr)
{
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    proto_checker.LogAllocations(filePtr);
}  // end ProtoCheckLogAllocations()

void ProtoCheckResetLogging()
{
    TRACE("enter ProtoCheckResetLogging() ...\n");
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    proto_checker.Reset();
    TRACE("exit ProtoCheckResetLogging()\n");
}  // end ProtoCheckResetLogging()

void ProtoCheckCacheInfo(const char* file, int line)
{
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    //TRACE("caching deletion info %s %d ...\n", file, line);
    proto_checker.CacheInfo(file, line);
}  // end ProtoCheckCacheInfo()
       
void* operator new(size_t size, const char* file, int line)
{
    void* p = malloc(size);
    if (0 == p)
        throw std::bad_alloc();
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    proto_checker.AddItem(p, size, file, line);
    return p;
}

void* operator new[](size_t size, const char* file, int line)
{
    void* p = malloc(size);
    if (0 == p)
        throw std::bad_alloc();
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    proto_checker.AddItem(p, size, file, line);
    return p;
}

void operator delete(void* p) throw()
{
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    //TRACE("operator delete (map_lock:%d)\n", proto_checker.GetMapLock());
    proto_checker.DeleteItem(p);
    free(p);
}

void operator delete[](void* p) throw()
{
    //if (NULL == proto_checker) proto_checker = new ProtoChecker;
    //TRACE("operator delete[] (map_lock:%d)\n", proto_checker.GetMapLock());
    proto_checker.DeleteItem(p);
    free(p);
}
        
#endif // USE_PROTO_CHECK
