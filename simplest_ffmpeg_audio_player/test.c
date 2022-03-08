#include <stdio.h>
#include <stdlib.h>
int main2(int argc, char** args) {
	int i = 5;
	if (argc > 1) {
		i = atoi(args[1]);
	}
	int array[10];
	for (int j = 0; j < i; j++) {
		array[j] = j;
	}

	for (int k = 0; k < i; k++) {
		printf("array[%d]:%d", k, array[k]);
	}
}