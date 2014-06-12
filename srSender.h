#ifndef _SR_SENDR_
#define _SR_SENDR_

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <list>
#include <vector>
#include <utility>
#include <string>

#include "packet.h"

class SRSendr {
	private:	//variables		
		int millisecs;
		char* filename;		
		int SRSendrSock;
		unsigned int portNum;
		char SRRecvHostName[HOSTNAME];

		// Socket datastructures
		sockaddr_in SRRecv_addr;
		addrinfo hints, *channelAddr;	

		/*Sliding window */
		std::vector<std::pair<bool,Packet *> >window;	
		void * buf;
		std::vector<char *> data;
		int expectedSeqNum;

		// Used for timer
		timeval tv;
		std::vector<unsigned long int> pktTime;
		int nearestSeqNumIndex;
		bool firstPkt;
		bool TimedOut;
		unsigned long int nextTimeoutVal;
		unsigned long int curTime;
	private:	// routines
		void setTimeOut( int ms );			
		int listenForAck();
		int findSeqInWin(int seqNum );
		void resendPkt( int resendIndex);
	public:
		SRSendr( int microsecs, char* argv1 );
		~SRSendr();
		void sendTo();		
};

#endif //_SR_SENDR_
