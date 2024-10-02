#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // declare a pointer variable to point to allocated heap space
    int    *p_array;
    double *d_array;

    // call malloc to allocate the appropriate number of bytes for the array

    p_array = malloc(sizeof(int) * 50);      // allocate 50 ints
    d_array = malloc(sizeof(double) * 100);  // allocate 100 doubles

    // always CHECK RETURN VALUE of functions and HANDLE ERROR return values
    if ( (p_array == NULL) || (d_array == NULL) ) {
        printf("ERROR: malloc failed!\n");
        exit(1);
    }

    // use [] notation to access array elements
    for (int i = 0; i < 50; i++) {
        p_array[i] = 0;
        d_array[i] = 0.0;
    }

    // free heap space when done using it
    free(p_array);
    p_array = NULL;

    free(d_array);
    d_array = NULL;
}