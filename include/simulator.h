#include <dirent.h>

#include <basic.h>
#include <socket.h>

/** \brief	Pfad für MAP-Nachrichten */
#define MAP_PATH	LIB_SAFARI"xml/map/"

/** \brief	Pfad für SPaT-Nachrichten */
#define SPAT_PATH 	LIB_SAFARI"xml/spat/"

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
 * \param[in] pathname	Nachrichtenverzeichnis (SPaT oder MAP / universal)
 * 
 * \return	xmlDocListHead->xmlDocListElement->ptr wenn OK, sonst NULL
 */
xmlListHead *getxmlptrlist(char *pathname);

/** \brief	Speicher einer Nachrichten-Liste freigeben
 * 
 * \param[in] head	Pointer auf das Header-Element
 *
*/
void freeList(xmlListHead * head);

/** \brief	String aus dem XML-Dokument erzeugen und an Socket senden
 * 
 * \param[in]	doc			XML-Dokument
 * \param[in]	msgSocket	Unix Domain Socket
 *
*/
void setMessage(int msgSocket, xmlDocPtr doc);
