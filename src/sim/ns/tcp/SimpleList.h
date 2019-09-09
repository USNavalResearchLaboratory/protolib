#ifndef _SIMPLELINKED_LIST
#define _SIMPLELINKED_LIST

#include "protoDebug.h"

/*
 *  SimpleList.h
 *
 *  Created by Ian Taylor on 17/07/2008.
 * 
 * Simple List is a linked list that consists of ListItem objects.  ListItemObjects can be 
 * subclassed by any data object in order to add your own data to Simple list. Therefore,
 * ListItem is an interface that a class can inherit from to become a an item 
 * in a SimpleList.
 *
 * SimpleList could implement lots of other linked list-type things, like inserting 
 * but the implementation assumes that uses of simple list will be much like a 
 * FIFO and elements are always inserted as a head, although it can be searched and
 * elements can be deleted from anywhere in the list.  Incidentally, you can use
 * SimpleList list a FIFO by using getHead() to get the head item and then removing 
 * that from the list if desired.
 */

class ListItem {	
public:
	ListItem();
									         
	ListItem* getPrev() {return prev;}
    void setPrev(ListItem* broker) {prev = broker;}
    ListItem* getNext() {return next;}
    void setNext(ListItem* broker) {next = broker;}
	
	void setIdentifier(unsigned int ident) { identifier=ident; } 
	void setAddress(unsigned int addr) { address=addr; } 
	void setPort(unsigned int portno) {  portnumber=portno; } 

	unsigned int getIdentifier() { return identifier; } 
	unsigned int getAddress() { return address; } 
	unsigned int getPort() { return portnumber; } 
	
	private:
		ListItem* prev;    
		ListItem* next;    
		unsigned int identifier;
		unsigned int portnumber;
		unsigned int address;
};
  
class SimpleList {
	public:
		SimpleList();
		~SimpleList();

		ListItem *getHead() { return head; }
		ListItem *getTail() { return tail; }
		
		void prepend(ListItem *item);
		void remove(ListItem *item);

		void print();
		
		/**  Find list item by searching for list items with address and port number as specified **/
		ListItem* findProxyByPortAndAddress(unsigned int  address, unsigned int  port);
						
		/**
		* Finds the Socket proxy by searching the IDs which
		* can be set and retrieved using the GetSocketPrioxyID and 
		* GetSocketPrioxyID respectively. 
		*/
		ListItem* findProxyByIdentifier(unsigned int  proxyID);
                        
		protected: 						
			ListItem*  head;
			ListItem*  tail;
};  // end class ProtoSimAgent::SocketProxy::List

#endif	
