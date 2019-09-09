#ifndef _TCPDATA
#define _TCPDATA

/*
 *  TcpData.h
 *
 *  Created by Ian Taylor on 28/12/2006.
 *
 * A simple class to package our data up in the Application interface for ns-2
 */


#include "ns-process.h"
#include "SimpleList.h"

#include "protoDebug.h"

class TcpData : public AppData, public ListItem {
private:
	int size_;
	char* bytes_; 
public:
	TcpData() : AppData(TCPAPP_STRING), size_(0), bytes_(NULL) {}
		
	virtual ~TcpData();

	void setData(const char* s, int size);

	char* getData() { return bytes_; }
	int getDataSize() { return size_; }
	
	virtual AppData* copy();
};

#endif
