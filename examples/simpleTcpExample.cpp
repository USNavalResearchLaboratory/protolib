/**
 * @class ProtoSocketStandaloneExample
 *
 * @brief This file illustrates ProtoSocket stand-alone usage in a
 * very simple "client" or "server" application.  The  
 * ProtoSocket class is used in a non-async fashion.
 *
 * (Note the "protoExample.cpp" file contains a more complex
 *  example using Protolib sockets and timers asynchronously
 *  by deriving from the ProtoApp base class which uses
 *  a "ProtoDispatcher" instance in its main loop.)
 *
 * To build this "simple" app (after making libProtokit.a)  use:
 * g++ -DUNIX -DHAVE_IPV6 -o simple simpleTcpExample.cpp ../unix/libProtokit.a
 *
 * (it can also be similarly built with -DWIN32 instead of -DUNIX)
 */
#include "protoSocket.h"
#include <stdio.h>

inline void usage() 
{
    fprintf(stderr, "Usage: simple {listen <port> | connect <serverAddr> <serverPort>}\n");
}

int main(int argc, char* argv[])
{
    UINT16 serverPort = 0;
    ProtoAddress serverAddr;
    if (3 == argc)      // "simple listen <portNumber>"
    {
        if (!strncmp("listen", argv[1], strlen(argv[1])))
        {
            if (1 != (sscanf(argv[2], "%hu", &serverPort)))
            {
                fprintf(stderr, "simple: bad <port>\n");
                serverPort = 0;    
            }            
        }
    }
    else if (4 == argc) // "simple connect <serverAddr> <serverPort> 
    {
        if (!strncmp("connect", argv[1], strlen(argv[1])))
        {
            if (1 != (sscanf(argv[3], "%hu", &serverPort)))
            {
                fprintf(stderr, "simple: bad <serverPort>\n");
                serverPort = 0;    
            }            
            else if (!serverAddr.ResolveFromString(argv[2]))
            {
                fprintf(stderr, "simple: bad <serverAddr>\n");
                serverPort = 0;
            }  
            else
            {
                serverAddr.SetPort(serverPort);   
            } 
        }
    }   
    
    if (!serverAddr.IsValid() && (0 == serverPort))
    {
        usage();
        return -1;   
    }
        
    if (serverAddr.IsValid())  // connect to server address as a "client" and make a request
    {
        ProtoSocket clientSocket(ProtoSocket::TCP);
        fprintf(stderr, "simple: client connecting to server: %s/%hu ...\n", 
                        serverAddr.GetHostString(), serverAddr.GetPort());
        if (!clientSocket.Connect(serverAddr))
        {
            fprintf(stderr, "simple: error connecting to server: %s/%hu\n",
                            serverAddr.GetHostString(), 
                            serverAddr.GetPort());  
            return -1; 
        }
        
        fprintf(stderr, "simple: client sending request to server ...\n");
        const char* clientRequest = "Hello Server, this is a simple protolib client!";
        unsigned int length = strlen(clientRequest) + 1;
        unsigned int sent = 0;
        while (sent < length)
        {
            unsigned int numBytes = length - sent;
            if (!clientSocket.Send(clientRequest+sent, numBytes))
            {
                fprintf(stderr, "simple: error sending to server\n");
                clientSocket.Close();
                return -1;
            }
            else
            {
                sent += numBytes;
            }               
        }
        fprintf(stderr, "simple: client awaiting response from server ...\n");
        bool receiving = true;
        while (receiving)
        {
            char buffer[256];
            buffer[255] = '\0';
            unsigned int numBytes = 255;
            if (!clientSocket.Recv(buffer, numBytes))
            {
                fprintf(stderr, "simple: error receiving from server\n");
                clientSocket.Close();
                return -1;
            }
            else if (numBytes > 0)
            {
                fprintf(stdout, "simple: client recvd \"%s\" from server: %s/%hu\n",
                                buffer, 
                                serverAddr.GetHostString(),
                                serverAddr.GetPort());
            }
            else
            {
                fprintf(stderr, "simple: server shutdown connection.\n");
                receiving = false;
            }
        }  // end while(receiving)
        clientSocket.Close();
    }
    else  // act as a "server" listening to the indicated port, responding to requests
    {
        for (;;)
        {
            ProtoSocket serverSocket(ProtoSocket::TCP);
            if (!serverSocket.Listen(serverPort))
            {
                fprintf(stderr, "simple: server error listening\n");
                serverSocket.Close();
                return -1;
            }
            else
            {
                fprintf(stderr, "simple: server listening on port:%hu ... (use <CTRL-C> to exit)\n", serverPort);
                if (!serverSocket.Accept())
                {
                    fprintf(stderr, "simple: server error accepting connection\n");
                    serverSocket.Close();
                    return -1;  
                }     
                fprintf(stderr, "simple: server accepted connection from client: %s/%hu ...\n", 
                                 serverSocket.GetDestination().GetHostString(),
                                 serverSocket.GetDestination().GetPort());
                bool receiving = true;
                while (receiving)
                {
                    char buffer[256];
                    buffer[255] = '\0';
                    unsigned int numBytes = 255;
                    if (!serverSocket.Recv(buffer, numBytes))
                    {
                        fprintf(stderr, "simple: error receiving from client\n");
                        serverSocket.Close();
                        return -1;
                    }
                    else if (numBytes > 0)
                    {
                        fprintf(stdout, "simple: server recvd \"%s\" from client: %s/%hu\n",
                                        buffer, 
                                        serverSocket.GetDestination().GetHostString(),
                                        serverSocket.GetDestination().GetPort());
                        if (NULL != strchr(buffer, '!'))
                        {
                            fprintf(stderr, "simple: server recvd EOT character '!' from client ...\n");
                            receiving = false;   
                        }  
                        break;                             
                    }
                    else
                    {
                        fprintf(stderr, "simple: client closed connection\n");
                        receiving = false;
                    }
                }    
                fprintf(stderr, "simple: server sending response to client ...\n");
                const char* serverResponse = "Hi there Client, this is a simple protolib server";
                unsigned int length = strlen(serverResponse) + 1;
                unsigned int sent = 0;
                while (sent < length)
                {
                    unsigned int numBytes = length - sent;
                    if (!serverSocket.Send(serverResponse+sent, numBytes))
                    {
                        fprintf(stderr, "simple: error sending to client\n");
                        serverSocket.Close();
                        return -1;
                    }
                    else
                    {
                        sent += numBytes;
                    }               
                }
                serverSocket.Shutdown();
                // After "Shutdown" on a blocking socket, ProtoSocket::Recv() will unblock upon
                // receiving "FIN" from client TCP
                char buffer[8];
                unsigned int numBytes = 8;
                serverSocket.Recv(buffer, numBytes);
                if (0!= numBytes)
                    fprintf(stderr, "simple: server received extra data from client?\n\n");
                else
                    fprintf(stderr, "simple: server transmission complete.\n\n");
            }
            serverSocket.Close();
        }  // end for (;;)
    }
}  // end main();
