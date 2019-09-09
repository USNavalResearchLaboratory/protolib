#include <stdio.h>

#include "protoJson.h"
#include "protoDebug.h"

int main(int argc, char* argv[])
{
    // Here we use the ProtoJson::Parser to parse JSON text from
    // STDIN. If a complete JSON document is processed, the resulting
    // document is printed to STDOUT in a pretty format.
    ProtoJson::Parser parser;
    ProtoJson::Parser::Status status = ProtoJson::Parser::PARSE_MORE;
    int result;
    char buffer[1024];
    while (0 != (result = fread(buffer, sizeof(char), 1023, stdin)))
    {
        status = parser.ProcessInput(buffer, result);
        // Note future extensions to parser could easily include
        // ongoing "stream" process with PARSE_DONE returned for
        // completed root level items.  I.e., an indefinite stream
        // of JSON content could be incrementally processed if
        // desired.
        if (ProtoJson::Parser::PARSE_ERROR == status) break;
    }
    
    if (ProtoJson::Parser::PARSE_DONE == status)
    {
    
        ProtoJson::Document* doc = parser.AccessDocument();
        
        // If uncommented, the code below adds a top level object 
        // to the loaded document to illustrate how a document can 
        // be built or, in this casee augmented.
        /*
        ProtoJson::Object* object = new ProtoJson::Object();
        ProtoJson::Array* array = new ProtoJson::Array();
        for (unsigned int i = 0; i < 10; i++)
        {
            if (0 != (i & 1))
            {
                // odd index array entries are integers
                ProtoJson::Number* number = new ProtoJson::Number((int)(i*2));
                array->AppendValue(*number);
            }
            else
            {
                // even index array entries are floating
                ProtoJson::Number* number = new ProtoJson::Number((double)i / 4.0);
                array->AppendValue(*number);
            }
        } 
        object->InsertEntry("exampleArray", *array);
        doc->AddItem(*object);
        */
                
        // Note the ProtoJson::Document::Print() method uses
        // the Document::Iterator class to do a depth-first
        // traversal of the JSON document content the parser loaded.
        // In the future, this example may be expanded to provide
        // more detailed illustration of the Document::Iterator _and_
        // methods to build ProtoJson::Document instances from 
        // scratch.
        doc->Print(stdout);
    }
    else if (ProtoJson::Parser::PARSE_MORE == status)
    {
        TRACE("jsonExample: input document was not complete?!\n");
    }
    else  // PARSE_ERROR
    {
        TRACE("jsonExample: error parsing input document!\n");
    }

    TRACE("jsonExample::Done.\n");
}  // end main()
