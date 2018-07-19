#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>
// #include <msg_queue.h>

#define REQ_SERV "./../xml/client_manager/req_serv.xml"
#define REQ_AUTH "./../xml/client_manager/req_auth.xml"
#define REQ_LOC "./../xml/client_manager/req_loc.xml"

typedef void (*sighandler_t)(int);

sighandler_t mySignal(int sigNr, sighandler_t signalHandler);

void noZombie(int sigNr);

typedef struct {
	int latitude;
	int longitude;
} location;

typedef struct {
	char * name;
	location pos;
	unsigned int services;
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
