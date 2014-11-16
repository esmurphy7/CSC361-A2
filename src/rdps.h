// Max number of bytes allowed in a packet
#define MAX_PAYLOAD_SIZE 1024
// Packet timeout value in seconds and microseconds respectively
#define TIMEOUT_S 5
#define TIMEOUT_US 5000000
// Max number of timers
#define MAX_TIMERS 10000

struct packet_hdr
{
	char* magic;
	int type;
	int seqno;
	int ackno;
	int winsize;
};

struct packet
{
	struct packet_hdr header;
	char* payload;
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

struct packet_timer
{
	int pckt_seqno;
	clock_t time_sent;
};

/*
 * Create and return new timer
 * 	It's implied that this method is called to start the timer as well
 */
struct packet_timer* create_timer(int pckt_seqno, int time)
{
	printf("creating timer with seqno %d...\n",pckt_seqno);
	struct packet_timer* timer = (struct packet_timer*)malloc(sizeof(*timer));
	timer->pckt_seqno = pckt_seqno;
	timer->time_sent = time;
	return timer;
}

/*
 * Add timer to timer list
 */
void add_timer(struct packet_timer** timer_list, struct packet_timer* timer)
{
	printf("adding timer with seqno %d...\n",timer->pckt_seqno);
	int i;
	for(i=0; timer_list[i] != 0; i++);
	timer_list[i] = timer;
}

/* Find and return timer corresponding to the packet in timer list
 * return: NULL if corresponding packet does not exist, corresponding timer otherwise
 */
struct packet_timer* find_timer(struct packet_timer** timer_list, struct packet* pckt)
{
	printf("finding timer with seqno %d...\n",pckt->header.seqno);
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(pckt->header.seqno == timer_list[i]->pckt_seqno)
			return timer_list[i];
	}
	return NULL;
}

/*
 * Return true if the packet's corresponding timer has timed out
 * param timer_list: the list of timers to search for the corresponding timer in
 * param current_time: checks if timer has timed out relative to this value
 */
bool timed_out(struct packet* pckt, struct packet_timer** timer_list, int current_time)
{
	struct packet_timer* timer = NULL;
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(pckt->header.seqno == timer_list[i]->pckt_seqno)
		{
			timer = timer_list[i];
			break;
		}
	}
	if(timer == NULL)
	{
		perror("sender: could not find corresponding timer\n");
		exit(-1);
	}
	if(current_time - timer->time_sent < TIMEOUT_S)
		return true;
	else
		return false;
}
