#include "protoApp.h"
#include "protoNet.h"
#include "protoRouteMgr.h"
#include "testFuncs.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

class UnitTests :
    public ProtoApp
{
public:
    UnitTests();
    ~UnitTests();
    
    /**
     * Override from ProtoApp 
     */
    bool OnStartup(int argc, const char*const* argv);
    /**
     * Override from ProtoApp 
     */
    bool ProcessCommands(int argc, const char*const* argv);
    /**
     * Override from ProtoApp 
     */
    void OnShutdown();
    
    bool DumpIfaceInfo();
    
    bool AddrTests();

    bool BasicAddrTests(const char* val, int port, ProtoAddress::Type type, bool mcast, bool bcast, bool loop, bool linkLocal, bool siteLocal, bool unspec,bool unicast);

    bool EqualityTests();

    bool EqualityTests(ProtoAddress& addr, ProtoAddress& cmpAddr, int equality);

    bool PrefixTests();

    bool AddrListTests();

    bool ManetMsgTests();

    int PrintList(ProtoAddressList& theList);
    
private:
    enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
    static CmdType GetCmdType(const char* string);
    bool OnCommand(const char* cmd, const char* val);        
    void Usage();
    
    static const char* const CMD_LIST[];
    int addr_listSize;
    // ProtoTimer/ UDP socket demo members
    ProtoTimer          tx_timer;
    ProtoAddress        dst_addr;
    ProtoAddressList    addr_list;

}; // end class UnitTests


// Our application instance 
PROTO_INSTANTIATE_APP(UnitTests) 

UnitTests::UnitTests() : addr_listSize(0)
{    
}

UnitTests::~UnitTests()
{
    
}

void UnitTests::Usage()
{
    fprintf(stderr, "unitTests [debug <n>][addr][dumpIfaces][manetMsgTests]\n");
    
}  // end UnitTests::Usage()


const char* const UnitTests::CMD_LIST[] =
{
    "-help",       // usage
    "+debug",      // Debug level
    "-addr",       // Run ProtoAddress tests
    "-manetMsgTests",   // Run manet message tests
    "-dumpIfaces",  // Get all of our ifaces and dump some info (name, MAC addr, IP addrs, etc)
    NULL
};

bool UnitTests::DumpIfaceInfo()
{

    // Here's some code to test the ProtoSocket routines for network interface info       

    ProtoAddress localAddress;
    char nameBuffer[256];
    nameBuffer[255] = '\0';
    
    if (localAddress.ResolveLocalAddress())
    {
        TRACE("unitTests: local default IP address: %s\n", localAddress.GetHostString());
        if (localAddress.ResolveToName(nameBuffer, 255))
            TRACE("unitTests: local default host name: %s\n", nameBuffer);
        else
            TRACE("unitTests: unable to resolve local default IP address to name\n");
    }      
    else
    {
        TRACE("unitTests: unable to determine local default IP address\n");
    }
    
    // Get all of our ifaces and dump some info (name, MAC addr, IP addrs, etc)
    unsigned int ifaceCount = ProtoNet::GetInterfaceCount();
    if (ifaceCount > 0)
    {
        // Allocate array to hold the indices
        unsigned int* indexArray = new unsigned int[ifaceCount];
        if (NULL == indexArray)
        {
            PLOG(PL_ERROR, "unitTests: new indexArray[%u] error: %s\n", ifaceCount, GetErrorString());
            return false;
        }
        if (ProtoNet::GetInterfaceIndices(indexArray, ifaceCount) != ifaceCount)
        {
            PLOG(PL_ERROR, "unitTests: GetInterfaceIndices() error?!\n");
            delete[] indexArray;
            return false;
        }
        for (unsigned int i = 0; i < ifaceCount; i++)
        {
            unsigned int index = indexArray[i];
            TRACE("unitTests: interface index %u ", index);
            // Get the iface name
            if (ProtoNet::GetInterfaceName(index, nameBuffer, 255))
                TRACE("has name \"%s\" ", nameBuffer);
            else
                TRACE("name \"unknown\" ");
            // Get MAC addr for the iface
            ProtoAddress ifaceAddr;
            if (ProtoNet::GetInterfaceAddress(nameBuffer, ProtoAddress::ETH, ifaceAddr))
                TRACE("with MAC addr %s\n", ifaceAddr.GetHostString());
            else
                TRACE("with no MAC addr\n");
            
            ProtoAddressList addrList;
            // Get IPv4 addrs for the iface
            if (ProtoNet::GetInterfaceAddressList(nameBuffer, ProtoAddress::IPv4, addrList))
            {
                TRACE("          IPv4 addresses:");
                ProtoAddressList::Iterator iterator(addrList);
                while (iterator.GetNextAddress(ifaceAddr))
                {
					TRACE(" %s/%d", ifaceAddr.GetHostString(),ProtoNet::GetInterfaceAddressMask(nameBuffer, ifaceAddr));
                    if (ifaceAddr.IsLinkLocal()) TRACE(" (link local)");
                }
                TRACE("\n");
            }
            addrList.Destroy();
            // Get IPv6 addrs for the iface
            if (ProtoNet::GetInterfaceAddressList(nameBuffer, ProtoAddress::IPv6, addrList))
            {
                TRACE("          IPv6 addresses:");
                ProtoAddressList::Iterator iterator(addrList);
                while (iterator.GetNextAddress(ifaceAddr))
                {

                    TRACE(" %s/%d", ifaceAddr.GetHostString(), ProtoNet::GetInterfaceAddressMask(nameBuffer, ifaceAddr));

					if (ifaceAddr.IsLinkLocal()) TRACE(" (link local)");
                }
                TRACE("\n");
            }
            addrList.Destroy();

			// Test our interface add routine
            /*
			char * ifaceName = "192.168.1.6";
			ProtoAddress tmpAddr;
			tmpAddr.ConvertFromString("192.168.1.6");
			int maskLen = 24;
			bool result = ProtoNet::AddInterfaceAddress(ifaceName, tmpAddr, maskLen);
            */


#ifndef WIN32
            // This code should work for Linux and BSD (incl. Mac OSX)
            // Get IPv4 group memberships for interfaces
            if (ProtoNet::GetGroupMemberships(nameBuffer, ProtoAddress::IPv4, addrList))
            {
                TRACE("          IPv4 memberships:");
                ProtoAddressList::Iterator iterator(addrList);
                ProtoAddress groupAddr;
                while (iterator.GetNextAddress(groupAddr))
                {
                    TRACE(" %s", groupAddr.GetHostString());
                }       
                TRACE("\n");
            }
            addrList.Destroy();
            if (ProtoNet::GetGroupMemberships(nameBuffer, ProtoAddress::IPv6, addrList))
            {
                TRACE("          IPv6 memberships:");
                ProtoAddressList::Iterator iterator(addrList);
                ProtoAddress groupAddr;
                while (iterator.GetNextAddress(groupAddr))
                {
                    TRACE(" %s", groupAddr.GetHostString());
                }       
                TRACE("\n");
            }
            addrList.Destroy();
#endif // WIN32
        }
        delete[] indexArray;
    }
    else
    {
        TRACE("unitTests: host has no network interfaces?!\n");
    }  
    // Here's some code to get the system routing table
    ProtoRouteTable routeTable;
    ProtoRouteMgr* routeMgr = ProtoRouteMgr::Create();
    if (NULL == routeMgr)
    {
        PLOG(PL_ERROR, "UnitTests::OnStartup() error creating route manager\n");
        return false;
    }
    if (!routeMgr->Open())
    {
        PLOG(PL_ERROR, "UnitTests::OnStartup() error opening route manager\n");
        return false;    
    }
    if (!routeMgr->GetAllRoutes(ProtoAddress::IPv4, routeTable))
        PLOG(PL_ERROR, "UnitTests::OnStartup() warning getting system routes\n");     
    // Display whatever routes we got
    ProtoRouteTable::Iterator iterator(routeTable);
    ProtoRouteTable::Entry* entry;
    PLOG(PL_ALWAYS, "IPv4 Routing Table:\n");
    PLOG(PL_ALWAYS, "%16s/Prefix %-12s   ifIndex   Metric\n", "Destination", "Gateway");
    while (NULL != (entry = iterator.GetNextEntry()))
    {
        PLOG(PL_ALWAYS, "%16s/%-3u     ",
                entry->GetDestination().GetHostString(), entry->GetPrefixSize());
        const ProtoAddress& gw = entry->GetGateway();
        PLOG(PL_ALWAYS, "%-16s %-02u     %d\n",
                gw.IsValid() ? gw.GetHostString() : "0.0.0.0", 
                entry->GetInterfaceIndex(),
                entry->GetMetric());
    }
    //ProtoAddress addr;
    //addr.ResolveFromString("10.1.2.3");
    //ProtoAddress gw;
    //routeMgr->SetRoute(addr, 32, gw, 4, 0);
    routeMgr->Close();

    return true;
}

