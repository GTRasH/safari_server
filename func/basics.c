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

void freeGeoTable(intersectGeo ** table) {
	intersectGeo * geoPtr, * temp;
	for (int i = 0; i < MAX_HASH; ++i) {
		geoPtr = table[i];
		while (geoPtr != NULL) {
			free(geoPtr->xml);
			temp	= geoPtr;
			geoPtr	= geoPtr->next;
			free(temp);
		}
		free(table[i]);
	}
}

void sqlError(MYSQL *con) {
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);
}

MYSQL * sqlConnect(char * db) {
	MYSQL *con = mysql_init(NULL);
	if (mysql_real_connect(con, "localhost", "root", "safari", 
			db, 0, NULL, 0) == NULL)
		sqlError(con);
	return con;
}
