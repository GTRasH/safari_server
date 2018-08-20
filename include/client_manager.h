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

/** \brief	Anfangs- und End-Tag einer SPaT Nachricht */
#define SPAT_TAG_START	"<SPAT>\n"
#define SPAT_TAG_END	"\n</SPAT>"

/** \brief	Ablaufzeit für SPaT-Nachrichten in ms */
#define SPAT_OBSOLETE 1000

/** \brief	Eine Minute in ms */
#define MINUTE 60000

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

/** \brief	Lane-Segment */
typedef struct segStruct {
	delimeters			borders;
	struct segStruct	* next;
} segStruct;

/** \brief	Lane (Local Service Area)
 * 
 *	\param pos		Geo-Koordinate der Stopp-Linie
 *	\param laneID	Eindeutige Identifizierung einer Lane
 *	\param segments	Liste der Lane-Segmente
 * 	\param next		Zeiger auf nächste Lane
 * */
typedef struct laneStruct {
	location 			pos;
	uint8_t				laneID;
	segStruct			* segments;
	struct laneStruct	* next;
} laneStruct;

/** \brief	Intersection (Global Service Area) */
typedef struct interStruct {
	uint32_t 			refID;
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
