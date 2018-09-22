/*	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#																			#
 * 	#				HfTL SAFARI PROTOTYP - SIMULATOR FUNCTIONS					#
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
#include <simulator.h>

static int compare(const void * a, const void * b) {
	return strcmp(*(const char **) a, *(const char **) b);
}

xmlListHead * getxmlptrlist(char *pathname) {
	DIR *dir;
	struct dirent * entry;
	char docname[256];
	int fileCount = 0, i = 0;
	xmlListElement * cur;
	xmlListHead * head;
	// Initialisierung Liste
	cur			= malloc(sizeof(xmlListElement));
	head		= malloc(sizeof(xmlListHead));
	head->first = cur;
	head->size	= 0;
	cur->next	= NULL;
	cur->ptr 	= NULL;
	// Aufbau der Liste mit XML-Pointern der jeweiligen Nachrichten-Datei
	if ((dir = opendir(pathname)) != NULL) {
		while ((entry = readdir(dir)) != NULL)
			if (entry->d_type == DT_REG)
				fileCount++;
		closedir(dir);
	}
	
	char ** files;

	files = malloc(fileCount * sizeof(char *));
	
	
	if ((dir = opendir(pathname)) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type == DT_REG) {		
				
				snprintf(docname, sizeof(docname), 
						"%s%s", pathname, entry->d_name);
				
				files[i] = malloc(strlen(docname)+1);
				
				strcpy(files[i], docname);
				strcat(files[i], TERM_NULL);
				i++;
				
			}
		}
		closedir(dir);
	}
	qsort(files, fileCount, sizeof(files[0]), compare);

	for (i = 0; i < fileCount; i++) {
		xmlListElement * new = malloc(sizeof(xmlListElement));
		new->next = NULL;
		new->ptr  = NULL;
		cur->next = new;
		cur->ptr  = getdoc(files[i]);
		cur = new;
		head->size++;
		free(files[i]);
	}
	free(files);
	return head;
}

void freeList(xmlListHead * head) {
	xmlListElement * ptr, * tmp;
	ptr = head->first;
	while (ptr != NULL) {
		xmlFreeDoc(ptr->ptr);
		tmp = ptr;
		ptr = ptr->next;
		free(tmp);
	}
	free(head);
}

void setMessage(int msgSocket, xmlDocPtr doc) {
	int bufferSize;
	xmlChar *xmlBuffer;
	char * reply;
	// String erzeugen
	xmlDocDumpMemory(doc, &xmlBuffer, &bufferSize);
	// Nachricht verschicken
	setSocketContent(msgSocket, (char *) xmlBuffer, (long unsigned) bufferSize);
	xmlFree(xmlBuffer);
	// Warte auf Bestätigung vom Message Manager (blockierender Aufruf!)
	reply = getSocketContent(msgSocket);
	free(reply);
}
