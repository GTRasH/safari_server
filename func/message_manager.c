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

int processMAP(xmlDocPtr message, msqList * clients, uint8_t test) {
	// MYSQL_RES * res;
	xmlDocPtr xmlInterDoc, xmlLaneDoc, xmlNodeDoc;
	char ** strRegion, ** strId, ** strLaneId, ** interLong, ** interLat,
		 ** strLaneWidth, ** strNodeWidth, ** strNodeLong, ** strNodeLat, 
		 ** temp, ** xmlInter, ** xmlLane, ** xmlNodes, ** strOffsetX, ** strOffsetY;
	int interMaxLong, interMaxLat, interMinLong, interMinLat, laneId,
		laneWidth, nodeWidth, nodeLong, nodeLat, offsetX, offsetY;
	uint16_t region, id;
	uint8_t update = 0;
	char query[Q_BUF], interUpdate[MSG_BUF], clientNotify[MSG_BUF];
	segParts slices;
	msqList * clientPtr;
	msqElement s2c;
	MYSQL * dbCon = sqlConnect("safari");
	
	// Aktualisierung-Benachrichtung für die Clients vorbereiten
	strcpy(clientNotify, XML_TAG);
	strcat(clientNotify, "<mapUpdate>\n");
	
	// Jedes Element von geometry ist ein gültiger XML-String
	if ((temp = getTree(message, "//IntersectionGeometry")) == NULL)
		return 1;

	xmlInter = getWellFormedXML(temp);
	freeArray(temp);

	for (int i = 0; *(xmlInter+i); ++i) {
		// IntersectionGeometry-Daten auslesen
		xmlInterDoc	 = xmlReadMemory(*(xmlInter+i), strlen(*(xmlInter+i)), NULL, NULL, 0);
		strRegion	 = getNodeValue(xmlInterDoc, "//id/region");
		strId		 = getNodeValue(xmlInterDoc, "//id/id");
		interLong	 = getNodeValue(xmlInterDoc, "//refPoint/long");
		interLat	 = getNodeValue(xmlInterDoc, "//refPoint/lat");
		strLaneWidth = getNodeValue(xmlInterDoc, "//laneWidth");
		// IntersectionReferenceID oder refPoint nicht gefunden -> nächste Intersection
		if (strRegion == NULL || strId == NULL || interLong == NULL 
			|| interLat == NULL || strLaneWidth == NULL) {
			xmlFreeDoc(xmlInterDoc);
			freeArray(strLaneWidth);
			freeArray(strRegion);
			freeArray(strId);
			freeArray(interLong);
			freeArray(interLat);
			continue;
		}
		// Kreuzungsumgebung bestimmen
		region		 = (uint16_t)strtol(*(strRegion),NULL,10);
		id			 = (uint16_t)strtol(*(strId),NULL,10);
		laneWidth	 = (int)strtol(*(strLaneWidth),NULL,10);
		interMaxLong = (int)strtol(*(interLong),NULL,10) + INTER_AREA;
		interMaxLat	 = (int)strtol(*(interLat),NULL,10) + INTER_AREA;
		interMinLong = (int)strtol(*(interLong),NULL,10) - INTER_AREA;
		interMinLat  = (int)strtol(*(interLat),NULL,10) - INTER_AREA;
		// DB aktualisieren
		sprintf	(query, 
				"INSERT INTO intersections "
				"(region, id, maxLong, maxLat, minLong, minLat) "
				"VALUES ('%u', '%u', '%i', '%i', '%i', '%i') "
				"ON DUPLICATE KEY UPDATE maxLong = '%i', maxLat = '%i', "
				"minLong = '%i', minLat = '%i'", region, id, 
				interMaxLong, interMaxLat, interMinLong, interMinLat, 
				interMaxLong, interMaxLat, interMinLong, interMinLat);
		if (mysql_query(dbCon, query))
			sqlError(dbCon);
		
		sprintf(interUpdate,"<region>%u</region>\n<id>%u</id>\n",region,id);
		strcat(clientNotify,interUpdate);
		update = 1;
							
		// Lanes auslesen
		if ((temp = getTree(xmlInterDoc, "//GenericLane")) == NULL) {
			xmlFreeDoc(xmlInterDoc);
			freeArray(strRegion);
			freeArray(strId);
			freeArray(interLong);
			freeArray(interLat);
			continue;
		}
		xmlLane = getWellFormedXML(temp);
		freeArray(temp);
		// Lanes abarbeiten
		for (int j = 0; *(xmlLane+j); ++j) {
			xmlLaneDoc = xmlReadMemory(*(xmlLane+j), strlen(*(xmlLane+j)),
										 NULL, NULL, 0);
			strLaneId  = getNodeValue(xmlLaneDoc, "//laneID");
			temp	   = getTree(xmlLaneDoc, "//NodeXY");
			// Keine laneID oder Nodes vorhanden
			if (strLaneId == NULL || temp == NULL) {
				xmlFreeDoc(xmlLaneDoc);
				freeArray(strLaneId);
				continue;
			}
			laneId	 = (int)strtol(*(strLaneId),NULL,10);
			xmlNodes = getWellFormedXML(temp);
			freeArray(temp);
			for (int k = 0; *(xmlNodes+k); ++k) {
				xmlNodeDoc	 = xmlReadMemory(*(xmlNodes+k), strlen(*(xmlNodes+k)),
											 NULL, NULL, 0);
				strNodeWidth = getNodeValue(xmlNodeDoc, "//dWidth");
				nodeWidth = (strNodeWidth != NULL) ?
							(int)strtol(*(strNodeWidth),NULL,10) + laneWidth :
							laneWidth;
											
				// Als ersten NodeXY wird der Referenzpunkt (Node-LLmD-64b) erwartet
				if (k == 0) {
					strNodeLong	= getNodeValue(xmlNodeDoc, "//node-LatLon/lon");
					strNodeLat	= getNodeValue(xmlNodeDoc, "//node-LatLon/lat");
					if (strNodeLong == NULL || strNodeLat == NULL) {
						xmlFreeDoc(xmlNodeDoc);
						freeArray(strNodeLong);
						freeArray(strNodeLat);
						break;
					}
					nodeLong = (int)strtol(*(strNodeLong),NULL,10);
					nodeLat	 = (int)strtol(*(strNodeLat),NULL,10);
					sprintf (query, 
							"INSERT INTO lanes "
							"(region, id, laneID, longitude, latitude) "
							"VALUES ('%u', '%u', '%i', '%i', '%i') "
							"ON DUPLICATE KEY UPDATE longitude = '%i', "
							"latitude = '%i'", region, id, laneId, 
							nodeLong, nodeLat, nodeLong, nodeLat);
							
					if (mysql_query(dbCon, query))
						sqlError(dbCon);
					freeArray(strNodeLong);
					freeArray(strNodeLat);
				}
				else
				{
					slices		= getSegmentParts(xmlNodeDoc);
					strOffsetX	= getNodeValue(xmlNodeDoc, "//x");
					strOffsetY	= getNodeValue(xmlNodeDoc, "//y");
					if (slices == err || strOffsetX == NULL || strOffsetY == NULL) {
						xmlFreeDoc(xmlNodeDoc);
						freeArray(strOffsetX);
						freeArray(strOffsetY);
						break;
					}
					offsetX	= (int)strtol(*(strOffsetX), NULL, 0);
					offsetY	= (int)strtol(*(strOffsetY), NULL, 0);
					setSegments(dbCon, region, id, laneId, slices, nodeWidth,
								nodeLong, nodeLat, offsetX, offsetY);
					// neuer Referenzpunkt für den nächsten Node
					nodeLat  += offsetX;
					nodeLong += offsetY;
					freeArray(strOffsetX);
					freeArray(strOffsetY);
				}
				xmlFreeDoc(xmlNodeDoc);
			}
			xmlFreeDoc(xmlLaneDoc);
			freeArray(strLaneId);
			freeArray(xmlNodes);
		//	freeArray(strLaneWidth);
		}
		xmlFreeDoc(xmlInterDoc);
		freeArray(xmlLane);
		freeArray(strRegion);
		freeArray(strId);
		freeArray(interLong);
		freeArray(interLat);
	}
	// Benachrichtigung der Clients
	if (update == 1) {
		clientPtr = clients;
		strcat(clientNotify, "</mapUpdate>");
		s2c.prio	= 2;
		sprintf(s2c.message, "%s", clientNotify);
		while (clientPtr != NULL) {
			if ((msgsnd(clientPtr->id, &s2c, MSQ_LEN, 0)) < 0)
				printf ("Konnte Nachricht an Client MQ %d nicht zustellen\n",
						clientPtr->id);
			else
				printf ("Nachricht an Client MQ %d zugestellt\n",
						clientPtr->id);
			clientPtr = clientPtr->next;
		}
	}
	mysql_close(dbCon);
	freeArray(xmlInter);
	return 0;
}

