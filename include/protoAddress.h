#ifndef _PROTO_ADDRESS
#define _PROTO_ADDRESS
// This is an attempt at some generic network address generalization
// along with some methods for standard address lookup/ manipulation.
// This will evolve as we actually support address types besides IPv4

#include "protoDefs.h"
#include "protoTree.h"
#include "protoDebug.h"

#ifdef UNIX
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>    
#ifndef INADDR_NONE
const unsigned long INADDR_NONE = 0xffffffff;
#endif // !INADDR_NONE
#endif // UNIX

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef HAVE_IPV6
#ifndef _SS_PAD1SIZE
#include "tpipv6.h"  // not in older Platform SDKs
#endif // !_SS_PAD1SIZE
#endif // HAVE_IPV6
#endif // WIN32

#ifdef SIMULATE

#ifdef NS2
#include "config.h"  // for nsaddr_t which is a 32 bit int?
typedef nsaddr_t SIMADDR;
#endif // NS2

#ifdef NS3
#error "ns-3 work in progress"
#endif  // NS3

#ifdef OPNET
#include "opnet.h"
#include "ip_addr_v4.h"
typedef IpT_Address SIMADDR;
extern IpT_Address IPI_BroadcastAddr;
#endif // OPNET

#ifndef _SOCKADDRSIM
#define _SOCKADDRSIM
struct sockaddr_sim
{
    SIMADDR         addr;
    unsigned short  port;  
};
#endif  // ! _SOCKADDRSIM

#endif // SIMULATE

#ifdef HAVE_IPV6
// returns IPv4 portion of mapped address in network byte order
inline unsigned long IN6_V4MAPPED_ADDR(struct in6_addr* a) {return (((UINT32*)a)[3]);}
#endif // HAVE_IPV6

/**
 * @class ProtoAddress
 *
 * @brief Network address container class with support
 * for IPv4, IPv6, and "SIM" address types.  Also
 * includes functions for name/address
 * resolution.
 */

#ifdef INVALID
#undef INVALID
#endif

class ProtoAddress
{
    public:            
        enum Type 
        {
            INVALID, 
            IPv4, 
            IPv6,
            ETH,
            SIM
        };
        // Construction/initialization
        ProtoAddress();
        ProtoAddress(const ProtoAddress& theAddr);
        ProtoAddress(const char* theAddr);  // must be a numeric address, not a host name!
        ~ProtoAddress();
        bool IsValid() const {return (INVALID != type);}
        void Invalidate()
        {
            type = INVALID;
            length = 0;
        }
        void Reset(ProtoAddress::Type theType, bool zero = true);
        
        
        static UINT8 GetLength(Type type)
        {
            switch (type)
            {
                case IPv4:
                    return 4;
                case IPv6:
                    return 16;
                case ETH:
                    return 6;
#ifdef SIMULATE
                case SIM:
                     return sizeof(SIMADDR);
#endif // SIMULATE
                default:
                    return 0;
            }   
        }
        
        // Address info
        ProtoAddress::Type GetType() const {return type;}
        UINT8 GetLength() const {return length;}
        bool IsBroadcast() const;
        bool IsMulticast() const;
        bool IsLoopback() const;
        bool IsSiteLocal() const;
        bool IsLinkLocal() const;
        bool IsUnspecified() const;
        bool IsUnicast() const
            {return (!IsMulticast() && !IsBroadcast() && !IsUnspecified());}
        
        const char* GetHostString(char*         buffer = NULL, 
                                  unsigned int  buflen = 0) const;
        
        // This infers the ProtoAddress::Type from length (in bytes)
        static ProtoAddress::Type GetType(UINT8 addrLength);
        
        // Host address/port get/set
        const char* GetRawHostAddress() const;
        bool SetRawHostAddress(ProtoAddress::Type   theType,
                               const char*          buffer,
                               UINT8                bufferLen);
        const struct sockaddr& GetSockAddr() const 
            {return ((const struct sockaddr&)addr);}
#ifdef HAVE_IPV6
        const struct sockaddr_storage& GetSockAddrStorage() const
            {return addr;}        
#endif // HAVE_IPV6        
        bool SetSockAddr(const struct sockaddr& theAddr);
        struct sockaddr& AccessSockAddr() 
            {return ((struct sockaddr&)addr);}
        UINT16 GetPort() const; 
        void SetPort(UINT16 thePort);
        void PortSet(UINT16 thePort);
        
