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

#define MAX_LOGIN 10
#define MAX_SERV 10
#define MAX_LOC 10
#define MAX_RUN 5
#define MAX_HASH 20

#define SPAT_TAG_START "<SPAT>\n"
#define SPAT_TAG_END "\n</SPAT>"


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

/** \brief	Segment einer Local Service Area */
typedef struct segStruct {
	delimeters borders;
	struct segStruct * next;
} segStruct;

/** \brief	Local Service Area (Fahrspur) */
typedef struct laneStruct {
	location pos;
	int laneID;
	segStruct * segments;
	struct laneStruct * next;
} laneStruct;

/** \brief	Global Service Area (Kreuzungsbereich) */
typedef struct interStruct {
	unsigned int refID;
	delimeters borders;
	laneStruct * lanes;
	struct interStruct * next;
} interStruct;

typedef void (*sighandler_t)(int);

sighandler_t mySignal(int sigNr, sighandler_t signalHandler);

void noZombie(int sigNr);

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

typedef enum moveType {
	unkown,
	feet,
	bike,
	motor
} moveType;

typedef struct {
	long prio;
	char message[MSQ_LEN];
} msqElement;

typedef struct {
	char name[50];
	location pos;
	uint16_t region;
	uint8_t serviceMask;
	unsigned int pid;
	moveType type;
	
} clientStruct;

/** \brief	Init Client bestehend aus Authentifizierung, 
 * 			Service- und Standortabfrage
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * 
 * \return	void
 */
int setClientInit(int sock, clientStruct * client);

/** \brief Authentifizierungs-Vorgang
 * 
 * \param[in]	clientSock	Socket des jeweiligen Client-Prozesses
 * 
 * \return	void
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

/** \brief Lokations-Daten aus einer Nachricht extrahieren und in der User-Struktur speichern
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

clientStruct * setClientStruct(unsigned int pid);

char * getServiceName(uint8_t servID);

/** \brief 	Liefert eine Hash-Table aller Intersections eines Anbieters
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
void getHash(uint16_t region, uint16_t id, unsigned int * refID, uint8_t * hash);

char * getClientMessage(interStruct ** interTable, char * msg, clientStruct * client);

void setInterUpdate(interStruct ** interTable, char ** regions, char ** ids);

void freeInterTable(interStruct ** table);

void freeInterStruct(interStruct * inter);
