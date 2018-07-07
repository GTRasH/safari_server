#include <message_manager.h>

xmlDocPtr getMessage(int createSocket, int *response) {
	int msgSize;
	char *recvBuffer;
	xmlDocPtr message;
	// Empfang der Nachrichtengröße
	recvBuffer = calloc(MSG_BUF, sizeof(char));
	recv(createSocket, recvBuffer, MSG_BUF, 0);
	msgSize = (int)strtol(recvBuffer, NULL, 10);
	free(recvBuffer);
	if (msgSize == 0) {	// => Keine Nachrichtengröße gesendet => Abbruch
		*response = 0;
		return NULL;
	}
	// Sonst Nachrichten-Buffer anpassen und Nachricht empfangen
	recvBuffer	= calloc(msgSize+1, sizeof(char));
	recv(createSocket, recvBuffer, msgSize, 0);
	recvBuffer[msgSize] = '\0';
	*response	= 1;
	message = xmlReadMemory(recvBuffer, msgSize, NULL, NULL, 0);
	free(recvBuffer);
	return message;
}

int processMAP(xmlDocPtr message, MYSQL *con, intersectGeo ** mapTable) {
	xmlDocPtr xmlPtr;
	uint32_t refID, xmlLen;
	char * query, ** geometry;
	// Jedes Element von geometry ist ein gültiger XML-String
	if ((geometry = getTree(message, "//IntersectionGeometry")) == NULL)
		return 1;
	
	geometry = getWellFormedXML(geometry);
	for (int i = 0; *(geometry+i); ++i) {
		xmlLen	= strlen(*(geometry+i));
		xmlPtr	= xmlReadMemory(*(geometry+i), xmlLen, NULL, NULL, 0);
		refID	= getReferenceID(xmlPtr);
		query	= calloc((2*xmlLen + MSG_BUF), sizeof(char));
		// Aktualisierung der DB
		sprintf(query,	"INSERT INTO map "
						"VALUES ('%i', '%s', '%i') "
						"ON DUPLICATE KEY UPDATE xml = '%s', timestamp='%i'"
						, refID, *(geometry+i), 0, *(geometry+i), 0);
		if (mysql_query(con, query))
			sqlError(con);
		free(query);
		// Aktualisierung der MAP-Table
		setGeoElement(mapTable, refID, "0", *(geometry+i));
	}
	freeArray(geometry);
	return 0;
}

int processSPAT(xmlDocPtr message, intersectGeo ** mapTable) {
	xmlDocPtr ptrSPAT, ptrMAP;
	uint32_t refID;
	char ** state, ** refPoint;

	if ((state = getTree(message, "//IntersectionState")) == NULL)
		return 1;
	state = getWellFormedXML(state);
	for (int i = 0; *(state+i); ++i) {
		ptrSPAT		= xmlReadMemory(*(state+i), strlen(*(state+i)), NULL, NULL, 0);
		refID		= getReferenceID(ptrSPAT);
		// Existiert keine MAP-Information, wird die SPaT-Nachricht übersprungen
		if ((ptrMAP = getGeoElement(mapTable, refID)) == NULL)
			continue;
		
		refPoint	= getTree(ptrMAP, "//refPoint");
		printf("%s\n", *(refPoint));
	}
	
	freeArray(state);
	
	return 0;
}

uint32_t getReferenceID(xmlDocPtr xmlDoc) {
	uint16_t regID, intID;
	uint32_t refID;
	char ** regulatorID, ** intersectID;
	// Werte aus dem XML-Dokument parsen
	regulatorID	= getNodeValue(xmlDoc, "//id/region");
	intersectID	= getNodeValue(xmlDoc, "//id/id");
	regID = (uint16_t)strtol(*(regulatorID), NULL, 10);
	intID = (uint16_t)strtol(*(intersectID), NULL, 10);
	freeArray(regulatorID);
	freeArray(intersectID);
	refID = regID;
	refID <<= 16;
	
	return (refID + intID);
}

char ** getTree(xmlDocPtr message, char * tag) {
	xmlNodeSetPtr nodes;
	xmlBufferPtr xmlBuff;
	char ** array;
	int countNodes, dumpSize;
	// Aufruf aller Knoten (Evaluierung mittels tag)
	nodes	= getNodes(message, tag)->nodesetval;
	if ((countNodes = (nodes) ? nodes->nodeNr : 0) == 0)
		return NULL;
	// erstellt für jeden gefundenen Tag einen XML-Doc-String
	array = calloc(countNodes+1, sizeof(char*));
	for (int i = 0; i < countNodes; ++i) {
		xmlBuff	 = xmlBufferCreate();
		dumpSize = xmlNodeDump(xmlBuff, message, nodes->nodeTab[i], 0, 0);
		array[i] = calloc((dumpSize + 1), sizeof(char));
		array[i] = strcpy(array[i], (char *) xmlBuff->content);
		array[i][dumpSize] = '\0';
		xmlBufferFree(xmlBuff);
	}
	array[countNodes] = '\0';
	return array;
}

