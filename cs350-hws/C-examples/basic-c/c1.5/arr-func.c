#include <stdio.h>
void test(int a[], int size) {
    if (size > 3) {
        a[3] = 8;
    }
    size = 2; // changing parameter does NOT change argument
}

/*
This example shows C does not "pass by reference". 
Modifying the copy of the array passed as a parameter, does not effect original!
*/
int main(void) {
    int arr[5], n = 5, i;

    for (i = 0; i < n; i++) {
        arr[i] = i;
    }

    printf("%d %d\n", arr[3], n);  // prints: 3 5

    test(arr, n);
    printf("%d %d\n", arr[3], n);  // prints: 8 5

    return 0;
}