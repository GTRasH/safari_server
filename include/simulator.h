#include <time.h>
#include <dirent.h>

#include <basic.h>
#include <socket.h>
#include <xml.h>

#define MAP_PATH "./../xml/map/"
#define SPAT_PATH "./../xml/spat/"

/** \brief Listen-Element für SPat- und MAP-Nachrichten (Lesevorgang) */
typedef struct xmlListElement {
	struct xmlListElement *next;
	xmlDocPtr ptr;
} xmlListElement;

/** \brief Listen-Kopf für einfach verkettete Nachrichten-Liste */
typedef struct {
	xmlListElement *first;
	size_t size;
} xmlListHead;

/** \brief Erzeugt eine Liste mit Pointern der xml-Dateien im übergebenen Pfad
 * 
 * \param[in] *pathname = Nachrichtenverzeichnis (SPaT oder MAP / universal)
 * 
 * \return	xmlDocListHead->xmlDocListElement->ptr != NULL
 * 			xmlDocListHead->xmlDocListElement->ptr = NULL im Fehlerfall
 */
xmlListHead *getxmlptrlist(char *pathname);

void sendMessage(int msgSocket, xmlDocPtr doc);

int getMinuteOfTheYear(void);
