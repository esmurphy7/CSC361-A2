#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

/*
 * Create and return new timer
 * 	It's implied that this method is called to start the timer as well
 */
struct packet_timer* create_timer(int pckt_ackno, clock_t time)
{
	struct packet_timer* timer = (struct packet_timer*)malloc(sizeof(*timer));
	timer->pckt_ackno = pckt_ackno;
	timer->time_sent = time;
	timer->running = false;
	timer->timedout = false;
	return timer;
}

/*
 * Add timer to timer list
 */
void add_timer(struct packet_timer** timer_list, struct packet_timer* timer)
{
	int i;
	for(i=0; timer_list[i] != 0; i++);
	timer_list[i] = timer;
}

/*
* Stop timer to signal that the corresponding packet is acknowledged
*/
void stop_timer(struct packet_timer** timer_list, int ackno)
{
	struct packet_timer* timer;
	if((timer = find_timer(timer_list, ackno)) != NULL)
	{
		timer->running = false;
	}
	else
	{
		printf("Could not find timer with ackno->%d to stop\n",ackno);
		exit(-1);
	}
}
/* Find and return timer corresponding to the packet in timer list
 * return: NULL if corresponding packet does not exist, corresponding timer otherwise
 */
struct packet_timer* find_timer(struct packet_timer** timer_list, int ackno)
{
	//printf("finding timer with ackno %d...\n",ackno);
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		if(ackno == timer_list[i]->pckt_ackno)
			return timer_list[i];
	}
	return NULL;
}

/*
 * Return true if the timer with ackno is running and has timed out
 * param timer_list: the list of timers to search for the corresponding timer in
 * param current_time: checks if timer has timed out relative to this value
 */
bool timed_out(int ackno, struct packet_timer** timer_list, clock_t current_time)
{
	struct packet_timer* timer = NULL;
	// for each timer
	int i;
	for(i=0; timer_list[i] != 0; i++)
	{
		// found the desired timer
		if(ackno == timer_list[i]->pckt_ackno)
		{
			timer = timer_list[i];
			break;
		}
	}
	if(timer == NULL)
	{
		printf("could not find if timer %d has timed out\n",timer->pckt_ackno);
		exit(-1);
	}
	clock_t cur_time = current_time;
	clock_t timeout_val = TIMEOUT_US/100;
	clock_t time_sent = timer->time_sent;
	/*
	printf("Current time: %ld, Timeout Value: %ld\nTimer: %d\n time_sent->%ld\n",
				cur_time,
				timeout_val,
				timer->pckt_ackno,
				time_sent);
				*/
	if(cur_time - time_sent > timeout_val &&
			timer->running == true)
	{
		//printf("Timer %d has timed out\n",timer->pckt_ackno);
		return true;
	}
	else
		return false;
}

/*
 * Start/restart timer with ackno. Update it if it exists, create new timer otherwise
 */
void start_timer(int ackno, struct packet_timer** timer_list)
{
	struct packet_timer* timer;
	// If timer already exists, update it
	if((timer = find_timer(timer_list, ackno)) != NULL)
	{
		//printf("Starting/Restarting timer %d\n",ackno);
		timer->time_sent = clock();
		timer->running = true;
		timer->timedout = false;
	}
	// Else timer doesn't exist yet, create one and add it to timer list
	else
	{
		printf("Cannot start timer, timer %d doesn't exist, quitting\n", ackno);
		exit(-1);
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
			printf("Timer: %d sent at %ld\n",timer_list[i]->pckt_ackno,
											timer_list[i]->time_sent);
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
			printf("Timer: %d sent at %ld\n",timer_list[i]->pckt_ackno,
												timer_list[i]->time_sent);
		}
	}
}
