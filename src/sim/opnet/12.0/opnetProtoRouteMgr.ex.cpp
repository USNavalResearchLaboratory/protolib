// LP 7-16-04 - replaced
// #include "OpnetProtoRouteMgr.h"
#include "opnetProtoRouteMgr.h"  
// end LP
 
OpnetProtoRouteMgr::OpnetProtoRouteMgr()
	{
#ifdef OP_DEBUG2
 printf("\t\tOpnetProtoRouteMgr::OpnetProtoRouteMgr()\n");
#endif
//	localTable.Init();
	}

OpnetProtoRouteMgr::~OpnetProtoRouteMgr()
	{
	}

// LP 4-22-04 - replaced for new ProtRouteMgr.h
/*
bool OpnetProtoRouteMgr::Open()
	{
#ifdef OP_DEBUG1
	printf("\t\tOpnetProtoRouteMgr::Open()\n");
#endif
	bool r_val = OPC_TRUE;
	return (r_val);
	}
*/

bool OpnetProtoRouteMgr::Open(const void* userData)
	{
#ifdef OP_DEBUG1
	printf("\t\tOpnetProtoRouteMgr::Open()\n");
#endif
	bool r_val = OPC_TRUE;
	return (r_val);
	}

// end LP

bool OpnetProtoRouteMgr::Open(IpT_Cmn_Rte_Table * cmn_rte_table, int nodeId, 
								IpT_Rte_Proc_Id olsr_protocol_id, IpT_Interface_Info* ip_intf_pnt)
	{	
	opnet_cmn_route_table = cmn_rte_table;
	my_node_id = nodeId;
	my_protocol_id = olsr_protocol_id;
	my_ip_interface_pnt = ip_intf_pnt;
#ifdef OP_DEBUG1
	printf("Node %d - OpnetProtoRouteMgr::Open() - cmn_route_table = %ld, protocol_id = %I64d, ip_interf_pnt = %ld\n", 
			my_node_id, opnet_cmn_route_table, my_protocol_id, my_ip_interface_pnt);
#endif
	
	bool r_value = OPC_TRUE;
	return r_value;
	}


bool OpnetProtoRouteMgr::IsOpen() const
	{
	if (opnet_cmn_route_table != OPC_NIL)
		return (OPC_TRUE);
	else
		return (OPC_FALSE);
	}

void OpnetProtoRouteMgr::Close()
	{
	}

bool OpnetProtoRouteMgr::GetAllRoutes(ProtoAddress::Type addrType,
							ProtoRouteTable& routeTable)
	{
	// This function is called by the nrlOLSR.pr.c as a part of 
	// the nrlolsr->Start().  When running using ns, this does nothing.
	// We may not need this function for Opnet either.  So, it's a 
	// place-holder for now.  LP 3-8-04
#ifdef OP_DEBUG1
	printf("\t\tOpnetProtoRouteMgr::GetAllRoutes().\n");
#endif

	return OPC_TRUE;
	}
	
bool OpnetProtoRouteMgr::GetRoute(const ProtoAddress& dst, 
                              unsigned int        prefixLen,
                              ProtoAddress&       gw,
                              unsigned int&       ifIndex,
                              int&                metric)
	{
#ifdef OP_DEBUG1
	printf("\t\tOpnetProtoRouteMgr::GetRoute() - Not used yet. \n");
#endif
	return OPC_TRUE;
	}
		
