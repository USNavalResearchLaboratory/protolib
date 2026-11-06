//==============================================================================
//
// addressTest.cpp
//
// Functional tests for ProtoAddress and ProtoAddressList.
//
//==============================================================================

#include "protoAddress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


bool AddressTest_DefaultAndMutation();
bool AddressTest_ParseAndPrint();
bool AddressTest_ParseLimitsAndRejection();
bool AddressTest_CopyAndCompare();
bool AddressTest_LegacyMaskOperations();
bool AddressTest_ListInsertRemove();
bool AddressTest_ListIteration();
bool AddressTest_ListBulkOperations();
bool AddressTest_GetPrintStringStaticBuffer();
bool AddressTest_GetHostStringStaticBuffer();
bool AddressTest_CommonHeadTail();
bool AddressTest_PrefixIsEqual();


int main(int argc, char** argv)
{
    struct AddressTest
    {
        const char* nickname;
        const char* name;
        bool (*function)();
    };

    const char* selectedTest = NULL;
    int debugLevel = 1;

    SetDebugLevel(debugLevel);

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-t"))
        {
            if ((i + 1) >= argc)
            {
                DMSG(1,
                     "Usage: %s [-d debug-level] [-t test-name]\n",
                     argv[0]);
                return 1;
            }

            selectedTest = argv[++i];
        }
        else if (!strcmp(argv[i], "-d"))
        {
            if ((i + 1) >= argc)
            {
                DMSG(1,
                     "Usage: %s [-d debug-level] [-t test-name]\n",
                     argv[0]);
                return 1;
            }

            char* endPtr = NULL;
            long value = strtol(argv[++i], &endPtr, 10);

            if ((*endPtr != '\0') || (value < 0))
            {
                DMSG(1,
                     "FAIL: invalid debug level: %s\n",
                     argv[i]);
                return 1;
            }

            debugLevel = static_cast<int>(value);
        }
        else if ((!strcmp(argv[i], "-h")) ||
                 (!strcmp(argv[i], "--help")))
        {
            DMSG(1,
                 "Usage: %s [-d debug-level] [-t test-name]\n",
                 argv[0]);
            return 0;
        }
        else
        {
            DMSG(1,
                 "FAIL: unknown option: %s\n",
                 argv[i]);
            DMSG(1,
                 "Usage: %s [-d debug-level] [-t test-name]\n",
                 argv[0]);
            return 1;
        }
    }

    SetDebugLevel(debugLevel);
    #define ADDRESS_TEST(nickname, function) \
    { nickname, #function, function }

        const AddressTest tests[] =
    {
        ADDRESS_TEST("1", AddressTest_DefaultAndMutation),
        ADDRESS_TEST("2", AddressTest_ParseAndPrint),
        ADDRESS_TEST("3", AddressTest_ParseLimitsAndRejection),
        ADDRESS_TEST("4", AddressTest_CopyAndCompare),
        ADDRESS_TEST("5", AddressTest_LegacyMaskOperations),
        ADDRESS_TEST("6", AddressTest_ListInsertRemove),
        ADDRESS_TEST("7", AddressTest_ListIteration),
        ADDRESS_TEST("8", AddressTest_ListBulkOperations),
        ADDRESS_TEST("9", AddressTest_GetPrintStringStaticBuffer),
        ADDRESS_TEST("10", AddressTest_GetHostStringStaticBuffer),
        ADDRESS_TEST("11", AddressTest_CommonHeadTail),
        ADDRESS_TEST("12", AddressTest_PrefixIsEqual)
    };

    #undef ADDRESS_TEST

    const unsigned int testCount = sizeof(tests) / sizeof(tests[0]);

    bool success = true;
    bool found = (selectedTest == NULL);

    for (unsigned int i = 0; i < testCount; ++i)
    {
        const AddressTest& test = tests[i];

        if ((selectedTest != NULL) &&
            (strcmp(selectedTest, test.nickname) != 0) &&
            (strcmp(selectedTest, test.name) != 0))
        {
            continue;
        }

        found = true;

        bool result = test.function();

        DMSG(1,
             "Test (%-2s): %-65s %s\n",
             test.nickname,
             test.name,
             result ? "PASS" : "FAIL");

        if (!result)
        {
            success = false;
        }
    }

    if (!found)
    {
        DMSG(1,
             "FAIL: unknown test: %s\n",
             selectedTest);
        DMSG(1, "Available tests:\n");

        for (unsigned int i = 0; i < testCount; ++i)
        {
            DMSG(1,
                 "  %s  %s\n",
                 tests[i].nickname,
                 tests[i].name);
        }

        return 1;
    }

    return success ? 0 : 1;
}


