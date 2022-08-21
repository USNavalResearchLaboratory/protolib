#ifndef _PROTO_SOCKET
#define _PROTO_SOCKET

#include "protoAddress.h"
#include "protoDebug.h"  // temp
#include "protoNotify.h"

#ifdef WIN32
/*#ifdef _WIN32_WCE
#ifndef LPGUID 
#include <guiddef.h>
#endif // !LPGUID
#endif // _WIN32_WCE*/
#ifndef __LPGUID_DEFINED__
#define __LPGUID_DEFINED__
typedef GUID *LPGUID;
#endif
#include <winsock2.h>  // for SOCKET type, etc
#include <MswSock.h>   // for WSARecvMsg() stuff
#else  // !WIN32
#include <errno.h> // for errno
#include <net/if.h>  // for struct ifconf
#include <unistd.h>  // for read()
#include <stdio.h>
#endif // if/else WIN32/UNIX
// "RAW" is predefined IOCTL macro in Gnu/Hurd Linux
// so this deconflicts it for the ProtoSocket::Protocol enum
#ifdef RAW
#undef RAW
#endif // RAW
/**
 * @class ProtoSocket
 *
 * @brief Network socket container class that provides
 * consistent interface for use of operating
 * system (or simulation environment) transport
 * sockets. Provides support for asynchronous
 * notification to ProtoSocket::Listeners.  
 * The ProtoSocket class may be used stand-alone, or
 * with other classes described below.  A
 * ProtoSocket may be instantiated as either a
 * UDP or TCP socket.
 */

class ProtoSocket : public ProtoNotify
{
    public:
			// Type definitions
        enum Domain 
		{
				LOCAL, 
				IPv4,
				IPv6
#ifdef SIMULATE
				,SIM
#endif // SIMULATE
		};
		enum Protocol {INVALID_PROTOCOL, UDP, TCP, RAW, ZMQ};  
		enum State {CLOSED, IDLE, CONNECTING, LISTENING, CONNECTED}; 

		enum EcnStatus
		{
				ECN_NONE = 0x00,  // not ECN-Capable
				ECN_ECT1 = 0x01,  // ECN-Capable Transport 1
				ECN_ECT0 = 0x02,  // ECN-Capable Transport 0      (old ECT)
				ECN_CE   = 0x03   // ECN "Congestion Experienced" (old CE)
		}; 

        enum IPv6SupportStatus
        {
            IPV6_UNKNOWN,
            IPV6_UNSUPPORTED,
            IPV6_SUPPORTED
        };

#ifdef SIMULATE
		class Proxy {};
		typedef Proxy* Handle;
		bool GetEcnStatus() const;
#else       
#ifdef WIN32
		typedef SOCKET Handle;
#else
		typedef int Handle;
#endif // if/else WIN32/UNIX
#endif // if/else SIMULATE
		static const Handle INVALID_HANDLE; 

		ProtoSocket(Protocol theProtocol);
		virtual ~ProtoSocket();      

		// Control methods
		bool Open(UINT16                thePort = 0, 
				  ProtoAddress::Type    addrType = ProtoAddress::IPv4,
				  bool                  bindOnOpen = true);
		bool Bind(UINT16 thePort = 0, const ProtoAddress* localAddr = NULL);
		bool Connect(const ProtoAddress& theAddress);
		void Disconnect();
		bool Listen(UINT16 thePort = 0);
		void Ignore();
		bool Accept(ProtoSocket* theSocket = NULL);
		bool Shutdown();
		void Close();

// First cut at IGMPv3 SSM support (TBD - refine this and expand platform support)
// On Mac OSX, only version 10.7 and later support IGMPv3 
// and the "MCAST_JOIN_GROUP" macro definition is a "tell" for this
// (we _reallly_ need to go to a more sophisticated build system!)
#if (!defined(WIN32) && !defined(ANDROID) && (!defined(MACOSX))) || (defined(MACOSX) && defined(MCAST_JOIN_GROUP))
#define _PROTOSOCKET_IGMPV3_SSM
#endif // !WIN32 && !ANDROID && (!MACOSX || MCAST_JOIN_GROUP)
        
