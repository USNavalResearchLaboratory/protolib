/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson, Joe Macker and Justin Dean of the 
 *       Naval Research Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/

// New router template for adding a protocol to CMU extensions of ns2

// IT changed absolute references to header files - 7/17/2008 - don't know why they
// would need to include protolib/ns ...
#include <nsProtoManetKernel.h>
#include <nsProtoManetKernel_packet.h>

/*
  TCL Hooks
*/
int hdr_ProtolibMK::offset_;

static class ProtolibMKHeaderClass : public PacketHeaderClass {  
public:

  ProtolibMKHeaderClass() : PacketHeaderClass("PacketHeader/ProtolibMK",//0){
    					   sizeof(hdr_ProtolibMK)) { //this is might not be correct...
    bind_offset(&hdr_ProtolibMK::offset_);
  } 
} class_rtProtoProtolibMK_hdr;


static class ProtolibMKclass : public TclClass {
public:
  ProtolibMKclass() : TclClass("Agent/ProtolibMK") {}
  TclObject* create(int argc, const char*const* argv) {
    assert(argc == 5); 
    return (new ProtolibMK((nsaddr_t) Address::instance().str2addr(argv[4]))); 
  }
} class_rtProtoProtolibMK;

// interface with tcl scripts can change valuse on the fly by getting the class instance handle in tcl
int 
ProtolibMK::command(int argc, const char*const* argv) {
  if(argc == 2) {
    Tcl& tcl = Tcl::instance(); 
    
    if(strncasecmp(argv[1], "id", 2) == 0) {
      tcl.resultf("%d", index);
      return TCL_OK;
    }
    
    if(strncasecmp(argv[1], "start", 2) == 0) { 
      return TCL_OK;
    }               
  }
  else if(argc == 3) {
    if(strcmp(argv[1], "port-dmux") == 0) {
      dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
      if (dmux_ == 0) {
	fprintf(stderr,"protolibManetKernel::command: %s lookup of %s failed\n",argv[1],argv[2]);
	return TCL_ERROR;
      }
      return TCL_OK;
    }
    else if(strcmp(argv[1], "attach-manet") == 0) {
      manetpointer = (Agent*)TclObject::lookup(argv[2]); 
      return TCL_OK;
    }
    else if(strcmp(argv[1], "log-target" ) == 0 || strcmp(argv[1], "tracetarget") == 0) {
      logtarget = (Trace*) TclObject::lookup(argv[2]);
      if(logtarget == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    else if(strcmp(argv[1], "if-queue") == 0) {
      ifqueue = (PriQueue*) TclObject::lookup(argv[2]);
      if(ifqueue == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    else if(strcmp(argv[1], "target") == 0) {
      Tcl& tcl = Tcl::instance();
      tcl.evalf("%s set node_", this->name());
      tcl.evalf("%s set ifq_(0)",tcl.result());
      ifqueue = (PriQueue*) TclObject::lookup(tcl.result());
      return Agent::command(argc, argv); 
    }
  }
  return Agent::command(argc, argv); 
}

/* 
   Constructor
*/

ProtolibMK::ProtolibMK(nsaddr_t id) : Agent(PT_PROTOLIBMK) {
  srand(10);
  index = id;
  logtarget = 0;
  numberofforwards = 0;
  numberofrecv = 0;
  idnumber = 0;
}

void
ProtolibMK::recv(Packet *p, Handler*) {
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  assert(initialized());
  /*  fprintf(stdout,"\nAddress %d at time %f\n ",(int)here_.addr_,Scheduler::instance().clock());
  fprintf(stdout,"Pmanet cm header %d type, %d size, %d uid, %d errorf",ch->ptype_,ch->size_,ch->uid_,ch->error_);
  fprintf(stdout,", %d errbit, %d fecsize, %f ts, %d iface",ch->errbitcnt_,ch->fecsize_,ch->ts_,ch->iface_);
  if(ch->direction_==-1)
    fprintf(stdout,", DOWN\n");
  else if(ch->direction_==0)
    fprintf(stdout,", NONE\n");
  else 
  fprintf(stdout,", UP\n");
  fprintf(stdout,"Pmanet cmd contu %d prev, %d next, %d last, %d num forwards\n",ch->prev_hop_,ch->next_hop_,ch->last_hop_,ch->num_forwards());*/
  //if(here_.addr_==1 && ih->dport()!=698){
  //  fprintf(stdout,"Pmanet ip header %d src %d dst %d ttl %d sport %d dport %d fid %d prio\n",(int)ih->src_.addr_,(int)ih->dst_.addr_,ih->ttl_,ih->sport(),ih->dport(),ih->fid_,ih->prio_);
  //  fflush(stdout);
  // }

  if(ch->ptype() == PT_PROTOLIBMK) {
    ih->ttl_ -= 1;
    //    fprintf(stdout,"sending packet to manetpointer recv and freeing up packet\n");
    manetpointer->recv(p,NULL);
    return;
  }
  if((ih->saddr() == index) && (ch->num_forwards() == 0)) {
    // if I'm originating a (nonOLSR) packet...
    //ch->size() += IP_HDR_LEN; //I don't think I need to add the ip header length after all
    if(ih->ttl_ == 0)
      ih->ttl_ = 255;       // default ttl value to 255
    ch->uid_ = idnumber++; //replacing global id number with local node id number
    //ih->fid_ = idnumber++; //attempting to use the ip flow id number instead of the unique id number for duplicate detection.
	if(ih->dport()==698){
      ch->ptype() = PT_PROTOLIBMK;
      ih->daddr() = IP_BROADCAST;
      //      fprintf(stdout,"setting type to pt_pmanet %d==%d?\n",ch->ptype(),PT_PROTOLIBMK);
    } 
    if(ih->daddr()== (int)IP_BROADCAST){ // if its broadcast message it need more configuration
      //ih->ttl_ = 1; //32 seems reasonable 
    }
  } else if(ih->saddr() == index) {
    //    fprintf(stdout,"dropping packet because of a route loop?");
    //drop(p, DROP_RTR_ROUTE_LOOP);
	Packet::free(p);
	return;
  } else {
    if(--ih->ttl_ == 0) {
      //      fprintf(stdout,"dropping packet because of ttl value\n");
      drop(p, DROP_RTR_TTL);
      return;
    }
  }
  if(ih->daddr()== (int)IP_BROADCAST){
    //fprintf(stdout,"at time %f %d is sending bcast packet to %d %d==%d? on port %d invalid %d\n",Scheduler::instance().clock(),here_.addr_,ih->daddr(),PT_PROTOLIBMK,ch->ptype(),ih->dport(),INVALID);
    if(ih->dport()!=-1){
      if(ih->dport()!=698){ //use forwarding engine to forward all broadcast packets
	//if(here_.addr_==1){
	//  fprintf(stdout,"ProtolibMK accepting bcast packet %d,%d at time %f\n",++numberofrecv,ih->fid_,Scheduler::instance().clock());
	//}
	//We need to do Classifier::find stuff but without the tcl code
	//NsObject* node = dmux_->find(p);
	NsObject* node = ProtolibMKFind(p);//does same stuff as classifier::find but without tcl error
	if (node) { //packet should be accpeted at this node
	  //  fprintf(stdout,"sending copy up to dmux in protomanetKernel\n");
	  Packet *copy_p=p->copy(); //send copy up to agent so that we can attempt to forward packet.
	  if(!copy_p) {
	    fprintf(stderr,"ProtoManetKernel::recv error copying packet returning without recving packet\n");
	    return;
	  }
	  dmux_->recv(copy_p,NULL);
	} 
	//attempt to send packet back out interface
	manetpointer->recv(p,this); //recv with non null is really a multicast forwarder and  will check to see if it should forward the packetit uses bcastforward to actually send the packet back out.

      } else { //end only non router packets
	forward(p,(nsaddr_t)ih->daddr());//send packet to bcast address
      }
    } else {
      //      fprintf(stdout,"dropping packet because ih->daddr() is broadcast but dport is equal to -1\n");
      drop(p, DROP_RTR_NO_ROUTE);
    }
  } else {
    //    fprintf(stdout,"%d is sending packet up to manetpointer\n",here_.addr_);
    manetpointer->recv(p,NULL); //manet will figure out where udp packet goes and then call our forward method    
  }
  return;
}
NsObject* ProtolibMK::ProtolibMKFind(Packet* p){
  NsObject* returnnode = NULL;
  int cl = ((Classifier*)dmux_)->classify(p);
  if ((returnnode = dmux_->slot(cl)) == 0) {
    //    if (dmux_->default_target_) 
    //  return dmux_->default_target_;
  }
  return returnnode;
}

// will get called when link layer detects error
void ProtolibMK::rt_failed(Packet *p) {
  drop(p,DROP_RTR_NO_ROUTE); // this line took me a week to find arggg!
}

void ProtolibMK::forward(Packet *p, nsaddr_t nexthop) {
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);

    /* check ttl */
    if (ih->ttl_ == 0){
      //      fprintf(stdout,"ProtolibMK dropping packet in forward because ttl == 0\n");
      drop(p, DROP_RTR_TTL);
      return;
    }

	//check to see if the packet is meant for myself
	if (nexthop == here_.addr_) {
	  ch->next_hop_ = nexthop;
	  ch->addr_type() = NS_AF_INET;
	  ch->direction() = hdr_cmn::UP;
	} else {
	  //    ch->num_forwards_++;
	  ch->prev_hop_ = here_.addr_;
	  ch->next_hop_ = nexthop;
	  ch->addr_type() = NS_AF_INET;
	  ch->direction() = hdr_cmn::DOWN;
	  //    fprintf(stdout,"ProtolibMK at time %f %d is attempting to foward packet to %d\n",Scheduler::instance().clock(),here_.addr_,nexthop);
	}
	  // Send the packet

    Scheduler::instance().schedule(target_, p, 0.);
}

bool ProtolibMK::bcastforward(Packet *p) { //right now this just does what forward does but returns true will add smarts later
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
	double jitter;

    /* check ttl */
    if (ih->ttl_ == 0){
      //DMSG(0,"ProtolibMK dropping packet in bcastforward because ttl == 0\n");
      drop(p, DROP_RTR_TTL);
      return false;
    }
    //   if(here_.addr_==1){
    //DMSG(2, "ProtolibMK forwarding bcast packet %d,%d at time %f\n",++numberofforwards,ih->fid_,Scheduler::instance().clock());
    // }
    //    ch->num_forwards_++;
    ch->prev_hop_ = here_.addr_;
    ch->next_hop_ = IP_BROADCAST;
    ch->addr_type() = NS_AF_INET;
    ch->direction() = hdr_cmn::DOWN;
    //    fprintf(stdout,"ProtolibMK at time %f %d is attempting to bcastfoward packet to %d\n",Scheduler::instance().clock(),here_.addr_,nexthop);
    // Send the packet
	jitter = 0.00 * (double)rand() / (double)RAND_MAX;
    Scheduler::instance().schedule(target_, p, jitter);
    return true;
}





 

