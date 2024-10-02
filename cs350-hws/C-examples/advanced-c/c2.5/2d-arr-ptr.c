#include <stdio.h>
#include <stdlib.h>

#define N 3
#define M 4

/*
 * initialize a 2D array
 * arr: the array
 * rows: number of rows
 * cols: number of columns
 */
void init2D_Method2(int **arr, int rows, int cols) {
    int i,j;

    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            arr[i][j] = 3;
        }
    }
}

/*
 * main: example of calling init2D_Method2
 */
int main(void) {
    int **two_d_array;

    // allocate an array of N pointers to ints
    // malloc returns the address of this array (a pointer to (int *)'s)
    two_d_array = malloc(sizeof(int *) * N);

    // for each row, malloc space for its column elements and add it to
    // the array of arrays
    for (int i = 0; i < N; i++) {
    // malloc space for row i's M column elements
        two_d_array[i] = malloc(sizeof(int) * M);
    }

    init2D_Method2(two_d_array, N, M);

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            printf("%d \t", two_d_array[i][j]);
        }
        printf("\n");
    }
   
}