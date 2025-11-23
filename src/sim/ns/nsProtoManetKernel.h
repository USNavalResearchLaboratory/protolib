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

#ifndef __ProtoManetKernel_h__
#define __ProtoManetKernel_h__

#include <cmu-trace.h>
#include <priqueue.h>
#include <rtqueue.h>
#include <rttable.h>
#include <classifier/classifier-port.h>
//#include <packet.h>
#include <red.h>
#include "protoDebug.h"       // for DMSG()


#define CURRENT_TIME    Scheduler::instance().clock()
#define INVALID                -1                          // invalid return value

class ProtolibMK; // forward decleration

/**
 * @class BroadcastID
 *
 * @brief Broadcast ID Cache
*/
class BroadcastID {
  friend class ProtolibMK;
 public:
        BroadcastID(nsaddr_t i, u_int32_t b) { src = i; id = b;  }
 protected:
        LIST_ENTRY(BroadcastID) link;
        nsaddr_t        src;
        u_int32_t       id;
        double          expire;         // now + BCAST_ID_SAVE s
};

LIST_HEAD(bcache, BroadcastID);


/**
 * @class ProtolibMK
 * 
 * @brief  The Routing Agent
 */
class ProtolibMK: public Agent {

 public:
        ProtolibMK(nsaddr_t id); // constructor

        void		recv(Packet *p, Handler *);
	void            rt_failed(Packet *p);
	int             index;
	void            forward(Packet *p, nsaddr_t nexthop);// used to forward packets in entirety
	bool            bcastforward(Packet *p); // used to decied to forward broadcast packets back out interface. returns true if it does.

 protected:
	int idnumber;
	int numberofforwards, numberofrecv;
        int             command(int, const char *const *); 
        int             initialized() { return 1 && target_; } 
	NsObject*       ProtolibMKFind(Packet* p); //used to see if listener is attached to node code modified from Classifier::find
	Agent*          manetpointer;
								/*
         * Packet TX Routines
         */
	void            forward(char *copysh); // used for forwarding the sub header part of the packet 
	
        bcache          bihead;                 // Broadcast ID Cache

        /*
         * A mechanism for logging the contents of the routing
         * table.
         */
	 Trace           *logtarget;

        /*
         * A pointer to the network interface queue that sits
         * between the "classifier" and the "link layer".
         */
        PriQueue        *ifqueue;
	/* for passing packets up to agents */
	PortClassifier *dmux_;

};

#endif /* __ProtoManetKernel_h__ */










