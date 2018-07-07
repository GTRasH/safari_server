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
#include <message_manager.h>

int main (int argc, char **argv) {
	// Deklaration
	int socketFD, response;
	char ** msgId;
	MYSQL * con;
	xmlDocPtr message;
	intersectGeo ** geoTable;
	// Initialsierung
	con		 = sqlConnect("archive");
	geoTable = getGeoTable(con);
	socketFD = getClientUDS();
	// Nachrichtenverarbeitung bis Simulator "quit" schickt oder abendiert
	do {
		message	= getMessage(socketFD, &response);
		msgId	= getNodeValue(message, "//messageId");
		if (msgId == NULL) {
			xmlFreeDoc(message);
			return error("Error: Message Manager beendet\n"
						 "Sendeschema (... -> Nachrichtengröße -> Nachricht -> ...) nicht eingehalten\n");
		}
		// messageId auswerten und weitere Verarbeitung triggern
		switch ((int)strtol(msgId[0], NULL, 10)) {
			case 18:	switch (processMAP(message, con, geoTable)) {
							case 0: printf("MAP-Nachricht erfolgreich verarbeitet\n");
									break;
							case 1:	printf("Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			case 19:	switch (processSPAT(message, geoTable)) {
							case 0: printf("SPaT-Nachricht erfolgreich verarbeitet\n");
									break;
							case 1:	printf("Error: SPaT-Nachricht ohne IntersectionState-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			default:	xmlFreeDoc(message);
						return error("Error: Nachricht mit unbekannter DSRCmsgID erhalten");
		}
		xmlFreeDoc(message);
		freeArray(msgId);
		
	} while (response > 0);
	
	freeGeoTable(geoTable);
	mysql_close(con);
	close(socketFD);
	
	return EXIT_SUCCESS;
}
