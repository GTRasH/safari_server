#include <message_manager.h>

xmlDocPtr getMessage(int sock) {
	char * msgBuf = getSocketContent(sock);
	xmlDocPtr msgXML;
	if (msgBuf == NULL) {
		free(msgBuf);
		return NULL;
	}

	msgXML = xmlReadMemory(msgBuf, strlen(msgBuf), NULL, NULL, 0);
	free(msgBuf);
	return msgXML;
}

int processMAP(xmlDocPtr message, MYSQL *con, intersectGeo ** mapTable) {
	xmlDocPtr xmlPtr;
	uint32_t refID, xmlLen;
	char * query, ** geoTree, ** geoXML;
	// Jedes Element von geometry ist ein gültiger XML-String
	if ((geoTree = getTree(message, "//IntersectionGeometry")) == NULL)
		return 1;
	
	geoXML = getWellFormedXML(geoTree);
	
	freeArray(geoTree);
	
	for (int i = 0; *(geoXML+i); ++i) {
		xmlLen	= strlen(*(geoXML+i));
		xmlPtr	= xmlReadMemory(*(geoXML+i), xmlLen, NULL, NULL, 0);
		refID	= getReferenceID(xmlPtr);
		query	= calloc((2*xmlLen + MSG_BUF), sizeof(char));
		// Aktualisierung der DB
		sprintf(query,	"INSERT INTO map "
						"VALUES ('%i', '%s', '%i') "
						"ON DUPLICATE KEY UPDATE xml = '%s', timestamp='%i'"
						, refID, *(geoXML+i), 0, *(geoXML+i), 0);
		if (mysql_query(con, query))
			sqlError(con);
		free(query);
		xmlFreeDoc(xmlPtr);
		// Aktualisierung der MAP-Table
		setGeoElement(mapTable, refID, "0", *(geoXML+i));
	}
	freeArray(geoXML);
	return 0;
}

int processSPAT(xmlDocPtr message, intersectGeo ** mapTable, msqList * clients) {
	int res;
	xmlDocPtr ptrSPAT, ptrMAP;
	uint32_t refID;
	char ** stateRaw, ** stateXML, ** refPoint;
	
	msqList * clientPtr;
	msqElement s2c;

	if ((stateRaw = getTree(message, "//IntersectionState")) == NULL) {
		freeArray(stateRaw);
		return 1;
	}
		
	stateXML = getWellFormedXML(stateRaw);
	freeArray(stateRaw);
	
	for (int i = 0; *(stateXML+i); ++i) {
		clientPtr = clients;
		// Schleifenabbruch wenn keine Clients registriert sind
		if (clientPtr == NULL)
			break;

		ptrSPAT		= xmlReadMemory(*(stateXML+i), strlen(*(stateXML+i)), 
									NULL, NULL, 0);
		refID		= getReferenceID(ptrSPAT);
		// Existiert keine MAP-Information, wird die SPaT-Nachricht übersprungen
		if ((ptrMAP = getGeoElement(mapTable, refID)) == NULL)
			continue;
			
		refPoint	= getTree(ptrSPAT, "//AdvisorySpeed");
		
		s2c.prio	= 2;
		sprintf(s2c.message, "%s", *refPoint);
		
		freeArray(refPoint);
		
		while (clientPtr != NULL) {
			
			res = msgsnd(clientPtr->id, &s2c, MSQ_LEN, 0);
      
			if (res < 0)
				printf ("Konnte Nachricht an Client MQ %d nicht zustellen\n",
						clientPtr->id);
			else
				printf("Nachricht an Client MQ %d zugestellt\n", clientPtr->id);
			  
			clientPtr = clientPtr->next;
		}
	}
	
	freeArray(stateXML);
	
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
	xmlXPathObjectPtr xpathObj;
	xmlNodeSetPtr nodeSet;
	xmlBufferPtr xmlBuff;
	char ** array;
	int countNodes, dumpSize;
	// Aufruf aller Knoten (Evaluierung mittels tag)
	xpathObj = getNodes(message, tag);
	nodeSet	 = xpathObj->nodesetval;
	if ((countNodes = (nodeSet) ? nodeSet->nodeNr : 0) == 0) {
		xmlXPathFreeNodeSet(nodeSet);
		return NULL;
	}

	// erstellt für jeden gefundenen Tag einen XML-Doc-String
	array = calloc(countNodes+1, sizeof(char*));
	for (int i = 0; i < countNodes; ++i) {
		xmlBuff	 = xmlBufferCreate();
		dumpSize = xmlNodeDump(xmlBuff, message, nodeSet->nodeTab[i], 0, 0);
		array[i] = calloc((dumpSize + 1), sizeof(char));
		array[i] = strcpy(array[i], (char *) xmlBuff->content);
		array[i][dumpSize] = '\0';
		xmlBufferFree(xmlBuff);
	}
	array[countNodes] = NULL;	// Array sicher abgeschlossen
	xmlXPathFreeObject(xpathObj);
	return array;
}

