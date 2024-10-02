#include <stdio.h>
#include <string.h>

int main(void) {
    // declare arrays specifying their type and total capacity
    float averages[30];   // array of float, 30 elements
    char  name[20];       // array of char, 20 elements
    int i;

    // access array elements
    for (i = 0; i < 10; i++) {
        averages[i] = 0.0 + i;
        name[i] = 'a' + i;
    }
    name[10] = '\0';    // name is being used for storing a C-style string

    // prints: 3 d abcdefghij
    printf("%g %c %s\n", averages[3], name[3], name);

    strcpy(name, "Hello");
    printf("%s\n", name);  // prints: Hello
}