bool OpnetProtoRouteMgr::SetRoute(const ProtoAddress&   dst,
                              unsigned int          prefixLen,
                              const ProtoAddress&   gw,
                              unsigned int          ifIndex,
                              int                   metric)
  	{
	/* 	Ip_Cmn_Rte_Table_Entry_Add (
		opnet_cmn_route_table,   // pointer to the IP Common Route Table
		src_obj_ptn,  // pointer to the entry in the source routing protocol, may be a function
		dest,  // IP address ofthe destination network
		mask,  // subnet mask of the destination network
		next_hop, // IP address ofthe interface that should be used as thenext hop for the destination
		port_info, // contans the "addr_index" of the interface used t o reach the next hop
		metric, // metric assigned to this next hop
		my_protocol_id, // the unique routing protocol id
		admin_distance     // the preference asssociated with this entry
		);
	*/
	Compcode comp;
	IpT_Address dest, mask, next_hop;
	IpT_Port_Info port_info;
	int admin_distance;
        // JPH - 12/6/06
	DeleteRoute(dst, prefixLen, gw, ifIndex);

	dest = (IpT_Address) dst.SimGetAddress();

	/* from Jim Hauser's */
	if(dest ==INADDR_ANY)  
		mask = NETMASK_DEFAULT;
	else
		mask = NETMASK_HOST;
	/* end JH  */

	port_info.intf_tbl_index = my_ip_interface_pnt->phys_intf_info_ptr->ip_addr_index;  // JPH changed addr_index to ip_addr_index
	port_info.output_info.intf_name = "WLAN"; // JPH  output_info union added
	port_info.minor_port = IPC_SUBINTF_PHYS_INTF; // This value is -1.  LP 7-23-04 - added
	
	next_hop = (IpT_Address) gw.SimGetAddress();
	if (next_hop == 0)   //  the dest is directly connected to this node
		next_hop = (IpT_Address) dst.SimGetAddress();  
	
	admin_distance = 0;  

#ifdef OP_DEBUG1
	printf("Node %d - OpnetProtoRouteMgr::SetRoute() - dest = %u, next-hop = %u\n", 
		my_node_id, dest, next_hop);

	printf("\t\t - port_info->intf_tbl_index = %hd, metrix = %d, admin_distance = %d\n", 
		port_info.intf_tbl_index, metric, admin_distance);
#endif
	
	// LP 3-16-04. Note that the source_obj_ptr is set to OPC_NIL here. That parameter
	// would be used in  Ip_Cmn_Rte_Table_Entry_Add() when creating Rte_Table_Entry.
	// In the Entry creation process, if the new route is a directly connected route, the
	// new_entry->route_src_obj_ptr = new_entry; else, it = source_obj_ptr.  Need to
	// double check if OPC_NIL is OK for OLSR case.
	
	//comp = Ip_Cmn_Rte_Table_Entry_Add (  JPH 8/17/2006 - add IPC_CMN_RTE_TABLE_ENTRY_ADD_INDIRECT_NEXTHOP_OPTION
	comp = Inet_Cmn_Rte_Table_Entry_Add_Options (
		opnet_cmn_route_table,
		my_ip_interface_pnt,
		//dest,
		//mask,
		ip_cmn_rte_table_v4_dest_prefix_create(dest,mask),
		inet_address_from_ipv4_address_create(next_hop),
		port_info,
		metric,
		my_protocol_id,
		admin_distance,
		IPC_CMN_RTE_TABLE_ENTRY_ADD_INDIRECT_NEXTHOP_OPTION);
	
	if (op_prg_odb_ltrace_active ("olsr route table"))
		ip_cmn_rte_table_print (opnet_cmn_route_table);
	return ((bool)comp);
	}
		
bool OpnetProtoRouteMgr::DeleteRoute(const ProtoAddress&    dst,
                                 unsigned int           prefixLen,
                                 const ProtoAddress&    gw,
                                 unsigned int           ifIndex)
	{
	Compcode comp;
	IpT_Address dest, mask, next_hop;
	IpT_Port_Info port_info;
	int admin_distance;
	
	dest = (IpT_Address) dst.SimGetAddress();
	
#ifdef OP_DEBUG1
	printf("Node %d - OpnetProtoRouteMgr::DeleteRoute() - dest = %u\n", my_node_id, dest);
#endif
	
	/* from Jim Hauser's */
	if(dest ==INADDR_ANY)  
		mask = NETMASK_DEFAULT;
	else
		mask = NETMASK_HOST;

	/* end JH  */

	port_info.intf_tbl_index = my_ip_interface_pnt->phys_intf_info_ptr->ip_addr_index;  // JPH  addr_index changed to ip_addr_index
	port_info.output_info.intf_name = "WLAN";  // JPH added output_info union

	next_hop = (IpT_Address) gw.SimGetAddress();	
	if (next_hop == 0)   //  - the dest is directly connected to this node
		next_hop = (IpT_Address) dst.SimGetAddress();  
	admin_distance = 0;  
	

	/* LP  - Note that the following command delete the entire destination entry
	 from the IP Route Table.  If we want to delte only a next hop from the entry from 
	 IP Route Table, the command would be Ip_comn_Rte_Table_Entry_Delete) */
	
	comp = Ip_Cmn_Rte_Table_Route_Delete (
		opnet_cmn_route_table,
		dest,
		mask, 
		my_protocol_id);

	
	if (op_prg_odb_ltrace_active ("olsr route table"))
		ip_cmn_rte_table_print (opnet_cmn_route_table);
	return ((bool)comp);
	}

