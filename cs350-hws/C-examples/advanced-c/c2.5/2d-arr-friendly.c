#include <stdio.h>
#include <stdlib.h>

#define N 3
#define M 4

int main(void) {
    // the 2D array variable is declared to be `int **` (a pointer to an int *)
    // a dynamically allocated array of dynamically allocated int arrays
    // (a pointer to pointers to ints)
    int **two_d_array;
    int i;

    // allocate an array of N pointers to ints
    // malloc returns the address of this array (a pointer to (int *)'s)
    two_d_array = malloc(sizeof(int *) * N);

    // for each row, malloc space for its column elements and add it to
    // the array of arrays
    for (i = 0; i < N; i++) {
    // malloc space for row i's M column elements
        two_d_array[i] = malloc(sizeof(int) * M);
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            two_d_array[i][j] = i+j;
        }
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            printf("%d \t", two_d_array[i][j]);
        }
        printf("\n");
    }
    
}