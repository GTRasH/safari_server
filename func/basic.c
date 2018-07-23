#include <basic.h>

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

void setError(char *message, int abend) {
	fprintf(stderr, "%s%s%s", message, strerror(errno), "\n");
	if (abend)
		exit(EXIT_FAILURE);
}

void freeArray(char ** array) {
	if (array == NULL)
		free(array);
	else {
		int i=0;
		while (*(array+i) != NULL)
			free(*(array+i++));
	
		free(*(array+i));
		free(array);
	}
}

char * getFileContent(const char * fileName) {
	long unsigned fileSize;
	size_t readBytes;
	// Init
	char * content	= NULL;
	FILE * filePtr	= fopen(fileName, "r");
	fileSize	= 0;
	if (filePtr != NULL) {
		// Setze den filePointer an das Datei-Ende
		if (fseek(filePtr, 0L, SEEK_END) == 0) {
			fileSize = ftell(filePtr);
			if (fileSize == -1)
				setError("Error while ftell() from file-pointer", 1);
			content = malloc(sizeof(char) * (fileSize + 1));
			if (fseek(filePtr, 0L, SEEK_SET) != 0)
				setError("Error while rewinding file", 1);
			readBytes = fread(content, sizeof(char), fileSize, filePtr);
			if (ferror(filePtr) != 0)
				setError("Error reading file", 1);
			else
				content[readBytes] = '\0';
		}
		fclose(filePtr);
	}
	return content;
}
