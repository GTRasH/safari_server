#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/shm.h>

#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>
#include <msq.h>

#define REQ_SERV	LIB_SAFARI"xml/client_manager/req_serv.xml"
#define REQ_AUTH	LIB_SAFARI"xml/client_manager/req_auth.xml"
#define REQ_LOC 	LIB_SAFARI"xml/client_manager/req_loc.xml"

/** \brief	Maximale Anforderungen zur Authentifizierung */
#define MAX_LOGIN	10

/** \brief	Maximale Anforderungen für Services (während ClintInit) */
#define MAX_SERV	10

/** \brief	Maximale Anforderungen für Standortdaten (während ClientInit) */
#define MAX_LOC		10

/** \brief	Maximale Anforderungen für Standort- oder Servicedaten */
#define MAX_RUN		5

/** \brief	Maximale Einträge in der HashTable */
#define MAX_HASH	20

/** \brief	Anzahl der Nachrichten in der Message-Queue */
#define MAX_MSG		10

/** \brief	Ablaufzeit für SPaT-Nachrichten in ms */
#define SPAT_OBSOLETE 1000

/** \brief	Eine Minute in ms */
#define MINUTE 60000

/** \brief	Strings zum bilden der SAFARI-Nachricht für den Client */
#define SAFARI_START "<SAFARI>\n"
#define SAFARI_END "</SAFARI>\n"

/** \brief	Strings zum bilden des SPaT-Teils der SAFARI-Nachricht */
#define SPAT_START " <SPAT>\n  <intersections>\n  "
						 
#define SPAT_END  "  </intersections>\n </SPAT>\n"

#define SPAT_BODY "  <IntersectionState>\n    <enabledLanes>\n     <LaneID></LaneID>\n\
    </enabledLanes>\n    <states>\n     <MovementState>\n\
      <state-time-speed>\n       <MovementEvent>\n        <timing>\n\
         <likelyTime></likelyTime>\n        </timing>\n        <speeds>\n\
         <AdvisorySpeed>\n           <type><greenwave>true</greenwave></type>\n\
          <speed></speed>\n         </AdvisorySpeed>\n        </speeds>\n\
       </MovementEvent>\n      </state-time-speed>\n     </MovementState>\n\
    </states>\n   </IntersectionState>\n"
    
/** \brief	Strings zum bilden des MAP-Teils der SAFARI-Nachricht */
#define MAP_START " <MAP>\n  <intersections>\n   <IntersectionGeometry>\n    <laneSet>\n"

#define MAP_END "    </laneSet>\n   </IntersectionGeometry>\n  </intersections>\n </MAP>\n"

#define MAP_BODY "     <GenericLane>\n      <laneID></laneID>\n      <maneuvers></maneuvers>\n\
     </GenericLane>\n"
    
/** \brief	Intervallgrenzen */
typedef struct {
	int maxLong;
	int maxLat;
	int minLong;
	int minLat;
} delimeters;

/** \brief	Geoposition */
typedef struct {
	int latitude;
	int longitude;
} location;

/** \brief	Lane-Segment
 *	\param 	borders		Intervallgrenzen des Segments
 *	\param 	next		Pointer auf das nächste Segment
 * */
typedef struct segStruct {
	delimeters			borders;
	struct segStruct	* next;
} segStruct;

/** \brief	Lane-Abschnitte
 * 
 *	\param 	pos			Geo-Koordinate des Referenzpunktes
 *	\param 	dist		Entfernung zur Stopp-Linie (in 0.1 m)
 *	\param 	segments	Pointer auf die Segmente des Teilabschnittes
 *	\param 	next		Pointer auf den nächsten Teilabschnitt
 * 
 * */
typedef struct partStruct {
	uint16_t			maxSpeed;
	uint16_t			distance;
	location			pos;
	segStruct			* segments;
	struct partStruct	* next;
} partStruct;

/** \brief	Lane (Local Service Area)
 * 
 *	\param laneID	Eindeutige Identifizierung einer Lane
 *	\param segments	Liste der Lane-Segmente
 * 	\param next		Zeiger auf nächste Lane
 * */
typedef struct laneStruct {
	uint8_t				laneID;
	uint16_t			maneuvers;
	partStruct			* parts;
	struct laneStruct	* next;
} laneStruct;

/** \brief	Intersection (Global Service Area) */
typedef struct interStruct {
	uint32_t 			refID;
	int					elevation;
	delimeters 			borders;
	laneStruct 			* lanes;
	struct interStruct	* next;
} interStruct;

/** \brief	Signal-Handler Pointer */
typedef void (*sighandler_t)(int);