UnitTests::CmdType UnitTests::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end UnitTests::GetCmdType()

bool UnitTests::OnStartup(int argc, const char*const* argv)
{
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "unitTests: Error! bad command line\n");
        return false;
    }  
    
    return true;
}  // end UnitTests::OnStartup()

void UnitTests::OnShutdown()
{
    PLOG(PL_INFO, "unitTests: Done.\n");

    CloseDebugLog();
}  // end UnitTests::OnShutdown()

bool UnitTests::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class UnitTests command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "UnitTests::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);

                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "UnitTests::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "UnitTests::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }

    }
    return true;  
}  // end UnitTests::ProcessCommands()

bool UnitTests::OnCommand(const char* cmd, const char* val)
{

    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "UnitTests::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("addr", cmd, len))
    {
        if (!AddrTests())
        {
            PLOG(PL_ERROR,"FAIL: AddrTests() \n\n");
        }
    }
    else if (!strncmp("manetMsgTests",cmd,len))
    {
        if (!ManetMsgTests())
        {
            PLOG(PL_ERROR,"FAIL: ManetMsgTests()\n\n");
        }
    }
    else if (!strncmp("dumpIfaces",cmd,len))
        {
            if (!DumpIfaceInfo())
            {
                PLOG(PL_ERROR,"FAIL: DumpIfaceInfo()\n\n");
            }
        }
    else if (!strncmp("debug",cmd,len))
    {
        SetDebugLevel(atoi(val));
    }
    else if (!strncmp("help",cmd,len))
    {
        Usage();
    }
    return true;
}  // end UnitTests::OnCommand()

