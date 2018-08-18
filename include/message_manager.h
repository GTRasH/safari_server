
#include <sys/shm.h>
#include <math.h>
#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>
#include <msq.h>

/** \brief	Kreuzungsbereich in m gemessen ab dem Mittelpunkt */
#define INTER_AREA 300

/** \brief	Mittlerer Erdradius nach WGS-84 in cm */
#define WGS84_RAD 637100080

/** \brief	Ungefähre Breite der Teilstücke eines Lane-Segments in cm.
			Der konkrete Wert richtet sich nach der Länge des Segments */
#define SEG_PART 200

/** \brief	Teilstücke die ab der Stopp-Linie übersprungen werden */
#define SEG_SKIP 0

/** \brief	Teilstücke die an den Beginn eines Folge-Segments gesetzt werden */
#define SEG_ADD 0

/** \brief Element für MAP-Nachrichten in Hash-Table */
typedef struct intersectGeo {
	struct intersectGeo *next;
	uint32_t referenceID;
	int timestamp;
	char * xml;
} intersectGeo;

/** \brief	Nachrichtenstruktur für Message-Queue-Elemente */
typedef struct {
	long prio;
	char message[MSQ_LEN];
} msqElement;

/** \brief	Listen-Element für Message-Queue-Clients */
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

/** \brief	Bindet die übergebenen Parameter zum Einfügen der Lane-Segmente
 * 
 * \param[in/out]	bind		MYSQL_BIND Pointer
 * \param[in/out]	region		Anbieter ID
 * \param[in/out]	id			Intersection ID
 * \param[in/out]	laneID		Lane-ID
 * \param[in/out]	segID		Segment-ID
 * \param[in/out]	maxLong		Obergrenze Längengrad
 * \param[in/out]	maxLat		Obergrenze Breitengrad
 * \param[in/out]	minLong		Untergrenze Längengrad
 * \param[in/out]	minLat		Untergrenze Breitengrad
 *
*/
void setInsertParam(MYSQL_BIND * bind, uint16_t * region, uint16_t * id, 
					uint8_t * laneID, uint8_t * segID, int * maxLong, 
					int * maxLat, int * minLong, int * minLat);

/** \brief	Berechnet die Lane-Segment und fügt sie in die segments-Table ein
 * 
 * \param[in]	stmt		MYSQL_STMT - Vorbeitetes Update
 * \param[in]	segID		Segment-ID
 * \param[in]	maxLong		Obergrenze Längengrad
 * \param[in]	maxLat		Obergrenze Breitengrad
 * \param[in]	minLong		Untergrenze Längengrad
 * \param[in]	minLat		Untergrenze Breitengrad
 * \param[in]	microDeg	1/100 µGrad entsrechend 1cm
 * \param[in]	laneWidth	Breite der Fahrspur
 * \param[in]	skip		Anzahl der zu überspringenden Segmente
 * \param[in]	nodeLong	Referenzpunkt Längengrad
 * \param[in]	nodeLat		Referenzpunkt Breitengrad
 * \param[in]	offsetX		Offset-Wert in X-Richtung (in cm)
 * \param[in]	offsetY		Offset-Wert in Y-Richtung (in cm)
 * 
*/
void setSegments(MYSQL_STMT * stmt, uint8_t * segID, int * maxLong, 
				 int * maxLat, int * minLong, int * minLat, double microDeg, 
				 uint16_t laneWidth, uint8_t skip, int nodeLong, int nodeLat, 
				 int16_t offsetX, int16_t offsetY);

/** \brief	Berechnet die Offset-Werte zur weiteren Berechnung der Segmente
 * 
 * \param[out]	offsetA			Siehe LSA-Algorithmus
 * \param[out]	offsetB			Siehe LSA-Algorithmus
 * \param[out]	offsetC			Siehe LSA-Algorithmus
 * \param[in]	cos				Ausrichtung der Lane
 * \param[in]	degLaneWidth	Lane-Breite in 1/10 µGrad
 * \param[in]	degNodeGap		Abstand der Knotenpunkte in 1/10 µGrad
 * 
*/
void setOffsets(uint8_t * offsetA, uint8_t * offsetB, uint8_t *offsetC,
				double cos, uint16_t degLaneWidth, uint16_t degNodeGap);

/** \brief	Fügt einen Client zur Liste registrierte MSQ-Clients hinzu
 * 
 * \param[in]	i		MSQ-ID des Clients
 * \param[in]	clients	Liste der registrieten Clients
 * 
  \return		neue Liste 
*/
msqList * msqListAdd(int i, msqList * clients);

/** \brief	Entfernt einen Client aus der Liste registrierter MSQ-Clients
 * 
 * \param[in]	i		MSQ-ID des Clients
 * \param[in]	clients	Liste der registrieten Clients
 *
 * \return		neue Liste 
*/
msqList * msqListRemove(int i, msqList * clients);

/** \brief	Verarbeitet Client-Registrierung für die Message-Queue
 * 
 * \param[in]	serverID	MSQ-ID des Servers
 * \param[in]	clients		Liste der registrieten Clients
 * 
* \return		neue Liste
*/
msqList * setMsqClients(int serverID, msqList * clients);
