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
	xmlDocPtr xmlInterDoc, xmlLaneDoc, xmlNodeDoc;
	char ** strRegion, ** strId, ** strLaneId, ** interLong, ** interLat,
		 ** strElevation, ** strLaneWidth, ** strNodeWidth,
		 ** strNodeLong, ** strNodeLat, ** temp, ** xmlInter, ** xmlLane, 
		 ** xmlNodes, ** strOffsetX, ** strOffsetY, query[Q_BUF], 
		 interUpdate[MSG_BUF], clientNotify[MSG_BUF], queryStmt[Q_BUF];
	int elevation, interOffset, interMaxLong, interMaxLat, interMinLong,
		interMinLat, nodeLong, nodeLat, maxLong, maxLat, minLong, minLat;
	double microDegree;
	uint16_t region, id, laneWidth, nodeWidth, offsetX, offsetY;
	uint8_t laneID, segID, segSkip, update = 0;
	msqList * clientPtr;
	msqElement s2c;
	MYSQL_STMT * stmt;
	MYSQL_BIND bind[12];
	MYSQL * dbCon = sqlConnect("safari");
	
	// Nachricht enthält keine IntersectionGeometry-Elemente
	if (!xmlContains(message, "//IntersectionGeometry"))
		return 1;

	// Aktualisierungs-Benachrichtung für die Clients vorbereiten
	strcpy(clientNotify, XML_TAG);
	strcat(clientNotify, "<mapUpdate>\n");

	// Datenbank für Segment-Update vorbereiten
	sprintf(queryStmt,"%s", "INSERT INTO segments "
							"(region, id, laneID, segID, maxLong, maxLat, minLong, minLat) "
							"VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
							"ON DUPLICATE KEY UPDATE maxLong=?, maxLat=?, minLong=?, minLat=?");
	stmt = mysql_stmt_init(dbCon);
	mysql_stmt_prepare(stmt, queryStmt, strlen(queryStmt));
	memset(bind, 0, sizeof(bind));
	setInsertParam(bind, &region, &id, &laneID, &segID, &maxLong, &maxLat, &minLong, &minLat);
	mysql_stmt_bind_param(stmt, bind);

	// IntersectionGeometry-Elemente abarbeiten
	temp	 = getTree(message, "//IntersectionGeometry");
	xmlInter = getWellFormedXML(temp);
	freeArray(temp);
	for (int i = 0; *(xmlInter+i); i++) {
		xmlInterDoc	 = xmlReadMemory(*(xmlInter+i), strlen(*(xmlInter+i)), NULL, NULL, 0);
		// IntersectionReferenceID, refPoint oder laneWidth nicht gefunden -> nächste Intersection
		if (!(xmlContains(xmlInterDoc, "//id/region") && xmlContains(xmlInterDoc, "//id/id") &&
			  xmlContains(xmlInterDoc, "//refPoint/long") && xmlContains(xmlInterDoc, "//refPoint/lat") &&
			  xmlContains(xmlInterDoc, "//laneWidth"))) {

			xmlFreeDoc(xmlInterDoc);
			continue;
		}
		// IntersectionGeometry-Daten auslesen
		strRegion	 = getNodeValue(xmlInterDoc, "//id/region");
		strId		 = getNodeValue(xmlInterDoc, "//id/id");
		interLong	 = getNodeValue(xmlInterDoc, "//refPoint/long");
		interLat	 = getNodeValue(xmlInterDoc, "//refPoint/lat");
		strLaneWidth = getNodeValue(xmlInterDoc, "//laneWidth");
		region		 = (uint16_t)strtol(*(strRegion),NULL,10);
		id			 = (uint16_t)strtol(*(strId),NULL,10);
		laneWidth	 = (int)strtol(*(strLaneWidth),NULL,10);
		// Höhe der Kreuzung berechnen
		if (!xmlContains(xmlInterDoc, "//refPoint/elevation"))
			elevation = 0;
		else {
			strElevation = getNodeValue(xmlInterDoc, "//refPoint/elevation");
			elevation	 = (int)strtol(*(strElevation),NULL,10);
			freeArray(strElevation);
		}
		// Kreuzungsumgebung (Global Service Area) bestimmen
		microDegree	 = get100thMicroDegree(elevation);
		interOffset	 = microDegree*INTER_AREA;
		interMaxLong = (int)strtol(*(interLong),NULL,10) + interOffset;
		interMaxLat	 = (int)strtol(*(interLat),NULL,10) + interOffset;
		interMinLong = (int)strtol(*(interLong),NULL,10) - interOffset;
		interMinLat  = (int)strtol(*(interLat),NULL,10) - interOffset;
		// Intersection auf der DB aktualisieren
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
		// Lanes auslesen
		if (!xmlContains(xmlInterDoc, "//GenericLane")) {
			xmlFreeDoc(xmlInterDoc);
			freeArray(strRegion);
			freeArray(strId);
			freeArray(interLong);
			freeArray(interLat);
			continue;
		}
		// Intersection in Aktualisierungs-Benachrichtigung aufnehmen
		sprintf(interUpdate,"<region>%u</region>\n<id>%u</id>\n",region,id);
		strcat(clientNotify,interUpdate);
		update	= 1;
		// Lanes abarbeiten
		temp	= getTree(xmlInterDoc, "//GenericLane");
		xmlLane = getWellFormedXML(temp);
		freeArray(temp);
		for (int j = 0; *(xmlLane+j); j++) {
			xmlLaneDoc = xmlReadMemory(*(xmlLane+j), strlen(*(xmlLane+j)), NULL, NULL, 0);
			// Keine laneID oder Nodes vorhanden -> nächste Lane
			if (!(xmlContains(xmlLaneDoc, "//laneID") && xmlContains(xmlLaneDoc, "//NodeXY"))) {
				xmlFreeDoc(xmlLaneDoc);
				freeArray(strLaneId);
				continue;
			}
			segSkip	  = SEG_SKIP;
			strLaneId = getNodeValue(xmlLaneDoc, "//laneID");
			temp	  = getTree(xmlLaneDoc, "//NodeXY");
			laneID	  = (uint8_t)strtol(*(strLaneId),NULL,10);
			segID	  = 0;
			xmlNodes  = getWellFormedXML(temp);
			freeArray(temp);
			// Lane-Segmente abarbeiten
			for (int k = 0; *(xmlNodes+k); k++) {
				xmlNodeDoc	 = xmlReadMemory(*(xmlNodes+k), strlen(*(xmlNodes+k)), NULL, NULL, 0);
				// Berechnung der individuellen Lane-Breite
				if (!xmlContains(xmlNodeDoc, "//dWidth"))
					nodeWidth = laneWidth;
				else {
					strNodeWidth = getNodeValue(xmlNodeDoc, "//dWidth");
					nodeWidth	 = (uint16_t)strtol(*(strNodeWidth),NULL,10) + laneWidth;
					freeArray(strNodeWidth);
				}
				// Als erstes NodeXY-Element wird der Referenzpunkt (Node-LLmD-64b) erwartet
				if (k == 0) {
					// Kein Node-LLmD-64b Element -> Abbruch der Lane-Segment-Abarbeitung
					if (!(xmlContains(xmlNodeDoc, "//node-LatLon/lon") && xmlContains(xmlNodeDoc, "//node-LatLon/lat"))) {
						xmlFreeDoc(xmlNodeDoc);
						break;
					}
					strNodeLong	= getNodeValue(xmlNodeDoc, "//node-LatLon/lon");
					strNodeLat	= getNodeValue(xmlNodeDoc, "//node-LatLon/lat");
					nodeLong	= (int)strtol(*(strNodeLong),NULL,10);
					nodeLat		= (int)strtol(*(strNodeLat),NULL,10);
					sprintf (query, 
							"INSERT INTO lanes "
							"(region, id, laneID, longitude, latitude) "
							"VALUES ('%u', '%u', '%u', '%i', '%i') "
							"ON DUPLICATE KEY UPDATE longitude = '%i', "
							"latitude = '%i'", region, id, laneID, 
							nodeLong, nodeLat, nodeLong, nodeLat);

					freeArray(strNodeLong);
					freeArray(strNodeLat);
					if (mysql_query(dbCon, query))
						sqlError(dbCon);
				}
				else
				{
					// Keine Offset-Werte enthalten -> Abbruch der Lane-Segment-Abarbeitung
					if (!(xmlContains(xmlNodeDoc, "//x") && xmlContains(xmlNodeDoc, "//y"))) {
						xmlFreeDoc(xmlNodeDoc);
						break;
					}
					strOffsetX	= getNodeValue(xmlNodeDoc, "//x");
					strOffsetY	= getNodeValue(xmlNodeDoc, "//y");
					offsetX		= (int)strtol(*(strOffsetX), NULL, 0);
					offsetY		= (int)strtol(*(strOffsetY), NULL, 0);
					setSegments(stmt, &segID, &maxLong, &maxLat, &minLong, 
								&minLat, microDegree, nodeWidth, segSkip, 
								nodeLong, nodeLat, offsetX, offsetY);
					// neuer Referenzpunkt für den nächsten Node
					segSkip	  = 0;
					nodeLong += (int)(microDegree*offsetX/100);
					nodeLat	 += (int)(microDegree*offsetY/100);
					freeArray(strOffsetX);
					freeArray(strOffsetY);
				}
				xmlFreeDoc(xmlNodeDoc);
			}
			xmlFreeDoc(xmlLaneDoc);
			freeArray(strLaneId);
			freeArray(xmlNodes);
		}
		xmlFreeDoc(xmlInterDoc);
		freeArray(xmlLane);
		freeArray(strRegion);
		freeArray(strId);
		freeArray(interLong);
		freeArray(interLat);
		freeArray(strLaneWidth);
	}
	// Benachrichtigung der Clients
	if (update == 1) {
		clientPtr	= clients;
		s2c.prio	= 2;
		strcat(clientNotify, "</mapUpdate>");
		strcat(clientNotify, TERM_NULL);
		sprintf(s2c.message, "%s", clientNotify);
		while (clientPtr != NULL) {
			msgsnd(clientPtr->id, &s2c, MSQ_LEN, 0);
			clientPtr = clientPtr->next;
		}
	}
	mysql_stmt_close(stmt);
	mysql_close(dbCon);
	freeArray(xmlInter);
	return 0;
}