bool UnitTests::EqualityTests()
{
    bool result = true;
    // Test equality 
    ProtoAddress addr = ProtoAddress("10.0.0.10");
    ProtoAddress cmpAddr = ProtoAddress("10.0.0.10");
    char nameBuffer[256];
    nameBuffer[255] = '\0';
    char cmpNameBuffer[256];
    cmpNameBuffer[255] = '\0';

    // addr == cmpAddr
    if (!EqualityTests(addr,cmpAddr,0))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d == cmpAddr %s/%d (default port assignment)\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
    }
    addr.SetPort(5000);
    cmpAddr.SetPort(5000);
    // addr == cmpAddr, ports equal
    if (!EqualityTests(addr,cmpAddr,0))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d == cmpAddr %s/%d (port assignment)\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
    }
    addr.SetPort(5000);
    cmpAddr.SetPort(5001);
    // addr == cmpAddr, ports !=
    // We have the same address but different ports, check address equality
    if (addr.IsEqual(cmpAddr) 
        || addr==cmpAddr
        || !(addr!=cmpAddr))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d == cmpAddr %s/%d port mismatch should not pass equality test\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
    } 

    cmpAddr.Reset(ProtoAddress::IPv4,0);
    cmpAddr.ResolveFromString("10.0.0.11");
    // addr < cmpAddr
     if (!EqualityTests(addr,cmpAddr,-1))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d < cmpAddr %s/%d \n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
    }

     cmpAddr.Reset(ProtoAddress::IPv4,0);
     cmpAddr.ResolveFromString("192.168.0.1");
     // addr < cmpAddr different networks
     if (!EqualityTests(addr,cmpAddr,-1))
     {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d < cmpAddr %s/%d (different networks) \n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
     }
     addr.Reset(ProtoAddress::IPv4,0);
     addr.ResolveFromString("10.0.0.1");
     cmpAddr.Reset(ProtoAddress::IPv4,0);
     cmpAddr.ResolveFromString("10.0.0.0");
     // addr > cmpAddr
     if (!EqualityTests(addr,cmpAddr,1))
     {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d > cmpAddr %s/%d \n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
     }

     addr.Reset(ProtoAddress::IPv4,0);
     addr.ResolveFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
     cmpAddr.Reset(ProtoAddress::IPv4,0);
     cmpAddr.ResolveFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
     // addr == cmpAddr
     if (!EqualityTests(addr,cmpAddr,0))
     {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d == cmpAddr %s/%d (IPv6)\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
     }

     addr.Reset(ProtoAddress::IPv6,0);
     addr.ResolveFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
     cmpAddr.Reset(ProtoAddress::IPv6,0);
     cmpAddr.ResolveFromString("2001:0db8:85a3:0000:0000:8a2e:0380:7334");
     // addr < cmpAddr
     if (!EqualityTests(addr,cmpAddr,-1))
     {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d < cmpAddr %s/%d (IPv6)\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
     }

     addr.Reset(ProtoAddress::IPv6,0);
     addr.ResolveFromString("2001:0db8:85a3:0000:0000:8a2e:0470:7334");
     cmpAddr.Reset(ProtoAddress::IPv6,0);
     cmpAddr.ResolveFromString("2001:0db8:85a3:0000:0000:8a2e:0380:7334");
     // addr > cmpAddr
     if (!EqualityTests(addr,cmpAddr,1))
     {
        result = false;
        PLOG(PL_ERROR,"FAIL: EqualityTests() addr %s/%d > cmpAddr %s/%d (IPv6)\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());
     }

    return result;
} // UnitTests:EqualityTests