        // Address comparison
        bool IsEqual(const ProtoAddress& theAddr) const
            {return (HostIsEqual(theAddr) && (GetPort() == theAddr.GetPort()));}
        bool HostIsEqual(const ProtoAddress& theAddr) const;
        int CompareHostAddr(const ProtoAddress& theAddr) const;
        bool operator==(const ProtoAddress &theAddr) const {return IsEqual(theAddr);}
        bool operator!=(const ProtoAddress &theAddr) const {return !IsEqual(theAddr);}
        bool operator>(const ProtoAddress &theAddr) const {return (CompareHostAddr(theAddr)>0);}
        bool operator<(const ProtoAddress &theAddr) const {return (CompareHostAddr(theAddr)<0);}
        bool operator>=(const ProtoAddress &theAddr) const {return !(*this<theAddr);}
        bool operator<=(const ProtoAddress &theAddr) const {return !(*this>theAddr);} 
        
        unsigned int SetCommonHead(const ProtoAddress &theAddr);
        unsigned int SetCommonTail(const ProtoAddress &theAddr);

        
        bool PrefixIsEqual(const ProtoAddress& theAddr, UINT8 prefixLen) const;
        UINT8 GetPrefixLength() const;  // finds length (in bits) of non-zero prefix
        void GeneratePrefixMask(ProtoAddress::Type theType, UINT8 prefixLen);
        void ApplyPrefixMask(UINT8 prefixLen);        
        void ApplySuffixMask(UINT8 suffixLen);
        void GetSubnetAddress(UINT8         prefixLen,  
                              ProtoAddress& subnetAddr) const;
        void GetBroadcastAddress(UINT8          prefixLen, 
                                 ProtoAddress&  broadcastAddr) const;
        // if you start at the subnet address, you can use
        // this Increment() method to iterate over the range
        // of subnet addresses, up to the broadcast address
        bool Increment();
        
        // Generates Ethernet mcast addr from IP mcast addr
        ProtoAddress& GetEthernetMulticastAddress(const ProtoAddress& ipMcastAddr);
        
        // Name/address resolution
        bool ResolveFromString(const char* text);
        bool ResolveToName(char* nameBuffer, unsigned int buflen) const;
        bool ResolveLocalAddress(char* nameBuffer = NULL, unsigned int buflen = 0);
        
        // Expects IPv4, IPv6, or ETH address in numeric notation form
        bool ConvertFromString(const char* text);
        // This expects a an "Ethernet" MAC address string 
        // in colon-delimited hexadecmial format
        bool ResolveEthFromString(const char* text);
        
        // Miscellaneous
        // This function returns a 32-bit number which might _sometimes_
        // be useful as an address-based end system identifier.
        // (For IPv4 it's the address in host byte order)
        
        UINT32 GetEndIdentifier() const;
        UINT32 EndIdentifier() const  // this is deprecated
            {return GetEndIdentifier();}
        
        // This lets us "manufacture" addresses from a 32-bit identifier value
        // The default addrType is IPv4, but if the address already has
        // a valid type, it is retained with the identifier applied
        // such that GetEndIdentifier() will return the set value   
        void SetEndIdentifier(UINT32 endIdentifier);
            
        // Return IPv4 address in host byte order
        UINT32 IPv4GetAddress() const 
            {return (IPv4 == type) ? EndIdentifier() : 0;}
#ifdef SIMULATE
        SIMADDR SimGetAddress() const
        {
            return ((SIM == type) ? ((struct sockaddr_sim*)&addr)->addr : 
                                    0);
        }
        void SimSetAddress(SIMADDR theAddr)
        {
            type = SIM;
            length = sizeof(SIMADDR); // NSv2 addresses are 4 bytes? (32 bits)
            ((struct sockaddr_sim*)&addr)->addr = theAddr;
        }
#endif // SIMULATE
#ifdef WIN32
            static bool Win32Startup() 
            {
				WSADATA wsaData;
                WORD wVersionRequested = MAKEWORD(2, 2);
                return (0 == WSAStartup(wVersionRequested, &wsaData));
            }
			static void Win32Cleanup() {WSACleanup(); }
#endif  // WIN32

