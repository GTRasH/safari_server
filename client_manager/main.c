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
	int clientID, serverID;
	char logText[LOG_BUF], * clientMsg;
	socket_t sockServer, sockClient;
	clientStruct * client;
	interStruct ** interTable;
	msqElement c2s, s2c;
	
	// # # # Socket einrichten (Microservice S11) # # #
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
		// # # #   Microservice S13   # # #
		pid = fork();
		switch (pid) {
			// Client-(Kind)prozess
			case 0:
				// # # # Client-Struktur initialisieren (Microservice S14) # # #
				client = setClientStruct((unsigned int)getpid());
				sprintf(logText, "[%u]   Client process started with pid "
						"%u\n", client->pid, client->pid);
				setLogText(logText, LOG_CLIENT);
				// # # # Anmelderoutine (Microservice S14 bis S22) - siehe client_manager.c
				if (setClientInit(sockClient, client))
					printf("Client Initialisierung fehlgeschlagen!");
				else {
					// # # # Intersection-Daten der Client-Region laden (Microservice S24) # # #
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
					// # # #   Message Queues einrichten (Microservice S23)   # # #
					// Einrichtung der Message-Queue für Nachrichten vom Message Manager
					clientID = msgget(IPC_PRIVATE, PERM | IPC_CREAT);
					// Einrichten der Message-Queue für Nachrichten an den Message Manager
					serverID = msgget(KEY, 0);
					// Registrierungsnachricht (eigene MSQ ID) an den Message Manager
					memset(c2s.message, 0, sizeof(c2s.message));
					c2s.prio = 2;
					sprintf (c2s.message, "%d", clientID);
					msgsnd (serverID, &c2s, MSQ_LEN, 0);
					// SAFARI für Client gestartet
					sprintf(logText,"[%s]   Message queue registration done - SAFARI started\n",
									client->name);
					setLogText(logText, LOG_CLIENT);
					// Funktions-Pointer zum Verarbeiten des Client-Response
					func = setClientResponse;
					// Nachrichten vom Message-Manager verarbeiten
					while (1) {
						// # # # msgrcv blockiert bis Nachrichten eintreffen (Microservice S25) # # #
						msgrcv(clientID, &s2c, MSQ_LEN, 0, 0);
						// # # # Verarbeitung der Nachricht vom Message Manager (Microservice S26, S27) - siehe client_manager.c
						clientMsg = getClientMessage(interTable, s2c.message, client);
						// # # # CLIENT BEFINDET SICH AUF EINER LANE
						// -> Sendet Nachricht und erwartet Standort- oder Service-Update (Microservice S27 und S32) # # #
						if (clientMsg != NULL) {
							if (getClientResponse(sockClient, MAX_RUN, func, clientMsg, client)) {
								sprintf(logText, "[%s]   Client responses %i times with invalid data\n",
										client->name, MAX_RUN);
								setLogText(logText, LOG_CLIENT);
								free(clientMsg);
								break;
							}
							free(clientMsg);
						}
						// # # # LETZTE STANDORT-AKTUALISIERUNG ZU ALT
						// -> Sendet Nachricht und erwartet Standort- oder Service-Update (Microservice S28 und S32) # # #
						else if (updateRequired(client)) {
							clientMsg = getFileContent(REQ_LOC);
							// # # # Sendet eine Standortanforderung und erwartet Standort- oder Service-Update (Microservice S28 und S32) # # #
							if (getClientResponse(sockClient, MAX_RUN, func, clientMsg, client)) {
								sprintf(logText, "[%s]   Client responses %i times with invalid data\n",
										client->name, MAX_RUN);
								setLogText(logText, LOG_CLIENT);
								free(clientMsg);
								break;
							}
							free(clientMsg);
						}
					}
					// # # # Deregistrierung senden (Microservice S35) # # #
					c2s.prio = 1;
					sprintf (c2s.message, "%d", clientID);
					msgsnd (serverID, &c2s, MSQ_LEN, 0);
					freeInterTable(interTable);
				}
				// # # # Client-Prozess beenden (Microservice S35) # # #
				sprintf(logText, "[%u]   Client process terminated\n", client->pid);
				setLogText(logText, LOG_CLIENT);
				fflush(stdout);
				close(sockClient);
				exit(EXIT_SUCCESS);
				break;

			case -1:
				sprintf(logText, "Unable to fork() !\n");
				setLogText(logText, LOG_SERVER);
				break;

			default:
				close(sockClient);
				break;
		}
	}
	return EXIT_SUCCESS;
}
