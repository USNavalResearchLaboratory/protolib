 
#include "protoPipe.h"
#include <stdio.h>  // for stderr output as needed
#include <stdlib.h> // for atoi()

void Usage()
{
    fprintf(stderr, "Usage: pcmd <pipeName> commands ...\n");
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "pcmd error: insufficient arguments!\n");
        Usage();
        return -1;
    }
    // Build command
    char cmdBuffer[8192];
    unsigned int cmdLen = 0;
    cmdBuffer[0] = '\0';
    for (int i = 2; i < argc; i++)
    {
        size_t argLen = strlen(argv[i]);
        if ((cmdLen + argLen) > 8192)
        {
            fprintf(stderr, "pcmd error: command is too large!\n");
            return -1;
        }
        else
        {
            strcat(cmdBuffer, argv[i]);
            cmdLen += argLen;
        }
        if ((i+1) < argc)
        {
            if (cmdLen < 8192)
            {
                strcat(cmdBuffer, " ");
                cmdLen++;
            }
            else
            {
                fprintf(stderr, "pcmd error: command message is too large!\n");
                return -1;
            }
        }
    }
    
    // Open pipe and send command
    ProtoPipe msgPipe(ProtoPipe::MESSAGE);
    if (!msgPipe.Connect(argv[1]))
    {
        fprintf(stderr, "pcmd error: error connecting to pipe \"%s\"\n", argv[1]);
        return -1;
    }
    if (!msgPipe.Send(cmdBuffer, cmdLen))
    {
        fprintf(stderr, "pcmd error: error sending to pipe \"%s\"\n", argv[1]);
        return -1;
    }
    msgPipe.Close();
    
    return 0;
}
