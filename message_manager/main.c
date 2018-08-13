/*	#########################################################
 * 	#		HfTL SAFARI PROTOTYP - MESSAGE MANAGER			#
 * 	#														#
 * 	#	Author:	Sebastian Neutsch							#
 * 	#	Mail:	sebastian.neutsch@t-online.de				#
 * 	#
 * 	#
 * 	#########################################################
*/
#include <basic.h>
#include <sql.h>
#include <xml.h>
#include <socket.h>
#include <msq.h>

#include <message_manager.h>

int main (int argc, char * argv[]) {
	int socketFD, serverID, count = 0;
	char ** msgId;
	uint8_t test = 0;
	char * startParamError = "Ungültige Testoption, bitte mit --test={time|map} starten\n";
	xmlDocPtr message;
	msqList * clients = NULL;
	// Auswertung der Testoption wenn ein Startparameter übergeben wurde
	if (argc > 1) {
		char ** startParam = getSplitString(argv[1], '=');
		if (startParam[1] == NULL)
			printf("%s", startParamError);
		else {
			if (strcmp(startParam[0],"--test") != 0)
				printf("%s", startParamError);
			else {
				if (strcmp(startParam[1], "time") == 0)
					test = 1;
				else if (strcmp(startParam[1], "map") == 0)
					test = 2;
				else
					printf("%s", startParamError);
			}
		}
		freeArray(startParam);
	}
	// Verbindungsaufbau zum Simulator
	if ((socketFD = getClientUDS()) == -1) {
		printf("Fehler beim Verbindungsaufbau zum Simulator! Bitte erst den Simulator starten.\n");
		return EXIT_FAILURE;
	}
	// Message-Queue für Client_Manager Registrierungen einrichten
	serverID = msgget(KEY, PERM  | IPC_CREAT);
	// Nachrichtenverarbeitung bis Simulator abendiert
	while (1) {
		// Client-Registrierungen bzw. -Deregistrierungen verarbeiten
		clients = setMsqClients(serverID, clients);
		// Nachricht vom Simulator empfangen
		if ((message = getMessage(socketFD)) == NULL) {
			printf ("Error: Message Manager beendet - "
					"Fehler beim Empfangen einer Nachricht vom Simulator\n");
			break;
		}
		if (!xmlContains(message, "//messageId")) {
			printf ("Error: Nachricht ohne DSRCid erhalten - Überspringe Nachricht\n");
			continue;
		}
		msgId = getNodeValue(message, "//messageId");
		// messageId auswerten und weitere Verarbeitung triggern
		switch ((int)strtol(msgId[0], NULL, 10)) {
			case 18:	switch (processMAP(message, clients, test)) {
							case 0: fprintf(stdout, "MAP-Nachricht verarbeitet\n");
									break;
							case 1:	fprintf(stderr, "Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			case 19:	switch (processSPAT(message, clients, test)) {
							case 0: fprintf(stdout, "SPaT-Nachricht # %i verarbeitet\n", ++count);
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
	}
	
	if (msgctl(serverID, IPC_RMID, NULL) == -1)
		setError("Error while deleting msq", 0);
	else
		printf("MSQ gelöscht\n");

	close(socketFD);
	
	return EXIT_SUCCESS;
}
