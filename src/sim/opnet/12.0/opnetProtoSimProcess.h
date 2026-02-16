#ifndef _OPNET_PROTO_SIM_PROCESS
#define _OPNET_PROTO_SIM_PROCESS

// The "OpnetProtoSimProcess" plays a role similar to that of the 
// "ProtoDispatcher" class which provides liason between Protolib 
// classes and the computer operating system.

// Opnet agents based on Protolib code can use this as a base
// class.  ProtoSockets used by the Protolib code _must_ have
// their "notifier" set as a pointer to their corresponding
// OpnetProtoSimProcess instance.  Similarly, ProtoTimerMgrs used by the 
// Protolib code _must_ have their "installer" set to their 
// corresponding "OpnetProtoSimProcess" instance.

#include "protoSimAgent.h"
#include "protoSocket.h"
#include "protoTimer.h"
#include "opnet.h"
#include <tcp_api_v3.h>
#include "mgen.h"

#define SELF_INTRT_CODE_TIMEOUT_EVENT 0
#define MAX_NUM_TCP_CONNECTIONS 32

class OpnetProtoMessageSink : public ProtoMessageSink
{
	public:
	OpnetProtoMessageSink();
	OpnetProtoMessageSink(Ici* iciptr, int ostrm, int port);
	~OpnetProtoMessageSink();
	bool HandleMessage(const char* txBuffer, unsigned int len,const ProtoAddress& srcAddr);
	
	private:
	Ici* app_ici_ptr;
	int outstrm_to_mgen;
	int loc_port;
};

class OpnetProtoSimProcess : public ProtoSimAgent
{
    public:
		// LP 6-13-04 - replaced
        // virtual ~OpnetProtoSimProcess();
        ~OpnetProtoSimProcess();
	
		// end LP
        
        // These must be overridden by derived OPNET process class
        virtual bool OnStartup(int argc, const char*const* argv) = 0;
        virtual bool ProcessCommands(int argc, const char*const* argv) = 0;
        virtual void OnShutdown() = 0;    

		// Some helper methods
        void ActivateTimer(ProtoTimer& theTimer)
            {ProtoSimAgent::ActivateTimer(theTimer);}
        
        void DeactivateTimer(ProtoTimer& theTimer)
            {ProtoSimAgent::DeactivateTimer(theTimer);}
        
        ProtoSocket::Notifier& GetSocketNotifier() 
            {return ProtoSimAgent::GetSocketNotifier();}
        
        ProtoTimerMgr& GetTimerMgr()
            {return ProtoSimAgent::GetTimerMgr();}
        
		virtual bool GetLocalAddress(ProtoAddress& localAddr)
		{
		    localAddr.SimSetAddress((unsigned int)op_id_self());
            return true;
		}
        
        void OnSystemTimeout() {ProtoSimAgent::OnSystemTimeout();}
        
        // Some OPNET specific utility methods
        void SetUdpProcessId(Objid udpProcessId) 
            {udp_process_id = udpProcessId;}
		void SetTcpHostAddress(ProtoAddress hostAddr)
			{tcp_host_addr = hostAddr;}
		void SetTcpRemAddress(ProtoAddress remAddr)  // JPH 7/27/2006 - TCP support
			{tcp_ind_rem_addr = remAddr;}
		void SetTcpAppHandle(ApiT_Tcp_App_Handle tcpHandle)
			{tcp_handle[0] = tcpHandle;}
		void SetTcpAppHandle(ApiT_Tcp_App_Handle tcpHandle, int cid)
			{tcp_handle[cid] = tcpHandle;}
        void OnReceive(int strm_indx,char* node_name);  // JPH 4/11/06 added strm_indx, 5/24/07 added node_name
        void OnAccept(char* node_name);  // JPH 5/25/07 
		SocketProxy* FindProxyByConn(int connId)
			{return socket_proxy_list.FindProxyByConn(connId);} // JPH 8/13/2006 - TCP support

        // These can be overridden for packet stats, etc ...
        virtual void ReceivePacketMonitor(Ici* ici, Packet* pkt) {}
        virtual void TransmitPacketMonitor(Ici* ici, Packet* pkt) {}
		virtual void HandleMgenMessage(char* 				buffer, 
										unsigned int        len, 
										const ProtoAddress& srcAddr) {};
               
    protected:
        OpnetProtoSimProcess();
        
        // override of ProtoSimAgent::UpdateSystemTimer() method
        bool UpdateSystemTimer(ProtoTimer::Command command,
                               double              delay)
        {
			// printf("\t\tOpnetProtoSimProcess.h - Class OpnetProtoSimProces::UpdateSystemTimer(command,delay)\n"); // LP
             switch (command)
            {
                case ProtoTimer::MODIFY:
                    op_ev_cancel(timer_handle);
                case ProtoTimer::INSTALL:					
                    timer_handle = 
                        op_intrpt_schedule_self(op_sim_time() + delay, SELF_INTRT_CODE_TIMEOUT_EVENT);
                    break;
                case ProtoTimer::REMOVE:
                    op_ev_cancel(timer_handle);
                    //timer_handle = (Evhandle)NULL;  // ???
                    break;   
            }  // end switch	
            return true;
        } // end bool UpdatesSystemTime()
		
        
        // The "UdpSocketProxy" provides liason between a UDP "ProtoSocket"
        // instance and the neighboring OPNET UDP process
		class UdpSocketProxy;
		friend UdpSocketProxy;
        class UdpSocketProxy : public ProtoSimAgent::SocketProxy
        {
            friend class OpnetProtoSimProcess;
            