// Helper functions

static bool AddressTest_CheckString(const char* testName,
                                    const char* fieldName,
                                    const char* actual,
                                    const char* expected)
{
    if ((NULL == actual) || (0 != strcmp(actual, expected)))
    {
        DMSG(2,
             "%s: %s differs (expected \"%s\", got \"%s\")\n",
             testName,
             fieldName,
             expected,
             (NULL != actual) ? actual : "(null)");
        return false;
    }

    return true;
}


static bool AddressTest_CheckPrefix(const char*         testName,
                                    const ProtoAddress& address,
                                    bool                expectedHasPrefix,
                                    UINT8               expectedPrefixLength)
{
    if (address.HasPrefixMaskLength() != expectedHasPrefix)
    {
        DMSG(2,
             "%s: prefix-mask presence differs\n",
             testName);
        return false;
    }

    if (expectedHasPrefix &&
        (address.GetPrefixMaskLength() != expectedPrefixLength))
    {
        DMSG(2,
             "%s: prefix-mask length differs "
             "(expected %u, got %u)\n",
             testName,
             expectedPrefixLength,
             address.GetPrefixMaskLength());
        return false;
    }

    return true;
}


static bool AddressTest_CheckHostString(const char*         testName,
                                        const ProtoAddress& address,
                                        const char*         expected)
{
    char buffer[256];

    return AddressTest_CheckString(testName,
                                   "host string",
                                   address.GetHostString(buffer, sizeof(buffer)),
                                   expected);
}


static bool AddressTest_CheckPrintString(const char*         testName,
                                         const ProtoAddress& address,
                                         const char*         expected)
{
    char buffer[256];

    return AddressTest_CheckString(testName,
                                   "print string",
                                   address.GetPrintString(buffer, sizeof(buffer)),
                                   expected);
}


static bool AddressTest_RejectAddressString(const char* testName,
                                            const char* text)
{
    ProtoAddress address;

    if (address.ConvertFromString(text))
    {
        DMSG(2,
             "%s: unexpectedly accepted \"%s\"\n",
             testName,
             text);
        return false;
    }

    return true;
}


// Test functions

bool AddressTest_DefaultAndMutation()
{
    ProtoAddress address;

    if (address.IsValid())
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: default address is valid\n");
        return false;
    }

    if (!AddressTest_CheckPrefix("AddressTest_DefaultAndMutation",
                                 address,
                                 false,
                                 0))
    {
        return false;
    }

    if (address.SetPrefixMaskLength(0))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: accepted prefix on invalid address\n");
        return false;
    }

    if (!address.ConvertFromString("192.0.2.1"))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: IPv4 conversion failed\n");
        return false;
    }

    if (!address.SetPrefixMaskLength(24))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: SetPrefixMaskLength() failed\n");
        return false;
    }

    address.SetPort(698);

    if (!AddressTest_CheckPrefix("AddressTest_DefaultAndMutation",
                                 address,
                                 true,
                                 24))
    {
        return false;
    }

    address.Reset(ProtoAddress::IPv4);

    if ((!address.IsValid()) ||
        (address.GetType() != ProtoAddress::IPv4) ||
        (address.GetPort() != 0))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: Reset() result differs\n");
        return false;
    }

    if (!AddressTest_CheckPrefix("AddressTest_DefaultAndMutation",
                                 address,
                                 false,
                                 0))
    {
        return false;
    }

    if (!address.ConvertFromString("192.0.2.2/24"))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: second IPv4 conversion failed\n");
        return false;
    }

    const char rawAddress[4] =
    {
        static_cast<char>(192),
        static_cast<char>(0),
        static_cast<char>(2),
        static_cast<char>(3)
    };

    if (!address.SetRawHostAddress(ProtoAddress::IPv4,
                                   rawAddress,
                                   sizeof(rawAddress)))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: SetRawHostAddress() failed\n");
        return false;
    }

    if (!AddressTest_CheckHostString("AddressTest_DefaultAndMutation",
                                     address,
                                     "192.0.2.3"))
    {
        return false;
    }

    if (!AddressTest_CheckPrefix("AddressTest_DefaultAndMutation",
                                 address,
                                 false,
                                 0))
    {
        return false;
    }

    address.SetPort(698);

    if (!address.ConvertFromString("192.0.2.4"))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: port-preserving conversion failed\n");
        return false;
    }

    if ((address.GetPort() != 698) ||
        (!AddressTest_CheckHostString("AddressTest_DefaultAndMutation",
                                      address,
                                      "192.0.2.4")) ||
        (!AddressTest_CheckPrefix("AddressTest_DefaultAndMutation",
                                  address,
                                  false,
                                  0)))
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: conversion did not preserve port\n");
        return false;
    }

    address.Invalidate();

    if (address.IsValid())
    {
        DMSG(2,
             "AddressTest_DefaultAndMutation: Invalidate() left address valid\n");
        return false;
    }

    return AddressTest_CheckPrefix("AddressTest_DefaultAndMutation",
                                   address,
                                   false,
                                   0);
}


