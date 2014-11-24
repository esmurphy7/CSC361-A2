#include <stdbool.h>
#include <sys/time.h>
#include <time.h>


// Packet timeout value in seconds and microseconds respectively
#define TIMEOUT_S 1
#define TIMEOUT_US 100000
// Max number of timers
#define MAX_TIMERS 50000
// Sender and receiver ultimate time out
#define RECEIVER_TIMEOUT_S 300
#define SENDER_TIMEOUT_S 300

struct packet_timer
{
	int pckt_ackno;
	clock_t time_sent;
	bool running;
	bool timedout;
};

struct packet_timer* create_timer(int, clock_t);
void add_timer(struct packet_timer**, struct packet_timer*);
void stop_timer(struct packet_timer**, int);
struct packet_timer* find_timer(struct packet_timer**, int);
bool timed_out(int, struct packet_timer**, clock_t);
void start_timer(int, struct packet_timer**);
void print_runningTimers(struct packet_timer**);
void print_stoppedTimers(struct packet_timer**);
