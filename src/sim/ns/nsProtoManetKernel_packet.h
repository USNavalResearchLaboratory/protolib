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

#ifndef __ProtolibMK_packet_h__
#define __ProtolibMK_packet_h__

/* =====================================================================
   Packet Formats...
   ===================================================================== */


#define INVALID                 -1
/*
 * NROUTER Routing Protocol Header Macros
 */

struct hdr_ProtolibMK { 
  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_ProtolibMK* access(const Packet* p) {
    return (hdr_ProtolibMK*) p->access(offset_);
  }
  inline static hdr_ProtolibMK* access(const Packet* p,const int loffset) {
    return (hdr_ProtolibMK*) p->access(offset_+loffset);
  }
};

#endif /* __ProtolibMK_packet_h__ */






