// This examples tests and demonstrates a number of the
// ProtoFile features including directory iteration, etc.

#include "protoFile.h"
#include <stdio.h>
#ifdef WIN32
#include <windows.h>  // for Sleep()
#else  // UNIX
#include <unistd.h>  // for sleep()
#endif // if/else WIN32/UNIX

// Usage:  fileTest path1 [path2  path3 ...]

int main(int argc, char* argv[])
{
    
    ProtoFile::PathList pathList;
    
    for (int i = 1; i < argc; i++)
    {
        if (!pathList.AppendPath(argv[i]))
        {
            fprintf(stderr, "fileTest error: unable to append item\n");
            return -1;
        }
    }    
    
    ProtoFile::PathList::PathIterator iterator(pathList, true);
    
    // Note PathIterator also has GetNextDirectory() and GetNextPath() methods
    // for other purposes
    
    char path[PATH_MAX + 1];
    while (iterator.GetNextFile(path))
    {
        const ProtoFile::Path* parentPath = iterator.GetCurrentPathItem();
        printf("1st iteration  to \"%s\" (parentPath: %s )\n", path, parentPath->GetPath());
    }
    
    fprintf(stderr, "fileTest: sleeping 5 seconds before 2nd iteration for updated files\n");

#ifdef WIN32
	Sleep(5000);
#else  // UNIX
    sleep(5);  // This provides an opportunity to touch a file to illustrate "updateOnly" iteration 
#endif  // if/else WIN32/UNIX

    iterator.Reset();
    while (iterator.GetNextFile(path))
        printf("2nd iteration  to \"%s\"\n", path);
    iterator.Init(true);
    
}  // end main()