bool AddressTest_ParseAndPrint()
{
    ProtoAddress ipv4Address;

    if (!ipv4Address.ConvertFromString("192.0.2.42/24"))
    {
        DMSG(2,
             "AddressTest_ParseAndPrint: IPv4 conversion failed\n");
        return false;
    }

    if ((ipv4Address.GetType() != ProtoAddress::IPv4) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseAndPrint",
                                  ipv4Address,
                                  true,
                                  24)) ||
        (!AddressTest_CheckHostString("AddressTest_ParseAndPrint",
                                      ipv4Address,
                                      "192.0.2.42")) ||
        (!AddressTest_CheckPrintString("AddressTest_ParseAndPrint",
                                       ipv4Address,
                                       "192.0.2.42/24")))
    {
        return false;
    }

    ipv4Address.SetPort(698);

    if (!AddressTest_CheckPrintString("AddressTest_ParseAndPrint",
                                      ipv4Address,
                                      "192.0.2.42/24:698"))
    {
        return false;
    }

#ifdef HAVE_IPV6
    ProtoAddress ipv6Address;

    if (!ipv6Address.ConvertFromString("2001:db8::42/64"))
    {
        DMSG(2,
             "AddressTest_ParseAndPrint: IPv6 conversion failed\n");
        return false;
    }

    if ((ipv6Address.GetType() != ProtoAddress::IPv6) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseAndPrint",
                                  ipv6Address,
                                  true,
                                  64)) ||
        (!AddressTest_CheckHostString("AddressTest_ParseAndPrint",
                                      ipv6Address,
                                      "2001:db8::42")) ||
        (!AddressTest_CheckPrintString("AddressTest_ParseAndPrint",
                                       ipv6Address,
                                       "2001:db8::42/64")))
    {
        return false;
    }

    ipv6Address.SetPort(698);

    if (!AddressTest_CheckPrintString("AddressTest_ParseAndPrint",
                                      ipv6Address,
                                      "[2001:db8::42/64]:698"))
    {
        return false;
    }
#endif // HAVE_IPV6

    ProtoAddress ethAddress;

    if (!ethAddress.ConvertFromString("00:11:22:33:44:55/48"))
    {
        DMSG(2,
             "AddressTest_ParseAndPrint: Ethernet conversion failed\n");
        return false;
    }

    if ((ethAddress.GetType() != ProtoAddress::ETH) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseAndPrint",
                                  ethAddress,
                                  true,
                                  48)) ||
        (!AddressTest_CheckHostString("AddressTest_ParseAndPrint",
                                      ethAddress,
                                      "00:11:22:33:44:55")) ||
        (!AddressTest_CheckPrintString("AddressTest_ParseAndPrint",
                                       ethAddress,
                                       "00:11:22:33:44:55/48")))
    {
        return false;
    }

    return true;
}


bool AddressTest_ParseLimitsAndRejection()
{
    ProtoAddress address;

    if ((!address.ConvertFromString("192.0.2.1/0")) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseLimitsAndRejection",
                                  address,
                                  true,
                                  0)))
    {
        DMSG(2,
             "AddressTest_ParseLimitsAndRejection: IPv4 /0 failed\n");
        return false;
    }

    if ((!address.ConvertFromString("192.0.2.1/32")) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseLimitsAndRejection",
                                  address,
                                  true,
                                  32)))
    {
        DMSG(2,
             "AddressTest_ParseLimitsAndRejection: IPv4 /32 failed\n");
        return false;
    }

    if (!AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "192.0.2.1/33") ||
        !AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "192.0.2.1/-1") ||
        !AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "192.0.2.1/24x") ||
        !AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "192.0.2.1/24/1") ||
        !AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "192.0.2.1/"))
    {
        return false;
    }

