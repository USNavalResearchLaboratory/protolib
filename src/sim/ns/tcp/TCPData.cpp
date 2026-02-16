/*
 *  TcpData.cpp
 *
 *  Created by Ian Taylor on 28/12/2006.
 *
 */  
 
#include "TCPData.h"

void TcpData::setData(const char* b, int size) {
		if (b == NULL) {
			bytes_ = NULL; 
			size_ = 0;
		} else {
			PLOG(PL_DEBUG, "TcpData: Setting creating data %i bytes\n", size);
			size_ = size;
			bytes_ = new char[size_]; 
			// assert(bytes_ != NULL);
			memcpy(bytes_, b, size_);
		}
	} 

TcpData::~TcpData() { 
	PLOG(PL_DEBUG, "TcpData Deleting Data Object\n");
//	if (bytes_ != NULL) // don't delete data, that is up to application to free
//		delete []bytes_; 
}
	
	
AppData* TcpData::copy() {
	TcpData *tcpData = new TcpData();
	tcpData->setData(getData(), getDataSize());
	return tcpData;
}

	 
