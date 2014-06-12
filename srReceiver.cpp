#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

#include "srReceiver.h"

using namespace std;

/* Constructor for SRReceiver
   Purpose: To initialize and create a Socket 

*/
SRRecvr::SRRecvr( char* argv1 ) : filename( argv1 ) {
	
	gethostname( SRRecvHostName, sizeof( SRRecvHostName ) );
	
	/* initializing the socket to retrive the port number */
	unsigned int addrlen = sizeof( SRRecv_addr );
	SRRecvSock = socket( AF_INET, SOCK_DGRAM, 0 );
	memset( &SRRecv_addr, '0', sizeof( SRRecv_addr ) );

	SRRecv_addr.sin_family = AF_INET;
	SRRecv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	SRRecv_addr.sin_port = htons(0);

	// Binding a name to the socket
	bind( SRRecvSock, (sockaddr*) &SRRecv_addr, addrlen);
	// Getting the port number
	getsockname( SRRecvSock, (sockaddr*) &SRRecv_addr, &addrlen );

	// Setting port number
	portNum = ntohs(SRRecv_addr.sin_port);

	// Converting integer port number to string to write it into a file
	stringstream s;
	string portStr;
	s << portNum;	
	s >> portStr;

	string socketDetails = SRRecvHostName;
	socketDetails += " ";
	socketDetails += portStr;

	// Opening a recvInfo file and writing the socket details into it
	ofstream recvInfoFile;
	recvInfoFile.open( "recvInfo" );

	recvInfoFile << socketDetails << endl;

	recvInfoFile.close();

	// Expected packet to receive
	expectedSeqNum = 0;

	cout << "Receiver is running..." << endl << endl;
}// SRReceiver::SRReceiver

SRRecvr::~SRRecvr() {
	file.close();
	close( SRRecvSock );	// closing the socket
}

void SRRecvr::recvFrom() {
	char buf[BUFLEN];
	void * retBuf;	
	file.open( filename );	
	
	unsigned int namelen = sizeof( SRSendr_addr );

	while( true ) {		
		cout << endl <<"recvfrom() is blocked in Receiver..." << endl;
		int recvBytes = recvfrom( SRRecvSock, (void*)buf, BUFLEN, 0, (sockaddr*) &SRSendr_addr, &namelen);
		cout << "recvfrom() is unblocked in Receiver with " << recvBytes << " bytes of data" << endl << endl;

		Packet *pkt = (Packet*)buf;

		if( pkt->pktType == EOT && windowBuffer.empty()) {
			cout << "PKT RECV EOT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;

			// Send back EOT packet			
			cout << "PKT SEND EOT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;
			int j = sendto(SRRecvSock, buf, ACK_EOT_SIZE, 0, (sockaddr*)&SRSendr_addr, sizeof(SRSendr_addr));		
			break;
		}//exit
		else if( pkt->pktType == EOT && !windowBuffer.empty()){
			cerr << "Outstanding packets remaining in the buffer! But received EOT" << endl;
			continue;
		}
		
		
		int seqNum = ntohl(pkt->seqNum);
		
		cout << "PKT RECV DAT " << seqNum << " " << ntohl(pkt->pktLen) << endl;

		if( expectedSeqNum == seqNum )	{
			
			// Write to the file
			file.write( pkt->payload, recvBytes - 12 );

			Packet ackPkt;
			ackPkt.createAckPkt( pkt->seqNum );
			retBuf = &ackPkt;
			cout << "PKT SEND ACK " << seqNum << " " << ntohl(ackPkt.pktLen) << endl;
			int j = sendto(SRRecvSock, retBuf, ACK_EOT_SIZE, 0, (sockaddr*)&SRSendr_addr, sizeof(SRSendr_addr));		

			// Reset expected Sequence number to the next one
			expectedSeqNum = (seqNum + 1) % SEQMODULO;			
			
		}//i
		else if( validPkt( seqNum) ) {	// We need to buffer these			
			if( !inWinBuffer(seqNum) && isFreshPkt(seqNum) ) {
				// This packet is fresh, can add to our window
				Packet * bufPkt = new Packet(DATA, seqNum, pkt->payload, recvBytes-12);				
				windowBuffer.push_back( bufPkt );			
			}				

			Packet ackPkt;
			ackPkt.createAckPkt( pkt->seqNum );
			retBuf = &ackPkt;
			cout << "PKT SEND ACK " << ntohl(ackPkt.seqNum) << " " << ntohl(ackPkt.pktLen) << endl;
			int j = sendto(SRRecvSock, retBuf, ACK_EOT_SIZE, 0, (sockaddr*)&SRSendr_addr, sizeof(SRSendr_addr));		
				
		}// else
		else {
			cerr << "Out of range packet received! Ignoring" << endl;
		}
		
		// Check the window to see if any seqNum are expected				
		checkBufferWin( recvBytes);
		
	}// while

	if( !windowBuffer.empty() ) {
		cerr << "Something went wrong, buffer should be 0, but its: " << windowBuffer.size() << endl;
	}
}

void SRRecvr::checkBufferWin(int recvBytes) {
	for(int i = 0; i < windowBuffer.size(); i++) {
		Packet *bufPkt = windowBuffer[i];		
		if( expectedSeqNum == ntohl( bufPkt->seqNum ) ) {				
			// Write to the file
			file.write( bufPkt->payload, recvBytes - 12 );
			
			// Reset expected Sequence number to the next one
			expectedSeqNum = (ntohl(bufPkt->seqNum) + 1) % SEQMODULO;

			// Remove the pkt from our sliding window				
			windowBuffer.erase( windowBuffer.begin()+i );
			i = -1;
			delete bufPkt;
		}				
	}
}

bool SRRecvr::isFreshPkt( int seqNum ) {	
	if( expectedSeqNum + N  > 31 ) {
		if( expectedSeqNum < seqNum 
			|| seqNum < MODULO(expectedSeqNum + N, SEQMODULO)) {
			return true; 
		}
	}
	else if( expectedSeqNum < seqNum && seqNum < (expectedSeqNum + N)) {
			return true;
	}
	
	return false;

}

bool SRRecvr::validPkt( int seqNum ) {
	if( expectedSeqNum + N > 31 ) {
		if( MODULO(expectedSeqNum + N, SEQMODULO) >= seqNum ||
			(expectedSeqNum - N) <= seqNum ){
			return true;
		}
	}
	else if( (expectedSeqNum + N) > seqNum 
			|| seqNum >= MODULO(expectedSeqNum - N, SEQMODULO) ) {
		return true;
	}
	return false;
}

bool SRRecvr::inWinBuffer(int seqNum ){	
	for(int i = 0; i < windowBuffer.size(); i++ ){		
		if(seqNum == ntohl(windowBuffer[i]->seqNum)){
			return true;
		}
	}
	return false;
}

int main(int argc, char* argv[]) {	
	if( argc == 2 ) {
		SRRecvr s( argv[1] );
		s.recvFrom();
	}//if
	else {
		cerr << "INVALID COMMANDLINE ARG" << endl;
	}//else
}//main