bool UnitTests::EqualityTests(ProtoAddress& addr, ProtoAddress& cmpAddr, int equality)
{
/*
 * equality 0 == addresses are equal
 *          1 == addr > cmpAddr
 *          -1 == addr < cmpAddr
 */

    char nameBuffer[256];
    nameBuffer[255] = '\0';
    char cmpNameBuffer[256];
    cmpNameBuffer[255] = '\0';

    PLOG(PL_INFO,"INFO: EqualityTests() addr %s/%d == cmpAddr %s/%d (default port assignment) equality>%d\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort(),equality);

    const char* test = "EqualityTests(addr,cmpAddr,equality)";
    bool result = true;

    if (addr.IsEqual(cmpAddr))
    {
        if (equality != 0)
        {
            PLOG(PL_ERROR,"FAIL: %s IsEqual()\n",test);
            result = false;
        }
    } else
    {
        if (equality == 0)
        {
            PLOG(PL_ERROR,"FAIL: %s IsEqual()\n",test);
            result = false;
        }

    }

    if (addr.HostIsEqual(cmpAddr))
    {
        if (equality != 0)
        {
            PLOG(PL_ERROR,"FAIL: %s HostIsEqual()\n",test);
            result = false;
        }
    } else
    {
        if (equality == 0)
        {
            PLOG(PL_ERROR,"FAIL: %s HostIsEqual()\n",test);
            result = false;
        }
    }
    if (addr.CompareHostAddr(cmpAddr) == 0)
    {
        if (equality != 0)
        {
            PLOG(PL_ERROR,"FAIL: %s CompareHostAddr() %d\n",test,addr.CompareHostAddr(cmpAddr));
            result = false;
        }
    } else
    {
        if (equality == 0)
        {
            PLOG(PL_ERROR,"FAIL: %s CompareHostAddr() %d\n",test,addr.CompareHostAddr(cmpAddr));
            result = false;
        }
    }
    if (addr==cmpAddr)
    {
        if (equality != 0)
        {
            PLOG(PL_ERROR,"FAIL: %s addr ==\n",test);
            result = false;
        }
    } else
    {
        if (equality == 0)
        {
            PLOG(PL_ERROR,"FAIL: %s addr ==\n",test);
            result = false;
        }
    }
    if (addr!=cmpAddr)
    {
        if (equality == 0)
        {
            PLOG(PL_ERROR,"FAIL: %s addr != \n",test);
            result = false;
        }
    } else
    {
        if (equality != 0)
        {
            PLOG(PL_ERROR,"FAIL: %s !=\n",test);
            result = false;
        }
    }
    if (addr<cmpAddr)
    {
        if (equality != -1)
        {
            PLOG(PL_ERROR,"FAIL: %s addr < \n",test);
            result = false;
        }
    } else
    {
        if (equality == -1)
        {
            PLOG(PL_ERROR,"FAIL: %s addr <\n",test);
            result = false;
        }
    }
    if (addr>=cmpAddr)
    {
        if ((equality != 0) && (equality != 1))
        {
            PLOG(PL_ERROR,"FAIL: %s addr >= %d \n",test,addr>=cmpAddr);
            result = false;
        }
    } else
    {
        if ((equality == 0) || (equality == 1))
        {
            PLOG(PL_ERROR,"FAIL: %s addr >= %d \n",test,addr>=cmpAddr);
            result = false;
        }
    } 
    if (addr<=cmpAddr)
    {
        if ((equality != 0) && (equality != -1))
        {
            PLOG(PL_ERROR,"FAIL: %s addr <= %d\n",test,addr<=cmpAddr);
            result = false;
        }
    } else
    {
        if ((equality == 0) || (equality == -1))
        {
            PLOG(PL_ERROR,"FAIL: %s addr <= %d\n",test,addr<=cmpAddr);
            result = false;
        }
    }
    if (result == false)
    {
        PLOG(PL_ERROR,"FAIL: EqualityTests(%s/%d,%s/%d,%d) module failure \n\n", addr.GetHostString(nameBuffer,255),addr.GetPort(),cmpAddr.GetHostString(cmpNameBuffer,255),cmpAddr.GetPort());

    }
    // TODO: Force equality on comparisons
    return result;
}
bool UnitTests::PrefixTests()
{
    const char* test = "PrefixTests()";
    bool result = true;
    char nameBuffer[256];
    nameBuffer[255] = '\0';
    char cmpNameBuffer[256];
    cmpNameBuffer[255] = '\0';

    ProtoAddress addr = ProtoAddress("169.254.0.1");
    ProtoAddress cmpAddr = ProtoAddress("169.254.0.2");

    ProtoAddress subnetAddr;
    addr.GetSubnetAddress(8,subnetAddr);
    if (strcmp(subnetAddr.GetHostString(),"169.0.0.0"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s Subnet %s != 169.0.0.0\n",test,subnetAddr.GetHostString());
    }
    addr.GetSubnetAddress(16,subnetAddr);
    if (strcmp(subnetAddr.GetHostString(),"169.254.0.0"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s GetSubnetAddress %s != 169.254.0.0 \n",test,subnetAddr.GetHostString());
    }
    ProtoAddress bcastAddr;
    addr.GetBroadcastAddress(16,bcastAddr);
    if (strcmp(bcastAddr.GetHostString(),"169.254.255.255"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s GetBroadcastAddress %s != 169.254.255.255 \n",test,bcastAddr.GetHostString());
    }
    if (!addr.PrefixIsEqual(cmpAddr,16))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s PrefixIsEqual/16 \n",test);
    }    

    cmpAddr = ProtoAddress("192.168.1.1");
    if (addr.PrefixIsEqual(cmpAddr,8))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s PrefixIsEqual/8 \n",test);
    }    

    addr = ProtoAddress("169.254.0.1");
    cmpAddr = ProtoAddress("169.254.0.2");

    addr.SetCommonHead(cmpAddr);
    if (strcmp(addr.GetHostString(),"169.254.0.0"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetCommonHead %s != 169.254.0.0\n",test,addr.GetHostString());
    }

    addr = ProtoAddress("169.254.0.1");
    cmpAddr = ProtoAddress("169.132.0.2");

    addr.SetCommonHead(cmpAddr);
    if (strcmp(addr.GetHostString(),"169.0.0.0"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetCommonHead %s != 169.0.0.0\n",test,addr.GetHostString());
    }

    addr = ProtoAddress("169.254.1.12");
    cmpAddr = ProtoAddress("169.132.1.12");

    addr.SetCommonTail(cmpAddr);
    if (strcmp(addr.GetHostString(),"0.0.1.12"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetCommonTail %s != 0.0.1.12\n",test,addr.GetHostString());
    }

    addr = ProtoAddress("fe80::426c");
    cmpAddr = ProtoAddress("fe80::423c");

    addr.SetCommonHead(cmpAddr);
    if (strcmp(addr.GetHostString(),"fe80::4200"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetCommonHead %s != fe80::4200 \n",test,addr.GetHostString());
    }

    // This works!
    addr = ProtoAddress("2001:db8:85a3:1212:2121:8a2e:3701:7334");
    cmpAddr = ProtoAddress("2001:db8:85a3:2121:1212:8a2e:3701:7334");
    addr.SetCommonTail(cmpAddr);
    if (strcmp(addr.GetHostString(),"::8a2e:3701:7334"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetCommonTail %s\n",test,addr.GetHostString());
    }


    // This returns the host in ipv4 notation..
    addr = ProtoAddress("2001:db8:85a3:1212:2121:8a2e:3701:7334");
    cmpAddr = ProtoAddress("2001:db8:85a3:2121:1212:8a2f:3701:7334");
    addr.SetCommonTail(cmpAddr);
    if (strcmp(addr.GetHostString(),"::3701:7334"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetCommonTail %s\n",test,addr.GetHostString());
    }

    addr.Invalidate();
    cmpAddr.Invalidate();
    //addr = ProtoAddress("192.168.1.12");
    addr.ApplyPrefixMask(16);
    if (strcmp(addr.GetHostString(),"192.168.0.0"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ApplyPrefixMask \n",test);
    }
    addr = ProtoAddress("192.168.1.12");
    addr.ApplySuffixMask(16);
    if (strcmp(addr.GetHostString(),"0.0.1.12"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ApplyPrefixMask \n",test);
    }

    addr = ProtoAddress("132.250.5.100");
    char* theName = new char[20];
    addr.ResolveToName(theName,20);
    if (strcmp(theName,"www.nrl.navy.mil"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ResolveToName %s\n",test,addr.GetHostString());
    }

    addr.Reset(ProtoAddress::IPv4,0);
    addr.ResolveFromString("www.nrl.navy.mil");
    if (strcmp(addr.GetHostString(),"132.250.5.100"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ResolveFromString %s\n",test,addr.GetHostString());
    }

    addr.Reset(ProtoAddress::IPv4,0);    
    addr.ConvertFromString("192.168.1.6");
    if (strcmp(addr.GetHostString(),"192.168.1.6"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ConvertFromString %s\n",test,addr.GetHostString());
    }
    addr.Reset(ProtoAddress::IPv6,0);    
    addr.ConvertFromString("2001:db8:85a3:0000:0000:8a2e:370:7334");
    if (strcmp(addr.GetHostString(),"2001:db8:85a3::8a2e:370:7334"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ConvertFromString %s\n",test,addr.GetHostString());
    }
    addr.Reset(ProtoAddress::IPv4,0);    
    addr.ResolveEthFromString("18:36:F3:98:4F:9A");
    if (strcasecmp(addr.GetHostString(),"18:36:F3:98:4F:9A"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ResolveEthFromString %s\n",test,addr.GetHostString());
    }

    addr.Reset(ProtoAddress::IPv4,0);    
    addr = ProtoAddress("192.168.1.12");
    if (addr.GetEndIdentifier() != 3232235788) 
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s GetEndIdentifier() %lu %s\n",test,addr.GetEndIdentifier(),addr.GetHostString());
    }
    addr.Reset(ProtoAddress::IPv4,0);
    addr.SetEndIdentifier(3232235788);
    if ((addr.GetEndIdentifier() == 3232235788) && (strcmp(addr.GetHostString(),"192.168.1.12")))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetEndIdentifier() %lu %s\n",test,addr.GetEndIdentifier(),addr.GetHostString());

    }   

    addr.Reset(ProtoAddress::IPv4,0);    
    addr = ProtoAddress("192.168.1.12");
    if ((addr.GetEndIdentifier() == 3232235788) && (strcmp(addr.GetHostString(),"192.168.1.12")))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s IPv4GetAddress() %lu %s\n",test,addr.IPv4GetAddress(),addr.GetHostString());
    }

    addr.Reset(ProtoAddress::IPv4,0);    
    addr = ProtoAddress("192.168.1.12");
    const char* rawAddress = addr.GetRawHostAddress();
    cmpAddr.Reset(ProtoAddress::IPv4,0);
    cmpAddr.SetRawHostAddress(ProtoAddress::IPv4,rawAddress,4);
    if (strcmp(cmpAddr.GetHostString(),"192.168.1.12"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s Set/GetRawHostAddress %s %s\n",test,addr.GetRawHostAddress(),addr.GetHostString());
    }

    addr.Invalidate();    
    addr = ProtoAddress("192.168.1.12");
    const struct sockaddr& theSockAddr= addr.GetSockAddr();

    ProtoAddress tmpAddr;
    tmpAddr.SetSockAddr(theSockAddr);
    if (strcmp(tmpAddr.GetHostString(),"192.168.1.12"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s SetSockAddr() %s\n", test, tmpAddr.GetHostString());
    }

    cmpAddr.Invalidate();
    cmpAddr.SetSockAddr(tmpAddr.AccessSockAddr());
    if (strcmp(cmpAddr.GetHostString(),"192.168.1.12"))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s AccessSockAddr() failed to return correct addr %s\n", test, cmpAddr.GetHostString());
    }

    /*
    ProtoAddress maskAddr;
    UINT8 maskLen = 8;
    maskAddr.GeneratePrefixMask(ProtoAddress::IPv4,maskLen);
    PLOG(PL_ERROR,"maskLen>%d prefixLen>%d %s \n",maskLen,addr.GetPrefixLength(),maskAddr.GetHostString());

    maskLen = 16;
    maskAddr.GeneratePrefixMask(ProtoAddress::IPv4,maskLen);
    PLOG(PL_ERROR,"maskLen>%d prefixLen>%d %s \n",maskLen,addr.GetPrefixLength(),maskAddr.GetHostString());

    ProtoAddress mcastAddr;
    addr = ProtoAddress("224.225.1.2");
    addr.GetEthernetMulticastAddress(mcastAddr);
    PLOG(PL_ERROR,"FAIL: GetEthernetMulticastAddress %s\n",mcastAddr.GetHostString());

    SIMULATED ADDRESSES?

    check copyconstructors equality tests work

  */  
    if (result == false)
    {        
        PLOG(PL_ERROR,"FAIL: PrefixTests() module failure \n\n");
    }

    return result;
}

bool UnitTests::BasicAddrTests(const char* val, int port, ProtoAddress::Type type, bool mcast, bool bcast, bool loop, bool linkLocal, bool siteLocal, bool unspec, bool unicast)
{
    const char* test = "BasicAddrTests()";
    ProtoAddress addr;
    char nameBuffer[256];
    nameBuffer[255] = '\0';
    char cmpNameBuffer[256];
    cmpNameBuffer[255] = '\0';

    bool result = true;

    if (type != ProtoAddress::ETH)
    {
        if (!addr.ResolveFromString(val))
        {
            PLOG(PL_ERROR,"FAIL: %s ResolveFromString(%s)\n",test,val);
            result = false;
        }
    }
    else
    {
        // Resolve from string doesn't work for eth?
        addr = ProtoAddress(val);
    }

    if (addr.GetType() != type)
    {
        PLOG(PL_ERROR,"FAIL: %s %d type != %d \n", test, type, addr.GetType());
        result = false;
    }
    addr.SetPort(port);    

    if (addr.GetPort() != port)
    {
        PLOG(PL_ERROR,"FAIL: %s port %d != addr.GetPort(%d)\n",test, port, addr.GetPort());
        result = false;
    }

    // Check out some constructors - by reference
    ProtoAddress cmpAddr = ProtoAddress(addr);
    if (strcasecmp(addr.GetHostString(nameBuffer,255),cmpAddr.GetHostString(cmpNameBuffer,255)))
    {
        PLOG(PL_ERROR,"FAIL: %s ProtoAddress(ProtoAddress& addr) addr %s != cmpAddr %s\n", test, addr.GetHostString(nameBuffer,255),cmpAddr.GetHostString(cmpNameBuffer,255));
        result = false;
    }

    if (!cmpAddr.IsValid())
    {
        PLOG(PL_ERROR,"FAIL: %s ProtoAddress(addr) %s is not valid\n",test, cmpAddr.GetHostString());
        result = false;
    }
    // by string
    cmpAddr = ProtoAddress(val); 
    if (strcasecmp(addr.GetHostString(nameBuffer,255),cmpAddr.GetHostString(cmpNameBuffer,255)))
    {
        PLOG(PL_ERROR,"FAIL: %s ProtoAddress(char* addr) addr %s != cmpAddr %s)\n",test, val,cmpAddr.GetHostString());
        result = false;
    }
    if (!cmpAddr.IsValid())
    {
        PLOG(PL_ERROR,"FAIL: %s ProtoAddress(*addr) constructor %s is not valid\n",test, val);
        result = false;
    }

    // Test our length/type settings from resolveFromString?
    switch (type)
    {
    case ProtoAddress::IPv4:
        if (addr.GetLength(type) != 4)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.getLength(IPV4) ! valid for type %d \n",test, addr.GetLength());
            result = false;
        }
        break;
    case ProtoAddress::IPv6:
        if (addr.GetLength(type) != 16)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.getLength(IPV6) ! valid for type %d \n", test, addr.GetLength());
            result = false;
        }
        break;
    case ProtoAddress::ETH:
        if (addr.GetLength(type) != 6)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.getLength(ETH) ! valid for type %d \n",test, addr.GetLength());
            result = false;
        }
        // case SIM not implemented
        break;
    default:
        PLOG(PL_ERROR,"FAIL: %s no valid type available val: %s\n",test, val);
        result = false;
    }
    switch (type)
    {
    case ProtoAddress::IPv4:
        if (addr.GetLength() != 4)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.getLength() ! valid for type IPV4 %d \n",test, addr.GetLength());
            result = false;
        }
        break;
    case ProtoAddress::IPv6:
        if (addr.GetLength() != 16)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.getLength() ! valid for type IPV6 %d \n", test, addr.GetLength());
            result = false;
        }
        break;
    case ProtoAddress::ETH:
        if (addr.GetLength() != 6)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.getLength() ! valid for type ETH %d \n",test, addr.GetLength());
            result = false;
        }
        break;
        // case SIM not implemented
    default:
        PLOG(PL_ERROR,"FAIL: %s no valid type available val: %s\n",test, val);
        result = false;
    }
    switch (type)
    {
    case ProtoAddress::IPv4:
        if (addr.GetType(addr.GetLength()) != ProtoAddress::IPv4)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.GetType(addr.getLength()) ! valid for type IPV4 %d \n",test, addr.GetLength());
            result = false;
        }
        break;
    case ProtoAddress::IPv6:
        if (addr.GetType(addr.GetLength()) != ProtoAddress::IPv6)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.GetType(addr.getLength()) ! valid for type IPV6 %d \n",test,  addr.GetLength());
            result = false;
        }
        break;
    case ProtoAddress::ETH:
        if (addr.GetType(addr.GetLength()) != ProtoAddress::ETH)
        {
            PLOG(PL_ERROR,"FAIL: %s addr.GetType(addr.getLength()) ! valid for type ETH %d \n",test, addr.GetLength());
            result = false;
        }
        // case SIM not implemented
        break;
    default:
        PLOG(PL_ERROR,"FAIL: %s no valid type available val: %s\n",test, val);
        result = false;
    }
    if (port != addr.GetPort())
    {
        PLOG(PL_ERROR,"FAIL: %s port %s != getPort(%d)\n",test, port, addr.GetPort());
        result = false;
    }

    if (addr.IsMulticast() != mcast)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed mcast test\n",test, val);
        result = false;
    }
    if (addr.IsBroadcast() != bcast)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed broadcast test\n",test, val);
        result = false;
    }

    if (addr.IsLoopback() != loop)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed loopback test\n",test, val);
        result = false;
    }

    if (addr.IsLinkLocal() != linkLocal)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed linkLocal test\n",test, val);
        result = false;
    }

    if (addr.IsSiteLocal() != siteLocal)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed siteLocal test\n",test, val);
        result = false;
    }

    if (addr.IsUnspecified() != unspec)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed unspec test\n",test, val);
        result = false;
    }
    if (addr.IsUnicast() != unicast)
    {
        PLOG(PL_ERROR,"FAIL: %s %s failed unicast test addr %d input %d\n",test, val,addr.IsUnicast(),unicast);
        result = false;
    }

    addr_list.Insert(addr);
    addr_listSize++;

        const char* addrtype;

    if (result == false)
    {
        // Isn't there already a function for this?
        switch (type)
        {
        case ProtoAddress::IPv4:
            addrtype = "IPv4";
            break;
        case ProtoAddress::IPv6:
            addrtype = "IPv6";
            break;
        case ProtoAddress::ETH:
            addrtype = "ETH";
            break;
        case ProtoAddress::SIM:
            addrtype = "SIM";
            break;
        default:
            addrtype = "No address type";
            
        }
    }
    if (result == false)
    {
        PLOG(PL_ERROR,"FAIL: BasicAddTests() addr>%s port>%d type>%s mcast>%d bcast>%d loopback>%d linkLocal>%d siteLocal>%d unspecified>%d unicast>%d module failure \n\n",val,port,addrtype,mcast,bcast,loop,linkLocal, siteLocal,unspec,unicast);
    }

    return result;
} // BasicAddresTests
int UnitTests::PrintList(ProtoAddressList& theList)
{
    ProtoAddressList::Iterator iterator(theList);
    ProtoAddress iterAddr;
    int listSize = 0;
    while (iterator.GetNextAddress(iterAddr))
    {
        PLOG(PL_INFO,"NextAddr %s\n",iterAddr.GetHostString());
        listSize++;
    }

    return listSize;
}
bool UnitTests::AddrListTests()
{
    const char* test = "AddrListTests()";
    bool result = true;
    if (addr_list.IsEmpty())
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_list.IsEmpty() \n",test);
    }
    ProtoAddress firstAddr;
    if (!addr_list.GetFirstAddress(firstAddr))
    {        
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_list.GetFirstAddress()\n",test);
    }
    PLOG(PL_INFO,"FirstAddr %s\n",firstAddr.GetHostString());

    int listSize = PrintList(addr_list);
    // Does list size equal what we originally added?
    if (addr_listSize != listSize)
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_listSize %d != listSize %d\n",test,addr_listSize, listSize);
    }
    PLOG(PL_INFO,"FirstAddr %s\n",firstAddr.GetHostString());

    // Does address list contain first address?
    if (!addr_list.Contains(firstAddr))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_list.contains(%s) (firstAddr) \n",test,firstAddr.GetHostString());
    }
    ProtoAddress fakeAddr; 
    fakeAddr.ResolveFromString("192.168.1.1");
    if (addr_list.Contains(fakeAddr))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_list.contains(%s) (fakeAddr) \n",test,fakeAddr.GetHostString());
    }
    // Insert fakeAddr
    if (!addr_list.Insert(fakeAddr))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_list.insert(fakeAddr)  \n",test);
    }
    // Remove fakeAddr - is it gone?
    addr_list.Remove(fakeAddr);
    if (addr_list.Contains(fakeAddr))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s addr_list.contains(%s) (removed fakeAddr) \n",test,fakeAddr.GetHostString());
    }
    // Create a second list to add to the first
    ProtoAddressList new_addr_list;
    new_addr_list.Insert(fakeAddr);
    fakeAddr.ResolveFromString("192.168.1.2");
    new_addr_list.Insert(fakeAddr);
    fakeAddr.ResolveFromString("192.168.1.3");
    new_addr_list.Insert(fakeAddr);
    fakeAddr.ResolveFromString("192.168.1.4");
    new_addr_list.Insert(fakeAddr);


    int new_listSize = PrintList(new_addr_list);

    // add new list to original list
    addr_list.AddList(new_addr_list);

    int combined_listSize = PrintList(addr_list);

    PLOG(PL_INFO,"Combined list size %d new list size %d\n",combined_listSize,new_listSize);
    if (combined_listSize != addr_listSize + new_listSize)
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s combined list (%d) != (addr_list(%d) + new_list(%d))\n",
             test,combined_listSize,addr_listSize, new_listSize);
    }
    // Now test that adding a list only adds new items

    // Add a new element to the new_addrList
    fakeAddr.ResolveFromString("192.168.1.5");
    new_addr_list.Insert(fakeAddr);

    // Add new list a second time - only 192.168.1.5 should be added    
    addr_list.AddList(new_addr_list);
    combined_listSize = PrintList(addr_list);

    PLOG(PL_INFO,"Combined list size %d new list size %d addr_list %d\n",combined_listSize,new_listSize, addr_listSize);

    if (combined_listSize != addr_listSize + new_listSize + 1)
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s combined list (%d) != (addr_list(%d) + new_list(%d) + 1)\n",
             test,combined_listSize,addr_listSize, new_listSize);
    }

    // Now remove the new_addr_list
    addr_list.RemoveList(new_addr_list);
    combined_listSize = PrintList(addr_list);
    PLOG(PL_INFO,"Combined list size after removal %d \n",combined_listSize);

    // New list size should equal original list size
    if (combined_listSize != addr_listSize)
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s combined list (%d) != (addr_list(%d) + new_list(%d) + 1) \n",
             test,combined_listSize,addr_listSize, new_listSize);
    }
    fakeAddr.ResolveFromString("192.168.1.6");
    addr_list.Insert(fakeAddr,"eth0");

    char* text = (char*)(addr_list.GetUserData(fakeAddr));
    if (strcasecmp(text,"eth0"))
    {
        PLOG(PL_ERROR,"FAIL: %s %s addr_list.GetUserData() %s != eth0\n",test,text);
        result = false;
    }
    if (result == false)
    {
        PLOG(PL_ERROR,"FAIL: AddrListTest() module failiure \n\n");
    }

    return result;
}

