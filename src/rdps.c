/*
 	 ROUTER: ssh csc361@192.168.1.1, ecs360
 */
#include <sys/file.h>
#include <math.h>
#include <pthread.h>

#include "packet.h"
#include "timer.h"

// ============== Prototypes =============
void setup_connection(char*,int,char*,int);
void packetize(char*, int, int);
void initiate_transfer(int);
void handle_packet(struct packet*);
void* poll_timers();
void fill_window(struct window*);
void update_windowBase(struct window*);
void resend_timedoutPackets();
bool packetACKd(struct packet*);
bool allPacketsAcknowledged();
void print_log(bool, bool, int, int, int);
char* get_adrString(struct in_addr, in_port_t);
void print_statistics();
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
/* Statistics globals */
int bytes_sent;
int unique_bytes_sent;
int unique_DAT_packets_sent;
int DAT_packets_sent;
int SYN_packets_sent;
int FIN_packets_sent;
int RST_packets_sent;
int ACK_packets_received;
int RST_packets_received;
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

	// Calculate payload size based on file length for files of size less than MAX_PAYLOAD_SIZE
	int payload_size = 0;
	int i;
	/*
	for(i=1; i<=MAX_PAYLOAD_SIZE; i++)
	{
		// found a factor
		if(file_length%i==0)
		{
			// if it's a greater factor
			if(i > payload_size)
				payload_size = i;
		}
	}
	*/
	payload_size = MAX_PAYLOAD_SIZE;
	printf("Payload size: %d\n",payload_size);
	// Calculate number of data packets to be stored
	float n = ((float)file_length) / ((float)payload_size);
	int num_packets = ceil(n);
	printf("Number of data packets: %d\n", num_packets);
	// Init globals
	data_packets = (struct packet**)calloc(num_packets+1, sizeof(struct packet*));
	// print contents of data_packets
	for(i=0; data_packets[i] == 0; i++)
	{
		//printf("data_packets[%d] is 0\n",i);
	}
	if(data_packets[num_packets] != 0){printf("data_packets[%d] is not 0\n",num_packets);}
	window_size = file_length/10;
	printf("window size: %d\n",window_size);

	setup_connection(sender_ip, sender_port, receiver_ip, receiver_port);
	packetize(data, file_length, payload_size);
	initiate_transfer(file_length+payload_size-1);
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
	// Set timeout value for the recvfrom
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000; // 1sec = 1,000,000 microseconds. 10th of a second
	if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
	{
	    perror("setsockopt():");
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
void packetize(char* data, int file_length, int payload_size)
{
	int current_byte;	// Index of "data"
	int bytes_read;		// Bytes read from "data"
	char* pckt_data;	// Bytes meant for a packet

	first_seqno = 0;
	// Move bytes from data buffer into pckt_data and create packets
	current_byte = 0;
	bytes_read = 0;
	pckt_data = (char*)malloc(sizeof(pckt_data)*payload_size);
	//printf("Reading file...\n");
	while(current_byte < file_length)
	{
		pckt_data[bytes_read] = data[current_byte];
		if(bytes_read == payload_size)
		{
			//printf("Current byte: %d, Bytes read: %d\n",current_byte, bytes_read);
			struct packet* pckt = create_packet(pckt_data, bytes_read, DAT, current_byte - bytes_read + 1);
			add_packet(pckt, data_packets);
			free(pckt_data);
			pckt_data = (char*)malloc(sizeof(pckt_data)*payload_size);
			bytes_read = 0;
		}
		//printf("bytes_read: %d\n",bytes_read);
		//printf("current_byte: %d\n",current_byte);
		bytes_read++;
		current_byte++;
	}
	// End of file, packet size is less than MAX_PAYLOAD_SIZE
	//		=>Shave off empty memory
	struct packet* pckt = create_packet(pckt_data, bytes_read, DAT, current_byte);
	add_packet(pckt, data_packets);

	//printf("sender: end of file\n");
	// Debug: print list of data packets
	int i = 0;
	while(data_packets[i] != 0)
	{
		//if(i){printf("data packet %d seqno: %d, ackno: %d\n",i,data_packets[i]->header.seqno, data_packets[i]->header.ackno);}
		//if(i){printf("Data_packets[%d]:\n",i);print_contents(data_packets[i]);}
		i++;
	}
}

/*
 * Begin the file transfer process
 */
void initiate_transfer(int last_ackno)
{
	// Init list of timers
	timer_list = (struct packet_timer**)calloc(MAX_TIMERS, sizeof(struct packet_timer*));
	// Start thread to poll list of timers and check if any have timed out
	pthread_t pthread;
	pthread_create(&pthread, NULL, poll_timers, NULL);

	// Create and add all DAT timers, don't start them until they're sent
	//printf("Creating and adding DAT timers...\n");
	pthread_mutex_lock(&timers_mutex);
	int j;
	for(j=0; data_packets[j] != 0; j++)
	{
		//if(j){printf("Attempting to create timer with data_packets[%d]...\n",j);}
		struct packet_timer* DAT_timer = create_timer(data_packets[j]->header.ackno, -1);
		add_timer(timer_list, DAT_timer);
	}
	//print_stoppedTimers(timer_list);
	pthread_mutex_unlock(&timers_mutex);

	// send SYN
	//printf("creating SYN packet...\n");
	struct packet* SYN_pckt = create_packet(NULL, 0, SYN, first_seqno);
	//printf("created SYN packet\n");
	// update statistics
	send_packet(SYN_pckt, socketfd, adr_receiver);
	// sent, unique, SYN, seqno, length)
	print_log(true, true, SYN, SYN_pckt->header.seqno, SYN_pckt->payload_length);
	bytes_sent += SYN_pckt->payload_length;
	unique_bytes_sent += SYN_pckt->payload_length;
	SYN_packets_sent++;
	// start SYN's timer
	pthread_mutex_lock(&timers_mutex);
	//printf("Creating SYN timer...\n");
	struct packet_timer* SYN_timer = create_timer(SYN_pckt->header.ackno, clock());
	add_timer(timer_list, SYN_timer);
	start_timer(SYN_pckt->header.ackno, timer_list);
	//printf("Created, added, and started SYN timer\n");
	pthread_mutex_unlock(&timers_mutex);
	// wait on SYN's ACK
	while(1)
	{
		struct sockaddr_in adr_src;
		struct packet* rec_pckt = receive_packet(socketfd, &adr_src);
		if(rec_pckt && rec_pckt->header.type == ACK)
		{
			print_log(false, true, ACK, rec_pckt->header.seqno, rec_pckt->payload_length);
			// update statistics
			ACK_packets_received++;
			printf("sender: SYN acknowledged\n");
			pthread_mutex_lock(&timers_mutex);
			stop_timer(timer_list, SYN_pckt->header.ackno);
			pthread_mutex_unlock(&timers_mutex);
			break;
		}
		// resend the SYN packet
		send_packet(SYN_pckt, socketfd, adr_receiver);
		print_log(true, false, SYN, SYN_pckt->header.seqno, SYN_pckt->payload_length);
		// update statistics
		bytes_sent += SYN_pckt->payload_length;
		SYN_packets_sent++;
		if(clock()/CLOCKS_PER_SEC >= SENDER_TIMEOUT_S)
		{
			terminate(-2);
		}
	}
	// Init window
	struct window* window = (struct window*)malloc(sizeof(*window));
	window->size = WINDOW_SIZE;
	window->occupied = 0;
	window->base_seqno = data_packets[0]->header.seqno;

	// Send initial wave of data packets to fill the window
	//printf("sender: initial wave of data...\n");
	fill_window(window);
	//printf("Window:\n size->%d\n base_seqno->%d\n occupied->%d\n",window->size,window->base_seqno,window->occupied);

	// Receive ACKs, send data to fill the window, and check for timeouts
	while(1)
	{
		/*
		pthread_mutex_lock(&timers_mutex);
		print_runningTimers(timer_list);
		pthread_mutex_unlock(&timers_mutex);
		*/
		// wait for packet
		struct sockaddr_in adr_src;
		struct packet* rec_pckt;
		if((rec_pckt = receive_packet(socketfd, &adr_src)) != NULL)
		{
			print_log(false, true, ACK, rec_pckt->header.seqno, rec_pckt->payload_length);
			// handle received ACKs
			handle_packet(rec_pckt);
		}
		// Update the window's base_seqno as ACKs are received
		//printf("Updating window...\n");
		update_windowBase(window);
		/* send new data packets if window permits */
		//printf("Filling window...\n");
		fill_window(window);

		//printf("Resending timedout packets...\n");
		resend_timedoutPackets();
		// check if all the data packets have been successfully acknowledged

		if(allPacketsAcknowledged() == true)
		{
			printf("ALL PACKETS ACKNOWLEDGED\n");
			// send FIN
			int FIN_seqno = last_ackno;
			struct packet* FIN_pckt = create_packet(NULL, 0, FIN, FIN_seqno);
			send_packet(FIN_pckt, socketfd, adr_receiver);
			print_log(true, true, FIN, FIN_pckt->header.seqno, FIN_pckt->payload_length);
			// Update statistics
			bytes_sent += FIN_pckt->payload_length;
			unique_bytes_sent += FIN_pckt->payload_length;
			FIN_packets_sent++;
			// Start its timer
			pthread_mutex_lock(&timers_mutex);
			struct packet_timer* FIN_timer = create_timer(FIN_pckt->header.ackno, clock());
			add_timer(timer_list, FIN_timer);
			start_timer(FIN_timer->pckt_ackno, timer_list);
			pthread_mutex_unlock(&timers_mutex);
			// wait for FIN's ACK
			if((rec_pckt = receive_packet(socketfd, &adr_src)) != NULL)
			{
				print_log(false, true, ACK, rec_pckt->header.seqno, rec_pckt->payload_length);
				handle_packet(rec_pckt);
				if(rec_pckt->header.seqno == FIN_pckt->header.ackno)
				{
					printf("FIN ACK RECEIVED\n");
					print_statistics();
					terminate(0);
				}
			}
		}

		// Global timeout
		if(clock()/CLOCKS_PER_SEC >= SENDER_TIMEOUT_S)
		{
			terminate(-2);
		}
	}
}
/*
 * pthread routine: Any timers that have timed out get their "timedout" value set to true
 */
void* poll_timers()
{
	while(1)
	{
		//printf("PTHREAD: polling\n");
		//sleep(1);
		struct timespec time, time2;
		time.tv_sec = 0;
		time.tv_nsec = 10000000L; // 1sec = 1,000,000,000 nanoseconds. 100th of a second
		nanosleep(&time, &time2);
		pthread_mutex_lock(&timers_mutex);
		int i;
		for(i=0; timer_list[i] != 0; i++)
		{
			struct packet_timer* timer = timer_list[i];
			// make sure that the timer is running and it's data packet has been sent

				//printf("PTHREAD: checking if timer %d has timed out with current time: %ld \n",timer->pckt_ackno, clock());
				if(timed_out(timer->pckt_ackno, timer_list, clock()))
				{
					//printf("PTHREAD: timer %d has timedout\n",timer->pckt_ackno);
					timer->timedout = true;
				}

		}
		pthread_mutex_unlock(&timers_mutex);
	}
	// return for compiler
	return NULL;
}
/*
 * Send fresh data packets to fill the window
 */
void fill_window(struct window* window)
{
	//printf("Filling window...\n");
	//printf("Window:\n size->%d\n base_seqno->%d\n space->%d\n",window->size,window->base_seqno,(window->size - window->occupied));
	int i;
	for(i=0; data_packets[i] != 0; i++)
	{
		// if window is not full
		if(window->occupied < window->size)
		{
			// if data_packet is within the window
			if(data_packets[i]->header.seqno >= window->base_seqno &&
				data_packets[i]->header.seqno <= window->base_seqno + window->size)
			{
				// if the packet has not yet been ACKd
				if(packetACKd(data_packets[i]) == false)
				{
					// send the data packet
					send_packet(data_packets[i], socketfd, adr_receiver);
					print_log(true, true, DAT, data_packets[i]->header.seqno, data_packets[i]->payload_length);
					// update statistics
					bytes_sent += data_packets[i]->payload_length;
					unique_bytes_sent += data_packets[i]->payload_length;
					unique_DAT_packets_sent++;
					DAT_packets_sent++;
					// Start its timer
					pthread_mutex_lock(&timers_mutex);
					start_timer(data_packets[i]->header.ackno, timer_list);
					pthread_mutex_unlock(&timers_mutex);
					// update occupied space in window
					window->occupied += data_packets[i]->payload_length;
				}
			}
		}
		// else window is full
		else
		{
			break;
		}
	}
}

/*
 * Update the window's base_seqno based on the data packets ACK'd so far
 */
void update_windowBase(struct window* window)
{
	// for every data packet
	int i;
	for(i=0; data_packets[i] != 0; i++)
	{
		// if it is the window's base
		if(data_packets[i]->header.seqno == window->base_seqno)
		{
			// if it has been ACK'd
			if(packetACKd(data_packets[i]))
			{
				//printf("packet is ACK'd\n");
				// move window base up to next packet's seqno
				// update the amount of space required
				if(data_packets[i+1] != 0)
				{
					window->base_seqno = data_packets[i+1]->header.seqno;
					window->occupied -= data_packets[i+1]->payload_length;
					//printf("Window:\n size->%d\n base_seqno->%d\n occupied->%d\n",window->size,window->base_seqno,window->occupied);
				}
			}
		}
	}
}

/*
 * Resend any packets that have timed out, based on their "timedout" value
 */
void resend_timedoutPackets()
{
	pthread_mutex_lock(&timers_mutex);
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		struct packet_timer* timer = timer_list[i];
		if(timer->timedout == true && timer->running == true)
		{
			int j;
			for(j=0; data_packets[j] != 0; j++)
			{
				if(data_packets[j]->header.ackno == timer->pckt_ackno)
				{
					// resend the data packet
					send_packet(data_packets[i], socketfd, adr_receiver);
					print_log(true, false, DAT, data_packets[j]->header.seqno, data_packets[i]->payload_length);
					// update statistics
					bytes_sent += data_packets[i]->payload_length;
					DAT_packets_sent++;
					// restart its timer
					start_timer(data_packets[i]->header.ackno, timer_list);
				}
			}
		}
	}
	pthread_mutex_unlock(&timers_mutex);
}
/*
 * Return true if packet has been acknowledged (its timer is not running)
 */
