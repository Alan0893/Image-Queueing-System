#include <stdio.h>
#include <stdlib.h>

#define N 3
#define M 4

int main(void) {
    int *two_d_array;    // the type is a pointer to an int (the element type)

    // allocate in a single malloc of N x M int-sized elements:
    two_d_array = malloc(sizeof(int) * N * M);

    if (two_d_array == NULL) {
        printf("ERROR: malloc failed!\n");
        exit(1);
    }

    // Contigious access in memory!
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            two_d_array[i*M + j] = i+j;
        }
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            printf("%d \t", two_d_array[i*M + j]);
        }
        printf("\n");
    }
}