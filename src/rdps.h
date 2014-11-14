#define MAX_DATA 1024	// Max number of bytes allowed in a packet

struct packet_hdr
{
	char* _magic;
	char* _type;
	int seqno;
	int winsize;
};

struct packet
{
	struct packet_hdr;
	char payload[MAX_DATA];
	int payload_length;
};