bool packetACKd(struct packet* pckt)
{
	bool retval = false;
	pthread_mutex_lock(&timers_mutex);
	// find timer
	struct packet_timer* timer;
	if((timer = find_timer(timer_list, pckt->header.ackno)) != NULL)
	{
		if(timer->running == false && timer->time_sent != -1)
			retval = true;
	}
	else
	{
		printf("Cannot check if packet %d is ACK'd\n",pckt->header.seqno);
		exit(-1);
	}
	pthread_mutex_unlock(&timers_mutex);
	return retval;
}

/*
 * Return true if all data packets have been acknowledged, false otherwise
 */
bool allPacketsAcknowledged()
{
	bool retval = true;
	// for every data packet
	int i;
	for(i=0; data_packets[i] != 0; i++)
	{
		// if packet hasn't been ACK'd yet
		if(packetACKd(data_packets[i]) == false)
		{
			//printf("Timer %d is still running\n",data_packets[i]->header.ackno);
			retval =  false;
			break;
		}
	}
	return retval;
}
/*
 * Handle packets from the receiver
 */
void handle_packet(struct packet* pckt)
{
	switch(pckt->header.type)
	{
		case DAT:
			break;
		case ACK:
			// update statistics
			ACK_packets_received++;
			// mark data packet as acknowledged by stopping its timer
			pthread_mutex_lock(&timers_mutex);
			stop_timer(timer_list, pckt->header.seqno);
			//printf("Timer %d stopped\n", pckt->header.seqno);
			pthread_mutex_unlock(&timers_mutex);
			break;
		case SYN:
			break;
		case RST:
			// update statistics
			RST_packets_received++;
			break;
		case FIN:
			break;
		default:
			break;
	}
}
/*
 * Real-time log as packets are sent and received
 */
