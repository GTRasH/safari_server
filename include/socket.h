#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>

#include <basic.h>

#define MSG_BUF 1024
#define UDS_FILE "/tmp/safari_sim.sock"
#define PORT 15000

#define socket_t int

/** \brief Liefert einen Unix Domain Socket für den Client (Message Manager)
 * 
 * \param[in]	void
 * 
 * \return	int (sock_addr) wenn OK, sonst exit(1)
 */
int getClientUDS(void);


/** \brief Liefert einen Unix Domain Socket für den Server (Simulator)
 * 
 * \param[out]	address		Pointer auf sockaddr_un
 * \param[out]	addrlen		Pointer auf socklen_t
 * 
 * \return	int (sock_addr) wenn OK, sonst exit(1)
 */
int getServerUDS(struct sockaddr_un * address, socklen_t * addrlen);

/** \brief Liefert einen TCP Socket für den Server (Client Manager)
 * 
 * \param[out]	address		Pointer auf sockaddr_un
 * \param[out]	addrlen		Pointer auf socklen_t
 * 
 * \return	int (sock_addr) wenn OK, sonst exit(1)
 */
int getTCPSocket(struct sockaddr_in * address, socklen_t * addrlen);

/** \brief Liefert einen Socket für den Server
 * 
 * \param[in]	af			Address Family (z.B. AF_INET -> IPv4)
 * \param[in]	type		z.B. SOCK_STREAM (TCP), SOCK_DGRAM (UDP) ...
 * \param[in]	protocol	Ggf. eigene Protokolle (0 = Übernahme von type)
 * 
 * \return	int (sock_addr) wenn OK, sonst exit(1)
 */
int getSocket(int af, int type, int protocol);

/** \brief Bindung an die Serveradresse bzw. einen bestimmten Port
 * 
 * \param[in]	sock
 * \param[in]	address
 * \param[in]	port
 * 
 * \return	exit(1) bei Fehler
 */
void setSocketBind(socket_t *sock, unsigned long address, unsigned short port);

/** \brief Der Socket nimmt Verbindungsanforderungen entgegen
 * 
 * \param[in]	sock
 * 
 * \return	exit(1) bei Fehler
 */
void setSocketListen(socket_t *sock );

/** \brief Verbindungsaufbau zum Server
 * 
 * \param[in]	sock
 * \param[in]	serv_addr
 * \param[in]	port
 * 
 * \return	exit(1) bei Fehler
 */
void setSocketConnect(socket_t *sock, char *serv_addr, unsigned short port);

/** \brief Verbindungsanforderungen annehmen - Blockierender Aufruf!
 * 
 * \param[in]	sock
 * \param[in]	newSock
 * 
 * \return	exit(1) bei Fehler
 */
void setSocketAccept(socket_t *sock, socket_t *newSock);

void setSocketContent(socket_t sock, char * message, long unsigned length);

char * getSocketContent(socket_t sock);
