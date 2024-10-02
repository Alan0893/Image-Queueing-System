#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int len = strlen(argv[1]);

	for (int i = len - 1; i >= 0; i--) {
		printf("%c", argv[1][i]);
	}
	printf("\n");
	
	return 0;
}
