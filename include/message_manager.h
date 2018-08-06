
#include <sys/shm.h>
#include <math.h>
#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>
#include <msq.h>

/** \brief	Kreuzungsbereich in cm gemessen ab dem Mittelpunkt */
#define INTER_AREA 30000

/** \brief	Mittlerer Erdradius nach WGS-84 in cm */
#define WGS84_RAD 637100080

/** \brief	Breite der Teilstücke eines Lane-Segments in cm */
#define SEG_PART 200

/** \brief	Teilstücke die ab der Stopp-Linie übersprungen werden */
#define SEG_SKIP 2

/** \brief Element für MAP-Nachrichten in Hash-Table */
typedef struct intersectGeo {
	struct intersectGeo *next;
	uint32_t referenceID;
	int timestamp;
	char * xml;
} intersectGeo;

typedef struct {
	long prio;
	char message[MSQ_LEN];
} msqElement;

typedef struct msqList {
	int id;
	struct msqList *next;
} msqList;

/** \brief	Empfang von Nachrichten aus dem SIM_SOCK und Deserialisierung
 * 
 * \param[in] createSocket	Socket
 * 
 * \return	xmlDocPtr auf xmlDoc einer Nachricht
 */
xmlDocPtr getMessage(int sock);

/** \brief	Verarbeitet MAP-Nachrichten
 * 			Aktualisiert die MAP-DB sowie die (MAP)Hash-Table
 * 
 * \param[in] message	xmlDocPtr auf map:MessageFrame
 * \param[in] con		MYSQL Connect
 * \param[io] mapTable	Hash-Table
 * 
 * \return	xmlDocPtr auf xmlDoc einer Nachricht
 */
int processMAP(xmlDocPtr message, msqList * clients, uint8_t test);

/** \brief	Verarbeitet SPaT-Nachrichten
 * 			Selektiert Intersection
 * 
 * \param[in] message	xmlDocPtr auf spat:MessageFrame
 * \param[in] mapTable	Hash-Table
 * 
 * \return	xmlDocPtr auf xmlDoc einer Nachricht
 */
int processSPAT(xmlDocPtr message, msqList * clients, uint8_t test);

/** \brief	Berechnet aus der Regulator- und IntersectionID (jeweils 16 Bit)
 * 			die 32 Bit IntersectionReferenceID zur Verwendung als Hash-Schlüssel
 * 
 * \param[in] Pointer auf das XML-Element
 * 
 * \return	IntersectionReferenceID
 */
uint32_t getReferenceID(xmlDocPtr xmlDoc);

/** \brief	Berechnet 1/100 µGrad in Abhängigkeit der Höhe
 * 
 * \param[in] elevation	Geographische Höhe der Kreuzung
 * 
 * \return 1/100 µGrad
*/
double get100thMicroDegree(int elevation);

void setSegments(MYSQL * dbCon, uint16_t region, uint16_t id, uint8_t laneID, 
				uint8_t nodeID, double microDegree, uint16_t nodeWidth, uint8_t skip,
				int nodeLong, int nodeLat, int16_t offsetX, int16_t offsetY);


msqList * msqListAdd(int i, msqList * clients);

msqList * msqListRemove(int i, msqList * clients);

msqList * setMsqClients(int serverID, msqList * clients);
