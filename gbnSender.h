#ifndef _GBNSENDR_
#define _GBNSENDR_

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <list>
#include <vector>
#include <string>

#include "packet.h"

class GbnSendr {
	private:	// variables		
		int millisecs;
		char* filename;		
		int gbnSendrSock;
		unsigned int portNum;
		char gbnRecvHostName[256];

		// Socket datastructures
		sockaddr_in gbnRecv_addr;
		addrinfo hints, *channelAddr;	

		/*Sliding window */
		std::list<Packet *> window;	
		void * buf;
		std::vector<char *> data;

	private:	// routines
		void setTimeOut( int ms );			
		bool listenForAck();
		bool searchForPkt(int seqNum, int &posn );
		void resendPkts( );

	public:
		GbnSendr( int microsecs, char* argv1 );
		~GbnSendr();
		void sendTo();		
};

#endif //_GBNSENDR_
