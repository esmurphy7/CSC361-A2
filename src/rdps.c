/* TODO: Change byte arrays from char types to appropriate types (unsigned int?)
 * 			- Replace string functions such as strcpy(), strlen(), (char*)malloc() with memcpy()
 * 		Trim the very last packet of the file of excess null memory
 * 		Calculate and include the window size in packet header upon creation
 * 		Init first_seqno to random number
 */
/*
 * QUESTIONS:
 * 			- After how many seconds of no and/or garbage response do we terminate and/or resend packets?
 * 			- Does a RST packet mean terminate?
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>

#include "rdps.h"

// ============== Prototypes =============
void setup_connection(char*,int,char*,int);
void packetize(char*, int);
struct packet* create_packet(char*, int, int, int);
void store_dataPacket(struct packet*);
void initiate_transfer();
void send_packet(struct packet*);
void wait_response();
void terminate(int);
char* type_itos(int);
// =============== Globals ===============
/* List of data packets to be sent */
struct packet** data_packets;
/* List of timers for each packet */
struct packet_timer** timer_list;
pthread_mutex_t timers_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Initial SYN's seqno */
int first_seqno;
/* Track the current sequence number during packetizing */
int current_seqno;;
int window_size;
/* Connection globals */
int socketfd;
struct sockaddr_in adr_sender;
struct sockaddr_in adr_receiver;
// =======================================

int main(int argc, char* argv[])
{
	char* sender_ip;
	int sender_port;
	char* receiver_ip;
	int receiver_port;

	char* filename;
	FILE* file;
	int file_length;	// Length of file in bytes
	char* data;			// Each element is a byte from the file

	if(argc != 6)
	{
		printf("Usage: $./rdps sender_ip sender_port receiver_ip receiver_port sender_file_name\n");
		exit(0);
	}

	sender_ip = argv[1];
	sender_port = atoi(argv[2]);
	receiver_ip = argv[3];
	receiver_port = atoi(argv[4]);
	filename = argv[5];

	// Open file
	if((file = fopen(filename, "r")) == NULL)
	{
		perror("sender: open\n");
		exit(-1);
	}
	// determine file size
	fseek(file, 0, SEEK_END);
	file_length = ftell(file);
	rewind(file);
	printf("file length: %d\n",file_length);
	data = (char*)malloc(sizeof(data)*file_length);
	// Read file byte by byte into data buffer
	if(fread(data, 1, file_length, file) != file_length)
	{
		perror("sender: fread");
		free(data);
		exit(-1);
	}
	fclose(file);

	// Calculate number of data packets to be stored
	float n = ((float)file_length) / ((float)MAX_PAYLOAD_SIZE);
	float num_packets = ceil(n);
	printf("Number of data packets: %f\n", num_packets);
	// Init globals
	data_packets = (struct packet**)calloc(num_packets, sizeof(struct packet*));
	window_size = file_length/10;

	setup_connection(sender_ip, sender_port, receiver_ip, receiver_port);
	packetize(data, file_length);
	initiate_transfer();
	printf("window size: %d\n",window_size);
	return 0;
}

/*
 * Create socket and sender/receiver addresses and bind to socket
 */
void setup_connection(char* sender_ip, int sender_port, char* receiver_ip, int receiver_port)
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
	adr_sender.sin_port = htons(sender_port);
	adr_sender.sin_addr.s_addr = atoi(sender_ip);
	// Create receiver address
	memset(&adr_receiver,0,sizeof adr_receiver);
	adr_receiver.sin_family = AF_INET;
	adr_receiver.sin_port = htons(receiver_port);
	adr_receiver.sin_addr.s_addr = atoi(receiver_ip);
	// Error check addresses
	if ( adr_sender.sin_addr.s_addr == INADDR_NONE ||
		 adr_receiver.sin_addr.s_addr == INADDR_NONE )
	{
		perror("sender: created bad address\n)");
		exit(-1);
	}
	// Bind to socket
	if(bind(socketfd, (struct sockaddr*)&adr_sender,  sizeof(adr_sender)) < 0)
	{
		perror("sender: bind()\n");
		exit(-1);
	}
}

/*
 * Convert the file's data into a number of packets
 */
void packetize(char* data, int file_length)
{
	int current_byte;	// Index of "data"
	int bytes_read;		// Bytes read from "data"
	char* pckt_data;	// Bytes meant for a packet

	first_seqno = 0;
	current_seqno = first_seqno + 1;
	// Move bytes from data buffer into pckt_data and create packets
	current_byte = 0;
	bytes_read = 0;
	pckt_data = (char*)malloc(sizeof(pckt_data)*MAX_PAYLOAD_SIZE);
	printf("Reading file...\n");
	while(current_byte < file_length)
	{
		pckt_data[bytes_read] = data[current_byte];
		if(bytes_read == MAX_PAYLOAD_SIZE)
		{
			//printf("Current byte: %d, Bytes read: %d\n",current_byte, bytes_read);
			struct packet* pckt = create_packet(pckt_data, bytes_read, DAT, current_seqno);
			store_dataPacket(pckt);
			free(pckt_data);
			pckt_data = (char*)malloc(sizeof(pckt_data)*MAX_PAYLOAD_SIZE);
			bytes_read = 0;
		}
		bytes_read++;
		current_byte++;
	}
	// End of file, packet size is less than MAX_PAYLOAD_SIZE
	//		=>Shave off empty memory
	struct packet* pckt = create_packet(pckt_data, bytes_read, DAT, current_seqno);
	store_dataPacket(pckt);

	printf("sender: end of file\n");
	// Debug: print list of data packets
	int i = 0;
	while(data_packets[i] != NULL)
	{
		//if(i<15){printf("data packet %d seqno: %d, ackno: %d\n",i,data_packets[i]->header.seqno, data_packets[i]->header.ackno);}
		i++;
	}
	free(data);
}