intersectGeo ** getGeoTable(MYSQL *con) {
	intersectGeo ** geometryTable, * geometry, * geoPtr;
	MYSQL_RES * result;
	MYSQL_ROW row;
	uint32_t refID;
	int hash;
	// MAP Information abfragen
	if (mysql_query(con, "SELECT referenceid, timestamp, xml FROM map"))
		sqlError(con);
	if ((result = mysql_store_result(con)) == NULL)
		sqlError(con);
	// Hash-Table initialisieren
	geometryTable = calloc(MAX_HASH, sizeof(intersectGeo *));
	for (int i = 0; i < MAX_HASH; i++)
		geometryTable[i] = NULL;
	// DB-Ergebnisse verarbeiten (Hash-Table füllen)
	while ((row = mysql_fetch_row(result))) {
		refID	 = (uint32_t)strtol(row[0], NULL, 10);
		hash	 = getHash(refID);
		geometry = getNewGeoElement(refID, row[1], row[2]);
		// Kollisions-Abfrage
		if (geometryTable[hash] == NULL)
			geometryTable[hash] = geometry;
		else {
			geoPtr = geometryTable[hash];
			while (geoPtr->next != NULL)
				geoPtr = geoPtr->next;
			geoPtr->next = geometry;
		}
	}
	return geometryTable;
}

intersectGeo * getNewGeoElement(uint32_t refID, char * timestamp, char * xml) {
	intersectGeo * geometry;
	// intersectGeo-Element aufbauen und mit Daten füllen
	geometry			  = malloc(sizeof(intersectGeo));
	geometry->xml		  = calloc(strlen(xml)+1, sizeof(char));
	geometry->referenceID = refID;
	geometry->timestamp	  = (int)strtol(timestamp, NULL, 10);
	geometry->next		  = NULL;
	strcpy(geometry->xml, xml);
	return geometry;
}

void setGeoElement(intersectGeo ** table, uint32_t refID, char * timestamp, char * xml) {
	intersectGeo * mapPtr = table[getHash(refID)];
	// Suche das Element mit der refID in der Hash-Table
	while (mapPtr != NULL) {
		if (mapPtr->referenceID != refID)
			mapPtr = mapPtr->next;
		else {
			// IntersectionGeometry gefunden -> Aktualisierung
			free(mapPtr->xml);
			mapPtr->xml = calloc(strlen(xml)+1, sizeof(char));
			strcpy(mapPtr->xml, xml);
			mapPtr->timestamp = (int)strtol(timestamp, NULL, 10);
			return;
		}
	}
	// Die referenceID wurde nicht gefunden -> Neues Element anlegen
	intersectGeo * newElement = getNewGeoElement(refID, timestamp, xml);
	if (mapPtr == NULL)
		mapPtr = newElement;
	else
		mapPtr->next = newElement;
}

xmlDocPtr getGeoElement(intersectGeo ** table, uint32_t refID) {
	intersectGeo * mapPtr = table[getHash(refID)];
	// Suche das Element mit der refID in der Hash-Table
	while (mapPtr != NULL)
		if (mapPtr->referenceID != refID)
			mapPtr = mapPtr->next;
		else
			return xmlReadMemory(mapPtr->xml, strlen(mapPtr->xml), NULL, NULL, 0);
	// NULL wenn Element nicht gefunden
	return NULL;
}

int getHash(uint32_t refID) {
	return refID % MAX_HASH;
}

char ** getWellFormedXML(char ** trees) {
	size_t xmlLength;
	char * xmlString;
	const char * xmlTag		= "<?xml version=\"1.0\"?>\n";
	const size_t xmlTagLen	= (int)strlen(xmlTag);
	for (int i = 0; trees[i]; i++) {
		xmlLength	= strlen(trees[i]);
		xmlString	= malloc(xmlLength);
		xmlString	= strcpy(xmlString, trees[i]);
		free(trees[i]);
		trees[i]	= calloc((xmlLength + xmlTagLen + 1), sizeof(char));
		strcpy(trees[i], xmlTag);
		strcat(trees[i], xmlString);
		free(xmlString);
	}
	return trees;
}

void freeGeoTable(intersectGeo ** table) {
	intersectGeo * geoPtr, * temp;
	for (int i = 0; i < MAX_HASH; ++i) {
		geoPtr = table[i];
		while (geoPtr != NULL) {
			free(geoPtr->xml);
			temp	= geoPtr;
			geoPtr	= geoPtr->next;
			free(temp);
		}
		free(table[i]);
	}
}
