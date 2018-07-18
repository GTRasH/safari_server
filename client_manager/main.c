/*	#########################################################
 * 	#		HfTL SAFARI PROTOTYP - CLIENT MANAGER			#
 * 	#														#
 * 	#	Author:	Sebastian Neutsch							#
 * 	#	Mail:	sebastian.neutsch@t-online.de				#
 * 	#
 * 	#
 * 	#########################################################
*/
#include <basic.h>
#include <socket.h>
#include <xml.h>
#include <sql.h>
#include <msg_queue.h>
#include <client_manager.h>

int main(void) {
//	int (*func) (char *, clientStruct *);

	unsigned int * seqSMS;
	socket_t sockServer, sockClient;
	clientStruct * client;
	sockServer = getSocket(AF_INET, SOCK_STREAM, 0);
	setSocketBind(&sockServer, INADDR_ANY, PORT);
	setSocketListen(&sockServer);
	
	mySignal(SIGCHLD, SIG_IGN);
	
	while (1) {
		int pid;
		
		printf("Warte auf Verbindungsaufbau eines Clients ... \n");
		
		setSocketAccept(&sockServer, &sockClient);

		pid = fork();
		switch (pid) {
			case 0:
				client = malloc(sizeof(clientStruct));
				printf("Client verbunden mit Socket %d und pid %d\n", sockClient, getpid());
				
				if (setClientInit(sockClient, client))
					printf("Client Initialisierung fehlgeschlagen!");
				else {
					printf(	"SAFARI-Dienst für User %s gestartet!\n"
							"Client Position latitude = %i | longitude = %i\n"
							, client->name, client->pos.latitude, client->pos.longitude);
							
					seqSMS = getSeqSMS(SEQ_SPAT);
					printf("Seqnum = %ui\n", *seqSMS);
				}

				printf("Client Socket %d beendet\r\n", sockClient);
				fflush(stdout);
				close(sockClient);
				exit(EXIT_SUCCESS);
				break;
			case -1:
				setError("Error while fork() : ", 0);
				break;
			default:
				close(sockClient);
				break;
		}
	}
	return EXIT_SUCCESS;
}
