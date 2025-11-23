#ifndef _OPNET_PROTO_ROUTEMGR_
#define _OPNET_PROTO_ROUTEMGR_

#include "protoRouteMgr.h"
#include "protoAddress.h"
#include "opnet.h"
#include <ip_addr_v4.h>
#include <ip_cmn_rte_table.h>
#include "ip_rte_support.h"
#include "ip_rte_v4.h"

/* LP - from Jim Hauser's files */

#define NETMASK_HOST 0xffffffff
#define NETMASK_DEFAULT 0x0

/* end LP */

class OpnetProtoRouteMgr : public ProtoRouteMgr
	{
	public:
		OpnetProtoRouteMgr();
		~OpnetProtoRouteMgr();

	    /* LP 4-22-04 - replaced for the new ProtoRoutMgr.h
		bool Open();
		*/
		bool Open(const void* userData = NULL);	
	
	    // end LP
	
		bool Open(IpT_Cmn_Rte_Table *cmn_rte_table, int nodeId, IpT_Rte_Proc_Id olsr_protocol_id, IpT_Interface_Info* ip_intf_pnt);

		int getNodeId() {return my_node_id; } 
		bool IsOpen() const;
		void Close();
		bool GetAllRoutes(ProtoAddress::Type addrType,
							ProtoRouteTable& routeTable);
	
		bool GetRoute(const ProtoAddress& dst, 
                              unsigned int        prefixLen,
                              ProtoAddress&       gw,
                              unsigned int&       ifIndex,
                              int&                metric);
		
		bool SetRoute(const ProtoAddress&   dst,
                              unsigned int          prefixLen,
                              const ProtoAddress&   gw,
                              unsigned int          ifIndex,
                              int                   metric);
		
		bool DeleteRoute(const ProtoAddress&    dst,
                                 unsigned int           prefixLen,
                                 const ProtoAddress&    gw,
                                 unsigned int           ifIndex);

		bool SetForwarding(bool state);
		static OpnetProtoRouteMgr* Create();
		
        
        // Note this only gets one address anyway
		bool GetInterfaceAddressList(unsigned int        ifIndex, 
			                         ProtoAddress::Type  addrType,
			                         ProtoAddress::List& addrList);  
		
		// LP 4-19-04 added due to a new ProtoRouteMgr
		unsigned int GetInterfaceIndex(const char* interfaceName);
        bool GetInterfaceName(unsigned int  interfaceIndex, 
                              char*         buffer, 
                              unsigned int  buflen);
		// end LP
		
		IpT_Interface_Info * GetIntfInfoPnt() {return my_ip_interface_pnt; } 
		int descriptor;
		UINT32 sequence;
		
	private:
  		int forwardingOn;
		// ProtoRouteTable localTable;
		IpT_Cmn_Rte_Table * opnet_cmn_route_table;
		int my_node_id;
		IpT_Rte_Proc_Id my_protocol_id;
		IpT_Interface_Info * my_ip_interface_pnt;


}; // end class OpnetProtoRouteMgr

#endif // _OPNET_PROTO_ROUTEMGR_