		bool JoinGroup(const ProtoAddress&  groupAddress, 
			           const char*          interfaceName = NULL,
                       const ProtoAddress*  sourceAddress = NULL);
		bool LeaveGroup(const ProtoAddress&  groupAddress, 
			            const char*          interfaceName = NULL,
                        const ProtoAddress*  sourceAddress = NULL);
        
        bool SetRawProtocol(Protocol theProtocol);
 
		void SetState(State st){state = st;}  // JPH 7/14/06 - for tcp development testing
#ifdef WIN32        
		void SetClosing(bool status) {closing = status;}
		bool IsClosing() const {return closing;}
#endif // WIN32            
		// Status methods        
		Domain GetDomain() const {return domain;}
		ProtoAddress::Type  GetAddressType();
		Protocol GetProtocol() const {return protocol;}
		State GetState() const {return state;}
		Handle GetHandle() const {return handle;}
		UINT16 GetPort() const {return port < 0 ? 0 : port;}
		bool IsOpen() const {return (CLOSED != state);}
		bool IsBound() const {return (IsOpen() ? (port >= 0) : false);}
		bool IsConnected() const {return (CONNECTED == state);}
		bool IsConnecting() const {return (CONNECTING == state);}
		bool IsIdle() const {return (IDLE == state);}
		bool IsClosed() const {return (CLOSED == state);}
		bool IsListening() const {return (LISTENING == state);}
#ifdef OPNET
		ProtoAddress& GetDestination() {return destination;}
#else
        // These are valid for connected sockets only
        const ProtoAddress& GetSourceAddr() const {return source_addr;}
		const ProtoAddress& GetDestination() const {return destination;}
		// I.T. helper method to set the destination on a socket after an ACCEPT.
		void SetDestination(const ProtoAddress& theDestination) 
            {destination = theDestination;}
#endif  // OPNET
#ifdef WIN32
		HANDLE GetInputEventHandle() {return input_event_handle;}
		HANDLE GetOutputEventHandle() {return output_event_handle;}
		bool IsOutputReady() {return output_ready;}
		bool IsInputReady() {return input_ready;}
		bool IsReady() {return (input_ready || output_ready);}
#endif  // WIN32
		static const char* GetErrorString()
		{
#ifdef WIN32
			static char errorString[256];
			errorString[255] = '\0';
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 
					      FORMAT_MESSAGE_IGNORE_INSERTS,
					      NULL,
					      WSAGetLastError(),
					      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					      (LPTSTR) errorString, 255, NULL);
			return errorString;
#else
			return strerror(errno);
#endif // if/else WIN32/UNIX
		}

		// Read/Write methods
		bool SendTo(const char* buffer, unsigned int &buflen, const ProtoAddress& dstAddr);
		bool RecvFrom(char* buffer, unsigned int& numBytes, ProtoAddress& srcAddr);
        bool RecvFrom(char* buffer, unsigned int& numBytes, ProtoAddress& srcAddr, ProtoAddress& dstAddr); 
		bool Send(const char* buffer, unsigned int& numBytes);
		bool Recv(char* buffer, unsigned int& numBytes);
#if !defined(WIN32) && !defined(SIMULATE)        
		// This was for debugging?? Remove??
        bool Read(char* buffer, unsigned int &numBytes)
        {
            ssize_t result = read(handle, buffer, numBytes);
            if (result < 0)
            {
                perror("read() error");
                numBytes = 0;
                switch (errno)
                {
                    case EAGAIN:
                        return true;
                    default:
                        return false;
                }
            }   
            else
            {
                numBytes = (unsigned int)result;
                return true;
            }
        }
#endif // !WIN32
		// Attributes
		bool SetTTL(unsigned char ttl);
		bool SetUnicastTTL(unsigned char ttl);
		bool SetLoopback(bool loopback);
		bool SetBroadcast(bool broadcast);
        bool SetFragmentation(bool enable);
		bool SetReuse(bool reuse);
		bool SetBindInterface(const char* interfaceName);
		bool SetMulticastInterface(const char* interfaceName);
		bool SetTOS(unsigned char tos);      
		bool SetEcnCapable(bool status);
		bool SetTxBufferSize(unsigned int bufferSize);
		unsigned int GetTxBufferSize();
		bool SetRxBufferSize(unsigned int bufferSize);
		unsigned int GetRxBufferSize();
        
        void EnableRecvDstAddr();

		// Helper methods
