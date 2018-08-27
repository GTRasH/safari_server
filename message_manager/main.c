/*	#########################################################
 * 	#		HfTL SAFARI PROTOTYP - MESSAGE MANAGER			#
 * 	#														#
 * 	#	Author:	Sebastian Neutsch							#
 * 	#	Mail:	sebastian.neutsch@t-online.de				#
 * 	#														#
 * 	#########################################################
*/
#include <basic.h>
#include <sql.h>
#include <xml.h>
#include <socket.h>
#include <msq.h>

#include <message_manager.h>

int main (int argc, char * argv[]) {
	int socketFD, serverID;
	char ** msgId;
	uint8_t test = 0;
	char * startParamError = "Ungültige Testoption, bitte mit -t={time|map|type} starten\n";
	xmlDocPtr message;
	msqList * clients = NULL;
	printf("# # # Message Manager gestartet # # #\n\n");
	// Auswertung der Testoption wenn ein Startparameter übergeben wurde
	if (argc > 1) {
		char ** startParam = getSplitString(argv[1], '=');
		if (startParam[1] == NULL)
			printf("%s", startParamError);
		else {
			if (strcmp(startParam[0],"-t") != 0)
				printf("%s", startParamError);
			else {
				if (strcmp(startParam[1], "time") == 0)
					test = 1;
				else if (strcmp(startParam[1], "map") == 0)
					test = 2;
				else if (strcmp(startParam[1], "type") == 0)
					test = 4;
				else
					printf("%s", startParamError);
			}
		}
		freeArray(startParam);
	}
	// # # # Verbindungsaufbau zum Simulator (Microservice S3) # # #
	printf("# # # Verbindungsaufbau zum Simulator # # #\n");
	if ((socketFD = getClientUDS()) != -1)
		printf("- Message Manager verarbeitet Nachrichten vom Simulator ...\n");
	else {
		printf("- Fehler beim Verbindungsaufbau zum Simulator! Bitte erst den Simulator starten.\n");
		return EXIT_FAILURE;
	}
	
	if (test > 0)
		printf("\n# # # Test-Protokoll # # #\n");
		
	// # # # Message-Queue für Client_Manager Registrierungen (Microservice S4) # # #
	serverID = msgget(KEY, PERM  | IPC_CREAT);
	// # # # Nachrichtenverarbeitung bis Simulator abendiert (Microservice S5) # # #
	while (1) {
		// Client-Registrierungen bzw. -Deregistrierungen verarbeiten
		clients = setMsqClients(serverID, clients);
		// # # # Nachricht vom Simulator empfangen (Microservice S6) # # #
		message = getMessage(socketFD);
		if (message == NULL) {
			printf ("\n# # # Message Manager beendet # # #\n"
					"- Fehler beim Empfangen einer Nachricht vom Simulator\n");
			break;
		}
		if (!xmlContains(message, "//messageId")) {
			printf ("Error: Nachricht ohne DSRCid erhalten - Überspringe Nachricht\n");
			continue;
		}
		msgId = getNodeValue(message, "//messageId");
		// # # # messageId auswerten und weitere Verarbeitung triggern (Microservice S8) # # #
		switch ((int)strtol(msgId[0], NULL, 10)) {
			// # # #   Microservice S9   # # #
			case 18:	switch (processMAP(message, clients, test)) {
							case 0: if (test & 4)
										fprintf(stdout, "MAP Nachricht eingegangen\n");
									break;
							case 1:	fprintf(stderr, "Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			// # # #   Microservice S10   # # #
			case 19:	switch (processSPAT(message, clients, test)) {
							case 0: if (test & 4)
										fprintf(stdout, "SPaT Nachricht eingegangen\n");
									break;
							case 1:	fprintf(stderr, "Error: SPaT-Nachricht ohne IntersectionState-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			default:	xmlFreeDoc(message);
						fprintf(stderr,"Error: Nachricht mit unbekannter DSRCmsgID erhalten");
		}
		xmlFreeDoc(message);
		freeArray(msgId);
		// Nachricht verarbeitet -> Simulator informieren
		setSocketContent(socketFD, MSG_OK, strlen(MSG_OK));
	}
	// Löschen der Message-Queue
	msgctl(serverID, IPC_RMID, NULL);
	close(socketFD);
	
	return EXIT_SUCCESS;
}
