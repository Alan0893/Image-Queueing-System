#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct personT {
    char *name;     // for a dynamically allocated string field
    int  age;
};

int main(void) {
    struct personT p1, *p2;

    // need to malloc space for the name field:
    p1.name = malloc(sizeof(char) * 8);
    strcpy(p1.name, "Zhichen");
    p1.age = 22;


    // first malloc space for the struct:
    p2 = malloc(sizeof(struct personT));

    // then malloc space for the name field:
    p2->name = malloc(sizeof(char) * 4);
    strcpy(p2->name, "Vic");
    p2->age = 19;

    printf("%s %d\n", p2->name, p2->age);
    printf("%s %d\n", p1.name, p1.age);

    // Note: for strings, we must allocate one extra byte to hold the
    // terminating null character that marks the end of the string.
}