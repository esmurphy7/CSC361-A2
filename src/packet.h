#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

// Max number of bytes allowed in a packet
#define MAX_PACKET_SIZE 2048
#define MAX_PAYLOAD_SIZE 1024
#define MAGIC_HDR "UVicCSc361"

#pragma pack(1)
struct packet_hdr
{
	char* magic;
	int type;
	int seqno;
	int ackno;
	int winsize;
};
#pragma pack(0)

#pragma pack(1)
struct packet
{
	struct packet_hdr header;
	char* payload;
	int payload_length;
};
#pragma pack(0)

enum packet_types
{
	DAT=0,
	ACK,
	SYN,
	FIN,
	RST
};

struct packet* create_packet(char*, int, int, int);
void add_packet(struct packet*, struct packet**);
void send_packet(struct packet*, int, struct sockaddr_in);
char* type_itos(int);
