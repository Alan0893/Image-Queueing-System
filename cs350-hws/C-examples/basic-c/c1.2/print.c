/* C input (printf) example */
#include <stdio.h>

int main(void) {

    printf("Name: %s,  Info:\n", "Vijay");
    printf("\tAge: %d \t Ht: %g\n",20,5.9);
    printf("\tYear: %d \t Dorm: %s\n",
            3,"Alice Paul");


    char ch;

    ch = 'A';
    printf("ch value is %d which is the ASCII value of  %c\n", ch, ch);

    ch = 99;
    printf("ch value is %d which is the ASCII value of  %c\n", ch, ch);
    return 0;
}