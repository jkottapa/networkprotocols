#ifndef _SR_RECVR_
#define _SR_RECVR_

#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <list>
#include <vector>

#include "packet.h"

class SRRecvr {
	private:	// variables
		std::ofstream file;
		char* filename;
		int SRRecvSock, portNum;
		char SRRecvHostName[HOSTNAME];

		sockaddr_in SRRecv_addr, SRSendr_addr;		
		uint32_t expectedSeqNum;

		/*Sliding window */
		std::vector<Packet *> windowBuffer;	

	private:	// routines
		bool inWinBuffer(int seqNum);
		bool isFreshPkt( int seqNum);
		bool validPkt( int seqNum );
		void checkBufferWin(int recvBytes);
	public:
		SRRecvr( char* argv1 );
		~SRRecvr();
		void recvFrom();		
};

#endif //_SR_RECVR_
