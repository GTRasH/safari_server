#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define MSG_BUF 1024
#define UDS_FILE "/tmp/safari_sim.sock"

#include <basic.h>

/** \brief Liefert einen Unix Domain Socket für den Client-Prozess
 * 
 * \param[in]	void
 * 
 * \return	int (sock_addr) wenn OK, 0 sonst exit(1)
 */
int getClientUDS(void);


/** \brief Liefert einen Unix Domain Socket für den Client-Prozess
 * 
 * \param[out]	struct 
 * 
 * \return	int (sock_addr) wenn OK, 0 sonst exit(1)
 */
 int getServerUDS(struct sockaddr_un * address, socklen_t * addrlen);
