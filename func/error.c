#include <stdlib.h>
#include <stdio.h>

#include <func.h>

int error(char *message) {
	fprintf(stderr, message);
	return EXIT_FAILURE;
}
