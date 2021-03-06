/*	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#																			#
 * 	#				HfTL SAFARI PROTOTYP - SOCKET HEADER						#
 * 	#																			#
 * 	#	Author:		Sebastian Neutsch											#
 * 	#	Mail:		sebastian.neutsch@t-online.de								#
 *  #	Copyright:	2018 Sebastian Neutsch / Deutsche Telekom AG				#
 * 	#																			#
 * 	#																			#
 * 	#	This file is part of safari_server.										#
 *	#																			#
 *	#	safari_server is free software: you can redistribute it and/or modify	#
 *	#	it under the terms of the GNU General Public License as published by	#
 *	#	the Free Software Foundation, either version 3 of the License, or		#
 *	#	(at your option) any later version.										#
 *	#																			#
 *	#	safari_server is distributed in the hope that it will be useful,		#
 *	#	but WITHOUT ANY WARRANTY; without even the implied warranty of			#
 *	#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			#
 *	#	GNU General Public License for more details.							#
 *	#																			#
 *	#	You should have received a copy of the GNU General Public License		#
 *	#	along with safari_server. If not, see <https://www.gnu.org/licenses/>.	#
 *	#																			#
 * 	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
*/
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
#include <xml.h>

/** \brief	Größe des Nachrichten Buffer */
#define MSG_BUF 1024

/** \brief	Maximale Größe einer Nachrichten (128 kB) */
#define MSG_MAX 131072

/** \brief	Bestätigung Message Manager -> Simulator */
#define MSG_OK "ok"

/** \brief	Wird gesendet, wenn die Nachrichtenlänge einem Vielfachen von
			MSG_BUF entspricht -> Signalisiert dem Empfänger das Ende */
#define MSG_EMPTY "\n"

/** \brief	Datei für Unix Domain Socket */
#define UDS_FILE "/tmp/safari_sim.sock"

/** \brief	TCP-Port für Verbindungen zum Client Manager */
#define PORT 15000

/** \brief	Socket Typ */
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

/** \brief Verbindungsaufbau zum Server - Nur für den lokalen Client (TESTING!)
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

/** \brief	Nachricht über einen Socket senden
 * 
 * \param[in]	sock	Socket
 * \param[in]	message	Zu versendende Nachricht
 * \param[in]	length	Nachrichten-Länge
 * 
 */
void setSocketContent(socket_t sock, char * message, long unsigned length);

/** \brief	Liest einen Socket aus und liefert den gesammten Inhalt als String
 * 
 * \param[in]	sock	Socket
 * 
 * \return	char * wenn OK, sonst NULL
 */
char * getSocketContent(socket_t sock);
