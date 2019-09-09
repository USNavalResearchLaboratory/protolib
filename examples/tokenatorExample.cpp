
#include "protoString.h"

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
    
}  // end main()
