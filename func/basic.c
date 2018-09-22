/*	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#																			#
 * 	#				HfTL SAFARI PROTOTYP - BASIC FUNCTIONS						#
 * 	#																			#
 * 	#	Author:		Sebastian Neutsch											#
 * 	#	Mail:		sebastian.neutsch@t-online.de								#
 *  #	Copyright:	2018 Sebastian Neutsch / Deutsche Telekom AG				#
 * 	#																			#
 * 	#																			#
 * 	#	This file is part of safari_server.										#
 *	#																			#
 *	#	safari_server is free software: you can redistribute it and/or modify	#
 *	#	it under the terms of the GNU General Public License as published by	#
 *	#	the Free Software Foundation, either version 3 of the License, or		#
 *	#	(at your option) any later version.										#
 *	#																			#
 *	#	safari_server is distributed in the hope that it will be useful,		#
 *	#	but WITHOUT ANY WARRANTY; without even the implied warranty of			#
 *	#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			#
 *	#	GNU General Public License for more details.							#
 *	#																			#
 *	#	You should have received a copy of the GNU General Public License		#
 *	#	along with safari_server. If not, see <https://www.gnu.org/licenses/>.	#
 *	#																			#
 * 	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
*/
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

void setLogText(char * text, const char * logFile) {
	FILE * fd;
	time_t timeVal;
	struct tm * timeLoc;
	char string[LOG_BUF], timeStr[20];
	
	time(&timeVal);
	timeLoc = localtime(&timeVal);
	
	fd = fopen(logFile, "a");
	
	strftime(timeStr, 20, "%Y/%m/%d %H:%M:%S", timeLoc);
	sprintf(string, "%s\t%s", timeStr, text);
	fwrite(string, strlen(string), 1, fd);
	fclose(fd);
}

char ** getSplitString(char* a_str, const char a_delim) {
	char ** result	  = 0;
	size_t count	  = 0;
	char * tmp		  = a_str;
	char * last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;

	/* Count how many elements will be extracted. */
	while (*tmp) {
		if (a_delim == *tmp) {
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

	/* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);

	/* Add space for terminating null string so caller
	   knows where the list of returned strings ends. */
	count++;

	result = malloc(sizeof(char*) * count);

	if (result) {
		size_t idx  = 0;
		char* token = strtok(a_str, delim);

		while (token) {
			assert(idx < count);
			*(result + idx++) = strdup(token);
			token = strtok(0, delim);
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}

	return result;
}

void getTimestamp(int * moy, int * mSec) {
	struct timeval tv;
	time_t timeVal;
	struct tm * timeLocal;
	
	time(&timeVal);
	timeLocal = localtime(&timeVal);
	
	gettimeofday(&tv, NULL);
	
	*mSec = (tv.tv_usec / 1000) + (timeLocal->tm_sec * 1000);
	
	*moy  = (24 * 60 * timeLocal->tm_yday) +
			(60 * timeLocal->tm_hour) + timeLocal->tm_min;
}

void get100thMicroDegree(int elevation, int latitude, double * microDegLat, double * microDegLong) {
	// Radiant für 1 cm
	double rad	  = asin(100000000.0/(WGS84_RAD + elevation * 10));
	// Umrechnung in 1/100 µGrad
	*microDegLat  = ((360/(2*M_PI)) * rad);
	// Längengrad-Wertigkeit wird zum Äquator größer -> Anpassung der Wertigkeit
	*microDegLong = *microDegLat / cos((2*M_PI/360.0)*(latitude/10000000.0));
}
