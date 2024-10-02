#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* define a new struct type (outside function bodies) */
struct studentT {
    char  name[64];
    int   age;
    float gpa;
    int   year;
};

void updateAges(struct studentT *classroom, int size) {
    int i;

    for (i = 0; i < size; i++) {
        strcpy(classroom[i].name, "student");
        classroom[i].age = 101;
    }
}

int main(void) {
    struct studentT classroom1[40];   // an array of 40 struct studentT

    struct studentT *classroom2;      // a pointer to a struct studentT
                                  // (for a dynamically allocated array)

    struct studentT *classroom3[40];  // an array of 40 struct studentT *


    // classroom1 is an array:
    //    use indexing to access a particular element
    //    each element in classroom1 stores a struct studentT:
    //    use dot notation to access fields
    classroom1[3].age = 21;

    // classroom2 is a pointer to a struct studentT
    //    call malloc to dynamically allocate an array
    //    of 15 studentT structs for it to point to:
    classroom2 = malloc(sizeof(struct studentT) * 15);

    // each element in array pointed to by classroom2 is a studentT struct
    //    use [] notation to access an element of the array, and dot notation
    //    to access a particular field value of the struct at that index:
    classroom2[3].year = 2013;

    // classroom3 is an array of struct studentT *
    //    use [] notation to access a particular element
    //    call malloc to dynamically allocate a struct for it to point to
    classroom3[5] = malloc(sizeof(struct studentT));

    // access fields of the struct using -> notation
    // set the age field pointed to in element 5 of the classroom3 array to 21
    classroom3[5]->age = 21;                            

    updateAges(classroom1, 40);
    updateAges(classroom2, 15);

    printf("Classroom1:\n");
    for(int i = 0; i < 40; i++) {
        printf("%s %d %d\n", classroom1[i].name, classroom1[i].age, classroom1[i].year);
    }

    printf("Classroom2:\n");

    for(int i = 0; i < 15; i++) {
        printf("%s %d %d\n", classroom2[i].name, classroom2[i].age, classroom2[i].year);
    }
}