bool UnitTests::AddrTests()
{
    bool result = true;
    char nameBuffer[256];
    nameBuffer[255] = '\0';

    const char* test = "AddrTests()";
    PLOG(PL_ERROR,"INFO: %s Need to add GeneratePrefixMask() test\n",test);
    PLOG(PL_ERROR,"INFO: %s Need to add GetEthernetMulciastAddress() test\n",test);
    PLOG(PL_ERROR,"INFO: %s Need to test SIM addresses \n",test);
    const char* eth = "18:36:f3:98:4F:9A";
    ProtoAddress addr;

    if (!addr.ResolveFromString(eth))
    {
        PLOG(PL_ERROR,"INFO: %s ResolveFromString(%s) not working for ETH addresses.  Should it?\n",test,eth);
    }
    addr.Invalidate();
    addr.ResolveLocalAddress(nameBuffer,20);
    {
        // TODO: automate
        // Planned failure
        PLOG(PL_ERROR,"INFO: ** User Action ** Verify that local host name and address are correct\n");
        PLOG(PL_ERROR,"      ResolveLocalAddress %s name %s\n\n",addr.GetHostString(), nameBuffer);
    }

    // Look up public nrl address & test get ResolveToName
    addr.Invalidate();
    const char* val = "132.250.5.100";

    if (!addr.ResolveFromString(val))
    {
        PLOG(PL_ERROR,"FAIL: AddrTests() ResolveFromString val %s hostName: %s %s\n", val, nameBuffer,addr.GetHostString());
        result = false;
    }

    if (!addr.ResolveToName(nameBuffer,255))
    {
        PLOG(PL_ERROR,"FAIL: AddrTests() ResolveToName val %s hostName: %s %s\n", val, nameBuffer,addr.GetHostString());
        result = false;
    }
    // ipv6 node address
    val = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv6,false,false,false,false,false,false,true))
    {
        result = false;
    }

    // ipv4 node address
    val = "192.168.10.1";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,false,false,false,false,false,false,true))
    {
        result = false;
    }

    // ipv6 mcast
    val = "FF01:0:0:0:0:0:0:1";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv6,true,false,false,false,false,false,false))
    {
        result = false;
    }

    // ipv4 mcast
    val = "224.225.1.2";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,true,false,false,false,false,false,false))
    {
        result = false;
    }

    // NO ipv6 broadcast address

    // ipv4 broadcast
    val = "255.255.255.255";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,false,true,false,false,false,false,false))
    {
        result = false;
    }

    // ipv6 loopback
    val = "::1";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv6,false,false,true,false,false,false,true))
    {
        result = false;
    }

    // ipv4 loopback
    val = "127.0.0.1";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,false,false,true,false,false,false,true))
    {
        result = false;
    }

    // ipv6 link local (prefix 1111111010)
    val = "fe80::426c:8fff:fe31:4f8e";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv6,false,false,false,true,false,false,true))
    {
        result = false;
    }

    // ipv4 link local 
    val = "169.254.0.1";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,false,false,false,true,false,false,true))
    {
        result = false;
    }    

    // ipv6 site local (first 10 bits are 1111111011)
    val = "fec0::426c:8fff:fe31:4f8e";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv6,false,false,false,false,true,false,true))
    {
        result = false;
    }

    // ipv4 site local (no IPv4 site local)
    val = "10.0.0.1";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,false,false,false,false,false,false,true))
    {
        result = false;
    }
    
    // ipv6 unspecified
    val = "00::";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv6,false,false,false,false,false,true,false))
    {
        result = false;
    }

    // ipv4 unspecified
    val = "0.0.0.0";
    if (!BasicAddrTests(val,5000,ProtoAddress::IPv4,false,false,false,false,false,true,false))
    {
        result = false;
    }

    // eth addr
    val = "18:36:F3:98:4F:9A";
    if (!BasicAddrTests(val,0,ProtoAddress::ETH,false,false,false,false,false,false,true))
    {
        result = false;
    }

    if (!AddrListTests())
    {
        result = false;
    }

    if (!EqualityTests())
    {
        result = false;
    }

    if (!PrefixTests())
    {
        result = false;
    }

    return result;
}

