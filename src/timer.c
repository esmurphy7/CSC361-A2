#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

/*
 * Create and return new timer
 * 	It's implied that this method is called to start the timer as well
 */
struct packet_timer* create_timer(int pckt_seqno, int time)
{
	//printf("creating timer with seqno %d...\n",pckt_seqno);
	struct packet_timer* timer = (struct packet_timer*)malloc(sizeof(*timer));
	timer->pckt_seqno = pckt_seqno;
	timer->time_sent = time;
	timer->running = true;
	timer->timedout = false;
	return timer;
}

/*
 * Add timer to timer list
 */
void add_timer(struct packet_timer** timer_list, struct packet_timer* timer)
{
	printf("Adding timer %d...\n",timer->pckt_seqno);
	int i;
	for(i=0; timer_list[i] != 0; i++);
	timer_list[i] = timer;
}

/*
* Stop timer to signal that the corresponding packet is acknowledged
*/
void stop_timer(struct packet_timer** timer_list, int seqno)
{
	struct packet_timer* timer;
	if((timer = find_timer(timer_list, seqno)) != NULL)
	{
		timer->running = false;
	}
	else
	{
		printf("Could not find timer with seqno->%d to stop\n",seqno);
		print_runningTimers(timer_list);
		print_stoppedTimers(timer_list);
		exit(-1);
	}
}
/* Find and return timer corresponding to the packet in timer list
 * return: NULL if corresponding packet does not exist, corresponding timer otherwise
 */
struct packet_timer* find_timer(struct packet_timer** timer_list, int seqno)
{
	//printf("finding timer with seqno %d...\n",seqno);
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(seqno == timer_list[i]->pckt_seqno)
			return timer_list[i];
	}
	return NULL;
}

/*
 * Return true if the timer with seqno is running and has timed out
 * param timer_list: the list of timers to search for the corresponding timer in
 * param current_time: checks if timer has timed out relative to this value
 */
bool timed_out(int seqno, struct packet_timer** timer_list, int current_time)
{
	struct packet_timer* timer = NULL;
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(seqno == timer_list[i]->pckt_seqno)
		{
			timer = timer_list[i];
			break;
		}
	}
	if(timer == NULL)
	{
		perror("could not find corresponding timer\n");
		exit(-1);
	}

	if(current_time - timer->time_sent < TIMEOUT_S &&
			timer->running == true)
	{
		return true;
	}
	else
		return false;
}

/*
 * Start/restart timer with seqno. Update it if it exists, create new timer otherwise
 */
void start_timer(int seqno, struct packet_timer** timer_list)
{
	struct packet_timer* timer;
	// If timer already exists, update it
	if((timer = find_timer(timer_list, seqno)) != NULL)
	{
		printf("Restarting timer %d\n",seqno);
		timer->time_sent = clock();
		timer->running = true;
		timer->timedout = false;
	}
	// Else timer doesn't exist yet, create one and add it to timer list
	else
	{
		struct packet_timer* new_timer = create_timer(seqno, clock());
		add_timer(timer_list, new_timer);
	}
}

/*
 * Print all the running timers for debug purposes
 */
void print_runningTimers(struct packet_timer** timer_list)
{
	printf("Running timers:\n");
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(timer_list[i]->running == true)
		{
			printf("Timer: %d\n",timer_list[i]->pckt_seqno);
		}
	}
}

/*
 * Print all the stopped timers for debug purposes
 */
void print_stoppedTimers(struct packet_timer** timer_list)
{
	printf("Stopped timers:\n");
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(timer_list[i]->running == false)
		{
			printf("Timer: %d\n",timer_list[i]->pckt_seqno);
		}
	}
}
