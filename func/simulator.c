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
				xmlListElement * new = malloc(sizeof(xmlListElement));
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
	char * reply;
	// String erzeugen
	xmlDocDumpMemory(doc, &xmlBuffer, &bufferSize);
	// Nachricht verschicken
	printf("Nachrichtlänge: %lu\n", (long unsigned) bufferSize);
	setSocketContent(msgSocket, (char *) xmlBuffer, (long unsigned) bufferSize);
	xmlFree(xmlBuffer);
	// Warte auf Bestätigung vom Message Manager (blockierender Aufruf!)
	reply = getSocketContent(msgSocket);
	free(reply);
}
