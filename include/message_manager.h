
#include <sys/shm.h>

#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>
#include <msq.h>

#define MAX_HASH 100

#define INTER_AREA 3000

/** \brief	Abhängig vom Node-Type werden n Segment-Teile berechnet */
typedef enum segParts {
	err	= 0,
	xy1	= 5,
	xy2	= 10,
	xy3	= 20,
	xy4	= 40,
	xy5	= 80,
	xy6	= 300
} segParts;

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

/** \brief	Liefert die Anzahl der Segmente in welche ein Node-Segment unterteilt wird
 * 
 * \param[in] xmlDocNode	zu untersuchender NodeXY
 * 
 * \return segParts
*/
segParts getSegmentParts(xmlDocPtr xmlNodeDoc);

void setSegments(MYSQL * dbCon, uint16_t region, uint16_t id, int laneID, 
				 segParts slices, int nodeWidth, int nodeLong, int nodeLat, 
				 int offsetX, int offsetY);


msqList * msqListAdd(int i, msqList * clients);

msqList * msqListRemove(int i, msqList * clients);

msqList * setMsqClients(int serverID, msqList * clients);
