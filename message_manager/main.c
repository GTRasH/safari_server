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

int main (int argc, char **argv) {
	// Deklaration
	int socketFD, serverID, clientID, count = 0;
	msqElement c2s;
	char ** msgId;
	MYSQL * con;
	xmlDocPtr message;
	msqList * clients;
	intersectGeo ** geoTable;
	// Initialsierung
	clients	 = NULL;
	con		 = sqlConnect("safari");	// DB-Connect
	geoTable = getGeoTable(con);		// IntersectionGeometry-HashTable
	
	if ((socketFD = getClientUDS()) == -1) {
		printf("Fehler beim Verbindungsaufbau zum Simulator!\n");
		return EXIT_FAILURE;
	}
	
	serverID = msgget(KEY, PERM  | IPC_CREAT);	// Message Queue Client-Reg
	
	// Nachrichtenverarbeitung bis Simulator "quit" schickt oder abendiert
	while (1) {
		// Message-Queue-Registrierungen abarbeiten
		while (msgrcv(serverID, &c2s, MSQ_LEN, 0, IPC_NOWAIT) != -1) {
			// Deregistrierung
			if (c2s.prio == 1 )
			{
				sscanf(c2s.message,"%d",&clientID);
				clients = msqListRemove(clientID, clients);
			}
			// Registrierung für SPaT
			else if (c2s.prio == 2)
			{
				sscanf(c2s.message,"%d",&clientID);
				clients = msqListAdd(clientID, clients);
			}
		}
		
		if ((message = getMessage(socketFD)) == NULL) {
			printf ("Error: Message Manager beendet - "
					"Fehler beim Empfangen einer Nachricht vom Simulator\n");
			break;
		}
		
		if ((msgId = getNodeValue(message, "//messageId")) == NULL) {
			xmlFreeDoc(message);
			printf ("Error: Message Manager beendet - "
					"Nachricht ohne DSRCid erhalten\n");
			break;
		}
		
		// messageId auswerten und weitere Verarbeitung triggern
		switch ((int)strtol(msgId[0], NULL, 10)) {
			case 18:	switch (processMAP(message, con, geoTable)) {
							case 0: fprintf(stdout, "MAP-Nachricht verarbeitet\n");
									break;
							case 1:	fprintf(stderr, "Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			case 19:	switch (processSPAT(message, geoTable, clients)) {
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

	freeGeoTable(geoTable);
	mysql_close(con);
	close(socketFD);
	
	return EXIT_SUCCESS;
}
