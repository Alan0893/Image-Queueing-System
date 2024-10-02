/*******************************************************************************
* Time Functions Library (implementation)
*
* Description:
*     A library to handle various time-related functions and operations.
*
* Author:
*     Renato Mancuso <rmancuso@bu.edu>
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 2023
*
* Last Update:
*     September 9, 2024
*
* Notes:
*     Ensure to link against the necessary dependencies when compiling and
*     using this library. Modifications or improvements are welcome. Please
*     refer to the accompanying documentation for detailed usage instructions.
*
*******************************************************************************/

#include "timelib.h"

/* Return the number of clock cycles elapsed when waiting for
 * wait_time seconds using sleeping functions */
uint64_t get_elapsed_sleep(long sec, long nsec)
{
	/* IMPLEMENT ME! */
	uint64_t start_cycles, end_cycles;	// store the TSC values (clock cycles)
	
	/**
	* struct - object
	* timespec - structure that holds : { seconds, nanoseconds } : { tv_sec, tv_nsec }
	*/
	struct timespec sleep_time = { sec, nsec };	// store time for program to sleep for

	get_clocks(start_cycles);	// number of clock cycles elapsed since CPU started

	nanosleep(&sleep_time, NULL);	// syscall to put program to sleep 

	get_clocks(end_cycles);	// after sleep is finished, get TSC

	return end_cycles - start_cycles;	// diff = total number of CPU clock cycles elapsed during sleep
}

/* Return the number of clock cycles elapsed when waiting for
 * wait_time seconds using busy-waiting functions */
uint64_t get_elapsed_busywait(long sec, long nsec)
{
	/* IMPLEMENT ME! */
	/**
	* begin_timestamp : stores current time when busy-wait starts
	* now : store curr time during each iteration of loop
	* target_time : time until which program should busy-wait
	*/
	struct timespec begin_timestamp, now, target_time;
	uint64_t start_cycles, end_cycles;	// store TSC values (clock cycles)
	
	clock_gettime(CLOCK_MONOTONIC, &begin_timestamp);	// retrieve curr time, stores it in begin_timestamp
	
	target_time = begin_timestamp;	// program starts tracking elapsed time
	
	struct timespec delay = { sec, nsec };	// amount of time to busy-wait
	timespec_add(&target_time, &delay);	// future time busy-wait should stop
	
	get_clocks(start_cycles);	// current TSC value, clock cycles elapsed since start

	// Busy-wait loop 
	do {
		clock_gettime(CLOCK_MONOTONIC, &now);	// gets curr time and stores it in now
	} while (timespec_cmp(&now, &target_time) < 0);	// curr time < future stop time

	get_clocks(end_cycles);	// after busy-wait finished, get TSC

	return end_cycles - start_cycles;	// diff = total number of CPU clock cycles elapsed during busy-wait
}

/* Utility function to add two timespec structures together. The input
 * parameter a is updated with the result of the sum. */
void timespec_add (struct timespec * a, struct timespec * b)
{
	/* Try to add up the nsec and see if we spill over into the
	 * seconds */
	time_t addl_seconds = b->tv_sec;
	a->tv_nsec += b->tv_nsec;
	if (a->tv_nsec > NANO_IN_SEC) {
		addl_seconds += a->tv_nsec / NANO_IN_SEC;
		a->tv_nsec = a->tv_nsec % NANO_IN_SEC;
	}
	a->tv_sec += addl_seconds;
}

/* Utility function to compare two timespec structures. It returns 1
 * if a is in the future compared to b; -1 if b is in the future
 * compared to a; 0 if they are identical. */
int timespec_cmp(struct timespec *a, struct timespec *b)
{
	if(a->tv_sec == b->tv_sec && a->tv_nsec == b->tv_nsec) {
		return 0;
	} else if((a->tv_sec > b->tv_sec) ||
		(a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec)) {
		return 1;
	} else {
		return -1;
	}
}

/* Busywait for the amount of time described via the delay
 * parameter */
uint64_t busywait_timespec(struct timespec delay)
{
	/* IMPLEMENT ME! (Optional but useful) */
}
