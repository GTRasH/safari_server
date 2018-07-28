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

int main(void) {
	int (*func) (char *, clientStruct *);

	int clientID, serverID, res;
	char logText[LOG_BUF];
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
				client = setClientStruct((unsigned int)getpid());
				sprintf(logText, "[%u]   Client process started with pid "
						"%u\n", client->pid, client->pid);
				setLogText(logText, LOG_CLIENT);
				
				if (setClientInit(sockClient, client))
					printf("Client Initialisierung fehlgeschlagen!");
				else {
					printf(	"SAFARI-Dienst für User %s gestartet!\n"
							"Bestätigte Dienste %i\n"
							"Client Position latitude = %i | longitude = %i\n"
							, client->name, client->serviceMask
							, client->pos.latitude, client->pos.longitude);

					printf("Getting client-message-queue... ");
					clientID = msgget(IPC_PRIVATE, PERM | IPC_CREAT);
					if (clientID < 0)
						printf("Failed\n");
					else
						printf("Done\n");

					printf("Getting server-message-queue... ");
					serverID = msgget(KEY, 0);
					if (serverID < 0)
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
					
					sprintf(logText, "[%s]   Message queue registration done - SAFARI started\n",
							client->name);
					setLogText(logText, LOG_CLIENT);
					
					func = setClientResponse;
					
					while (1) {
						res = msgrcv(clientID, &s2c, MSQ_LEN, 0, IPC_NOWAIT);
						if (res != -1) {
							if (getClientResponse(sockClient, MAX_RUN, func, s2c.message, client)) {
								sprintf(logText, "[%s]   Client responses %i times with invalid data\n",
										client->name, MAX_RUN);
								setLogText(logText, LOG_CLIENT);
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
				sprintf(logText, "[%u]   Client process terminated",
						client->pid);
				setLogText(logText, LOG_CLIENT);
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