void print_log(bool sent, bool unique, int typeno, int no, int length)
{
	char* type = type_itos(typeno);
	char* source_adr;
	char* dest_adr;
	char* current_time = (char*)malloc(sizeof(current_time)*100);
	char* event_type = (char*)malloc(sizeof(event_type)*10);
	char* seqackno = (char*)malloc(sizeof(seqackno)*10);
	char* payloadwindow = (char*)malloc(sizeof(payloadwindow)*10);
	char* full_log = (char*)malloc(sizeof(full_log)*200);

	sprintf(seqackno, "%d", no);
	sprintf(payloadwindow, "%d", length);
	if(sent)
	{
		if(unique)
			strcpy(event_type, "s");
		else
			strcpy(event_type, "S");
		source_adr = get_adrString(adr_sender.sin_addr, adr_sender.sin_port);
		dest_adr = get_adrString(adr_receiver.sin_addr, adr_receiver.sin_port);
	}
	else
	{
		if(unique)
			strcpy(event_type, "r");
		else
			strcpy(event_type, "R");
		dest_adr = get_adrString(adr_sender.sin_addr, adr_sender.sin_port);
		source_adr = get_adrString(adr_receiver.sin_addr, adr_receiver.sin_port);
	}
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	current_time = asctime (timeinfo);
	// stip newline from current time
	int i;
	for(i=0; current_time[i] != '\0'; i++)
	{
		if(current_time[i] == '\n')
			current_time[i] = '\0';
	}
	//printf ( "Current local time and date: %s\n", current_time );
	// cat them all together
	char space[2] = " ";
	strcpy(full_log, current_time);
	strcat(full_log, space);
	strcat(full_log, event_type);
	strcat(full_log, space);
	strcat(full_log, source_adr);
	strcat(full_log, space);
	strcat(full_log, dest_adr);
	strcat(full_log, space);
	strcat(full_log, type);
	strcat(full_log, space);
	strcat(full_log, seqackno);
	strcat(full_log, space);
	strcat(full_log, payloadwindow);
	printf("%s\n",full_log);
}
/*
 * Construct and return an address string for the sake of logging
 */
char* get_adrString(struct in_addr adrIp, in_port_t adrPort)
{
	char* ip = inet_ntoa(adrIp);
	int pt = ntohs(adrPort);
	char* strport = (char*)malloc(sizeof(strport)*10);
	sprintf(strport, "%d", pt);
	strcat(ip, ":");
	strcat(ip, strport);
	return ip;
}
/*
 * Log sender statistics
 */
void print_statistics()
{
	printf("Sender Statistics:\n");
	printf(" Total data bytes sent: %d\n",bytes_sent);
	printf(" Unique data bytes sent: %d\n",unique_bytes_sent);
	printf(" Total data packets sent: %d\n",DAT_packets_sent);
	printf(" Unique data packets sent: %d\n",unique_DAT_packets_sent);
	printf(" SYN packets sent: %d\n",SYN_packets_sent);
	printf(" FIN packets sent: %d\n",FIN_packets_sent);
	printf(" RST packets sent: %d\n",RST_packets_sent);
	printf(" ACK packets received: %d\n",ACK_packets_received);
	printf(" RST packets received: %d\n",RST_packets_received);
	clock_t time = clock();
	double int_time = (double)time/CLOCKS_PER_SEC;
	printf(" Transfer duration: %f\n",int_time);
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