/** \brief	Signal-Handler zum Beenden von Kindprozessen
 * 
 * \param[in]	intNr			z.B. SIGCHLD
 * \param[in]	signalHandler	z.B. SIG_IGN
 * 
 *  \return sighandler_t
 */
sighandler_t mySignal(int sigNr, sighandler_t signalHandler);

/** \brief	Service-Definition (2er Potenzen) */
typedef enum service {
	unreg	= 1,	// Client von Message-Queue abmelden
	glosa	= 2,	// GLOSA
	udef1	= 4,
	udef2	= 8,
	udef3	= 16,
	udef4	= 32,
	udef5	= 64,
	empty	= 128	// Client hat ausschließlcih Dienste angefordert,
					// die am Aufenthaltsort nicht angeboten werden!
} Service;

/** \brief	Fortbewegungsmittel (Berechnung der Update-Intervalle) */
typedef enum moveType {
	unkown,
	motor,
	bike,
	feet
} moveType;

/** \brief	Maximale Geschwindigkeit eines Fortbewegungsmittels in 0.1m/s */
typedef enum speedType {
	speedUnkown	= 500,
	speedMotor	= 139,	// 50 km/h
	speedBike	= 70,	// 25 km/h
	speedFeet	= 22	// 8 km/h
} speedType;

/** \brief	Nachrichten-Element der Message Queue */
typedef struct {
	long prio;
	char message[MSQ_LEN];
} msqElement;

/** \brief	Zeitpunkt der letzten Positions-Aktualisierung */
typedef struct {
	int moy;
	int mSec;
	int timeGap;
} timeStruct;

/** \brief	Struktur für die Daten des Clients */
typedef struct {
	char 		 name[50];
	location 	 pos;
	timeStruct	 update;
	uint16_t 	 region;
	uint8_t 	 serviceMask;
	unsigned int pid;
	moveType	 type;
	speedType	 maxSpeed;
} clientStruct;

/** \brief	Init Client bestehend aus Authentifizierung, 
 * 			Service- und Standortabfrage
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * \param[in]	client		Client-Daten
 * 
 * \return	0 wenn OK, sonst 1
 */
int setClientInit(int sock, clientStruct * client);

/** \brief	Sendet eine Nachricht an den Client und verarbeitet das Ergebnis
 * 			in der übergebenen Funktion
 * 
 * \param[in]	sock		Socket des jeweiligen Client-Prozesses
 * \param[in]	retries		Anzahl der Neuversuche bei fehlerhafter Antwort
 * \param[in]	func		Funktionspointer zur Verarbeitung der Antwort
 * \param[in]	message		Nachricht an den Client
 * \param[in]	client		Client-Struktur zum Speichern der Daten
 * 
 * \return	0 wenn OK, sonst 1
 */
int getClientResponse(	int sock, int retries, 
						int (* func) (char *, clientStruct *), 
						char * message, clientStruct * client);

/** \brief User-Daten aus einer Nachricht extrahieren und gegen DB prüfen
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * \param[in]	msg			Zu verarbeitende Nachricht
 * 
 * \return	> 0 wenn NOK, 0 sonst
 */
int setClientAuth(char * msg, clientStruct * client);

/** \brief Lokations-Daten aus einer Nachricht extrahieren und in der Client-Struktur speichern
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * \param[in]	msg			Zu verarbeitende Nachricht
 * 
 * \return	> 0 wenn NOK, 0 sonst
 */
int setClientLocation(char * msg, clientStruct * client);

/** \brief Services aus einer Nachricht extrahieren und in der User-Struktur speichern
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * \param[in]	msg			Zu verarbeitende Nachricht
 * 
 * \return	> 0 wenn NOK, 0 sonst
 */
int setClientServices(char * msg, clientStruct * client);

/** \brief 	Erkennt eine Client-Antwort als Lokations- oder Service-Response
 * 			und ruft die jeweilige Funktion auf
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * \param[in]	msg			Zu verarbeitende Nachricht
 * 
 * \return	> 0 wenn NOK, 0 sonst
 */
int setClientResponse(char * msg, clientStruct * client);

/** \brief 	Initialisiert die Client-Struktur
 * 
 * \param[in]	pid	   Prozess-ID
  * 
 * \return	clientStruct wenn OK, 0 sonst
 */
clientStruct * setClientStruct(unsigned int pid);

/** \brief 	Bestätigte Client-Services in lesbarer Form (Logging)
 * 
 * \param[in]	servID	Siehe clientStruct \param serviceMask
 * 
 * \return	String der Services
 */
char * getServiceName(uint8_t servID);

/** \brief 	Liefert eine Hash-Table aller Intersections eines Anbieters
 * 
 * \param[in]	region	Anbieter-ID
 * 
 * \return	Hash-Table wenn OK, sonst NULL
 */
