/*******************************************************************************
* CPU Clock Measurement Using RDTSC
*
* Description:
*     This C file provides functions to compute and measure the CPU clock using
*     the `rdtsc` instruction. The `rdtsc` instruction returns the Time Stamp
*     Counter, which can be used to measure CPU clock cycles.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 2023
*
* Notes:
*     Ensure that the platform supports the `rdtsc` instruction before using
*     these functions. Depending on the CPU architecture and power-saving
*     modes, the results might vary. Always refer to the CPU's official
*     documentation for accurate interpretations.
*
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "timelib.h"

int main (int argc, char ** argv)
{
	long wait_seconds = strtol(argv[1], NULL, 0);
	long wait_nanoseconds = strtol(argv[2], NULL, 0);
	char wait_method = argv[3][0];
	double clock_speed, waittime_dec;
	uint64_t elapsed;

	/* Suppress warning on unused argc paramter */
	(void)argc;

	waittime_dec = wait_seconds + ((double)wait_nanoseconds) / NANO_IN_SEC;

	if (wait_method == 's') {
		/* Use sleep-based wait */
		elapsed = get_elapsed_sleep(wait_seconds, wait_nanoseconds);
		clock_speed = (double)(elapsed) / waittime_dec;
		printf("WaitMethod: SLEEP\n");
	} else if (wait_method == 'b') {
		/* Use busywait */
		elapsed = get_elapsed_busywait(wait_seconds, wait_nanoseconds);
		clock_speed = (double)(elapsed) / waittime_dec;
		printf("WaitMethod: BUSYWAIT\n");
	} else {
		return EXIT_FAILURE;
	}

	/* Print out final statistics */

	printf("WaitTime: %ld %ld\n", wait_seconds, wait_nanoseconds);
	printf("ClocksElapsed: %ld\n", elapsed);
	printf("ClockSpeed: %lf\n", clock_speed / (1000 * 1000));

	return EXIT_SUCCESS;
}
