
#include "protoString.h"
#include "protoDefs.h"  // for PATH_MAX
#include <ctype.h>
#include <string.h>

int main(int argc, char* argv[])
{
    const char* text1 = " trains, ,planes, motorcycles  , trucks, cars";
    const char* text2 = "trains  planes   motorcycles  trucks cars ";
    const char* text3 = NULL;
    char token = ' ';
            
    if (argc > 1) text3 = argv[1];
    if (argc > 2) token = argv[2][0];
    
    printf("\nComma-delimited, stripped tokenization of:\n    \"%s\"\n", text1);
    ProtoTokenator tk(text1, ',', true);
    const char* item;
    while (NULL != (item = tk.GetNextItem()))
        printf("\"%s\"\n", item);
    
    printf("\nReverse, comma-delimited tokenization of:\n    \"%s\"\n", text1);
    ProtoTokenator tkr(text1, ',', false, 0, true);
    while (NULL != (item = tkr.GetNextItem()))
        printf("\"%s\"\n", item);
    
    printf("\nSpace-delimited, stripped tokenization of:\n    \"%s\"\n", text2);
    tk.Reset(text2, ' ');
    while (NULL != (item = tk.GetNextItem()))
        printf("\"%s\"\n", item);
    
    printf("\nReverse, space-delimited tokenization of:\n    \"%s\"\n", text2);
    tkr.Reset(text2, ' ');
    while (NULL != (item = tkr.GetNextItem()))
        printf("\"%s\"\n", item);
    
    if (NULL != text3)
    {
        if (isspace(token))
            printf("\nWhitespace-delimited, stripped tokenization of:\n    \"%s\"\n", text3);
        else
            printf("\n'%c'-delimited, stripped tokenization of:\n    \"%s\"\n", token, text3);
        tk.Reset(text3, token);
        while (NULL != (item = tk.GetNextItem()))
            printf("\"%s\"\n", item);
        if (isspace(token))
            printf("\nReverse, whitespace-delimited, stripped tokenization of:\n    \"%s\"\n", text3);
        else
            printf("\nReverse, '%c'-delimited, tokenization of:\n    \"%s\"\n", token, text3);
        tkr.Reset(text3, token);
        // This demos the 'detach' option and the need to delete[] the returned text item
        while (NULL != (item = tkr.GetNextItem(true)))
        {
            printf("\"%s\"\n", item);
            delete[] item;
        }
    }    
    
    // This section illustrates using ProtoTokenator to help parse file path strings
    // (Note the constant "PROTO_PATH_DELIMITER" is available for cross-platform use)
    // The dirname/basename here works like Python os.path.dirname() and os.path.basename(),
    // i.e., not exactly like Unix basename() function
    const char* pathList[] = 
    {
        "/usr/include/stdio.h",
        "/usr/local/include//"
    };
    char dirname[PATH_MAX + 1];
    char basename[PATH_MAX + 1];
    for (int i = 0; i < sizeof(pathList)/sizeof(pathList[0]); i++)
    {
        const char* thePath = pathList[i];
        // Note we "stripTokens" to remove any extra trailing path delimiters on dirname
        // delimiters on paths
        ProtoTokenator pk(thePath, '/', true, 1, true, true);
        const char* item = pk.GetNextItem();  // reverse gets basename first
        if (NULL != item)
            strncpy(basename, item, PATH_MAX);
        else
            basename[0] = '\0';
        item = pk.GetNextItem();  // reverse gets dirname second (if present)
        if (NULL != item)
            strncpy(dirname, item, PATH_MAX);
        else
            dirname[0] = '\0';
        printf("path: \"%s\" dirname: \"%s\" basename: \"%s\"\n", thePath, dirname, basename);
    }
    
}  // end main()