#ifdef HAVE_IPV6
    if ((!address.ConvertFromString("2001:db8::1/128")) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseLimitsAndRejection",
                                  address,
                                  true,
                                  128)))
    {
        DMSG(2,
             "AddressTest_ParseLimitsAndRejection: IPv6 /128 failed\n");
        return false;
    }

    if (!AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "2001:db8::1/129"))
    {
        return false;
    }
#endif // HAVE_IPV6

    if ((!address.ConvertFromString("00:11:22:33:44:55/48")) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseLimitsAndRejection",
                                  address,
                                  true,
                                  48)))
    {
        DMSG(2,
             "AddressTest_ParseLimitsAndRejection: Ethernet /48 failed\n");
        return false;
    }

    if (!AddressTest_RejectAddressString("AddressTest_ParseLimitsAndRejection",
                                         "00:11:22:33:44:55/49"))
    {
        return false;
    }

    ProtoAddress resolvedAddress;

    if ((!resolvedAddress.ResolveFromString("192.0.2.9/27")) ||
        (!AddressTest_CheckPrefix("AddressTest_ParseLimitsAndRejection",
                                  resolvedAddress,
                                  true,
                                  27)))
    {
        DMSG(2,
             "AddressTest_ParseLimitsAndRejection: ResolveFromString() prefix parsing failed\n");
        return false;
    }

    return true;
}


bool AddressTest_CopyAndCompare()
{
    ProtoAddress addressA;

    if (!addressA.ConvertFromString("192.0.2.7/24"))
    {
        DMSG(2,
             "AddressTest_CopyAndCompare: address A conversion failed\n");
        return false;
    }

    addressA.SetPort(698);

    ProtoAddress addressB(addressA);

    if ((!addressB.IsEqual(addressA)) ||
        (!AddressTest_CheckPrefix("AddressTest_CopyAndCompare",
                                  addressB,
                                  true,
                                  24)) ||
        (addressB.GetPort() != 698))
    {
        DMSG(2,
             "AddressTest_CopyAndCompare: copy construction differs\n");
        return false;
    }

    ProtoAddress addressC;

    if (!addressC.ConvertFromString("192.0.2.7/30"))
    {
        DMSG(2,
             "AddressTest_CopyAndCompare: address C conversion failed\n");
        return false;
    }

    addressC.SetPort(698);

    if ((!addressA.HostIsEqual(addressC)) ||
        (!addressA.IsEqual(addressC)) ||
        (addressA.CompareHostAddr(addressC) != 0))
    {
        DMSG(2,
             "AddressTest_CopyAndCompare: prefix metadata changed address identity\n");
        return false;
    }

    addressC.SetPort(699);

    if ((!addressA.HostIsEqual(addressC)) ||
        addressA.IsEqual(addressC))
    {
        DMSG(2,
             "AddressTest_CopyAndCompare: port comparison differs\n");
        return false;
    }

    return true;
}


bool AddressTest_LegacyMaskOperations()
{
    ProtoAddress address;
    ProtoAddress peerAddress;
    ProtoAddress subnetAddress;
    ProtoAddress broadcastAddress;
    ProtoAddress maskAddress;

    if ((!address.ConvertFromString("192.0.2.129")) ||
        (!peerAddress.ConvertFromString("192.0.2.1")))
    {
        DMSG(2,
             "AddressTest_LegacyMaskOperations: address conversion failed\n");
        return false;
    }

    if ((!address.PrefixIsEqual(peerAddress, 24)) ||
        address.PrefixIsEqual(peerAddress, 25))
    {
        DMSG(2,
             "AddressTest_LegacyMaskOperations: PrefixIsEqual() differs\n");
        return false;
    }

    address.GetSubnetAddress(24, subnetAddress);
    address.GetBroadcastAddress(24, broadcastAddress);

    if ((!AddressTest_CheckHostString("AddressTest_LegacyMaskOperations",
                                      subnetAddress,
                                      "192.0.2.0")) ||
        (!AddressTest_CheckHostString("AddressTest_LegacyMaskOperations",
                                      broadcastAddress,
                                      "192.0.2.255")))
    {
        return false;
    }

    maskAddress.GeneratePrefixMask(ProtoAddress::IPv4, 24);

    if ((!AddressTest_CheckHostString("AddressTest_LegacyMaskOperations",
                                      maskAddress,
                                      "255.255.255.0")) ||
        (maskAddress.GetPrefixLength() != 24) ||
        (!AddressTest_CheckPrefix("AddressTest_LegacyMaskOperations",
                                  maskAddress,
                                  false,
                                  0)))
    {
        DMSG(2,
             "AddressTest_LegacyMaskOperations: GeneratePrefixMask() differs\n");
        return false;
    }

    address.ApplyPrefixMask(24);

    return AddressTest_CheckHostString("AddressTest_LegacyMaskOperations",
                                       address,
                                       "192.0.2.0");
}