            public:
                UdpSocketProxy(OpnetProtoSimProcess* sim_process); // JPH  sim_process param
                virtual ~UdpSocketProxy();
            
                bool Bind(UINT16& thePort);
				bool Connect(const ProtoAddress& theAddress){return false;}
				bool Listen(UINT16 thePort){return false;}
				bool Accept(ProtoSocket* theSocket){return false;}
                bool RecvFrom(char* buffer, unsigned int& numBytes, ProtoAddress& srcAddr);
                                
                bool JoinGroup(const ProtoAddress& groupAddr);
                bool LeaveGroup(const ProtoAddress& groupAddr);
                void SetTTL(unsigned char ttl) {mcast_ttl = ttl;}
                void SetLoopback(bool loopback) {mcast_loopback = loopback;}
                void SetStream(int theStream) {strm_index = theStream;}  // JPH 3/30/06

                virtual bool SendTo(const char*         buffer, 
                                    unsigned int&       numBytes, 
                                    const ProtoAddress& dstAddr);
									
				void OnReceive(char*               recvData, 
                               unsigned int        recvLen,
                               const ProtoAddress& srcAddr,
                               const ProtoAddress& dstAddr);
                int GetConn(){return 0;}
 
            protected:
                //Objid           udp_process_id;
			    OpnetProtoSimProcess* sim_process; // JPH need sim_process rather than udp_process_id
                unsigned char   mcast_ttl;
                bool            mcast_loopback;
                char*           recv_data;
                UINT16          recv_data_len;
                ProtoAddress    src_addr;
			    Ici*			udp_command_ici;  // LP 6-21-04 - added
				int				strm_index;		  // JPH 3/30/06 - added
        };  // end class OpnetProtoSimProcess::UdpSocketProxy
        
        // The "TcpSocketProxy" provides liason between a TCP "ProtoSocket"  -  JPH 7/3/06
        // instance and the neighboring OPNET TCP process
		class TcpSocketProxy;
        class TcpSocketProxy : public ProtoSimAgent::SocketProxy
        {
            friend class OpnetProtoSimProcess;
            
            public:
                TcpSocketProxy(OpnetProtoSimProcess* sim_process); // JPH  sim_process param
                virtual ~TcpSocketProxy();
            
                bool Bind(UINT16& thePort);
				bool Connect(const ProtoAddress& theAddress);
				bool Listen(UINT16 thePort);
				bool Accept(ProtoSocket* theSocket);
                bool RecvFrom(char* buffer, unsigned int& numBytes, ProtoAddress& srcAddr);
                bool JoinGroup(const ProtoAddress& groupAddr);
                bool LeaveGroup(const ProtoAddress& groupAddr);
                void SetTTL(unsigned char ttl) {}
                void SetLoopback(bool loopback) {}
                void SetStream(int theStream) {strm_index = theStream;}  // JPH 3/30/06
			
                virtual bool SendTo(const char*         buffer, 
                                    unsigned int&       numBytes, 
                                    const ProtoAddress& dstAddr);
									
 									
				void OnReceive(char*               recvData, 
                               unsigned int        recvLen,
                               const ProtoAddress& srcAddr,
                               const ProtoAddress& dstAddr);
				
				UINT16 GetPort(){return local_port;}
				ProtoAddress GetTcpHostAddress(){return sim_process->tcp_host_addr;}
				ProtoAddress GetTcpRemAddress() {return sim_process->tcp_ind_rem_addr;}
                bool Send(const char*         buffer, 
                          unsigned int&       numBytes);
                bool Recv(char*         buffer, 
                          unsigned int&       numBytes);
				int GetConn(){return conn_id;}
				void SetSrcAddr(ProtoAddress saddr){src_addr = saddr;}
				ProtoAddress GetSrcAddr(){return src_addr;}
                 
            protected:
                class TcpSockList : public ProtoSimAgent::SocketProxy::List
                {
                    public:
                        TcpSockList(){}
                        ~TcpSockList(){}
                        SocketProxy* FindProxyByConn(int connId);
                };  // end class TcpSockList
			
                friend class TcpSockList;
                     
                //ProtoSocket*        proto_socket;
                
                //SocketProxy*        prev;    
                //SocketProxy*        next;    
			    OpnetProtoSimProcess* sim_process; 
                char*           recv_data;
                UINT16          recv_data_len;
                ProtoAddress    src_addr;
				Ici*			tcp_command_ici;
				int				strm_index;	
				int				conn_id;
				UINT16			local_port;
				int				recv_data_read_index;
        };  // end class OpnetProtoSimProcess::TcpSocketProxy
        
	
        ProtoSimAgent::SocketProxy* OpenSocket(ProtoSocket& theSocket);
        void CloseSocket(ProtoSocket& theSocket);
	    Objid GetUdpProcId() {return udp_process_id;}
		ApiT_Tcp_App_Handle* GetTcpAppHandle(){return &tcp_handle[0];}
		ApiT_Tcp_App_Handle* GetTcpAppHandle(int cid){return &tcp_handle[cid];}
        
        Objid                           udp_process_id;
		ProtoAddress    				tcp_host_addr;
		ProtoAddress    				tcp_ind_rem_addr;
        //ProtoSimAgent::SocketProxy::List  socket_proxy_list; 
        TcpSocketProxy::TcpSockList  	socket_proxy_list; 
        Evhandle                        timer_handle;
		ApiT_Tcp_App_Handle  			tcp_handle[MAX_NUM_TCP_CONNECTIONS];  // JPH - needed to declare this last??

		
	};  // end class OpnetProtoSimProcess

#endif  //_OPNET_PROTO_SIM_PROCESS