intersectGeo * getNewGeoElement(uint32_t refID, char * timestamp, char * xml) {
	intersectGeo * geometry;
	// intersectGeo-Element aufbauen und mit Daten füllen
	geometry			  = malloc(sizeof(intersectGeo));
	geometry->xml		  = calloc(strlen(xml), sizeof(char));
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
	const char * xmlTag		= "<?xml version=\"1.0\"?>\n";
	const size_t xmlTagLen	= (int)strlen(xmlTag);
	int count;
	
	for (count = 0; trees[count]; count++);
	
	char ** ret = calloc(count+1, sizeof(char *));
	
	for (int i = 0; i < count; i++) {
		xmlLength	= strlen(trees[i]);
		ret[i]		= calloc(xmlLength+xmlTagLen+1, sizeof(char));
		strcpy(ret[i], xmlTag);
		strcat(ret[i], trees[i]);
	}
	ret[count] = NULL;
	return ret;
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
	if ((result = mysql_store_result(con)) == NULL) {
		mysql_free_result(result);
		sqlError(con);
	}
	// Hash-Table initialisieren
	geometryTable = malloc(MAX_HASH * sizeof(intersectGeo *));
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
	mysql_free_result(result);
	return geometryTable;
}

void freeGeoTable(intersectGeo ** table) {
	intersectGeo * ptr, * tmp;
	for (int i = 0; i < MAX_HASH; ++i) {
		ptr = table[i];
		while (ptr != NULL) {
			free(ptr->xml);	 // XML-String freigeben
			tmp	= ptr;
			ptr	= ptr->next;
			free(tmp);		 // Element freigeben
		}
	}
	free(table);
}

msqList * msqListAdd(int i, msqList * clients) {
   msqList * ptr;
   if(clients == NULL) {
		clients = malloc(sizeof( msqList ));
		if (clients == NULL )
			exit(EXIT_FAILURE);
		clients->id = i;
		clients->next = NULL;
   }
   else
   {
		ptr = clients;
		while (ptr->next != NULL)
			ptr=ptr->next;
		ptr->next = malloc(sizeof( msqList ));
		ptr = ptr->next;
		ptr->id = i;
		ptr->next = NULL;
   }
   return clients;
}

msqList * msqListRemove(int i, msqList * clients) {
	msqList * ptr_tmp;
	msqList * ptr;
	if(clients == NULL)
		return NULL;
	if( clients->id == i ) {
		ptr = clients->next;
		free(clients);
		clients = ptr;
		return clients;
	}
	ptr = clients;
	while(ptr->next != NULL) {
		ptr_tmp = ptr->next;
		if( ptr_tmp->id == i ) {
			ptr->next = ptr_tmp->next;
			free(ptr_tmp);
			break;
		}
		ptr=ptr_tmp;
	}
	return clients;
}