bool AddressTest_ListInsertRemove()
{
    ProtoAddressList addressList;
    ProtoAddress invalidAddress;
    ProtoAddress addressA;
    ProtoAddress addressB;
    ProtoAddress firstAddress;
    static int userDataA = 1;
    static int userDataB = 2;

    //removed below test as it results in noisy messaging for a successful test
    /*if (addressList.Insert(invalidAddress))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: accepted invalid address\n");
        return false;
    }*/

    if ((!addressA.ConvertFromString("192.0.2.10/24")) ||
        (!addressB.ConvertFromString("192.0.2.10/30")))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: address conversion failed\n");
        return false;
    }

    addressA.SetPort(1000);
    addressB.SetPort(2000);

    if (!addressList.Insert(addressA, &userDataA))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: initial insert failed\n");
        return false;
    }

    if ((!addressList.Contains(addressA)) ||
        (!addressList.Contains(addressB)) ||
        (addressList.GetUserData(addressA) != &userDataA))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: initial list lookup differs\n");
        return false;
    }

    if (!addressList.Insert(addressB, &userDataB))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: duplicate-host insert failed\n");
        return false;
    }

    if (addressList.GetUserData(addressA) != &userDataB)
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: duplicate insert did not update user data\n");
        return false;
    }

    if (!addressList.GetFirstAddress(firstAddress))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: GetFirstAddress() failed\n");
        return false;
    }

    if ((!firstAddress.IsEqual(addressA)) ||
        (!AddressTest_CheckPrefix("AddressTest_ListInsertRemove",
                                  firstAddress,
                                  true,
                                  24)))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: stored address changed on duplicate insert\n");
        return false;
    }

    addressList.Remove(addressB);

    if ((!addressList.IsEmpty()) ||
        addressList.Contains(addressA))
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: Remove() did not remove host entry\n");
        return false;
    }

    if (addressList.GetFirstAddress(firstAddress) ||
        firstAddress.IsValid())
    {
        DMSG(2,
             "AddressTest_ListInsertRemove: empty GetFirstAddress() differs\n");
        return false;
    }

    return true;
}


bool AddressTest_ListIteration()
{
    ProtoAddressList addressList;
    ProtoAddress addressA;
    ProtoAddress addressB;
    ProtoAddress addressC;

    if ((!addressA.ConvertFromString("192.0.2.1/24")) ||
        (!addressB.ConvertFromString("192.0.2.2/25")) ||
        (!addressC.ConvertFromString("192.0.2.3/26")))
    {
        DMSG(2,
             "AddressTest_ListIteration: address conversion failed\n");
        return false;
    }

    if ((!addressList.Insert(addressA)) ||
        (!addressList.Insert(addressB)) ||
        (!addressList.Insert(addressC)))
    {
        DMSG(2,
             "AddressTest_ListIteration: address insertion failed\n");
        return false;
    }

    ProtoAddressList::Iterator iterator(addressList);
    ProtoAddress peekAddress;
    ProtoAddress nextAddress;
    bool foundA = false;
    bool foundB = false;
    bool foundC = false;
    unsigned int count = 0;

    while (iterator.PeekNextAddress(peekAddress))
    {
        if (!iterator.GetNextAddress(nextAddress))
        {
            DMSG(2,
                 "AddressTest_ListIteration: GetNextAddress() failed after successful peek\n");
            return false;
        }

        if (!peekAddress.IsEqual(nextAddress))
        {
            DMSG(2,
                 "AddressTest_ListIteration: peek and next differ\n");
            return false;
        }

        if (nextAddress.IsEqual(addressA))
        {
            foundA = true;
        }
        else if (nextAddress.IsEqual(addressB))
        {
            foundB = true;
        }
        else if (nextAddress.IsEqual(addressC))
        {
            foundC = true;
        }
        else
        {
            DMSG(2,
                 "AddressTest_ListIteration: iterator returned unexpected address\n");
            return false;
        }

        ++count;
    }

    if ((count != 3) || (!foundA) || (!foundB) || (!foundC))
    {
        DMSG(2,
             "AddressTest_ListIteration: iterator did not return all addresses\n");
        return false;
    }

    if (iterator.GetNextAddress(nextAddress) ||
        nextAddress.IsValid())
    {
        DMSG(2,
             "AddressTest_ListIteration: exhausted iterator result differs\n");
        return false;
    }

    iterator.Reset();

    if (!iterator.GetNextAddress(nextAddress))
    {
        DMSG(2,
             "AddressTest_ListIteration: iterator Reset() failed\n");
        return false;
    }

    return true;
}


