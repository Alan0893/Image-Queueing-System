#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "timelib.h"

int main(int argc, char *argv[])
{
    // Getting the 3 parameters
    long sec = strtol(argv[1], NULL, 10);   // convert first param from string to long int
    long nsec = strtol(argv[2], NULL, 10);  // convert second param from string to long int
    char method = argv[3][0];   // get the first char of the third param ('s', 'b')

    uint64_t clocks_elapsed;    // store number of clock cycles during wait period
    double wait_time_sec = sec + nsec / 1e9;    // convert wait time into sec
    double clock_speed; // store calculated CPU clock speed (MHz)

    // Measure elapsed clock cycles based on the method
    switch (method) {
        case 's':
            clocks_elapsed = get_elapsed_sleep(sec, nsec);
            break;
        case 'b':
            clocks_elapsed = get_elapsed_busywait(sec, nsec);
            break;
    }

    clock_speed = (clocks_elapsed / wait_time_sec) / 1e6;  // Convert Hz to MHz

    // Print results
    printf("WaitMethod: %s\n", method == 's' ? "SLEEP" : "BUSYWAIT");
    printf("WaitTime: %ld %ld\n", sec, nsec);
    printf("ClocksElapsed: %lu\n", clocks_elapsed);
    printf("ClockSpeed: %.2f\n", clock_speed);

    return EXIT_SUCCESS;
}