int processSPAT(xmlDocPtr message, msqList * clients, uint8_t test) {
	int res;
	xmlDocPtr ptrSPAT;
	char ** stateRaw, ** stateXML, ** refPoint, ** moy, ** mSec;
	msqList * clientPtr;
	msqElement s2c;

	// Erzeuge ein Array aus IntersectionState-Elementen
	if ((stateRaw = getTree(message, "//IntersectionState")) == NULL) {
		freeArray(stateRaw);
		return 1;
	}
	stateXML = getWellFormedXML(stateRaw);
	freeArray(stateRaw);
	
	for (int i = 0; *(stateXML+i); ++i) {
		clientPtr = clients;
		// Schleifenabbruch wenn keine Clients registriert sind und Testing deaktiviert
		if (clientPtr == NULL && test == 0)
			break;

		ptrSPAT	= xmlReadMemory(*(stateXML+i), strlen(*(stateXML+i)), 
								NULL, NULL, 0);

		// TESTING - Zeitstempel auf Konsole ausgeben
		if (test & 1) {
			moy	 = getNodeValue(ptrSPAT, "//moy");
			mSec = getNodeValue(ptrSPAT, "//timeStamp");
			
			printf ("IntersectionState mit moy = %s und mSec = %s verarbeitet\n",
					*(moy), *(mSec));
					
			freeArray(moy);
			freeArray(mSec);
		}
		
		refPoint	= getTree(ptrSPAT, "//AdvisorySpeed");
		s2c.prio	= 2;
		sprintf(s2c.message, "%s", *(refPoint));
		while (clientPtr != NULL) {
			
			res = msgsnd(clientPtr->id, &s2c, MSQ_LEN, 0);
      
			if (res < 0)
				printf ("Konnte Nachricht an Client MQ %d nicht zustellen\n",
						clientPtr->id);
			else
				printf ("Nachricht an Client MQ %d zugestellt\n",
						clientPtr->id);
			  
			clientPtr = clientPtr->next;
		}
		
		freeArray(refPoint);
		xmlFreeDoc(ptrSPAT);
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

msqList * setMsqClients(int serverID, msqList * clients) {
	int clientID;
	msqElement c2s;
	// Client Manager Registrierungen abarbeiten
	while (msgrcv(serverID, &c2s, MSQ_LEN, 0, IPC_NOWAIT) != -1) {
		// Deregistrierung
		if (c2s.prio == 1 ) {
			sscanf(c2s.message,"%d",&clientID);
			clients = msqListRemove(clientID, clients);
		}
		// Registrierung für SPaT
		else if (c2s.prio == 2) {
			sscanf(c2s.message,"%d",&clientID);
			clients = msqListAdd(clientID, clients);
		}
	}
	return clients;
}

segParts getSegmentParts(xmlDocPtr xmlNodeDoc) {
	xmlXPathObjectPtr nodeCheck;
	segParts result;
	nodeCheck = getNodes(xmlNodeDoc, "//node-XY1");
	if (nodeCheck != NULL) {
		result = xy1;
		xmlXPathFreeObject(nodeCheck);
	} else {
		nodeCheck = getNodes(xmlNodeDoc, "//node-XY2");
		if (nodeCheck != NULL) {
			result = xy2;
			xmlXPathFreeObject(nodeCheck);
		} else {
			nodeCheck = getNodes(xmlNodeDoc, "//node-XY3");
			if (nodeCheck != NULL) {
				result = xy3;
				xmlXPathFreeObject(nodeCheck);
			} else {
				nodeCheck = getNodes(xmlNodeDoc, "//node-XY4");
				if (nodeCheck != NULL) {
					result = xy4;
					xmlXPathFreeObject(nodeCheck);
				} else {
					nodeCheck = getNodes(xmlNodeDoc, "//node-XY5");
					if (nodeCheck != NULL) {
						result = xy5;
						xmlXPathFreeObject(nodeCheck);
					} else {
						nodeCheck = getNodes(xmlNodeDoc, "//node-XY6");
						if (nodeCheck != NULL) {
							result = xy6;
							xmlXPathFreeObject(nodeCheck);
						} else {
							xmlXPathFreeObject(nodeCheck);
							result = err;
						}
					}
				}
			}
		}
	}
	return result;
}

void setSegments(MYSQL * dbCon, uint16_t region, uint16_t id, int laneID, 
				segParts slices, int nodeWith, int nodeLong, int nodeLat, 
				int offsetX, int offsetY) {
	printf("Jetzt geht's ab ^^\n");
}
