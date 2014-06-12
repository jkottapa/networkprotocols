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
#include <sys/time.h>
#include <csignal>

#include "srSender.h"

using namespace std;

//*********************************** \\

SRSendr::SRSendr( int ms, char* fn) : millisecs(ms), filename(fn) {
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
		SRRecv_addr = *temp;
		
		expectedSeqNum = 0;

		cout << "Sender is running..." << endl << endl;
	}
	else {
		cerr << "Could not read the channelInfo file" << endl;
	}
		
}// SRSendr constructor

SRSendr::~SRSendr() {
	close( SRSendrSock );
	freeaddrinfo( channelAddr );

	for(int i = 0; i < data.size(); i++){
		delete data[i];
	}
}	// destructor

/*Purpose: To send packets upon retriving the content from given file to the
			receiver via the channel emulator using Selective Repeat protocol
*/
void SRSendr::sendTo() {	
	// Open a socket for communication
	SRSendrSock = socket( AF_INET, SOCK_DGRAM, 0);
	
	ifstream transferFile( filename );
	
	if( !transferFile ) {
		cerr << "Could not read the transfer file" << endl;
		return;
	}
	
	int curPktNum = 0;
	nearestSeqNumIndex = 0;
	firstPkt = true;
	nextTimeoutVal = millisecs;

	while( true ) {

		// Take 500 bytes from the file to read and send to receiver
		char * fileContentBuffer = new char [PAYLOAD_SIZE];
		transferFile.read( fileContentBuffer, PAYLOAD_SIZE );
		data.push_back( fileContentBuffer );
		
		// Loop will terminate upon retriving the eof packet
		if( transferFile.gcount() == 0 ) {
			break;
		}// exit;

		while( window.size() == N ) {
			cout << "Window is full, wait for acks" << endl;			
			
			int resendIndex = listenForAck();	
			setTimeOut( 0 ); // reset timer before resending
			
			if( TimedOut ){				
				resendPkt( resendIndex );				
			}//if

			if( window.empty() ){
				setTimeOut( millisecs );
			}
		}// while

		// The window has slots available to we can send more packets	
		Packet *pkt = new Packet(DATA, curPktNum, fileContentBuffer, transferFile.gcount());
		cout << "PKT SEND DAT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;				
		
		window.push_back( make_pair(false, pkt) );
		curPktNum++;
		buf = (void*) pkt;

		// Send to the receiver
		sendto(SRSendrSock, buf, ntohl(pkt->pktLen), 0, (sockaddr*)&SRRecv_addr, sizeof(SRRecv_addr));

		gettimeofday(&tv, NULL);		
		pktTime.push_back((tv.tv_sec*SECS_2_MICROSECS) + tv.tv_usec);

		if( firstPkt ){
			setTimeOut( millisecs );
			firstPkt = false;
		}					
	}

	// Wait for the remaining packets to be acknowledged
	while( !window.empty() ) {
		cout << "Packets waiting to be acked" << endl;
		
		int resendIndex = listenForAck();		
		setTimeOut( 0 ); // reset timer before resending
			
		if( TimedOut ){				
			resendPkt( resendIndex );			
		}//if
	}// while*/

	setTimeOut( 0 );

	// Once all packets are acknowledged send the eot packet
	Packet EOTPkt;
	EOTPkt.createEOTPkt(curPktNum);
	cout << "PKT SEND EOT " << ntohl(EOTPkt.seqNum) << " " << ntohl(EOTPkt.pktLen) << endl;
	buf = &EOTPkt;
	sendto(SRSendrSock, buf, ACK_EOT_SIZE, 0, (sockaddr*)&SRRecv_addr, sizeof(SRRecv_addr));

	// Make sure to check for delayed packets
	// Wait for the EOT packet
	unsigned int namelen = sizeof( SRRecv_addr );
	
	while( true ) {
		cout << "recvfrom() is blocked in Sender..." << endl;
		int response = recvfrom( SRSendrSock, buf, ACK_EOT_SIZE, 0, (sockaddr*) &SRRecv_addr, &namelen);
		cout << "recvfrom() is unblocked Sender" << endl;
		
		Packet *pkt = (Packet*) buf;
		if( pkt->pktType != EOT ) {
			cout << "PKT RECV ACK " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;	
			cerr << "Ignoring unexpected packet type, expecting: EOT -- received: ACK" << endl;
			continue;
		}		
		cout << "PKT RECV EOT " << ntohl(pkt->seqNum) << " " << ntohl(pkt->pktLen) << endl;
		break;	
	}// while
}

void SRSendr::setTimeOut(int ms ) {	
	timeval to;
	to.tv_sec = 0;
	to.tv_usec = ms * MS_2_MICROSECS;

	int i  = setsockopt( SRSendrSock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof( to ));
}

