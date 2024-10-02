/*
    The Hello World Program in C
*/

/* C math and I/O libraries */
#include <math.h>
#include <stdio.h>

/* main function definition: */
int main(void) {
    // statements end in a semicolon (;)
    printf("Hello World\n");
    printf("sqrt(4) is %f\n", sqrt(4));

    printf("number of bytes in an int: %lu\n", sizeof(int));
    printf("number of bytes in a short: %lu\n", sizeof(short));

    return 0;  // main returns value 0
}