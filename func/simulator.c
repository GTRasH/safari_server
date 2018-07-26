#include <simulator.h>

xmlListHead * getxmlptrlist(char *pathname) {
	DIR *dir;
	struct dirent *entry;
	char docname[256];
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
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type == DT_REG) {
				xmlListElement *new = malloc(sizeof(xmlListElement));
				new->next = NULL;
				new->ptr  = NULL;
				snprintf(docname, sizeof(docname), 
						"%s%s", pathname, entry->d_name);
				cur->next = new;
				cur->ptr  = getdoc(docname);
				cur = new;
				head->size++;
			}
		}
		closedir(dir);
	}
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
	xmlDocDumpMemory(doc, &xmlBuffer, &bufferSize);
	setSocketContent(msgSocket, (char *) xmlBuffer, (long unsigned) bufferSize);
	xmlFree(xmlBuffer);
}

void getTimestamp(int * moy, int * mSec) {
	struct timeval tv;
	time_t timeVal;
	struct tm * timeLocal;
	
	time(&timeVal);
	timeLocal = localtime(&timeVal);
	
	gettimeofday(&tv, NULL);
	
	*mSec = (tv.tv_usec / 1000) + (timeLocal->tm_sec * 1000);
	
	*moy = 	(24 * 60 * timeLocal->tm_yday) +
			(60 * timeLocal->tm_hour) + timeLocal->tm_min;
}
