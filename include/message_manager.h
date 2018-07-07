#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>

#define MAX_HASH 100

/** \brief Element für MAP-Nachrichten in Hash-Table */
typedef struct intersectGeo {
	struct intersectGeo *next;
	uint32_t referenceID;
	int timestamp;
	char * xml;
} intersectGeo;

/** \brief	Empfang von Nachrichten aus dem SIM_SOCK und Deserialisierung
 * 
 * \param[in] createSocket	Socket
 * \param[out] response		Rückgabewert zur Prozesssteuerung
 * 
 * \return	xmlDocPtr auf xmlDoc einer Nachricht
 */
xmlDocPtr getMessage(int createSocket, int *response);

/** \brief	Verarbeitet MAP-Nachrichten
 * 			Aktualisiert die MAP-DB sowie die (MAP)Hash-Table
 * 
 * \param[in] message	xmlDocPtr auf map:MessageFrame
 * \param[in] con		MYSQL Connect
 * \param[io] mapTable	Hash-Table
 * 
 * \return	xmlDocPtr auf xmlDoc einer Nachricht
 */
int processMAP(xmlDocPtr message, MYSQL *con, intersectGeo ** mapTable);

/** \brief	Verarbeitet SPaT-Nachrichten
 * 			Selektiert Intersection
 * 
 * \param[in] message	xmlDocPtr auf spat:MessageFrame
 * \param[in] mapTable	Hash-Table
 * 
 * \return	xmlDocPtr auf xmlDoc einer Nachricht
 */
int processSPAT(xmlDocPtr message, intersectGeo ** mapTable);

/** \brief	Berechnet aus der Regulator- und IntersectionID (jeweils 16 Bit)
 * 			die 32 Bit IntersectionReferenceID zur Verwendung als Hash-Schlüssel
 * 
 * \param[in] Pointer auf das XML-Element
 * 
 * \return	IntersectionReferenceID
 */
uint32_t getReferenceID(xmlDocPtr xmlDoc);

/** \brief	Liefert ein String-Array. Jeder String ist ein Baum ab 'tag'.
 * 
 * \param[in] message	Pointer auf ein XML-Dokument
 * \param[in] tag		zu suchender Tag-Name (XPath-Expression)
 * 
 * \return	String
 */
char ** getTree(xmlDocPtr message, char * tag);

/** \brief	Ergänzt die Strings im Array mit <?xml version="1.0"?>
 * 
 * \param[in] trees		Bäume
 * 
 * \return	XML-String (beginnend mit <?xml version="1.0"?>)
 */
char ** getWellFormedXML(char ** trees);

/** \brief	Generiert ein intersectionGeometry-Element für die Hash-Table
 * 
 * \param[in] refID		IntersectionReferenceID
 * \param[in] timestamp	MinuteOfTheYear
 * \param[in] xml		IntersectionGeometry als wohlgformter XML-String
 * 
 * \return	intersectionGeometry-Element
 */
intersectGeo * getNewGeoElement(uint32_t refID, char * timestamp, char * xml);

/** \brief	Sucht ein intersectionGeometry-Element in der Hash-Table mittels refID
 * 
 * \param[in] refID		IntersectionReferenceID
 * 
 * \return	xmlDocPtr, wenn Element gefunden
 * 			NULL, sonst
 */
xmlDocPtr getGeoElement(intersectGeo ** table, uint32_t refID);

/** \brief	Einmaliger Aufruf beim Start des Message Manager
 * 			Abfrage der MAP-Informationen auf der DB
 * 
 * \param[in] con	MYSQL connect
 * 
 * \return	intersectionGeometry Hash-Table
 */
intersectGeo ** getGeoTable(MYSQL *con);

/** \brief	Sucht eine Geometry-Element anhand der refID in der Hash-Table
 * 			Aktualisiert den Eintrag wenn vorhanden oder legt einen neuen an
 * 
 * \param[io] table		intersectionGeometry Hash-Table
 * \param[in] refID		IntersectionReferenceID
 * \param[in] timestamp	MinuteOfTheYear
 * \param[in] xml		IntersectionGeometry als wohlgformter XML-String
 * 
 * \return	void
 */
void setGeoElement(intersectGeo ** table, uint32_t refID, char * timestamp, char * xml);

/** \brief	Hash-Funktion
 * 
 * \param[in] refID		IntersectionReferenceID
 * 
 * \return	int Hash
 */
int getHash(uint32_t refID);

/** \brief Speicherfreigabe der IntersectionGeometry-Table
 * 
 * \param[in] ** hashTable	= zu leerende Hash
  * 
 * \return	void
 */
void freeGeoTable(intersectGeo ** hashTable);
