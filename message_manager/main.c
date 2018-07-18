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
#include <msg_queue.h>
#include <message_manager.h>

int main (int argc, char **argv) {
	// Deklaration
	int socketFD, response;
	unsigned int * seqSpat;
	char ** msgId;
	MYSQL * con;
	xmlDocPtr message;
	intersectGeo ** geoTable;
	// Initialsierung
	con		 = sqlConnect("safari");
	seqSpat	 = setSeqSMS(SEQ_SPAT);
	geoTable = getGeoTable(con);
	socketFD = getClientUDS();
	// Nachrichtenverarbeitung bis Simulator "quit" schickt oder abendiert
	do {
		message	= getMessage(socketFD, &response);
		msgId	= getNodeValue(message, "//messageId");
		if (msgId == NULL) {
			xmlFreeDoc(message);
			setError("Error: Message Manager beendet\n"
					"Sendeschema (... -> Nachrichtengröße -> Nachricht -> ...) nicht eingehalten\n", 1);
		}
		// messageId auswerten und weitere Verarbeitung triggern
		switch ((int)strtol(msgId[0], NULL, 10)) {
			case 18:	switch (processMAP(message, con, geoTable)) {
							case 0: fprintf(stdout, "MAP-Nachricht erfolgreich verarbeitet\n");
									break;
							case 1:	fprintf(stderr, "Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			case 19:	switch (processSPAT(message, geoTable)) {
							case 0: fprintf(stdout, "SPaT-Nachricht erfolgreich verarbeitet\n");
									*seqSpat = 123;
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
