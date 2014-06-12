#ifndef _PACKET_
#define _PACKET_

#define SEQMODULO 32
#define BUFLEN 512
#define PAYLOAD_SIZE 500
#define WINSIZE 10
#define HOSTNAME 256
#define ACK_EOT_SIZE 12

// Timestamp conversion constants
#define SECS_2_MICROSECS 1000000
#define MS_2_MICROSECS 1000

// Packet types
#define DATA 0
#define ACK htonl(1)
#define EOT htonl(2)

// Window details
#define N 10
#define MODULO(x, n) (x%n + n)%n

#include <iostream>
#include <string>
#include <arpa/inet.h>

class Packet {
	public:
		uint32_t pktType;
		uint32_t seqNum;
		uint32_t pktLen;
		char payload[PAYLOAD_SIZE];
	public:
		Packet() { /*do nothing */ } 
		Packet(uint32_t type,
			   uint32_t seqNum,
			   char * pl,
			   int payload_len) : pktType( type ),
			   					  seqNum( htonl(seqNum % SEQMODULO) ) {
					
			pktLen = htonl(3 * sizeof(uint32_t) + payload_len);
			for(int i = 0; i < PAYLOAD_SIZE; i++) {
				payload[i] = pl[i];
			}
			
		}
		void createAckPkt( uint32_t seqN ) {
			pktType = ACK;
			seqNum = seqN;
			pktLen = htonl( sizeof(uint32_t) * 3);
		}

		void createEOTPkt( uint32_t seqN ) {
			pktType = EOT;
			seqNum = htonl(seqN % SEQMODULO);
			pktLen = htonl( sizeof(uint32_t) * 3);
		}

};

#endif //_PACKET_
