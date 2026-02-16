/*
 *  SimpleList.cpp
 *  AgentJ
 *
 *  Created by Ian Taylor on 17/07/2008.
 *
 */
  
#include "SimpleList.h"

ListItem::ListItem() : identifier(-1), portnumber(-1), address(-1) {}

SimpleList::SimpleList() : head(NULL), tail(NULL) {
}

SimpleList::~SimpleList() {
    ListItem* next = head;
    ListItem* todelete;
	
	// empty the list and delete objects
	while (next) {
		todelete = next;
		next = next->getNext(); 
		delete todelete;  
    }
}

void SimpleList::print() {
    ListItem* next = head;
	int count=0;
	// empty the list and delete objects
	while (next) {
		PLOG(PL_INFO, "Item %i pointer = %u\n", count, next); 
		PLOG(PL_INFO, "Prev = %u Next = %u\n", next->getPrev(), next->getNext()); 
		next = next->getNext(); 
		++count;
    }
}

 
void SimpleList::prepend(ListItem *proxy) {
	PLOG(PL_DEBUG, "Entering SimpleList::prepend\n"); 
    proxy->setPrev(NULL);
    proxy->setNext(head);
    if (head) {
		head->setPrev(proxy);
		if (head->getNext()==NULL) tail=head;
	} else tail=proxy;

	head = proxy;

	PLOG(PL_DEBUG, "Leaving SimpleList::prepend\n"); 
}  // end SimpleList::prepend()

/**
 * Removes the object from the list but does not delete the object. This is 
 * the responsibility of the caller.
 */
void SimpleList::remove(ListItem *proxy)
{
	PLOG(PL_DEBUG, "Entering SimpleList::remove\n"); 
    ListItem* prev = proxy->getPrev();
    ListItem* next = proxy->getNext();

//	PLOG(PL_DETAIL, "SimpleList, prev = %u, next =%u\n", prev, next); 
//	PLOG(PL_DETAIL, "SimpleList, head = %u, tail =%u\n", head, tail); 

    if (prev)
        prev->setNext(next);
    else
        head = next;
    if (next)
        next->setPrev(prev);
	else
		tail=prev;

//	PLOG(PL_DETAIL, "SimpleList, head = %u, tail =%u\n", head, tail); 
		
	PLOG(PL_DEBUG, "Leaving SimpleList::remove\n"); 
}  // end SimpleList::remove()

ListItem* SimpleList::findProxyByPortAndAddress(unsigned int address, unsigned int port) {
	PLOG(PL_DEBUG, "Entering SimpleList::findProxyByPortAndAddress\n"); 
    ListItem* next = head;
    while (next) {
		PLOG(PL_DEBUG, "SimpleList: comparing %u and port %u\n", next->getAddress(), next->getPort());
		PLOG(PL_DEBUG, "SimpleList: with %u and port %u\n", address, port);
        if ((next->getPort() == port) && (next->getAddress()==address))
            return next;
        else
            next = next->getNext();   
    }
	PLOG(PL_DEBUG, "Leaving SimpleList::findProxyByPortAndAddress\n"); 
    return NULL;
}  // end SimpleList::findProxyByPortAndAddress()

ListItem* SimpleList::findProxyByIdentifier(unsigned int identifier)
{
	PLOG(PL_DEBUG, "Entering SimpleList::findProxyByIdentifier\n"); 
    ListItem* next = head;
    while (next)
    {
        if (next->getIdentifier() == identifier)
            return next;
        else
            next = next->getNext();   
    }
	PLOG(PL_DEBUG, "Leaving SimpleList::findProxyByIdentifier\n"); 
    return NULL;
}  // end SimpleList::findProxyByIdentifier()
