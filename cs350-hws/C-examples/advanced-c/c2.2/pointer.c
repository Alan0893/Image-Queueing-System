#include <stdio.h>

int main(void) {
    int x = 30;
    int* p_x = &x;

    printf("x is %d\n", x);
    printf("p_x is %p\n", p_x);

    *p_x = 40;
    printf("x is now %d\n", x);

    char c = 'Z';
    char* p_c = &c;

    printf("c is %c\n", c);
    printf("p_c is %p\n", p_c);

    *p_c = 'D';
    printf("c is now %c\n", c);

    return 0;
}