int processSPAT(xmlDocPtr message, msqList * clients, uint8_t test) {
	int mSecSys, moySys;
	xmlDocPtr ptrSPAT;
	char ** stateRaw, ** stateXML, ** moy, ** mSec, logText[LOG_BUF];
	msqList * clientPtr;
	msqElement s2c;

	if (!xmlContains(message, "//IntersectionState"))
		return 1;
	// Erzeuge ein Array aus IntersectionState-Elementen
	stateRaw = getTree(message, "//IntersectionState");
	stateXML = getWellFormedXML(stateRaw);
	freeArray(stateRaw);
	// Abarbeitung der IntersectionState-Elemente
	for (int i = 0; *(stateXML+i); ++i) {
		clientPtr = clients;
		// Schleifenabbruch wenn keine Clients registriert sind und Testing deaktiviert
		if (clientPtr == NULL && test == 0)
			break;

		ptrSPAT	= xmlReadMemory(*(stateXML+i), strlen(*(stateXML+i)), NULL, NULL, 0);

		// TESTING - Zeitstempel auf Konsole ausgeben
		if (test & 1) {
			if (!(xmlContains(ptrSPAT, "//moy") && xmlContains(ptrSPAT, "//timeStamp")))
				printf ("Testing für eine SPaT-Nachricht fehlgeschlagen"
						"- Nachricht enthält kein moy- oder timeStamp-Element!\n");
				
			else {
				getTimestamp(&moySys, &mSecSys);
				moy	 = getNodeValue(ptrSPAT, "//moy");
				mSec = getNodeValue(ptrSPAT, "//timeStamp");
			
				printf ("IntersectionState empfangen: moy = %s  |  mSec = %s\n"
						"Aktuelle Systemzeit: moy = %i  |  mSec = %i\n",
						*(moy), *(mSec), moySys, mSecSys);
					
				freeArray(moy);
				freeArray(mSec);
			}
		}
		// Versenden der Nachricht an registrierte Client-Prozesse
		s2c.prio	= 2;
		sprintf(s2c.message, "%s", *(stateXML+i));
		while (clientPtr != NULL) {
			if ((msgsnd(clientPtr->id, &s2c, MSQ_LEN, 0)) < 0) {
				sprintf(logText,"Unable sending message to client MQ %d\n",clientPtr->id);
				setLogText(logText, LOG_SERVER);
			}
			clientPtr = clientPtr->next;
		}
		xmlFreeDoc(ptrSPAT);
	}
	freeArray(stateXML);
	return 0;
}