bool UnitTests::ManetMsgTests()
{
    bool result = true;
    const char* test = "ManetMsgTests()";

    UINT32 buffer[PACKET_SIZE_MAX/4];
    ManetPkt pkt;
    if (!BuildPacket((UINT32*)buffer,pkt))
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s BuildPacket() failed \n",test);
    }

    // OK, let's parse a "received" packet using sent "buffer" and "pktLen"
    PLOG(PL_INFO,"msgTests: Parsing \"recvPkt\" ...\n");
    UINT16 pktLen = pkt.GetLength();

    if (GetDebugLevel() == PL_INFO) 
        MakeDump((char*)buffer, pktLen);

    if (ParseBuffer(buffer, pktLen) != 0)
    {
        result = false;
        PLOG(PL_ERROR,"FAIL: %s ParseBuffer() failed \n",test);
    }

    PLOG(PL_INFO,"msgTests: \"recvPkt\" packet parsing completed.\n\n");

    PLOG(PL_INFO,"Generated buffer:\n");
    if (GetDebugLevel() == PL_INFO) 
        MakeDump((char*)buffer, pktLen);

    PLOG(PL_INFO,"msgTests: Read in reference packet dump \n");

    UINT32 refBuffer[1024];
    unsigned int refLen = 0;

    char data[] = "04 00 02 06 00 01 83 00 45 c0 a8 01 01 00 09 01 10 01 01 02 10 02 00 02 08 c0 02 c0 a8 01 01 02 03 04 05 06 07 08 09 00 15 04 14 08 02 03 02 03 02 03 02 03 03 30 02 05 01 02 05 10 01 01 04 80 03 c0 a8 02 02 03 04 05 00 00 ";
    char* referenceDump = data;

    unsigned int val;
    int offset;
    while (1 == sscanf(referenceDump,"%x%n",&val,&offset))
    {
        ((char*)refBuffer)[refLen++] = (char)val;
        referenceDump += offset;
    }

    if (refLen == 0)
    {
        PLOG(PL_ERROR,"FAIL: %s ParseBuffer(%s) failed \n",test,data);
        result = false;
    }

    PLOG(PL_INFO,"Reference buffer:\n");
    if (GetDebugLevel() == PL_INFO) 
        MakeDump((char*)refBuffer,refLen);
 
    // TODO: compare results of referencePktDump w/ours and move common code out of
    // message example into unittests?

    // Compare our buffer with reference buffer
    if (pktLen == refLen)
    {
        if (memcmp((const char*)buffer,(const char*)refBuffer,pktLen) != 0)
        {
            PLOG(PL_ERROR,"FAIL: %s ParseBuffer() buffer != refBuffer\n",test);
            result = false;
        }
    } else
    {
        PLOG(PL_ERROR,"FAIL: %s Parsed msg buffer != reference buffer\n",test);
        result = false;
    }

    return result;

}


       

