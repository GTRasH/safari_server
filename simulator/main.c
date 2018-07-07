/*	#########################################################
 * 	#			HfTL SAFARI PROTOTYP - SIMULATOR			#
 * 	#														#
 * 	#	Author:	Sebastian Neutsch							#
 * 	#	Mail:	sebastian.neutsch@t-online.de				#
 * 	#
 * 	#
 * 	#########################################################
*/

#include <basic.h>
#include <socket.h>
#include <simulator.h>

int main (void) {
	// Deklarationen
	xmlListHead * mapHead, * spatHead;
	xmlListElement * mapElement, * spatElement;
	int createSocket, msgSocket;
	
	socklen_t addrlen;
	struct sockaddr_un address;
	
	// # # # Laden der Nachrichten # # #
	fprintf(stdout, "# # # Nachrichten werden eingelesen # # #\n");
	mapHead = getxmlptrlist("./../xml/map/");
	if (mapHead->first->ptr == NULL)
		return error("Error: Unable to load MAP messages\n");

	spatHead = getxmlptrlist("./../xml/spat/");
	if (spatHead->first->ptr == NULL)
		return error("Error: Unable to load SPaT messages\n");
	
	fprintf(stdout, "- Einlesevorgang erfolgreich\n"
					"  Es werden %lu MAP- und %lu SPaT-Nachrichten verarbeitet\n\n",
					mapHead->size, spatHead->size);
	// # # # Nachrichten erfolgreich geladen # # #
	
	// # # # Socket aufbauen, binden und auf connect warten # # #
	fprintf(stdout, "# # # Nachrichten-Server wird gestartet # # #\n");
	
	createSocket = getServerUDS(&address, &addrlen);
	
	if (bind(createSocket, (struct sockaddr *) &address, sizeof(address)) != 0)
		return error("Error: Unable to bind socket\n");

	listen(createSocket, 1);
	fprintf(stdout, "- Server wartet auf Verbindung vom Message Manager\n");
	// # # # Simulator wartet nun auf connect # # #
	

	while((msgSocket = accept(createSocket, (struct sockaddr *) &address, &addrlen)) >= 0) {
		fprintf(stdout, "- Message Manager verbunden\n");
	
//		do {
			// SPaT Nachrichten senden
			spatElement = spatHead->first;
			while (spatElement->ptr != NULL) {
				setNodeValue(spatElement->ptr, "//timeStamp", int2string(time(NULL)));
				sendMessage(msgSocket, spatElement->ptr);
				spatElement = spatElement->next;
				sleep(1);
			}
			// MAP Nachrichten senden
			mapElement = mapHead->first;
			while (mapElement->ptr != NULL) {
				setNodeValue(mapElement->ptr, "//timeStamp", int2string(time(NULL)));
				sendMessage(msgSocket, mapElement->ptr);
				mapElement = mapElement->next;
				sleep(1);
			}
			char *buffer = "quit";
			send(msgSocket, buffer, MSG_BUF, 0);
        
//		} while (strcmp(xmlbuff, "complete"));
	}
 	close (createSocket);
	return EXIT_SUCCESS;
}