#ifdef HAVE_IPV6
	static bool HostIsIPv6Capable();
        // Temporarily retained for backward compatability
	static bool SetHostIPv6Capable() {return true;}
	bool SetFlowLabel(UINT32 label);
#endif //HAVE_IPV6

        // These are some static network "helper" functions that get information
        // about the system network devices (interfaces), addresses, etc
        //
        // Note that the "ProtoSocket" implementation of this is being replaced
        // by implementation in the "ProtoNet" namespace (see "protoNet.h") and
        // the implementations here will eventually be removed _and_ the ProtoSocket
        // static method declarations themselves will be eventually deprecated  
        //          
	// This appends addresses of type "addrType" to the "addrList"
	static bool GetHostAddressList(ProtoAddress::Type  addrType,
				       ProtoAddressList&   addrList);

        static bool GetInterfaceAddressList(const char*         ifName, 
				                            ProtoAddress::Type  addrType,
				                            ProtoAddressList&   addrList,
                                            unsigned int*       ifIndex = NULL); 
        
        static bool GetInterfaceAddress(const char*         ifName, 
				                        ProtoAddress::Type  addrType,
				                        ProtoAddress&       theAddress,
                                        unsigned int*       ifIndex = NULL);
        
        static unsigned int GetInterfaceIndices(unsigned int* indexArray, unsigned int indexArraySize);
        static unsigned int GetInterfaceIndex(const char* interfaceName);
        static bool FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress);
        static bool GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen);   
        static bool GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen); 
        
        class Notifier
        {
            public:
                virtual ~Notifier() {}
                virtual bool UpdateSocketNotification(ProtoSocket& theSocket, 
                                                      int          notifyFlags) {return true;}
        };
        bool SetNotifier(ProtoSocket::Notifier* theNotifier);
		Notifier* GetNotifier() const {return notifier;}
        
        bool StartInputNotification();
        void StopInputNotification();
        bool InputNotification() const
            {return notify_input;}
		bool StartOutputNotification();
        void StopOutputNotification();
        bool OutputNotification() const
            {return notify_output;}
        bool StartExceptionNotification();
        void StopExceptionNotification();
        bool ExceptionNotification() const
            {return notify_exception;}

        void OnNotify(ProtoNotify::NotifyFlag theFlag);
        
        enum Event {INVALID_EVENT, CONNECT, ACCEPT, SEND, RECV, DISCONNECT, ERROR_, EXCEPTION};
        
        // NOTE: For VC++ 6.0 Debug builds, you _cannot_ use pre-compiled
        // headers with this template code.  Also, "/ZI" or "/Z7" compile options 
        // must NOT be specified.  (or else VC++ 6.0 experiences an "internal compiler error")
        // (Later Visual Studio versions have fixed this error)
        template <class listenerType>
        bool SetListener(listenerType* theListener, void(listenerType::*eventHandler)(ProtoSocket&, Event))
        {
            bool doUpdate = ((NULL != theListener) || (NULL != listener));
            if (NULL != listener) delete listener;
            listener = (NULL != theListener) ? new LISTENER_TYPE<listenerType>(theListener, eventHandler) : NULL;
            bool result = (NULL != theListener) ? (NULL != listener) : true;
            return result ? (doUpdate ? UpdateNotification() : true) : false;
        }
        
        
        bool HasListener() 
            {return (NULL != listener);}
        void SetUserData(const void* userData) {user_data = userData;}
        const void* GetUserData() {return user_data;}
        
        /**
		* @class List
		*
		* @brief A helper linked list class 
        */
		class List
        {
            public:
                List();
                ~List();
                void Destroy();  // deletes list Items _and_ their socket
                
                bool AddSocket(ProtoSocket& theSocket);
                void RemoveSocket(ProtoSocket& theSocket);
                
                class Item;
                Item* FindItem(const ProtoSocket& theSocket) const;
            
            public:
				/**
				* @class Iterator
				*
				* @brief List Iterator
				*/
                class Iterator
                {
                    public:
                        Iterator(const List& theList);
                        const Item* GetNextItem()
                        {
                            const Item* current = next;
                            next = current ? current->GetNext() : current;
                            return current;   
                        }
                        ProtoSocket* GetNextSocket()
                        {
                            const Item* nextItem = GetNextItem();
                            return nextItem ? nextItem->GetSocket() : NULL;   
                        }
                        
                        
                    private:
                        const class Item*   next;
                        
                };  // end class ProtoSocketList::Iterator
                friend class List::Iterator;
                /**
				* @class Item
				*
				* @brief List Item
				*/
                class Item
                {
                    friend class List;
                    friend class Iterator;
                    public:
                        Item(ProtoSocket* theSocket);
                        ProtoSocket* GetSocket() const {return socket;}
                        const void* GetUserData() {return user_data;}
                        void SetUserData(const void* userData)
                            {user_data = userData;}
                    private:    
                        Item* GetPrev() const {return prev;}
                        void SetPrev(Item* item) {prev = item;}
                        Item* GetNext() const {return next;}
                        void SetNext(Item* item) {next = item;}
                        
                        ProtoSocket*    socket;
                        const void*     user_data;
                        Item*           prev;
                        Item*           next;
                };   // end class ProtoSocketList::Item
                
                Item*   head;
        };  // end class ProtoSocketList
          
            
    protected:
		/** 
		* @class Listener
		*
		* @brief Listens for socket activity and invokes socket event handler.
		*/
        class Listener
        {
            public:
                virtual ~Listener() {}
                virtual void on_event(ProtoSocket& theSocket, Event theEvent) = 0;
                virtual Listener* duplicate() = 0;
        };

		/**
		* @class LISTENER_TYPE
		*
		* @brief Listener template
		*/
        template <class listenerType>
        class LISTENER_TYPE : public Listener
        {
            public:
                LISTENER_TYPE(listenerType* theListener, 
                              void(listenerType::*eventHandler)(ProtoSocket&, Event))
                    : listener(theListener), event_handler(eventHandler) {}
                void on_event(ProtoSocket& theSocket, Event theEvent) 
                    {(listener->*event_handler)(theSocket, theEvent);}
                Listener* duplicate()
                    {return (static_cast<Listener*>(new LISTENER_TYPE<listenerType>(listener, event_handler)));}
            private:
                listenerType* listener;
                void(listenerType::*event_handler)(ProtoSocket&, Event);
        };
        virtual bool SetBlocking(bool blocking);
        bool UpdateNotification();
    
        Domain                  domain;
        Protocol                protocol;    
        Protocol                raw_protocol;  // only applies to raw sockets
        State                   state;       
        Handle                  handle; 
        int                     port; 
        UINT8                   tos;           // IPv4 TOS or IPv6 traffic class
        bool                    ecn_capable;
        bool                    ip_recvdstaddr;  // set "true" if RecvFrom() w/ destAddr is invoked
