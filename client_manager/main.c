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
#include <msq.h>
#include <client_manager.h>

typedef struct {
	long prio;
	char message[MSQ_LEN];
} msqElement;

int main(void) {
	int (*func) (char *, clientStruct *);

	int clientID, serverID, res;

	socket_t sockServer, sockClient;
	clientStruct * client;
	sockServer = getSocket(AF_INET, SOCK_STREAM, 0);
	setSocketBind(&sockServer, INADDR_ANY, PORT);
	setSocketListen(&sockServer);
	
	msqElement c2s, s2c;
	
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

					printf("Getting server-message-queue... ");
					serverID = msgget(KEY, 0);
					if (serverID < 0)
						printf("Failed\n");
					else
						printf("Done\n");
					
					printf("Getting client-message-queue... ");
					clientID = msgget(IPC_PRIVATE, PERM | IPC_CREAT);
					if (clientID < 0)
						printf("Failed\n");
					else
						printf("Done\n");
					
					c2s.prio = 2;
					
					sprintf (c2s.message, "%d", clientID);
					
					res = msgsnd (serverID, &c2s, MSQ_LEN, 0);
					
					printf("Sending message queue registration... ");
					if (res == -1)
						printf("Failed\n");
					else
						printf("Done\n");
					
					func = setClientResponse;
					
					while (end) {
						res = msgrcv(clientID, &s2c, MSQ_LEN, 0, IPC_NOWAIT);
						if (res != -1) {
							
							if (getClientResponse(sockClient, 10, func, s2c.message, client)) {
								printf("Client hat nicht mit Standort oder Services geantwortet\n");
								break;
							}
						}
						usleep(10000);
					}
					// Deregistrierung senden
					c2s.prio = 1;
					sprintf (c2s.message, "%d", clientID);
					msgsnd (serverID, &c2s, MSQ_LEN, 0);
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
