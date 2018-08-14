/*	#########################################################
 * 	#		HfTL SAFARI PROTOTYP - CLIENT MANAGER			#
 * 	#														#
 * 	#	Author:	Sebastian Neutsch							#
 * 	#	Mail:	sebastian.neutsch@t-online.de				#
 * 	#														#
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
	char logText[LOG_BUF], * clientMsg;
	socket_t sockServer, sockClient;
	clientStruct * client;
	interStruct ** interTable;
	msqElement c2s, s2c;
	
	// Socket einrichten
	sockServer = getSocket(AF_INET, SOCK_STREAM, 0);
	setSocketBind(&sockServer, INADDR_ANY, PORT);
	setSocketListen(&sockServer);
	// SignalHandler registrieren
	mySignal(SIGCHLD, SIG_IGN);
	// Server-Schleife - setSocketAccept blockiert!
	while (1) {
		int pid;
		printf("Warte auf Verbindungsaufbau eines Clients ... \n");
		setSocketAccept(&sockServer, &sockClient);
		pid = fork();
		switch (pid) {
			// Client-(Kind)prozess
			case 0:
				// Client-Struktur initialisieren
				client = setClientStruct((unsigned int)getpid());
				sprintf(logText, "[%u]   Client process started with pid "
						"%u\n", client->pid, client->pid);
				setLogText(logText, LOG_CLIENT);
				// Anmelderoutine (Authentifizierung, Service- und Standortanforderung)
				if (setClientInit(sockClient, client))
					printf("Client Initialisierung fehlgeschlagen!");
				else {
					printf(	"SAFARI-Dienst für User %s gestartet!\n"
							"Bestätigte Dienste %i\n"
							"Client Position latitude = %i | longitude = %i\n",
							client->name, client->serviceMask,
							client->pos.latitude, client->pos.longitude);
					// Intersection-Daten der Client-Region (Verkehrsdienst) laden
					if ((interTable = getInterStructTable(client->region)) == NULL) {
						sprintf(logText, 
								"[%u]   Client process terminated due to intersection-table error",
								client->pid);
						setLogText(logText, LOG_CLIENT);
						freeInterTable(interTable);
						fflush(stdout);
						close(sockClient);
						exit(EXIT_FAILURE);
					}
					// Einrichtung der Message-Queue für Nachrichten vom Message Manager
					printf("Getting client-message-queue... ");
					if ((clientID = msgget(IPC_PRIVATE, PERM | IPC_CREAT)) < 0)
						printf("Failed\n");
					else
						printf("Done\n");
					// Einrichten der Message-Queue für Nachrichten an den Message Manager
					printf("Getting server-message-queue... ");
					if ((serverID = msgget(KEY, 0)) < 0)
						printf("Failed\n");
					else
						printf("Done\n");
					c2s.prio = 2;
					sprintf (c2s.message, "%d", clientID);
					// Registrierungsnachricht (eigene MSQ ID) an den Message Manager
					res = msgsnd (serverID, &c2s, MSQ_LEN, 0);
					printf("Sending message queue registration... ");
					if (res == -1)
						printf("Failed\n");
					else
						printf("Done\n");
					
					sprintf(logText,"[%s]   Message queue registration done - SAFARI started\n",
									client->name);
					setLogText(logText, LOG_CLIENT);
					// Nachrichten vom Message-Manager verarbeiten
					while (1) {
						res = msgrcv(clientID, &s2c, MSQ_LEN, 0, IPC_NOWAIT);
						if (res != -1) {
							// Verarbeitung der Nachricht vom Message Manager
							clientMsg = getClientMessage(interTable, s2c.message, client);
							// Client befindet sich auf einer Lane
							if (clientMsg != NULL) {
								func = setClientResponse;
								// Sendet die Nachricht und erwartet ein Standort- oder Service-Update
								if (getClientResponse(sockClient, MAX_RUN, func, clientMsg, client)) {
									sprintf(logText, "[%s]   Client responses %i times with invalid data\n",
											client->name, MAX_RUN);
									setLogText(logText, LOG_CLIENT);
									free(clientMsg);
									break;
								}
								free(clientMsg);
							}
							// Neue Standortdaten benötigt
							else if (updateRequired(client)) {
								clientMsg = getFileContent(REQ_LOC);
								func	  = setClientLocation;
								// Sendet eine Standortanforderung
								if (getClientResponse(sockClient, MAX_RUN, func, clientMsg, client)) {
									sprintf(logText, "[%s]   Client responses %i times with invalid location\n",
											client->name, MAX_RUN);
									setLogText(logText, LOG_CLIENT);
									free(clientMsg);
									break;
								}
								free(clientMsg);
							}
						}
						// System V Message-Queues bieten kein Notify, deshalb Polling
						usleep(10000);
					}
					// Deregistrierung senden
					c2s.prio = 1;
					sprintf (c2s.message, "%d", clientID);
					msgsnd (serverID, &c2s, MSQ_LEN, 0);
					freeInterTable(interTable);
				}
				// Client-Prozess beenden
				sprintf(logText, "[%u]   Client process terminated", client->pid);
				setLogText(logText, LOG_CLIENT);
				fflush(stdout);
				close(sockClient);
				exit(EXIT_SUCCESS);
				break;

			case -1:
				sprintf(logText, "Unable to fork() !");
				setLogText(logText, LOG_SERVER);
				break;

			default:
				close(sockClient);
				break;
		}
	}
	return EXIT_SUCCESS;
}