/*
 * Create a single packet of type "pckt_type"
 */
struct packet* create_packet(char* data, int data_length, int pckt_type, int seqno)
{
	//printf("creating %s packet...\n", type_itos(pckt_type));
	struct packet_hdr header;
	struct packet* pckt;
	char* pckt_data;

	int ackno;
	switch(pckt_type)
	{
		case SYN:
			ackno = seqno + 1;
			break;

		case DAT:
			seqno = current_seqno;
			current_seqno += data_length;
			ackno = current_seqno;
			pckt_data = (char*)malloc(sizeof(pckt_data)*data_length);
			strcpy(pckt_data, data);
			break;

		case ACK:
			ackno = seqno + 1;
			break;

		case FIN:
			ackno = seqno + 1;
			break;

		case RST:
			ackno = seqno + 1;
			break;

		default:
			perror("sender: unknown packet type\n");
			exit(-1);
	}

	header.magic = "UVicCSc361";
	header.type = pckt_type;
	header.seqno = seqno;
	header.ackno = ackno;
	//header->winsize = winsize;
	pckt = (struct packet*)malloc(sizeof(*pckt));
	pckt->header = header;
	pckt->payload = pckt_data;
	pckt->payload_length = data_length;

	return pckt;
}

/*
 * Store the data packet in the global list of data packets
 */
void store_dataPacket(struct packet* pckt)
{
	// add packet to list of packets
	if(pckt->header.type != DAT)
	{
		perror("sender: attempted to store non-DAT packet\n");
		exit(-1);
	}
	int i=0;
	while(data_packets[i] != 0)
	{
		i++;
	}
	data_packets[i] = pckt;
	//printf("stored packet with seqno %d at data_packets[%d]\n",pckt->header.seqno,i);
}

/*
 * Begin the file transfer process
 */
void initiate_transfer()
{
	// Init list of timers
	timer_list = (struct packet_timer**)calloc(MAX_TIMERS, sizeof(struct packet_timer*));
	// Start thread to poll list of timers and check if any have timed out
	pthread_t pthread;
	//pthread_create(&pthread, NULL, , );

	// send SYN
	struct packet* SYN_pckt = create_packet(NULL, 0, SYN, first_seqno);
	send_packet(SYN_pckt);
	// wait on SYN's ACK
	wait_response();
	// send initial data packets in accordance with window size

	while(1)
	{
		// wait_response
		// send new data packets if allowed
		// send FIN if all data packets successfully ACK'd
		// if there are any timedout timers
			// resend corresponding data packets
		// if all data packets ACK'd
			// send FIN
			// wait_response

	}
}

/*
 * Send packet to receiver.
 */
void send_packet(struct packet* pckt)
{
	printf("sending %s packet...\n", type_itos(pckt->header.type));
	if(sendto(socketfd,
				pckt,
				sizeof(pckt),
				0,
				(struct sockaddr*)&adr_receiver,
				sizeof(struct sockaddr_in)) < 0)
	{
		perror("sender: sendto()\n");
		exit(-1);
	}
	// Add/update corresponding timer to timer_list
	//printf("locking...\n");
	pthread_mutex_lock(&timers_mutex);
	struct packet_timer* timer;
	// If timer already exists, update it
	if((timer = find_timer(timer_list, pckt)) != NULL)
	{
		printf("timer with seqno %d already exists\n",pckt->header.seqno);
		timer->time_sent = clock();
	}
	// Else timer doesn't exist yet, create one and add it to timer list
	else
	{
		struct packet_timer* new_timer = create_timer(pckt->header.seqno, clock());
		add_timer(timer_list, new_timer);
	}
	pthread_mutex_unlock(&timers_mutex);
	printf("sender: sent %s packet\n",type_itos(pckt->header.type));
}

/*
 * Wait for ACK or RST response from receiver
 */
void wait_response()
{
	char buffer[sizeof(struct packet*)];
	socklen_t receiver_len = sizeof(adr_receiver);

	if(recvfrom(socketfd,
					&buffer,
					sizeof(struct packet*),
					0,
					(struct sockaddr*)&adr_receiver,
					&receiver_len) < 0)
	{
		perror("sender: recvfrom()\n");
		exit(-1);
	}
	// Init received packet
	struct packet* received_packet = (struct packet*)malloc(sizeof(struct packet*));
	memcpy(received_packet, &buffer, sizeof(struct packet*));
	// ACK received

	// RST received

}

/*
 * Terminate the transfer process with failure or success
 * param code: <0 failure (RST), >=0 success (FIN)
 */
void terminate(int code)
{
	if(code < 0)
	{
		perror("sender: terminating with failure (RST)\n");
		exit(-1);
	}
	printf("sender: terminating with success (FIN)\n");
	exit(0);
}

/*
 * Convert packet type from int to string (for printing purposes)
 */
char* type_itos(int pckt_type)
{
	char* type = (char*)malloc(sizeof(type)*3);
	switch(pckt_type)
	{
		case SYN:
			type = "SYN";
			break;
		case DAT:
			type = "DAT";
			break;
		case ACK:
			type = "ACK";
			break;
		case FIN:
			type = "FIN";
			break;
		case RST:
			type = "RST";
			break;
		default:
			perror("sender: attempted to convert unknown packet type\n");
			exit(-1);
	}
	return type;
}