int SRSendr::listenForAck() {
	char listen_buf[BUFLEN];
	int posn = 0;
	int resendIndex = 0;
	unsigned int namelen = sizeof( SRRecv_addr );

	while( true ) {		
		cout << "recvfrom() is blocked in Sender..." << endl;
		int response = recvfrom( SRSendrSock, (void*)listen_buf, BUFLEN, 0, (sockaddr*) &SRRecv_addr, &namelen);
		cout << "recvfrom() is unblocked Sender" << endl;
		
		if( response == -1 ) {			
			TimedOut = true;
			cerr << "********TIMED OUT*********" << endl;					

			return nearestSeqNumIndex;
		}//exit

		if( listen_buf == NULL ) {
			cerr << "Invalid acknowledgement packet - NULL buffer" << endl;
			continue;
		}
		Packet *pkt = (Packet*) listen_buf;
		int recvSeqNum = ntohl(pkt->seqNum);
		
		if( pkt->pktType != ACK ){
			cerr << "Invalid acknowledgement packet - wrong pktType" << endl;
			continue;
		}
		
		cout << "PKT RECV ACK " << recvSeqNum << " " << ntohl(pkt->pktLen) << endl;
				
		int ackIndex = findSeqInWin( recvSeqNum );
		
		if( ackIndex == -1 ) {
			cerr << "Invalid acknowledgement packet, window overflow" << endl;
			continue;
		}
		window[ackIndex].first = true;
		
		if( recvSeqNum == expectedSeqNum ) {
		
			for(int i = 0; i < window.size(); i++) {
				Packet* winPkt = window[i].second;
				bool isAcked = window[i].first;
				if(ntohl(winPkt->seqNum) == expectedSeqNum &&
					isAcked) {									
					i = -1;
					expectedSeqNum = (ntohl(winPkt->seqNum) + 1) % SEQMODULO;
					window.erase(window.begin());	
					delete winPkt;

					//Resetting the next timeout val
					pktTime.erase(pktTime.begin());
		
					gettimeofday(&tv, NULL);
					unsigned long int curTime = tv.tv_sec*SECS_2_MICROSECS + tv.tv_usec;						
					int calTime = ((curTime - pktTime[0])/MS_2_MICROSECS);
					nextTimeoutVal = (calTime >= millisecs)? 1 : (millisecs - calTime );					
					
					setTimeOut( nextTimeoutVal );
				}//if
			}//for
			// have to reset index because its has been moved
			nearestSeqNumIndex = 0;
		}//if
		
		if( !window.empty()){
						nearestSeqNumIndex = MODULO(nearestSeqNumIndex, window.size());
						// find the nearest seqNum where the packed in unacked
						while( window[nearestSeqNumIndex].first ){
							nearestSeqNumIndex = MODULO(nearestSeqNumIndex+1, window.size());
						}
					}
		else {
			nearestSeqNumIndex = 0;
			break;
		}
	}// while*/
	
}

int SRSendr::findSeqInWin( int seqNum ){
	for(int i = 0; i < window.size(); i++){
		if(ntohl(window[i].second->seqNum) == seqNum){
			return i;
		}
	}
	return -1;
}

void SRSendr::resendPkt( int resendIndex ){
	cout << "Reseding Packet" << endl;
	Packet* resend = window[ resendIndex ].second;				
	cout << "PKT SEND DAT " << ntohl(resend->seqNum) << " " << ntohl(resend->pktLen) << endl << endl;					
	buf = (void*) resend;
	// Resending the packet
	sendto(SRSendrSock, buf, ntohl(resend->pktLen), 0, (sockaddr*)&SRRecv_addr, sizeof(SRRecv_addr));

	gettimeofday(&tv, NULL);
	pktTime.push_back( (tv.tv_sec*SECS_2_MICROSECS) + tv.tv_usec);

	TimedOut = false;
	pktTime.erase(pktTime.begin());

	// resetting the next nearest seqnum		
	unsigned long int curTime = tv.tv_sec*SECS_2_MICROSECS + tv.tv_usec;						
	int calTime = ((curTime - pktTime[0])/MS_2_MICROSECS);
	nextTimeoutVal = (calTime >= millisecs)? 1 : (millisecs - calTime );
	nearestSeqNumIndex = MODULO(nearestSeqNumIndex+1, window.size());

	// find the nearest seqNum where the packed in unacked
	while( window[nearestSeqNumIndex].first ){
		nearestSeqNumIndex = MODULO(nearestSeqNumIndex+1, window.size());
	}

	setTimeOut( nextTimeoutVal );
}

int main(int argc, char *argv[]) {
	if( argc == 3 ) {
		SRSendr *sender = new SRSendr( atoi(argv[1]), argv[2] );
		sender->sendTo();
		delete sender;
		
	}//if
	else {
		cerr << "INVALID COMMANDLINE ARG" << endl;
	}//else
}
