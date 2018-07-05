#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <func.h>

char * int2string(int value) {
	int length;
	const int n = snprintf(NULL, 0, "%i", value);
	assert(n > 0);
	char *ret = malloc(n+1);
	length = snprintf(ret, n+1, "%i", value);
	assert(ret[n] == '\0');
	assert(n == length);
	return ret;
}

int error(char *message) {
	fprintf(stderr, message);
	return EXIT_FAILURE;
}

void freeArray(char ** array) {
	int i;
	for (i=0; *(array+i); ++i)
		free(*(array+i));

	free(array);
}
