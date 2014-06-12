#ifndef _GBNRECV_
#define _GBNRECV_

#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>

#include "packet.h"

class GbnReceiver {
	private:
		std::ofstream file;
		char* filename;
		int gbnRecvSock, portNum;
		char gbnRecvHostName[HOSTNAME];

		sockaddr_in gbnRecv_addr, gbnSendr_addr;		
		uint32_t expectedSeqNum;
		
	public:
		GbnReceiver( char* argv1 );
		~GbnReceiver();
		void recvFrom();
};

#endif //_GBNRECV_
