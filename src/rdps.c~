/* TODO: Change byte arrays from char types to appropriate types (unsigned int?)
 * 			- Replace string functions such as strcpy(), strlen(), (char*)malloc() with memcpy()
 * 		Trim the very last packet of the file of excess null memory
 * 		Calculate and include the window size in packet header upon creation
 * 		Init first_seqno to random number
 * 		Change max payload size from 1024 to 1024-header_size
 */
/*
 * QUESTIONS:
 * 			- After how many seconds of no and/or garbage response do we terminate and/or resend packets?
 * 			- What exactly defines a packet with errors? How can my sender/receiver identify a packet as having errors?
 * 			- How should my sender/receiver handle packets with errors? Discard them? Attempt to fix them?
 * 			- Does a RST packet mean terminate?
 * 			- How to choose a window size and packet size?
 * 			- Why random seqno?
 * 			- Need to transfer different types of files? How does this affect the way I store packet payload (char*)?
 * 			- Anyway to connect to “WAN” interface (10.10.1.100) and “LAN” interfaces (192.168.1.100) remotely?
 */
#include <sys/file.h>
#include <math.h>
#include <pthread.h>

#include "packet.h"
#include "timer.h"

// ============== Prototypes =============
void setup_connection(char*,int,char*,int);
void packetize(char*, int);
void initiate_transfer();
void terminate(int);
// =============== Globals ===============
/* List of data packets to be sent */
struct packet** data_packets;
/* List of timers for each packet */
struct packet_timer** timer_list;
pthread_mutex_t timers_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Initial SYN's seqno */
int first_seqno;
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
	printf("window size: %d\n",window_size);

	setup_connection(sender_ip, sender_port, receiver_ip, receiver_port);
	packetize(data, file_length);
	initiate_transfer();
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
	adr_sender.sin_addr.s_addr = inet_addr(sender_ip);
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
	if(bind(socketfd, (struct sockaddr*)&adr_sender,  sizeof(adr_sender)) < 0)
	{
		perror("sender: bind()\n");
		exit(-1);
	}
	printf("sender: running on %s:%d\n",inet_ntoa(adr_sender.sin_addr), ntohs(adr_sender.sin_port));
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
			struct packet* pckt = create_packet(pckt_data, bytes_read, DAT, current_byte - bytes_read + 1);
			add_packet(pckt, data_packets);
			free(pckt_data);
			pckt_data = (char*)malloc(sizeof(pckt_data)*MAX_PAYLOAD_SIZE);
			bytes_read = 0;
		}
		bytes_read++;
		current_byte++;
	}
	// End of file, packet size is less than MAX_PAYLOAD_SIZE
	//		=>Shave off empty memory
	struct packet* pckt = create_packet(pckt_data, bytes_read, DAT, current_byte);
	add_packet(pckt, data_packets);

	printf("sender: end of file\n");
	// Debug: print list of data packets
	int i = 0;
	while(data_packets[i] != NULL)
	{
		if(i){printf("data packet %d seqno: %d, ackno: %d\n",i,data_packets[i]->header.seqno, data_packets[i]->header.ackno);}
		i++;
	}

}

/*
 * Begin the file transfer process
 */
void initiate_transfer()
{
	// Init list of timers
	timer_list = (struct packet_timer**)calloc(MAX_TIMERS, sizeof(struct packet_timer*));
	// Start thread to poll list of timers and check if any have timed out
	//pthread_t pthread;
	//pthread_create(&pthread, NULL, , );

	// send SYN
	struct packet* SYN_pckt = create_packet(NULL, 0, SYN, first_seqno);
	send_packet(SYN_pckt, socketfd, adr_receiver);
	pthread_mutex_lock(&timers_mutex);
	update_timer(SYN_pckt->header.seqno, timer_list);
	pthread_mutex_unlock(&timers_mutex);
	// wait on SYN's ACK
	while(1)
	{
		struct packet* rec_pckt = receive_packet(socketfd);
		if(rec_pckt && rec_pckt->header.type == ACK)
		{
			printf("sender: SYN acknowledged\n");
			break;
		}
		if(clock()/CLOCKS_PER_SEC >= SENDER_TIMEOUT_S)
		{
			terminate(-2);
		}
	}
	// Init window
	struct window window;
	window.size = WINDOW_SIZE;
	window.occupied = 0;
	window.base_seqno = data_packets[0]->header.seqno;
	// Send initial wave of data packets and fill the window
	// for each data packet
	int i;
	for(i=0; data_packets[i] != 0; i++)
	{
		// if window is not full
		if(window.occupied != window.size)
		{
			// send the data packet
			send_packet(data_packets[i], socketfd, adr_receiver);
			// update occupied space in window
			window.occupied += data_packets[i]->payload_length;
		}
		// else window is full
		else
		{
			break;
		}

	}

	while(1)
	{
		// wait for packet
		// send new data packets if allowed
		// if there are any timedout timers
			// resend corresponding data packets
		// if all data packets ACK'd
			// send FIN
			// wait_response
		if(clock()/CLOCKS_PER_SEC >= SENDER_TIMEOUT_S)
		{
			terminate(-2);
		}
	}
}


/*
 * Terminate the transfer process with failure or success
 * param code: <0 failure (RST), >=0 success (FIN)
 */
void terminate(int code)
{
	if(code == -1)
	{
		perror("sender: terminating with failure (RST)\n");
		exit(-1);
	}
	else if(code == -2)
	{
		perror("sender: terminating, transfer took too long\n");
		exit(-1);
	}
	printf("sender: terminating with success (FIN)\n");
	exit(0);
}









