#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>

#include "gbnSender.h"

using namespace std;

GbnSendr::GbnSendr( int ms, char* fn) : millisecs(ms), filename(fn) {
	string hostName, portStr;
	ifstream channelInfo( "channelInfo" );
	
	if( channelInfo ) {
		channelInfo >> hostName >> portStr;

		portNum = atoi(portStr.c_str());

		memset( &hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0;

		getaddrinfo( hostName.c_str(), portStr.c_str(), &hints, &channelAddr );

		sockaddr_in *temp = (sockaddr_in*) channelAddr->ai_addr;	
		gbnRecv_addr = *temp;

		cout << "Sender is running..." << endl << endl;
	}
	else {
		cerr << "Could not read the channelInfo file" << endl;
	}
		
}// GbnSendr constructor


GbnSendr::~GbnSendr() {
	close( gbnSendrSock );
	freeaddrinfo( channelAddr );

	for(int i = 0; i < data.size(); i++){
		delete data[i];
	}
}

void GbnSendr::sendTo() {	
	// Open a socket for communication
	gbnSendrSock = socket( AF_INET, SOCK_DGRAM, 0);
	
	ifstream transferFile( filename );
	
	if( !transferFile ) {
		cerr << "Could not read the transfer file" << endl;
		return;
	}

	int curPktNum = 0;
	bool TimedOut = false;
	while( true ) {

		// Take 500 bytes from the file to read and send to receiver
		char * fileContentBuffer = new char [PAYLOAD_SIZE];
		transferFile.read( fileContentBuffer, PAYLOAD_SIZE );
		data.push_back( fileContentBuffer );
		
		if(transferFile.gcount() == 0 ) {
			break;
		}//exit
		
		while( window.size() == N )	{ // Window is full, waiting for Acks
			cout << "Window is full, waiting for Ack" << endl;
			
			setTimeOut( millisecs );
			TimedOut = listenForAck();			
			setTimeOut( 0 );	// resetting the timeout

			if( TimedOut ){				
				resendPkts();	
			}//if
		}// while
		
		// The window has slots available to we can send more packets			
		Packet *pkt = new Packet(DATA, curPktNum, fileContentBuffer, transferFile.gcount());
		cout << "PKT SEND DAT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;				
		
		window.push_back( pkt );
		curPktNum++;
		buf = (void*) pkt;

		// Send to the receiver		
		sendto(gbnSendrSock, buf, ntohl(pkt->pktLen), 0, (sockaddr*)&gbnRecv_addr, sizeof(gbnRecv_addr));
	
	}//while
	
	while( !window.empty() ) {
		cout << "Packets waiting to be acked" << endl;
		
		setTimeOut( millisecs );
		TimedOut = listenForAck();			
		setTimeOut( 0 );	// resetting the timeout

		if( TimedOut ){				
			resendPkts();
		}//if
	}// while

	// Once all packets are acknowledged send the eot packet
	Packet EOTPkt;
	EOTPkt.createEOTPkt(curPktNum);
	cout << "PKT SEND EOT " << ntohl(EOTPkt.seqNum) << " " << ntohl(EOTPkt.pktLen) << endl;
	buf = &EOTPkt;
	sendto(gbnSendrSock, buf, ACK_EOT_SIZE, 0, (sockaddr*)&gbnRecv_addr, sizeof(gbnRecv_addr));

	// Wait for the EOT packet
	unsigned int namelen = sizeof( gbnRecv_addr );
	while(true) {
		cout << "recvfrom() is blocked in Sender..." << endl;
		int response = recvfrom( gbnSendrSock, buf, ACK_EOT_SIZE, 0, (sockaddr*) &gbnRecv_addr, &namelen);
		cout << "recvfrom() is unblocked Sender" << endl;
	
		Packet *pkt = (Packet*) buf;
		if( pkt->pktType != EOT ) {
			cout << "PKT RECV ACK " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;	
			cerr << "Ignoring unexpected packet type, expecting: EOT -- received: ACK" << endl;
			continue;
		}
		
		cout << "PKT RECV EOT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;	
		break;
	}
}

void GbnSendr::setTimeOut( int ms ) {
	timeval to;
	to.tv_sec = 0;
	to.tv_usec = ms * 1000;

	int i  = setsockopt( gbnSendrSock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof( to ));
	
}

bool GbnSendr::searchForPkt( int seqNum, int &posn ) {
	posn = 0;
	for(list<Packet*>::iterator i = window.begin(); i != window.end(); i++ ){
		if( seqNum == ntohl( (*i)->seqNum) ) {			
			return false;
		}
		posn++;
	}
	return true;
}

void GbnSendr::resendPkts( ) {
	cout << "Reseding Packets" << endl;
	for(list<Packet*>::iterator i = window.begin(); i != window.end(); i++){					
		cout << "PKT SEND DAT " << ntohl((*i)->seqNum) << " " << ntohl((*i)->pktLen) << endl;
		buf = (void*) *i;

		// Resending the packets
		sendto(gbnSendrSock, buf, ntohl((*i)->pktLen), 0, (sockaddr*)&gbnRecv_addr, sizeof(gbnRecv_addr));
	}//for
	cout << endl;
}

bool GbnSendr::listenForAck() {
	char buf[BUFLEN];
	int posn = 0;
	unsigned int namelen = sizeof( gbnRecv_addr );

	while( true ) {
		cout << "recvfrom() is blocked in Sender..." << endl;
		int response = recvfrom( gbnSendrSock, (void*)buf, BUFLEN, 0, (sockaddr*) &gbnRecv_addr, &namelen);
		cout << "recvfrom() is unblocked Sender" << endl;

		int expectedSeqNum = window.front()->seqNum;
		if( response == -1 ) {
			cerr << "********TIMED OUT*********" << endl;
			return true;
		}

		if( buf == NULL ) {
			cerr << "Invalid acknowledgement packet - NULL buffer" << endl;
			continue;
		}
		Packet *pkt = (Packet*) buf;

		if( pkt->pktType != ACK ){
			cerr << "Invalid acknowledgement packet - wrong pktType" << endl;
			continue;
		}
		
		cout << "PKT RECV ACK " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;
		if( searchForPkt( ntohl(pkt->seqNum), posn ) ) {
			cerr << "Invalid acknowledgement packet - Expecting seqNum: " << ntohl(expectedSeqNum) 
				<< " Received: " << ntohl(pkt->seqNum) << endl;
			continue;
		}
		
		// Cumulative acknowledgements can also be removed from our window
		for( ;posn > -1; ) {
			// clean up and removal of the acked packet
			Packet *ackedPkt = window.front();
			window.pop_front();
			delete ackedPkt;	
			posn--;
		}//for

		// No more packets
		if( window.empty() ){
			return false;		
		}
	}// while
}

int main(int argc, char* argv[]) {
	if( argc == 3 ) {
		GbnSendr s( atoi(argv[1]), argv[2] );
		s.sendTo();
		
	}//if
	else {
		cerr << "INVALID COMMANDLINE ARG" << endl;
	}//else

}// main