interStruct ** getInterStructTable(uint16_t region);

/** \brief 	Initialisiert eine interStruct inkl. Abfrage der Lanes und Lane-Segmente
 * 
 * \return	void
 */
interStruct * getInterStruct(MYSQL * db, uint16_t region, uint16_t id);

/** \brief 	Setzt Region- und Intersection-ID zur IntersectionReferenceID
 * 			zusammen und berechnet den hash
 * 
 * \param[in]	region	Dienstanbieter / Verkehrsbetrieb
 * \param[in]	id		KreuzungsID
 * \param[out]	refID	1. 16 Bit -> Anbieter / 2. 16 Bit -> Kreuzung
 * \param[out]	hash	Berechneter Hash
 * 
 * \return	void
 */
void getHash(uint16_t region, uint16_t id, uint32_t * refID, uint8_t * hash);

/** \brief 	Vergleicht Client-Position mit den Segmenten einer Lane (Local Service Area)
 * 			und gibt bei einem Treffer eine Nachricht zurück
 * 
 * \param[in]	interTable	HashTable aller Intersections eines Anbieters
 * \param[in]	msg			Nachricht vom Message Manager
 * \param[in]	client		Client-Daten
 * 
 * \return Nachricht wenn Treffer, sonst NULL
 */
char * getClientMessage(interStruct ** interTable, char * msg, clientStruct * client);

/** \brief	Aktualisiert bestimmte Intersections in der HashTable
 * 
 * \param[in]	interTable	HashTable aller Intersections eines Anbieters
 * \param[in]	regions		Region- bzw- Anbieter-IDs
 * \param[in]	ids			IntersectionIDs
 *
 */
void setInterUpdate(interStruct ** interTable, char ** regions, char ** ids);

/** \brief	Gibt den gesammten Speicher der Intersection-HashTable frei
 * 
 * \param[in]	interTable	HashTable aller Intersections eines Anbieters
 *
 */
void freeInterTable(interStruct ** table);

/** \brief	Gibt den Speicher eines Intersection-Elements in der HashTable frei
 * 
 * \param[in]	inter	Intersection-Element
 *
 */
void freeInterStruct(interStruct * inter);

/** \brief	Vergleicht die letzte Aktualisierung der Lokationsdaten mit der 
 * 			Systemzeit in Abhängigkeit des Fortbewegungsmittels
 * 
 * \param[in]	client	Client-Daten
 *
 * \return 1 wenn neue Lokationsdaten benötigt werden, sonst 0
 */
uint8_t updateRequired(clientStruct * client);

/** \brief	Vergleicht den Zeitstempel eine SPaT Nachricht mit der Systemzeit
 * 
 * \param[in]	moy		Minute of the year
 * \param[in]	mSec	ms der Minute
 *
 * \return 1 wenn Nachricht zu alt, sonst 0
 */
uint8_t spatObsolete(int moy, int mSec);

/** \brief	Berechnet den Abstand der übergebenen Werte in der Systemzeit in ms
 * 
 * \param[in]	moy		Minute of the year
 * \param[in]	mSec	ms der Minute
 *
 * \return	Abstand in ms
 */
int getTimeGap(int moy, int mSec);

/** \brief	Liefert ein IntersectionState-Element mit Geschwindigkeitsempfehlung
 * 
 * \param[in]	state		IntersectionState
 * \param[in]	laneID		laneID
 * \param[in]	client		Client-Daten
 * \param[in]	part		Lane-Abschnitt
 * \param[in]	elevation	Höhe der Kreuzung
 * \param[in]	moy			Nachrichtenzeitpunkt - Minute of the year
 * \param[in]	mSec		Nachrichtenzeitpunkt - ms der Minute
 *
 * \return	IntersectionState wenn OK, sonst NULL
 */
char * getState(char * state, uint8_t laneID, clientStruct * client, 
				partStruct * part, int elevation, int moy, int mSec);

/** \brief	Berechnet die Geschwindigkeitsempfehlung in 0.1m/s
 * 			Es wird keine zulässige Höchstgeschwindigkeiten berücksichtigt!
 * 			-> Hier ist Optimierungspotenzial
 * 
 * \param[in]	client		Client-Daten
 * \param[in]	dist		Errechnete Entfernung zur Kreuzung
 * \param[in]	timeLeft	Restzeit der Ampelphase
 * 
 * \return	Geschwindigkeitsempfehlung wenn OK, sonst 500
 */
int getSpeedAdvise(clientStruct * client, double dist, double timeLeft, int maxSpeed);

/** \brief	Liefert den Map-Teil einer Safari-Nachricht
 * 
 * \param[in]	lane	Lane-Daten
 * 
 * \return char *
 */
char * getMapBody(laneStruct * lane);
