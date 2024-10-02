#include <stdio.h>
#include <stdlib.h>

void init_array(int *arr, int size) {
    int i;
    for (i = 0; i < size; i++) {
        arr[i] = i;
    }
}

int main(void) {
    int *arr1;

    arr1 = malloc(sizeof(int) * 10);
    if (arr1 == NULL) {
        printf("malloc error\n");
        exit(1);
    }

    /* pass the value of arr1 (base address of array in heap) */
    init_array(arr1, 10);

    for (int i = 0; i < 10; i++) {
        printf("%d ", arr1[i]);
    }
    printf("\n");

    free(arr1);
}