    private:
        char* AccessRawHostAddress() const;
            
        Type                    type; 
        UINT8                   length;        
#ifdef SIMULATE
        struct sockaddr_sim	    addr;   // for OPNET and NS2 builds
#else         
#ifdef _WIN32_WCE
        struct sockaddr_in      addr;  // WINCE doesn't like anything else???
#else
        struct sockaddr_storage addr;  // this is the _preferred_ way to build Protolib
#endif // if/else _WIN32_WCE               
#endif // if/else SIMULATE
};  // end class ProtoAddress


/**
 * @class ProtoAddressList
 *
 * @brief This "ProtoAddressList" helper class uses a Patricia
 * ree (ProtoTree) to keep a listing of addresses. Note it keeps in
 * own internal copy of addresses that are added to the list.
 * 
 * An "Iterator" is also provided
 * to go through the list when needed.
 */
class ProtoAddressList
{
    public:
        ProtoAddressList();
        ~ProtoAddressList();
        void Destroy();
        // Note the Insert() makes an internal copy of the addr passed in.
        bool Insert(const ProtoAddress& addr, const void* userData = NULL);
        
        bool Contains(const ProtoAddress& addr) const
            {return (NULL != addr_tree.Find(addr.GetRawHostAddress(), addr.GetLength() << 3));}
        
        const void* GetUserData(const ProtoAddress& addr) const
        {
            Item* entry = static_cast<Item*>(addr_tree.Find(addr.GetRawHostAddress(), addr.GetLength() << 3));
            return (NULL != entry) ? entry->GetUserData() : NULL;
        }
        void Remove(const ProtoAddress& addr);
        
        // Add (non-duplicative) / Remove addresses from input list
        // (input list is not modified)
        bool AddList(ProtoAddressList& addrList);
        void RemoveList(ProtoAddressList& addrList);
        
        bool IsEmpty() const
            {return (addr_tree.IsEmpty());}
        
        // Note this is not really a "first address" necessarily
        bool GetFirstAddress(ProtoAddress& firstAddr) const;
        
        class Iterator;
        friend class Iterator;
        class Iterator
        {
            public:
                Iterator(ProtoAddressList& addrList);
                ~Iterator();

                void Reset() {ptree_iterator.Reset();}
                bool GetNextAddress(ProtoAddress& nextAddr);
                bool PeekNextAddress(ProtoAddress& nextAddr);

            private:
                ProtoTree::Iterator ptree_iterator;
        };  // end class ProtoAddressList::Iterator

        class Item : public ProtoTree::Item
        {
            public:
                Item(const ProtoAddress& theAddr, const void* userData = NULL);
                ~Item();

                const ProtoAddress& GetAddress() const
                    {return addr;}
                
                const void* GetUserData() const
                    {return user_data;}
                
                // Use this with care (only when item is not in list)
                void SetAddress(const ProtoAddress& theAddress)
                    {addr = theAddress;}
                
                void SetUserData(const void* userData)
                    {user_data = userData;}
                
                // Required ProtoTree::Item overrides
                const char* GetKey() const {return addr.GetRawHostAddress();}
                unsigned int GetKeysize() const {return (addr.GetLength() << 3);}

            private:
                ProtoAddress    addr;    
                const void*     user_data;   // this will be deprecated!
        };  // end class ProtoAddressList::Item    
        
        bool InsertItem(ProtoAddressList::Item& theItem)
            {return addr_tree.Insert(theItem);}
        void RemoveItem(ProtoAddressList::Item& theItem)
            {addr_tree.Remove(theItem);}
        
        Item* RemoveRootItem()
            {return static_cast<Item*>(addr_tree.RemoveRoot());}
        
    private:
        ProtoTree           addr_tree;
};  // end class ProtoAddressList

extern const ProtoAddress PROTO_ADDR_NONE;      // invalid ProtoAddress (useful as a default)
extern const ProtoAddress PROTO_ADDR_BROADCAST; // Ethernet broadcast address

#endif // _PROTO_ADDRESS

