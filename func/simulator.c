#include <simulator.h>

xmlListHead *getxmlptrlist(char *pathname) {
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
				snprintf(docname, sizeof(docname), "%s%s", pathname, entry->d_name);
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

xmlDocPtr getdoc(char *docname) {
	xmlDocPtr doc;
	doc = xmlParseFile(docname);
	if (doc == NULL) {
		fprintf(stderr, "Fehler beim Einlesen der XML-Datei!\n");
		return NULL;
	}
	return doc;
}

int sendMessage(int msgSocket, xmlDocPtr doc) {
	int bufferSize, sentBytes;
	xmlChar *xmlBuffer;
	
	xmlDocDumpMemory(doc, &xmlBuffer, &bufferSize);
	char *size	= int2string(bufferSize);
	
	// Größe der Nachricht senden
	send(msgSocket, size, MSG_BUF, 0);
	// Nachricht senden
	xmlBuffer[bufferSize] = '\0';
	sentBytes = send(msgSocket, xmlBuffer, bufferSize, 0);
	free(xmlBuffer);
	if (sentBytes != bufferSize)
		return error("Nachricht unvollständig gesendet\n");
	else {
		printf("Nachricht versendet!\n");
		return 1;
	}
}