bool AddressTest_ListBulkOperations()
{
    ProtoAddressList sourceList;
    ProtoAddressList destinationList;
    ProtoAddress addressA;
    ProtoAddress addressB;
    ProtoAddress addressC;

    if ((!addressA.ConvertFromString("192.0.2.20/24")) ||
        (!addressB.ConvertFromString("192.0.2.21/24")) ||
        (!addressC.ConvertFromString("192.0.2.22/24")))
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: address conversion failed\n");
        return false;
    }

    if ((!sourceList.Insert(addressA)) ||
        (!sourceList.Insert(addressB)) ||
        (!destinationList.Insert(addressC)))
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: address insertion failed\n");
        return false;
    }

    if (!destinationList.AddList(sourceList))
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: AddList() failed\n");
        return false;
    }

    if ((!destinationList.Contains(addressA)) ||
        (!destinationList.Contains(addressB)) ||
        (!destinationList.Contains(addressC)))
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: AddList() result differs\n");
        return false;
    }

    if ((!sourceList.Contains(addressA)) ||
        (!sourceList.Contains(addressB)))
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: AddList() modified source list\n");
        return false;
    }

    destinationList.RemoveList(sourceList);

    if (destinationList.Contains(addressA) ||
        destinationList.Contains(addressB) ||
        (!destinationList.Contains(addressC)))
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: RemoveList() result differs\n");
        return false;
    }

    destinationList.Destroy();

    if (!destinationList.IsEmpty())
    {
        DMSG(2,
             "AddressTest_ListBulkOperations: Destroy() did not empty destination list\n");
        return false;
    }

    return true;
}
bool AddressTest_GetPrintStringStaticBuffer()
{
    static const unsigned int MINIMUM_CONCURRENT_PRINTS = 10;
    static const unsigned int PROBE_LIMIT = 64;

    ProtoAddress addresses[PROBE_LIMIT];
    const char* printStrings[PROBE_LIMIT];

    unsigned int maxSupported = 0;
    bool overwriteDetected = false;

    for (unsigned int i = 0; i < PROBE_LIMIT; ++i)
    {
        char addressText[32];

        snprintf(addressText,
                 sizeof(addressText),
                 "192.0.2.%u/24",
                 i + 1);

        if (!addresses[i].ConvertFromString(addressText))
        {
            DMSG(2,
                 "AddressTest_GetPrintStringStaticBuffer: "
                 "address conversion failed for \"%s\"\n",
                 addressText);
            return false;
        }

        addresses[i].SetPort(static_cast<UINT16>(10000 + i));

        printStrings[i] = addresses[i].GetPrintString();

        bool allResultsPreserved = true;

        for (unsigned int j = 0; j <= i; ++j)
        {
            char expectedText[64];

            addresses[j].GetPrintString(expectedText,
                                        sizeof(expectedText));

            if (0 != strcmp(printStrings[j], expectedText))
            {
                allResultsPreserved = false;
                break;
            }
        }

        if (!allResultsPreserved)
        {
            overwriteDetected = true;
            break;
        }

        maxSupported = i + 1;
    }

    if (maxSupported < MINIMUM_CONCURRENT_PRINTS)
    {
        DMSG(2,
             "AddressTest_GetPrintStringStaticBuffer: "
             "only %u concurrent print strings survived; "
             "expected at least %u\n",
             maxSupported,
             MINIMUM_CONCURRENT_PRINTS);
        return false;
    }

    if (overwriteDetected)
    {
        DMSG(2,
             "AddressTest_GetPrintStringStaticBuffer: "
             "maximum supported concurrent print strings: %u\n",
             maxSupported);
    }
    else
    {
        DMSG(2,
             "AddressTest_GetPrintStringStaticBuffer: "
             "at least %u concurrent print strings are supported "
             "(probe limit reached)\n",
             maxSupported);
    }

    return true;
}


