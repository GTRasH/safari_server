/* simulator.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#include <list.h>
#include <func.h>

xmlDocPtr getdoc(char *docname);

xmlDocListHead *getxmlptrlist(char *pathname);

int sendMessage(int msgSocket, xmlDocPtr doc);

int getMinuteOfTheYear(void);

int main (void) {
	// Deklarationen
	xmlDocListHead *mapHead;
	xmlDocListHead *spatHead;
	xmlDocListElement *mapElement;
	xmlDocListElement *spatElement;
	int createSocket, msgSocket;
	socklen_t addrlen;
	struct sockaddr_un address;
	
	// # # # Laden der Nachrichten # # #
	fprintf(stdout, "# # # Nachrichten werden eingelesen # # #\n");
	mapHead = getxmlptrlist("./../xml/map/");
	if (mapHead->first->ptr == NULL)
		return error("Error: Unable to load MAP messages\n");

	spatHead = getxmlptrlist("./../xml/spat/");
	if (spatHead->first->ptr == NULL)
		return error("Error: Unable to load SPaT messages\n");
	
	fprintf(stdout, "- Einlesevorgang erfolgreich\n"
					"  Es werden %lu MAP- und %lu SPaT-Nachrichten verarbeitet\n\n",
					mapHead->size, spatHead->size);
	// # # # Nachrichten erfolgreich geladen # # #
	
	// # # # Socket aufbauen, binden und auf connect warten # # #
	fprintf(stdout, "# # # Nachrichten-Server wird gestartet # # #\n");
	
	
	if((createSocket = socket(AF_LOCAL, SOCK_STREAM, 0)) <= 0)
		return error("Error: Unable to create socket\n");
	
	addrlen = sizeof(struct sockaddr_in);
	unlink(SIM_FILE);
	address.sun_family = AF_LOCAL;
	strcpy(address.sun_path, SIM_FILE);
	
	if (bind(createSocket, (struct sockaddr *) &address, sizeof(address)) != 0)
		return error("Error: Unable to bind socket\n");

	listen(createSocket, 1);
	fprintf(stdout, "- Server wartet auf Verbindung vom Message Manager\n");
	// # # # Simulator wartet nun auf connect # # #
	
//	while (1) {
		msgSocket = accept(createSocket, (struct sockaddr *) &address, &addrlen);
		if (msgSocket > 0)
			fprintf(stdout, "- Message Manager verbunden\n");
	
//		do {
			// SPaT Nachrichten senden
			spatElement = spatHead->first;
			while (spatElement->ptr != NULL) {
				setNodeValue(spatElement->ptr, "//timeStamp", int2string(time(NULL)));
				sendMessage(msgSocket, spatElement->ptr);
				spatElement = spatElement->next;
				sleep(1);
			}
			// MAP Nachrichten senden
			mapElement = mapHead->first;
			while (mapElement->ptr != NULL) {
				setNodeValue(mapElement->ptr, "//timeStamp", int2string(time(NULL)));
				sendMessage(msgSocket, mapElement->ptr);
				mapElement = mapElement->next;
				sleep(1);
			}
			char *buffer = "quit";
			send(msgSocket, buffer, BUF, 0);
        
//		} while (strcmp(xmlbuff, "complete"));
//	}
 	close (createSocket);
	return EXIT_SUCCESS;
}

/** \brief Erzeugt eine Liste mit Pointern der xml-Dateien im übergebenen Pfad
 * 
 * \param[in] *pathname = Nachrichtenverzeichnis (SPaT oder MAP / universal)
 * 
 * \return	xmlDocListHead->xmlDocListElement->ptr != NULL
 * 			xmlDocListHead->xmlDocListElement->ptr = NULL im Fehlerfall
 * 
 */
xmlDocListHead *getxmlptrlist(char *pathname) {
	// Initialisierung Verzeichnis-Funktion
	DIR *dir;
	struct dirent *entry;
	char docname[256];
	// Initialisierung Liste
	xmlDocListElement *cur = malloc(sizeof(xmlDocListElement));
	xmlDocListHead *head = malloc(sizeof(xmlDocListHead));
	head->first = cur;
	head->size	= 0;
	cur->next = NULL;
	cur->ptr  = NULL;
	// Aufbau der Liste mit XML-Pointern der jeweiligen Nachrichten-Datei
	if ((dir = opendir(pathname)) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type == DT_REG) {
				xmlDocListElement *new = malloc(sizeof(xmlDocListElement));
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

/** \brief Erzeugt eine Liste mit Pointern der xml-Dateien im übergebenen Pfad
 * 
 * \param[in] *docname = Dateipfad
 * 
 * \return	xmlDocPtr
 * 			NULL im Fehlerfall
 */
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
	send(msgSocket, size, BUF, 0);
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
