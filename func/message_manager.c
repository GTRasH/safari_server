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
		 ** strElevation, ** strLaneWidth, ** strNodeWidth, ** strMaxSpeed,
		 ** strNodeLong, ** strNodeLat, ** temp, ** xmlInter, ** xmlLane, 
		 ** xmlNodes, ** strOffsetX, ** strOffsetY, query[Q_BUF], 
		 interUpdate[MSG_BUF], clientNotify[MSG_BUF], queryStmt[Q_BUF];
	int elevation, interOffset, interMaxLong, interMaxLat, interMinLong,
		interMinLat, nodeLong, nodeLat, maxLong, maxLat, minLong, minLat,
		latitude, longitude, offsetX, offsetY;
	double microDegLat, microDegLong;
	uint16_t region, id, partID, segID, laneWidth, nodeWidth, dist, 
			 maneuvers, maxSpeed = 500;
	uint8_t laneID, segSkip, update = 0;
	msqList * clientPtr;
	msqElement s2c;
	MYSQL_STMT * stmt;
	MYSQL_BIND bind[13];
	MYSQL * dbCon = sqlConnect("safari");
	
	// Nachricht enthält keine IntersectionGeometry-Elemente
	if (!xmlContains(message, "//IntersectionGeometry"))
		return 1;

	// Aktualisierungs-Benachrichtung für die Clients vorbereiten
	strcpy(clientNotify, XML_TAG);
	strcat(clientNotify, "<mapUpdate>\n");

	// Datenbank für Segment-Update vorbereiten
	sprintf(queryStmt,"%s", "INSERT INTO segments "
							"(region, id, laneID, partID, segID, maxLong, maxLat, minLong, minLat) "
							"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
							"ON DUPLICATE KEY UPDATE maxLong=?, maxLat=?, minLong=?, minLat=?");
	stmt = mysql_stmt_init(dbCon);
	mysql_stmt_prepare(stmt, queryStmt, strlen(queryStmt));
	memset(bind, 0, sizeof(bind));
	setInsertParam(bind, &region, &id, &laneID, &partID, &segID, &maxLong, &maxLat, &minLong, &minLat);
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
		latitude	 = (int)strtol(*(interLat),NULL,10);
		longitude	 = (int)strtol(*(interLong),NULL,10);

		get100thMicroDegree(elevation, latitude, &microDegLat, &microDegLong);
		
		interOffset  = microDegLat * INTER_AREA * 10;
		
		interMaxLat	 = latitude + interOffset;
		interMinLat  = latitude - interOffset;

		interOffset  = microDegLong * INTER_AREA * 10;
		
		interMaxLong = longitude + interOffset;
		interMinLong = longitude - interOffset;
		
		// Intersection auf der DB aktualisieren
		sprintf	(query, 
				"INSERT INTO intersections "
				"(region, id, maxLong, maxLat, minLong, minLat, elevation) "
				"VALUES ('%u', '%u', '%i', '%i', '%i', '%i', '%i') "
				"ON DUPLICATE KEY UPDATE maxLong = '%i', maxLat = '%i', "
				"minLong = '%i', minLat = '%i', elevation = '%i'", region, id, 
				interMaxLong, interMaxLat, interMinLong, interMinLat, elevation,
				interMaxLong, interMaxLat, interMinLong, interMinLat, elevation);

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
				continue;
			}
			// Zurücksetzen und Setzen des Maneuver-Bit-Value
			maneuvers = 0;
			if (xmlContains(xmlLaneDoc, "//maneuverStraightAllowed"))
				maneuvers += 1;
			if (xmlContains(xmlLaneDoc, "//maneuverLeftAllowed"))
				maneuvers += 2;
			if (xmlContains(xmlLaneDoc, "//maneuverRightAllowed"))
				maneuvers += 4;
				
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
					dist		= 0;
					freeArray(strNodeLong);
					freeArray(strNodeLat);
					
					// Zulässige Höchstgeschwindigkeit auf der Lane (1. Lane-Abschnitt)
					if (!xmlContains(xmlNodeDoc, "//vehicleMaxSpeed"))
						maxSpeed	= 500;
					else {
						strMaxSpeed = getNodeValue(xmlNodeDoc, "//speed");
						maxSpeed	= (uint16_t)strtol(*(strMaxSpeed),NULL,10);
						freeArray(strMaxSpeed);
					}
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
					offsetX		= (int)strtol(*(strOffsetX), NULL, 10);
					offsetY		= (int)strtol(*(strOffsetY), NULL, 10);
					partID		= k-1;
					segID		= 0;
					sprintf (query, 
							"INSERT INTO lanes "
							"(region, id, laneID, partID, distance, maxSpeed, longitude, latitude, maneuvers) "
							"VALUES ('%u', '%u', '%u', '%u', '%i', '%i', '%i', '%i', '%u') "
							"ON DUPLICATE KEY UPDATE distance = '%u', maxSpeed = '%u', "
							"longitude = '%i', latitude = '%i', maneuvers = '%u'", region, id, laneID, partID, dist, 
							maxSpeed, nodeLong, nodeLat, maneuvers, dist, maxSpeed, nodeLong, nodeLat, maneuvers);

					if (mysql_query(dbCon, query))
						sqlError(dbCon);
					
					setSegments(stmt, &segID, &maxLong, &maxLat, &minLong, 
								&minLat, microDegLat, microDegLong, nodeWidth, segSkip, 
								nodeLong, nodeLat, offsetX, offsetY);

					// Abstand zum Referenzpunkt (Node-LLmD-64b) in cm
					dist	 += (uint16_t)sqrt(offsetX*offsetX + offsetY*offsetY);
					
					segSkip	  = 0;
					// neuer Referenzpunkt für den nächsten Node
					nodeLong += (microDegLong*offsetX)/10;
					nodeLat	 += (microDegLat*offsetY)/10;
					freeArray(strOffsetX);
					freeArray(strOffsetY);
					// Zulässige Höchstgeschwindigkeit auf der Lane (weitere Lane-Abschnitte)
					if (xmlContains(xmlNodeDoc, "//vehicleMaxSpeed")) {
						strMaxSpeed = getNodeValue(xmlNodeDoc, "//speed");
						maxSpeed	= (uint16_t)strtol(*(strMaxSpeed),NULL,10);
						freeArray(strMaxSpeed);
					}
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
		memset(s2c.message, 0, sizeof(s2c.message));
		sprintf(s2c.message, "%s", clientNotify);
		while (clientPtr != NULL) {
			msgsnd(clientPtr->id, &s2c, MSQ_LEN, IPC_NOWAIT);
			// Client-MSQ wurde während der Verarbeitung gelöscht
			if (errno == EIDRM)
				clients = msqListRemove(clientPtr->id, clients);

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
	char ** stateRaw, ** stateXML, ** moy, ** mSec;
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
		// Die Nachricht ist zu groß für ein MSQ-Element
		if (strlen(*(stateXML)) > MSQ_LEN) {
			xmlFreeDoc(ptrSPAT);
			continue;
		}
		// Versenden der Nachricht an registrierte Client-Prozesse
		memset(s2c.message, 0, sizeof(s2c.message));
		s2c.prio	= 2;
		sprintf(s2c.message, "%s", *(stateXML+i));
		while (clientPtr != NULL) {
			msgsnd(clientPtr->id, &s2c, MSQ_LEN, IPC_NOWAIT);
			// Client-MSQ wurde während der Verarbeitung gelöscht
			if (errno == EIDRM)
				clients = msqListRemove(clientPtr->id, clients);

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

void setSegments(MYSQL_STMT * stmt, uint16_t * segID, int * maxLong, 
				 int * maxLat, int * minLong, int * minLat, double microDegLat,
				 double microDegLong, uint16_t laneWidth, uint8_t skip, 
				 int nodeLong, int nodeLat, int offsetX, int offsetY) {
	
	long int degOffsetX, degOffsetY;
	uint32_t nodeGap, degNodeGap, degGap;
	uint16_t degLaneWidth, segments;
	uint16_t offsetA, offsetB, offsetC;
	double adjacent, opposite, cos, degAdjacent, degOpposite, ratio;
	
	// Umrechnung der Offset-Werte von cm in 1/100 µGrad
	degOffsetX = (long int)(offsetX * microDegLong);
	degOffsetY = (long int)(offsetY * microDegLat);
	
	// Verhältnis der Wertigkeit von Längen- und Breitengrad
	ratio = microDegLong / microDegLat;
	
	nodeGap	= (uint32_t)sqrt((offsetX*offsetX)+(offsetY*offsetY));
	// Strecke zwischen Bezugs- und Offset-Node in 1/100 µGrad
	degGap	= (uint32_t)sqrt((degOffsetX*degOffsetX)+(degOffsetY*degOffsetY));
	
	// Anzahl der Teilstücke der Strecke zwischen Bezugs- und Offset-Node
	// Sollte das Segment < SEG_PART sein, wird ein Teilstück gebildet.
	if ((segments = (uint16_t)ceil(degGap/(SEG_PART*microDegLat))) == 0)
		segments = 1;
	// Der erste Lane-Abschnitt ist zu kurz und wird übersprungen
	if (segments <= skip)
		return;
	// Teilstücke in 1/10 µGrad
	degNodeGap 	= degGap/segments;
	// 0° <= Segment-Winkel < 90°
	if (offsetX > 0 && offsetY >= 0) {
		// 0° <= Segment-Winkel < 45° (Winkel zwischen Lane-Segment-Ausrichtung und Abzisse)
		if ((cos = offsetX/(double)nodeGap) > M_SQRT1_2) {
			// Berechnung der Spur-Breite
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLat;
			degOpposite	 = opposite * microDegLong;
			degLaneWidth = (uint16_t)sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	= (offsetA * ratio)/10;
			offsetB /= 10;
			offsetC /= 10;
			// # # # Initiale Referenzwerte # # #
			// Im 1. Lane-Segment werden Teilstücke vom Start abgezogen
			if (skip > 0) {
				segments -= skip;
				*maxLat  = nodeLat + (int)ceil(offsetC/2.0) + skip * offsetB;
				*maxLong = nodeLong + skip * offsetA;
			// In den Folgesegmenten an den Start hinzugefügt
			} else {
				segments += SEG_ADD;
				*maxLat  = nodeLat + (int)ceil(offsetC/2.0) - (SEG_ADD * offsetB);
				*maxLong = nodeLong - (SEG_ADD * offsetA);
			}
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
			cos = offsetY/(double)nodeGap;
			// Berechnung der Spur-Breite
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLong;
			degOpposite	 = opposite * microDegLat;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	/= 10;
			offsetB = (offsetB * ratio)/10;
			offsetC = (offsetC * ratio)/10;
			if (skip > 0) {
				segments -= skip;
				*maxLong = nodeLong + (int)ceil(offsetC/2.0) + skip * offsetB;
				*maxLat	 = nodeLat + skip * offsetA;
			} else {
				segments += SEG_ADD;
				*maxLong = nodeLong + (int)ceil(offsetC/2.0) - (SEG_ADD * offsetB);
				*maxLat	 = nodeLat - (SEG_ADD * offsetA);
			}
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
		if ((cos = abs(offsetX)/(double)nodeGap) < M_SQRT1_2) {
			cos = offsetY/(double)nodeGap;
			// Berechnung der Spur-Breite
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLong;
			degOpposite	 = opposite * microDegLat;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	/= 10;
			offsetB = (offsetB * ratio)/10;
			offsetC = (offsetC * ratio)/10;
			if (skip > 0) {
				segments -= skip;
				*maxLat	 = nodeLat + skip * offsetA;
				*minLong = nodeLong - ((int)ceil(offsetC/2.0) + skip * offsetB);
			} else {
				segments += SEG_ADD;
				*maxLat	 = nodeLat - (SEG_ADD * offsetA);
				*minLong = nodeLong - (int)ceil(offsetC/2.0) + (SEG_ADD * offsetB);
			}
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
			// Berechnung der Spur-Breite
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLat;
			degOpposite	 = opposite * microDegLong;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	= (offsetA * ratio)/10;
			offsetB /= 10;
			offsetC /= 10;
			if (skip > 0) {
				segments -= skip;
				*minLong = nodeLong - skip * offsetA;
				*maxLat	 = nodeLat + (int)ceil(offsetC/2.0) + skip * offsetB;
			} else {
				segments += SEG_ADD;
				*minLong = nodeLong + SEG_ADD * offsetA;
				*maxLat	 = nodeLat + (int)ceil(offsetC/2.0) - SEG_ADD * offsetB;
			}
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
		if ((cos = abs(offsetX)/(double)nodeGap) > M_SQRT1_2) {
			// Berechnung der Spur-Breite in 1/100 µGrad (abhänig von der Ausrichtung)
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLat;
			degOpposite	 = opposite * microDegLong;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	= (offsetA * ratio)/10;
			offsetB /= 10;
			offsetC /= 10;
			if (skip > 0) {
				segments -= skip;
				*minLat	 = nodeLat - ((int)ceil(offsetC/2.0) + skip * offsetB);
				*minLong = nodeLong - skip * offsetA;
			} else {
				segments += SEG_ADD;
				*minLat	 = nodeLat - (int)ceil(offsetC/2.0) + SEG_ADD * offsetB;
				*minLong = nodeLong + SEG_ADD * offsetA;
			}
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
			cos = abs(offsetY)/(double)nodeGap;
			// Berechnung der Spur-Breite in 1/100 µGrad (abhänig von der Ausrichtung)
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLong;
			degOpposite	 = opposite * microDegLat;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	/= 10;
			offsetB = (offsetB * ratio)/10;
			offsetC = (offsetC * ratio)/10;
			if (skip > 0) {
				segments -= skip;
				*minLong = nodeLong - ((int)ceil(offsetC/2.0) + skip * offsetB);
				*minLat	 = nodeLat - skip * offsetA; 
			} else {
				segments += SEG_ADD;
				*minLong = nodeLong - (int)ceil(offsetC/2.0) + SEG_ADD * offsetB;
				*minLat	 = nodeLat + SEG_ADD * offsetA;
			}
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
		if ((cos = offsetX/(double)nodeGap) < M_SQRT1_2) {
			cos = abs(offsetY)/(double)nodeGap;
			// Berechnung der Spur-Breite in 1/100 µGrad (abhänig von der Ausrichtung)
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLong;
			degOpposite	 = opposite * microDegLat;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	/= 10;
			offsetB = (offsetB * ratio)/10;
			offsetC = (offsetC * ratio)/10;
			if (skip > 0) {
				segments -= skip;
				*maxLong = nodeLong + (int)ceil(offsetC/2.0) + skip * offsetB;
				*minLat	 = nodeLat - skip * offsetA;
			} else {
				segments += SEG_ADD;
				*maxLong = nodeLong + (int)ceil(offsetC/2.0) - SEG_ADD * offsetB;
				*minLat	 = nodeLat + SEG_ADD * offsetA;
			}
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
			// Berechnung der Spur-Breite in 1/100 µGrad (abhänig von der Ausrichtung)
			adjacent	 = cos * laneWidth;
			opposite	 = sqrt(laneWidth*laneWidth - adjacent*adjacent);
			degAdjacent  = adjacent * microDegLat;
			degOpposite	 = opposite * microDegLong;
			degLaneWidth = sqrt(degAdjacent*degAdjacent + degOpposite*degOpposite);
			// Berechnung der Offset-Werte
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Verhältnis von Längen- und Breitengrad einbeziehen und Umrechnung in 1/10 µGrad
			offsetA	= (offsetA * ratio)/10;
			offsetB /= 10;
			offsetC /= 10;
			if (skip > 0) {
				segments -= skip;
				*minLat	 = nodeLat - ((int)ceil(offsetC/2.0) + skip * offsetB);
				*maxLong = nodeLong + skip * offsetA;
			} else {
				segments += SEG_ADD;
				*minLat	 = nodeLat - (int)ceil(offsetC/2.0) + SEG_ADD * offsetB;
				*maxLong = nodeLong - SEG_ADD * offsetA;
			}
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
					uint8_t * laneID, uint16_t * partID, uint16_t * segID,
					int * maxLong, int * maxLat, int * minLong, int * minLat) {

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
	
	bind[3].buffer_type	 = MYSQL_TYPE_SHORT;
	bind[3].buffer		 = (char *)partID;
	bind[3].is_null 	 = 0;
	bind[3].length		 = 0;
	
	bind[4].buffer_type	 = MYSQL_TYPE_SHORT;
	bind[4].buffer		 = (char *)segID;
	bind[4].is_null 	 = 0;
	bind[4].length		 = 0;
	
	bind[5].buffer_type	 = MYSQL_TYPE_LONG;
	bind[5].buffer		 = (char *)maxLong;
	bind[5].is_null 	 = 0;
	bind[5].length		 = 0;
	
	bind[6].buffer_type	 = MYSQL_TYPE_LONG;
	bind[6].buffer		 = (char *)maxLat;
	bind[6].is_null 	 = 0;
	bind[6].length		 = 0;
	
	bind[7].buffer_type	 = MYSQL_TYPE_LONG;
	bind[7].buffer		 = (char *)minLong;
	bind[7].is_null 	 = 0;
	bind[7].length		 = 0;
	
	bind[8].buffer_type	 = MYSQL_TYPE_LONG;
	bind[8].buffer		 = (char *)minLat;
	bind[8].is_null 	 = 0;
	bind[8].length		 = 0;
	
	bind[9].buffer_type	 = MYSQL_TYPE_LONG;
	bind[9].buffer		 = (char *)maxLong;
	bind[9].is_null 	 = 0;
	bind[9].length		 = 0;
	
	bind[10].buffer_type = MYSQL_TYPE_LONG;
	bind[10].buffer		 = (char *)maxLat;
	bind[10].is_null 	 = 0;
	bind[10].length		 = 0;
	
	bind[11].buffer_type = MYSQL_TYPE_LONG;
	bind[11].buffer		 = (char *)minLong;
	bind[11].is_null 	 = 0;
	bind[11].length		 = 0;
	
	bind[12].buffer_type = MYSQL_TYPE_LONG;
	bind[12].buffer		 = (char *)minLat;
	bind[12].is_null 	 = 0;
	bind[12].length		 = 0;
}

void setOffsets(uint16_t * offsetA, uint16_t * offsetB, uint16_t *offsetC,
				double cos, uint16_t degLaneWidth, uint32_t degNodeGap) {
	
	*offsetA = (uint16_t)floor(cos * degNodeGap);
	*offsetB = (uint16_t)ceil(sqrt((degNodeGap*degNodeGap)-(*offsetA * *offsetA)));
	*offsetC = (uint16_t)ceil(degLaneWidth/cos);
}