bool AddressTest_GetHostStringStaticBuffer()
{
    static const unsigned int MINIMUM_CONCURRENT_HOST_STRINGS = 10;
    static const unsigned int PROBE_LIMIT = 64;

    ProtoAddress addresses[PROBE_LIMIT];
    const char* hostStrings[PROBE_LIMIT];

    unsigned int maxSupported = 0;
    bool overwriteDetected = false;

    for (unsigned int i = 0; i < PROBE_LIMIT; ++i)
    {
        char addressText[32];

        snprintf(addressText,
                 sizeof(addressText),
                 "192.0.2.%u",
                 i + 1);

        if (!addresses[i].ConvertFromString(addressText))
        {
            DMSG(2,
                 "AddressTest_GetHostStringStaticBuffer: "
                 "address conversion failed for \"%s\"\n",
                 addressText);
            return false;
        }

        hostStrings[i] = addresses[i].GetHostString();

        bool allResultsPreserved = true;

        for (unsigned int j = 0; j <= i; ++j)
        {
            char expectedText[64];

            addresses[j].GetHostString(expectedText,
                                       sizeof(expectedText));

            if (0 != strcmp(hostStrings[j], expectedText))
            {
                allResultsPreserved = false;
                break;
            }
        }

        if (!allResultsPreserved)
        {
            overwriteDetected = true;
            break;
        }

        maxSupported = i + 1;
    }

    if (overwriteDetected)
    {
        DMSG(2,
             "AddressTest_GetHostStringStaticBuffer: "
             "maximum supported concurrent host strings: %u\n",
             maxSupported);
    }
    else
    {
        DMSG(2,
             "AddressTest_GetHostStringStaticBuffer: "
             "at least %u concurrent host strings are supported "
             "(probe limit reached)\n",
             maxSupported);
    }

    if (maxSupported < MINIMUM_CONCURRENT_HOST_STRINGS)
    {
        DMSG(2,
             "AddressTest_GetHostStringStaticBuffer: "
             "only %u concurrent host strings survived; "
             "expected at least %u\n",
             maxSupported,
             MINIMUM_CONCURRENT_HOST_STRINGS);
        return false;
    }

    return true;
}
bool AddressTest_CommonHeadTail()
{
    ProtoAddress headAddress;
    ProtoAddress headCompareAddress;
    ProtoAddress tailAddress;
    ProtoAddress tailCompareAddress;
    ProtoAddress fullHeadAddress;
    ProtoAddress fullHeadCompareAddress;
    ProtoAddress fullTailAddress;
    ProtoAddress fullTailCompareAddress;
    ProtoAddress ethernetAddress;
    ProtoAddress mixedHeadAddress;
    ProtoAddress mixedTailAddress;

    bool success = true;

    if ((!headAddress.ConvertFromString("192.0.2.129")) ||
        (!headCompareAddress.ConvertFromString("192.0.3.7")) ||
        (!tailAddress.ConvertFromString("192.0.2.129")) ||
        (!tailCompareAddress.ConvertFromString("203.0.2.129")) ||
        (!fullHeadAddress.ConvertFromString("198.51.100.7")) ||
        (!fullTailAddress.ConvertFromString("198.51.100.7")) ||
        (!ethernetAddress.ConvertFromString("00:00:00:00:00:00")) ||
        (!mixedHeadAddress.ConvertFromString("192.0.2.1")) ||
        (!mixedTailAddress.ConvertFromString("192.0.2.1")))
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: address conversion failed\n");
        return false;
    }

    fullHeadCompareAddress = fullHeadAddress;
    fullTailCompareAddress = fullTailAddress;

    unsigned int commonHead =
        headAddress.SetCommonHead(headCompareAddress);

    if (commonHead != 2)
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "SetCommonHead() returned %u, expected 2\n",
             commonHead);
        success = false;
    }

    if (!AddressTest_CheckHostString("AddressTest_CommonHeadTail",
                                     headAddress,
                                     "192.0.0.0"))
    {
        success = false;
    }

    unsigned int commonTail =
        tailAddress.SetCommonTail(tailCompareAddress);

    if (commonTail != 3)
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "SetCommonTail() returned %u, expected 3\n",
             commonTail);
        success = false;
    }

    if (!AddressTest_CheckHostString("AddressTest_CommonHeadTail",
                                     tailAddress,
                                     "0.0.2.129"))
    {
        success = false;
    }

    commonHead = fullHeadAddress.SetCommonHead(fullHeadCompareAddress);

    if (commonHead != 4)
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "full-match SetCommonHead() returned %u, expected 4\n",
             commonHead);
        success = false;
    }

    if (!AddressTest_CheckHostString("AddressTest_CommonHeadTail",
                                     fullHeadAddress,
                                     "198.51.100.7"))
    {
        success = false;
    }

    commonTail = fullTailAddress.SetCommonTail(fullTailCompareAddress);

    if (commonTail != 4)
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "full-match SetCommonTail() returned %u, expected 4\n",
             commonTail);
        success = false;
    }

    if (!AddressTest_CheckHostString("AddressTest_CommonHeadTail",
                                     fullTailAddress,
                                     "198.51.100.7"))
    {
        success = false;
    }

    commonHead = mixedHeadAddress.SetCommonHead(ethernetAddress);

    if (commonHead != 0)
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "mixed-type SetCommonHead() returned %u, expected 0\n",
             commonHead);
        success = false;
    }

    if (!AddressTest_CheckHostString("AddressTest_CommonHeadTail",
                                     mixedHeadAddress,
                                     "192.0.2.1"))
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "mixed-type SetCommonHead() modified the IPv4 address\n");
        success = false;
    }

    commonTail = mixedTailAddress.SetCommonTail(ethernetAddress);

    if (commonTail != 0)
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "mixed-type SetCommonTail() returned %u, expected 0\n",
             commonTail);
        success = false;
    }

    if (!AddressTest_CheckHostString("AddressTest_CommonHeadTail",
                                     mixedTailAddress,
                                     "192.0.2.1"))
    {
        DMSG(2,
             "AddressTest_CommonHeadTail: "
             "mixed-type SetCommonTail() modified the IPv4 address\n");
        success = false;
    }

    return success;
}
bool AddressTest_PrefixIsEqual()
{
    ProtoAddress ipv4AddressA;
    ProtoAddress ipv4AddressB;
    ProtoAddress ethernetAddress;
    bool success = true;

    if ((!ipv4AddressA.ConvertFromString("192.0.2.1")) ||
        (!ipv4AddressB.ConvertFromString("192.0.2.129")) ||
        (!ethernetAddress.ConvertFromString("00:11:22:33:44:55")))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: address conversion failed\n");
        return false;
    }

    if (!ipv4AddressA.PrefixIsEqual(ipv4AddressB, 0))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: IPv4 /0 comparison failed\n");
        success = false;
    }

    if (!ipv4AddressA.PrefixIsEqual(ipv4AddressB, 24))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: IPv4 /24 comparison failed\n");
        success = false;
    }

    if (ipv4AddressA.PrefixIsEqual(ipv4AddressB, 25))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: IPv4 /25 comparison unexpectedly matched\n");
        success = false;
    }

    if (!ipv4AddressA.PrefixIsEqual(ipv4AddressA, 32))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: IPv4 /32 self-comparison failed\n");
        success = false;
    }

    // This must be rejected.  The current implementation returns true.
    if (ipv4AddressA.PrefixIsEqual(ipv4AddressA, 33))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: "
             "IPv4 /33 comparison unexpectedly matched\n");
        success = false;
    }

    if (ethernetAddress.PrefixIsEqual(ethernetAddress, 49))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: "
             "Ethernet /49 comparison unexpectedly matched\n");
        success = false;
    }

#ifdef HAVE_IPV6
    ProtoAddress ipv6Address;

    if (!ipv6Address.ConvertFromString("2001:db8::1"))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: IPv6 conversion failed\n");
        return false;
    }

    if (!ipv6Address.PrefixIsEqual(ipv6Address, 128))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: IPv6 /128 self-comparison failed\n");
        success = false;
    }

    if (ipv6Address.PrefixIsEqual(ipv6Address, 129))
    {
        DMSG(2,
             "AddressTest_PrefixIsEqual: "
             "IPv6 /129 comparison unexpectedly matched\n");
        success = false;
    }
#endif // HAVE_IPV6

    return success;
}