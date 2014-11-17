#include "packet.h"
#include "timer.h"

// ============== Prototypes =============
void setup_connection(char*, int);
// =============== Globals ===============
int window_size;
/* Connection globals */
int socketfd;
struct sockaddr_in adr_sender;
struct sockaddr_in adr_receiver;
// =======================================

int main(int argc, char* argv[])
{
	char* receiver_ip;
	int receiver_port;
	char* filename;

	if(argc != 4)
	{
		printf("Usage: $./rdpr receiver_ip receiver_port filename\n");
		exit(0);
	}

	receiver_ip = argv[1];
	receiver_port = atoi(argv[2]);
	filename = argv[3];

	setup_connection(receiver_ip, receiver_port);

	char test_data[MAX_PAYLOAD_SIZE] = "this is a test packet";
	struct packet* test_pckt = create_packet(test_data, MAX_PAYLOAD_SIZE, DAT, 69);
	send_packet(test_pckt, socketfd, adr_sender);

	return 0;
}

/*
 * Create socket and sender/receiver addresses and bind to socket
 */
void setup_connection(char* receiver_ip, int receiver_port)
{
	//	 Create UDP socket
	if((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("sender: socket()\n");
		exit(-1);
	}
	// Create sender address
	memset(&adr_sender,0,sizeof adr_sender);
	adr_sender.sin_family = AF_INET;
	adr_sender.sin_port = htons(8080);
	//adr_sender.sin_addr.s_addr = inet_addr("192.168.1.100");
	//adr_sender.sin_addr.s_addr = inet_addr("142.104.74.69");
	adr_sender.sin_addr.s_addr = inet_addr("127.0.0.1");
	// Create receiver address
	memset(&adr_receiver,0,sizeof adr_receiver);
	adr_receiver.sin_family = AF_INET;
	adr_receiver.sin_port = htons(receiver_port);
	adr_receiver.sin_addr.s_addr = inet_addr(receiver_ip);
	// Error check addresses
	if ( adr_sender.sin_addr.s_addr == INADDR_NONE ||
		 adr_receiver.sin_addr.s_addr == INADDR_NONE )
	{
		perror("sender: created bad address\n)");
		exit(-1);
	}
	// Bind to socket
	if(bind(socketfd, (struct sockaddr*)&adr_receiver,  sizeof(adr_receiver)) < 0)
	{
		perror("sender: bind()\n");
		exit(-1);
	}
	printf("receiver: running on %s:%d\n",inet_ntoa(adr_receiver.sin_addr), ntohs(adr_receiver.sin_port));
}
