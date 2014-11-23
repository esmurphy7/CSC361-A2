#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Window size (1024*1024 = 1048576 bytes for 1mb file)
#define WINDOW_SIZE 1024*10
// Max number of bytes allowed in a packet
#define PACKET_SIZE 1500
#define MAX_PAYLOAD_SIZE 1024
#define MAX_PACKETS 2000
#define MAGIC_HDR "UVicCSc361"
// Number of fields in the packet
#define NUM_FIELDS 7

struct packet_hdr
{
	char magic[11];
	int type;
	int seqno;
	int ackno;
	int winsize;
};

struct packet
{
	struct packet_hdr header;
	char payload[MAX_PAYLOAD_SIZE];
	int payload_length;
};

enum packet_types
{
	DAT=0,
	ACK,
	SYN,
	FIN,
	RST
};

struct window
{
	int size;
	int occupied;
	int base_seqno;
};

struct packet* create_packet(char*, int, int, int);
void add_packet(struct packet*, struct packet**);
void send_packet(struct packet*, int, struct sockaddr_in);
struct packet* receive_packet();
char* construct_string(struct packet*);
struct packet* deconstruct_string(char*);
char* charAppend(char*, char);
char* type_itos(int);
void print_contents(struct packet*);