msqList * msqListAdd(int i, msqList * clients) {
   msqList * ptr;
   // Liste ist leer -> neues Element an den Anfang
   if(clients == NULL) {
		clients = malloc(sizeof( msqList ));
		if (clients == NULL )
			exit(EXIT_FAILURE);
		clients->id = i;
		clients->next = NULL;
   }
   // Sonst neues Element an das Ende
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
	// Liste ist leer
	if(clients == NULL)
		return NULL;
	// Gesuchter Client-Eintrag am Anfang der Liste
	if( clients->id == i ) {
		ptr = clients->next;
		free(clients);
		clients = ptr;
		return clients;
	}
	// Client suchen und aushängen
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
		// Weitere Registrierungen für neue Dienste möglich !
	}
	return clients;
}

void setSegments(MYSQL_STMT * stmt, uint8_t * segID, int * maxLong, 
				 int * maxLat, int * minLong, int * minLat, double microDeg, 
				 uint16_t laneWidth, uint8_t skip, int nodeLong, int nodeLat, 
				 int16_t offsetX, int16_t offsetY) {
	
	uint16_t degLaneWidth, degNodeGap, segments;
	uint8_t offsetA, offsetB, offsetC;
	double nodeGap, cos;
	// Strecke zwischen Bezugs- und Offset-Node in cm
	nodeGap	 	 = sqrt((offsetX*offsetX)+(offsetY*offsetY));
	// Umrechnung cm in 1/10 µGrad
	degLaneWidth = (uint16_t)(microDeg*laneWidth)/100;
	degNodeGap	 = (uint16_t)(microDeg*nodeGap)/100;
	// Anzahl der Teilstücke der Strecke zwischen Bezugs- und Offset-Node
	// Sollte das Segment < SEG_PART sein, wird ein Teilstück gebildet.
	if ((segments = (uint16_t)ceil(nodeGap/SEG_PART)) == 0)
		segments = 1;
	// Der erste Lane-Abschnitt ist zu kurz und wird übersprungen
	if (segments <= skip)
		return;
	// Teilstücke in 1/10 µGrad
	degNodeGap 	/= segments;
	segments	-= skip;
	// 0° <= Segment-Winkel < 90°
	if (offsetX > 0 && offsetY >= 0) {
		// 0° <= Segment-Winkel < 45° (Winkel zwischen Lane-Segment-Ausrichtung und Abzisse)
		if ((cos = offsetX/nodeGap) > M_SQRT1_2) {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Initiale Referenzwerte
			*maxLat  = nodeLat + (int)ceil(offsetC/2.0) + skip * offsetB;
			*maxLong = nodeLong + skip * offsetA;
			// Grenzen für jedes Teilstück bestimmen
			for (int i = 0; i < segments; i++) {
				*maxLat	 = *maxLat + offsetB;
				*minLat	 = *maxLat - (offsetB + offsetC);
				*minLong = *maxLong;
				*maxLong = *minLong + offsetA;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
		// 45° <= Segment-Winkel < 90°
		else {
			// Winkel zwischen Lane-Segment-Ausrichtung und Ordinate
			cos = offsetY/nodeGap;
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*maxLong = nodeLong + (int)ceil(offsetC/2.0) + skip * offsetB;
			*maxLat	 = nodeLat + skip * offsetA;
			for (int i = 0; i < segments; i++) {
				*maxLong = *maxLong + offsetB;
				*minLong = *maxLong - (offsetB + offsetC);
				*minLat	 = *maxLat;
				*maxLat	 = *minLat + offsetA;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
	}
	// 90° <= Segment-Winkel < 180°
	else if (offsetX <= 0 && offsetY > 0) {
		// 90° <= Segment-Winkel < 135°
		if ((cos = abs(offsetX)/nodeGap) < M_SQRT1_2) {
			cos = offsetY/nodeGap;
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*maxLat	 = nodeLat + skip * offsetA;
			*minLong = nodeLong - ((int)ceil(offsetC/2.0) + skip * offsetB);
			for (int i = 0; i < segments; i++) {
				*minLat	 = *maxLat;
				*maxLat	 = *minLat + offsetA;
				*minLong = *minLong - offsetB;
				*maxLong = *minLong + offsetB + offsetC;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
		// 135° <= Segment-Winkel < 180°
		else {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLong = nodeLong - skip * offsetA;
			*maxLat	 = nodeLat + (int)ceil(offsetC/2.0) + skip * offsetB;
			for (int i = 0; i < segments; i++) {
				*maxLong = *minLong;
				*minLong = *maxLong - offsetA;
				*maxLat	 = *maxLat + offsetB;
				*minLat	 = *maxLat - (offsetB + offsetC);
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
	}
	// 180° <= Segment-Winkel < 270°
	else if (offsetX < 0 && offsetY <= 0) {
		// 180° <= Segment-Winkel < 225°
		if ((cos = abs(offsetX)/nodeGap) > M_SQRT1_2) {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLat	 = nodeLat - ((int)ceil(offsetC/2.0) + skip * offsetB);
			*minLong = nodeLong - skip * offsetA;
			for (int i = 0; i < segments; i++) {
				*maxLong = *minLong;
				*minLong = *maxLong - offsetA;
				*minLat	 = *minLat - offsetB;
				*maxLat	 = *minLat + offsetB + offsetC;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
				
		}
		// 225° <= Segment-Winkel < 270°
		else {
			cos = abs(offsetY)/nodeGap;
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLong = nodeLong - ((int)ceil(offsetC/2.0) + skip * offsetB);
			*minLat	 = nodeLat - skip * offsetA;
			for (int i = 0; i < segments; i++) {
				*minLong = *minLong - offsetB;
				*maxLong = *minLong + offsetB + offsetC;
				*maxLat	 = *minLat;
				*minLat	 = *maxLat - offsetA;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
	}
	// 270° <= Segment-Winkel < 360°
	else if (offsetX >= 0 && offsetY < 0) {
		// 270° <= Segment-Winkel < 315°
		if ((cos = offsetX/nodeGap) < M_SQRT1_2) {
			cos = abs(offsetY)/nodeGap;
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*maxLong = nodeLong + (int)ceil(offsetC/2.0) + skip * offsetB;
			*minLat	 = nodeLat - skip * offsetA;
			for (int i = 0; i < segments; i++) {
				*maxLong = *maxLong + offsetB;
				*minLong = *maxLong - (offsetB + offsetC);
				*maxLat	 = *minLat;
				*minLat	 = *maxLat - offsetA;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
		// 315° <= Segment-Winkel < 360°
		else {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLat	 = nodeLat - ((int)ceil(offsetC/2.0) + skip * offsetB);
			*maxLong = nodeLong + skip * offsetA;
			for (int i = 0; i < segments; i++) {
				*minLong = *maxLong;
				*maxLong = *minLong + offsetA;
				*minLat	 = *minLat - offsetB;
				*maxLat	 = *minLat + offsetB + offsetC;
				mysql_stmt_execute(stmt);
				*segID	+= 1;
			}
		}
	}
}

void setInsertParam(MYSQL_BIND * bind, uint16_t * region, uint16_t * id, 
					uint8_t * laneID, uint8_t * segID, int * maxLong, 
					int * maxLat, int * minLong, int * minLat) {

	bind[0].buffer_type	 = MYSQL_TYPE_SHORT;
	bind[0].buffer		 = (char *)region;
	bind[0].is_null 	 = 0;
	bind[0].length		 = 0;
	
	bind[1].buffer_type	 = MYSQL_TYPE_SHORT;
	bind[1].buffer		 = (char *)id;
	bind[1].is_null 	 = 0;
	bind[1].length		 = 0;
	
	bind[2].buffer_type	 = MYSQL_TYPE_TINY;
	bind[2].buffer		 = (char *)laneID;
	bind[2].is_null 	 = 0;
	bind[2].length		 = 0;
	
	bind[3].buffer_type	 = MYSQL_TYPE_TINY;
	bind[3].buffer		 = (char *)segID;
	bind[3].is_null 	 = 0;
	bind[3].length		 = 0;
	
	bind[4].buffer_type	 = MYSQL_TYPE_LONG;
	bind[4].buffer		 = (char *)maxLong;
	bind[4].is_null 	 = 0;
	bind[4].length		 = 0;
	
	bind[5].buffer_type	 = MYSQL_TYPE_LONG;
	bind[5].buffer		 = (char *)maxLat;
	bind[5].is_null 	 = 0;
	bind[5].length		 = 0;
	
	bind[6].buffer_type	 = MYSQL_TYPE_LONG;
	bind[6].buffer		 = (char *)minLong;
	bind[6].is_null 	 = 0;
	bind[6].length		 = 0;
	
	bind[7].buffer_type	 = MYSQL_TYPE_LONG;
	bind[7].buffer		 = (char *)minLat;
	bind[7].is_null 	 = 0;
	bind[7].length		 = 0;
	
	bind[8].buffer_type	 = MYSQL_TYPE_LONG;
	bind[8].buffer		 = (char *)maxLong;
	bind[8].is_null 	 = 0;
	bind[8].length		 = 0;
	
	bind[9].buffer_type = MYSQL_TYPE_LONG;
	bind[9].buffer		 = (char *)maxLat;
	bind[9].is_null 	 = 0;
	bind[9].length		 = 0;
	
	bind[10].buffer_type = MYSQL_TYPE_LONG;
	bind[10].buffer		 = (char *)minLong;
	bind[10].is_null 	 = 0;
	bind[10].length		 = 0;
	
	bind[11].buffer_type = MYSQL_TYPE_LONG;
	bind[11].buffer		 = (char *)minLat;
	bind[11].is_null 	 = 0;
	bind[11].length		 = 0;
}

void setOffsets(uint8_t * offsetA, uint8_t * offsetB, uint8_t *offsetC,
				double cos, uint16_t degLaneWidth, uint16_t degNodeGap) {
	
	*offsetA = (uint8_t)ceil(cos * degNodeGap);
	*offsetB = (uint8_t)ceil(sqrt((degNodeGap*degNodeGap)-(*offsetA * *offsetA)));
	*offsetC = (uint8_t)ceil(degLaneWidth/cos);
}

double get100thMicroDegree(int elevation) {
	// Radiant für 1 cm
	double rad = asin(100000000.0/(WGS84_RAD + elevation * 10));
	// Umrechnung in 1/100 µGrad
	return ((360/(2*M_PI)) * rad);
}
