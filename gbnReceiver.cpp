#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

#include "gbnReceiver.h"

using namespace std;

/* Constructor for gbnReceiver
   Purpose: To initialize and create a Socket 

*/
GbnReceiver::GbnReceiver( char* argv1 ) : filename( argv1 ) {
	
	gethostname( gbnRecvHostName, sizeof( gbnRecvHostName ) );
	
	/* initializing the socket to retrive the port number */
	unsigned int addrlen = sizeof( gbnRecv_addr );
	gbnRecvSock = socket( AF_INET, SOCK_DGRAM, 0 );
	memset( &gbnRecv_addr, '0', sizeof( gbnRecv_addr ) );

	gbnRecv_addr.sin_family = AF_INET;
	gbnRecv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	gbnRecv_addr.sin_port = htons(0);

	// Binding a name to the socket
	bind( gbnRecvSock, (sockaddr*) &gbnRecv_addr, addrlen);
	// Getting the port number
	getsockname( gbnRecvSock, (sockaddr*) &gbnRecv_addr, &addrlen );

	// Setting port number
	portNum = ntohs(gbnRecv_addr.sin_port);

	// Converting integer port number to string to write it into a file
	stringstream s;
	string portStr;
	s << portNum;	
	s >> portStr;

	string socketDetails = gbnRecvHostName;
	socketDetails += " ";
	socketDetails += portStr;

	// Opening a recvInfo file and writing the sockaddret details into it
	ofstream recvInfoFile;
	recvInfoFile.open( "recvInfo" );

	recvInfoFile << socketDetails << endl;

	recvInfoFile.close();

	// Expected packet to receive
	expectedSeqNum = 0;

	cout << "Receiver is running..." << endl << endl;
}// GbnReceiver::GbnReceiver

GbnReceiver::~GbnReceiver() {
	close( gbnRecvSock );	// closing the socket
}

void GbnReceiver::recvFrom() {
	char buf[BUFLEN];
	void * retBuf;	
	file.open( filename );	
	
	unsigned int namelen = sizeof( gbnSendr_addr );

	Packet ackPkt;
	ackPkt.createAckPkt( htonl(31) );	
	Packet *previousAck = &ackPkt;

	while( true ) {		
		cout << endl <<"recvfrom() is blocked in Receiver..." << endl;
		int recvBytes = recvfrom( gbnRecvSock, (void*)buf, BUFLEN, 0, (sockaddr*) &gbnSendr_addr, &namelen);
		cout << "recvfrom() is unblocked in Receiver with " << recvBytes << " bytes of data" << endl << endl;

		Packet *pkt = (Packet*)buf;

		if( pkt->pktType == EOT) {
			cout << "PKT RECV EOT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;

			// Send back EOT packet			
			cout << "PKT SEND EOT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;
			int j = sendto(gbnRecvSock, buf, ACK_EOT_SIZE, 0, (sockaddr*)&gbnSendr_addr, sizeof(gbnSendr_addr));		
			break;
		}//exit
		
		
		int seqNum = ntohl(pkt->seqNum);
		
		cout << "PKT RECV DAT " << seqNum << " " << ntohl(pkt->pktLen) << endl;

		if( expectedSeqNum == seqNum )	{
			
			// Write to the file
			file.write( pkt->payload, recvBytes - 12 );

			Packet ackPkt;
			ackPkt.createAckPkt( pkt->seqNum );
			retBuf = &ackPkt;
			cout << "PKT SEND ACK " << seqNum << " " << ntohl(ackPkt.pktLen) << endl;
			int j = sendto(gbnRecvSock, retBuf, ACK_EOT_SIZE, 0, (sockaddr*)&gbnSendr_addr, sizeof(gbnSendr_addr));		
			previousAck = &ackPkt;
			// Reset expected Sequence number to the next one
			expectedSeqNum = (seqNum + 1) % SEQMODULO;			
			
		}//if
		else {
			cerr << "Unexpected packet received, expecting: " << expectedSeqNum << " received: " << seqNum << endl;
			retBuf = (void*)previousAck;
			cout << "PKT SEND ACK " << ntohl(previousAck->seqNum) << " " << ntohl(previousAck->pktLen) << endl;
			int j = sendto(gbnRecvSock, retBuf, ACK_EOT_SIZE, 0, (sockaddr*)&gbnSendr_addr, sizeof(gbnSendr_addr));		
		}
		
	}// while
	file.close();
}

int main(int argc, char* argv[]) {	
	if( argc == 2 ) {
		GbnReceiver s( argv[1] );
		s.recvFrom();
	}//if
	else {
		cerr << "INVALID COMMANDLINE ARG" << endl;
	}//else
}//main