#ifdef HAVE_IPV6
        UINT32                  flow_label;    // IPv6 flow label      
#endif // HAVE_IPV6
        // These cache the "getsockname()" source/dest for connected/accepted sockets
        // TBD - perhaps should have "GetLocalAddress()" and "GetRemoteAddress()" methods instead?
        //       althought the cached information is useful for post-mortem (closure) purposes
        ProtoAddress            source_addr;  // connected/accepted local address/port
        ProtoAddress            destination;  // connected/accepted destination address/port
                
        Notifier*               notifier;                          
        bool                    notify_output;    
        bool                    notify_input;    
        bool                    notify_exception;
#ifdef WIN32
        HANDLE                  input_event_handle;
        HANDLE                  output_event_handle;
        bool                    output_ready;  // used to morph edge triggered Win32 sockets to level-triggered behavior
        bool                    input_ready;   // used to morph edge triggered Win32 sockets to level-triggered behavior
        bool                    closing;
#endif // WIN32
        Listener*               listener;   
        
        const void*             user_data;

	private:
		static const int IFBUFSIZ = 256;  // for GetHostAddressList
		static const int IFIDXSIZ = 256;  //   "
#ifdef WIN32
		static LPFN_WSARECVMSG  WSARecvMsg;
#else
		static int GetInterfaceList(struct ifconf& conf);  // helper fn
#endif  // if/else WIN32

        static IPv6SupportStatus ipv6_support_status;

};  // end class ProtoSocket

#endif // _PROTO_SOCKET
