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
	int socketFD, serverID, clientID, response, DSRCid;
	msqElement c2s;
	char ** msgId;
	MYSQL * con;
	xmlDocPtr message;
	intersectGeo ** geoTable;
	// Initialsierung
	con		 = sqlConnect("safari");	// DB-Connect
	geoTable = getGeoTable(con);		// IntersectionGeometry-HashTable
	socketFD = getClientUDS();
	
	msqList * clients = NULL;
	msqList * clientsPtr;
	
	serverID = msgget(KEY, PERM  | IPC_CREAT);	// Message Queue Client-Reg
	
	// Nachrichtenverarbeitung bis Simulator "quit" schickt oder abendiert
	do {
		// Message-Queue-Registrierungen abarbeiten
		while (msgrcv(serverID, &c2s, MSQ_LEN, 0, IPC_NOWAIT) != -1) {
			// Deregistrierung
			if (c2s.prio == 1 )
			{
				printf("Löschanforderung für die SPaT-Message-Queue empfangen\n");
				sscanf(c2s.message,"%d",&clientID);
				clients = msqListRemove(clientID, clients);
			}
			// Registrierung für SPaT
			else if (c2s.prio == 2)
			{
				printf("Registrierungsanforderung für SPaT-Message-Queue empfangen\n");
				sscanf(c2s.message,"%d",&clientID);
				clients = msqListAdd(clientID, clients);
			}
			if (clients != NULL) {
				printf("Client_Manager-Kinder auf der SPaT-Message-Queue\n");
				clientsPtr = clients;
				while (clientsPtr != NULL) {
					printf("client-msq-id = %i\n", clientsPtr->id);
					clientsPtr = clientsPtr->next;
				}
			}
		}
		
		message	= getMessage(socketFD, &response);
		msgId	= getNodeValue(message, "//messageId");
		if (msgId == NULL) {
			xmlFreeDoc(message);
			setError("Error: Message Manager beendet\n"
					"Sendeschema (... -> Nachrichtengröße -> Nachricht -> ...) nicht eingehalten\n", 1);
		}
		
		DSRCid = (int)strtol(msgId[0], NULL, 10);
		
		// messageId auswerten und weitere Verarbeitung triggern
		switch (DSRCid) {
			case 18:	switch (processMAP(message, con, geoTable)) {
							case 0: fprintf(stdout, "MAP-Nachricht verarbeitet\n");
									break;
							case 1:	fprintf(stderr, "Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			case 19:	switch (processSPAT(message, geoTable, clients)) {
							case 0: fprintf(stdout, "SPaT-Nachricht verarbeitet\n");
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
		
	} while (response > 0);
	
	freeGeoTable(geoTable);
	mysql_close(con);
	close(socketFD);
	
	return EXIT_SUCCESS;
}
