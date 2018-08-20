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

int main (int argc, char * argv[]) {
	xmlListHead * mapHead, * spatHead;
	xmlListElement * mapElement, * spatElement;
	int uds, msgSocket;
	char * strMoy, * strMSec;
	socklen_t addrlen;
	struct sockaddr_un address;
	int period = 1000000;	// µs für 1 Hz
	double tmp;
	int moy, mSec;
	// TESTING
	// int count = 0;
	
	// # # # Ggf. Nachrichtenfrequenz einstellen # # #
	printf("# # # Simulator gestartet # # #\n");
	if (argc == 1)
		printf("- Nachrichtenfrequenz: 1 Hz\n\n");
	else {
		tmp = strtod(argv[1],NULL);
		if (tmp == 0.0 || tmp > 100.0)
			printf ("- max. Nachrichtenfrequenz = 100 Hz ... "
					"Standardfrequenz eingestellt\n\n");
		else {
			period *= (1/tmp);
			printf ("- Nachrichtenfrequenz: %.1f Hz\n\n", tmp);
		}
	}
	// # # # Laden der Nachrichten # # #
	fprintf(stdout, "# # # Nachrichten werden eingelesen # # #\n");
		
	mapHead = getxmlptrlist(MAP_PATH);
	if (mapHead->first->ptr == NULL)
		setError("Error: Unable to load MAP messages\n", 1);

	spatHead = getxmlptrlist(SPAT_PATH);
	if (spatHead->first->ptr == NULL)
		setError("Error: Unable to load SPaT messages\n", 1);
	
	fprintf(stdout, "- Einlesevorgang erfolgreich\n"
					"  Es werden %lu MAP- und %lu SPaT-Nachrichten verarbeitet\n\n",
					mapHead->size, spatHead->size);
	// # # # Nachrichten erfolgreich geladen # # #
	
	// # # # Socket aufbauen, binden und auf connect warten # # #
	fprintf(stdout, "# # # Nachrichten-Server wird gestartet # # #\n");
	
	uds = getServerUDS(&address, &addrlen);
	
	if (bind(uds, (struct sockaddr *) &address, sizeof(address)) != 0)
		setError("Error: Unable to bind socket\n", 1);

	listen(uds, 1);
	fprintf(stdout, "- Server wartet auf Verbindung vom Message Manager\n");
	// # # # Simulator wartet nun auf connect # # #
	

	while((msgSocket = accept(uds, (struct sockaddr *) &address, &addrlen)) >= 0) {
		fprintf(stdout, "- Message Manager verbunden\n");
		while(1) {
			// SPaT Nachrichten senden
			spatElement = spatHead->first;
			while (spatElement->ptr != NULL) {
				// Zeitangabe aktualisieren
				getTimestamp(&moy, &mSec);
				strMoy	= int2string(moy);
				strMSec	= int2string(mSec);
				setNodeValue(spatElement->ptr, "//IntersectionState/moy", strMoy);
				setNodeValue(spatElement->ptr, "//SPAT/timeStamp", strMoy);
				setNodeValue(spatElement->ptr, "//IntersectionState/timeStamp", strMSec);
				free(strMoy);
				free(strMSec);
				
				setMessage(msgSocket, spatElement->ptr);
				// TESTING
				//printf ("SPaT Nachricht # %i versendet. Moy = %i mSec = %i\n",
				//		++count, moy, mSec);
				spatElement = spatElement->next;
				usleep(period);
			}
			// MAP Nachrichten senden
			mapElement = mapHead->first;
			while (mapElement->ptr != NULL) {
				getTimestamp(&moy, &mSec);
				strMoy	= int2string(moy);
				setNodeValue(mapElement->ptr, "//timeStamp", strMoy);
				free(strMoy);
				setMessage(msgSocket, mapElement->ptr);
				// TESTING
				// printf("MAP Nachricht versendet\n");
				mapElement = mapElement->next;
				usleep(period);
			}
		 }
	}
	freeList(mapHead);
	freeList(spatHead);
 	close (uds);
	return EXIT_SUCCESS;
}
