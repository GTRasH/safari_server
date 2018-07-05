/* uds_client.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mysql.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <func.h>

#define BUF 1024
#define UDS_FILE "/tmp/safari_sim.sock"

xmlDocPtr getMessage(int createSocket, int *response);

int processMAP(xmlDocPtr message, MYSQL *con);
int processSPAT(xmlDocPtr message);

void sqlError(MYSQL *con);

MYSQL * sqlConnect(void);

/** \brief	Berechnet aus der Regulator- und IntersectionID (jeweils 16 Bit)
 * 			die 32 Bit IntersectionReferenceID zur Verwendung als Hash-Schlüssel
 * 
 * \param[in] Pointer auf das XML-Element
 * 
 * \return	IntersectionReferenceID
 */
uint32_t getReferenceID(xmlDocPtr xmlDoc);

char ** getSubXMLString(xmlDocPtr message, char * tag);

int main (int argc, char **argv) {
	int createSocket, response;
	struct sockaddr_un address;
	char ** msgId;
	char * tmp;
	xmlDocPtr message;
	
	MYSQL * con = sqlConnect();
  
	if((createSocket=socket (PF_LOCAL, SOCK_STREAM, 0)) > 0)
		printf ("Socket wurde angelegt\n");
	address.sun_family = AF_LOCAL;
	strcpy(address.sun_path, UDS_FILE);

	if (connect(createSocket, (struct sockaddr *) &address, sizeof(address)) == 0)
		printf ("Verbindung mit dem Server hergestellt\n");

	// Nachrichtenverarbeitung bis Simulator "quit" schickt oder abendiert
	do {
		message	= getMessage(createSocket, &response);
		msgId	= getNodeValue(message, "//messageId");
		if (msgId == NULL) {
			xmlFreeDoc(message);
			return error("Error: Message Manager beendet\n"
						 "Sendeschema (... -> Nachrichtengröße -> Nachricht -> ...) nicht eingehalten\n");
		}
		// messageId auswerten und weitere Verarbeitung triggern
		switch ((int)strtol(msgId[0], &tmp, 10)) {
			case 18:	switch (processMAP(message, con)) {
							case 0: printf("MAP-Nachricht erfolgreich verarbeitet\n");
									break;
							case 1:	printf("Error: MAP-Nachricht ohne IntersectionGeometry-Knoten!\n");
									break;
							default: ; break;
						}
						break;
			case 19:	switch (processSPAT(message)) {
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
	
	return EXIT_SUCCESS;
}

xmlDocPtr getMessage(int createSocket, int *response) {
	int msgSize, recvBytes;
	char *recvBuffer, *tmp;
	xmlDocPtr message;
	// Empfang der Nachrichtengröße
	recvBuffer = calloc(BUF, sizeof(char));
	recv(createSocket, recvBuffer, BUF, 0);
	msgSize = (int)strtol(recvBuffer, &tmp, 10);
	free(recvBuffer);
	if (msgSize == 0) {	// => Keine Nachrichtengröße gesendet => Abbruch
		*response = 0;
		return NULL;
	}
	// Sonst Nachrichten-Buffer anpassen und Nachricht empfangen
	recvBuffer	= calloc(msgSize, sizeof(char));
	recvBytes	= recv(createSocket, recvBuffer, msgSize, 0);
	*response	= 1;
	if (recvBytes > 0)
		recvBuffer[msgSize] = '\0';
	message = xmlReadMemory(recvBuffer, msgSize, NULL, NULL, 0);
	free(recvBuffer);
	return message;
}

uint32_t getReferenceID(xmlDocPtr xmlDoc) {
	uint16_t regID, intID;
	uint32_t refID;
	char ** regulatorID, ** intersectID, * tmp;
	// Werte aus dem XML-Dokument parsen
	regulatorID	= getNodeValue(xmlDoc, "//id/region");
	intersectID	= getNodeValue(xmlDoc, "//id/id");
	regID = (uint16_t)strtol(*regulatorID, &tmp, 10);
	intID = (uint16_t)strtol(*intersectID, &tmp, 10);
	freeArray(regulatorID);
	freeArray(intersectID);
	refID = regID;
	refID <<= 16;
	refID += intID;
	
	return refID;
}

int processMAP(xmlDocPtr message, MYSQL *con) {
	xmlDocPtr xmlPtr;
	uint32_t refID;
	const char * queryConst = "INSERT INTO map VALUES('";
	char * xmlStr, ** geometry, * query, * refStr;
	int i, xmlLen;

	// Jedes Element von geometry ist ein gültiger XML-String
	if ((geometry = getSubXMLString(message, "//IntersectionGeometry")) == NULL)
		return 1;
	for (i = 0; *(geometry+i); ++i) {
		xmlStr	= *(geometry+i);
		xmlLen	= strlen(xmlStr);
		xmlPtr	= xmlReadMemory(xmlStr, xmlLen, NULL, NULL, 0);
		refID	= getReferenceID(xmlPtr);
		refStr	= int2string(refID);
		query	= calloc((36 + strlen(refStr) + strlen(xmlLen)), sizeof(char));
		query	= strcat(query, queryConst);
		query	= strcat(query, refStr);
		query	= strcat(query, "', '");
		query	= strcat(query, xmlStr);
		query	= strcat(query, "', '0')");
		if (mysql_query(con, query))
			sqlError(con);
		free(query);
	}
	freeArray(geometry);
	return 0;
}

void sqlError(MYSQL *con) {
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);
}

int processSPAT(xmlDocPtr message) {
	xmlDocPtr xmlPtr;
	uint32_t refID;
	char * xmlStr, ** state;
	int i;

	if ((state = getSubXMLString(message, "//IntersectionState")) == NULL)
		return 1;
	for (i = 0; *(state+i); ++i) {
		xmlStr	= *(state+i);
		xmlPtr	= xmlReadMemory(xmlStr, strlen(xmlStr), NULL, NULL, 0);
		refID	= getReferenceID(xmlPtr);
	}
	
	freeArray(state);
	
	return 0;
}

char ** getSubXMLString(xmlDocPtr message, char * tag) {
	xmlNodeSetPtr nodes;
	xmlBufferPtr xmlBuff;
	const char * xmlTag = "<?xml version=\"1.0\"?>";
	char ** array;
	int i, countNodes, dumpSize, xmlLen;
	// Aufruf aller Knoten (Evaluierung mittels tag)
	nodes	= getNodes(message, tag)->nodesetval;
	if ((countNodes = (nodes) ? nodes->nodeNr : 0) == 0)
		return NULL;
	// erstellt für jeden jeden gefundenen Tag einen XML-Doc-String
	array = calloc(countNodes+1, sizeof(char*));
	for (i = 0; i < countNodes; ++i) {
		xmlBuff	 = xmlBufferCreate();
		dumpSize = xmlNodeDump(xmlBuff, message, nodes->nodeTab[i], 0, 0);
		xmlLen	 = ((int)strlen(xmlTag) + dumpSize);
		array[i] = calloc(xmlLen+1, sizeof(char));
		array[i] = strcpy(array[i], xmlTag);
		array[i] = strcat(array[i], (char *) xmlBuff->content);
		array[i][xmlLen] = '\0';
		xmlBufferFree(xmlBuff);
	}
	// definiertes Ende des Arrays
	array[countNodes] = '\0';
	return array;
}

MYSQL * sqlConnect(void) {
	MYSQL *con = mysql_init(NULL);
	if (mysql_real_connect(con, "localhost", "root", "safari", 
			"archive", 0, NULL, 0) == NULL)
		sqlError(con);
	return con;
}