bool OpnetProtoRouteMgr::SetForwarding(bool state)
	{
	forwardingOn = state;
	return OPC_TRUE;
	}

OpnetProtoRouteMgr* OpnetProtoRouteMgr::Create()
{
#ifdef OP_DEBUG1
	printf("\t\tOpnetProtoRouteMgr::Create()\n");
#endif
  return (new OpnetProtoRouteMgr);
}   // end OpnetProtoRouteMgr::Create()


ProtoRouteMgr* ProtoRouteMgr::Create()
{
  return (ProtoRouteMgr*)(new OpnetProtoRouteMgr);
} // end ProtoRouteMgr::Create()


// LP 4-22-04 - added
// JPH 11-3-06 modified 
//bool OpnetProtoRouteMgr::GetInterfaceAddress(unsigned int       ifIndex, 
//			   ProtoAddress::Type addrType,
//			   ProtoAddress&    theAddress)
bool OpnetProtoRouteMgr::GetInterfaceAddressList(unsigned int       ifIndex, 
			   ProtoAddress::Type addrType,
			   ProtoAddressList& addrList)
{
//	FIN (OpnetProtoRouteMgr::GetLocalAddress()); JPH
	FIN (OpnetProtoRouteMgr::GetInterfaceAddressList(ifIndex,addrType,addrList))
	IpT_Interface_Info * ip_intf_pnt;
	ip_intf_pnt = this->GetIntfInfoPnt();
 	IpT_Address IpAddr = ip_intf_pnt->addr_range_ptr->address;
//	IpT_Address IPSubnetMask = ip_intf_pnt->addr_range_ptr->subnet_mask;
    ProtoAddress ifAddr;
	ifAddr.SimSetAddress((SIMADDR)IpAddr);
    bool result = addrList.Insert(ifAddr);
    if (!result)
        PLOG(PL_ERROR, "OpnetProtoRouteMgr::GetLocalAddress() error: "
                "unable to add addr to list\n");
#ifdef OP_DEBUG1
	printf("OpnetProtoRouteMgr::GetLocalAddress(my_Ip_Addr = %u)\n", IpAddr);
#endif
	 FRET (result);
		

}

// LP 4-19-04 - added 


unsigned int OpnetProtoRouteMgr::GetInterfaceIndex(const char* interfaceName)
{
	// LP 5-4-04 - replaced
	// return OPC_NIL; 
	FIN (OpnetProtoRouteMgr::GetInterfaceIndex());
	IpT_Interface_Info * ip_intf_pnt;
	ip_intf_pnt = this->GetIntfInfoPnt();
	
	// Note that in Opnet, the interface index starts from 0.  However,
	// in NRL-OLSR code, the interface index is expected to start from 1.
	// So, we incrementthe Opnet index by 1 here.
	
	int index = ip_intf_pnt->phys_intf_info_ptr->ip_addr_index + 1; /* JPH addr_index changed to ip_addr_index */
#ifdef OP_DEBUG1
	printf("Node %d - opnetProtoRouteMgr.pr.c - GetInterfaceIndex() - name = %s, index = %d\n",
				this->getNodeId(), interfaceName, index);
#endif
	FRET ( (unsigned int) index);
	// end LP 5-4-04

}
 

bool OpnetProtoRouteMgr::GetInterfaceName(unsigned int  interfaceIndex, 
                                      char*         buffer, 
                                      unsigned int  buflen)
	{
	// LP 5-4-04 - replaced
	// return OPC_TRUE;

	FIN (OpnetProtoRouteMgr::GetInterfaceName());
	IpT_Interface_Info * ip_intf_pnt;
	ip_intf_pnt = this->GetIntfInfoPnt();
	strcpy (buffer, ip_intf_pnt->full_name);
#ifdef OP_DEBUG1
	printf("Node %d - opnetProtoRouteMgr.pr.c - GetInterfaceName() - name = %s\n",
				this->getNodeId(), buffer);
#endif	
	FRET (OPC_TRUE);

	// end LP 5-4-04
	}

// end LP 4-19-04

/*  OPNET data type
typedef unsigned int				IpT_Address;
typedef IpT_Address					IpT_Router_Id;
typedef OpT_uInt8	InetT_Subnet_Mask;

typedef struct InetT_Address
	{
	OpT_uInt8					addr_family;		/ InetT_Addr_Family value	/
	union
		{
		IpT_Address				ipv4_addr;			/ Ipv4 address.			/
		struct Ipv6T_Address*	ipv6_addr_ptr;		/ Ipv6 address.			/
		} address;
	} InetT_Address;

*/
