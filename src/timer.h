#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

// Packet timeout value in seconds and microseconds respectively
#define TIMEOUT_S 5
#define TIMEOUT_US 5000000
// Max number of timers
#define MAX_TIMERS 10000
// Sender and receiver ultimate time out
#define RECEIVER_TIMEOUT_S 300
#define SENDER_TIMEOUT_S 300

struct packet_timer
{
	int pckt_seqno;
	clock_t time_sent;
};

struct packet_timer* create_timer(int, int);
void add_timer(struct packet_timer**, struct packet_timer*);
struct packet_timer* find_timer(struct packet_timer**, int);
bool timed_out(int, struct packet_timer**, int);
void update_timer(int, struct packet_timer**);
