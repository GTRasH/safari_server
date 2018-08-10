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
		 ** strElevation, ** strLaneWidth, ** strNodeWidth,
		 ** strNodeLong, ** strNodeLat, ** temp, ** xmlInter, ** xmlLane, 
		 ** xmlNodes, ** strOffsetX, ** strOffsetY;
	int elevation, interOffset, interMaxLong, interMaxLat, interMinLong,
		interMinLat, nodeLong, nodeLat, maxLong, maxLat, minLong, minLat;
	double microDegree;
	uint16_t region, id, laneWidth, nodeWidth, offsetX, offsetY;
	uint8_t laneID, segID, nodeID, segPartSkip, update = 0;
	char query[Q_BUF], interUpdate[MSG_BUF], clientNotify[MSG_BUF];
	msqList * clientPtr;
	msqElement s2c;
	MYSQL * dbCon = sqlConnect("safari");
	
	// Aktualisierung-Benachrichtung für die Clients vorbereiten
	strcpy(clientNotify, XML_TAG);
	strcat(clientNotify, "<mapUpdate>\n");
	// Jedes Element von geometry ist ein gültiger XML-String
	if ((temp = getTree(message, "//IntersectionGeometry")) == NULL)
		return 1;

	MYSQL_STMT * stmt;
	MYSQL_BIND bind[13];
	// Datenbank Update vorbereiten
	char * qStmt =	"INSERT INTO segments "
					"(region, id, laneID, nodeID, segID, maxLong, maxLat, minLong, minLat) "
					"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
					"ON DUPLICATE KEY UPDATE maxLong=?, maxLat=?, minLong=?, minLat=?";
	stmt = mysql_stmt_init(dbCon);
	mysql_stmt_prepare(stmt, qStmt, strlen(qStmt));
	memset(bind, 0, sizeof(bind));
	setParamBind(bind, &region, &id, &laneID, &nodeID, &segID, &maxLong, &maxLat, &minLong, &minLat);
	mysql_stmt_bind_param(stmt, bind);

	xmlInter = getWellFormedXML(temp);
	freeArray(temp);

	for (int i = 0; *(xmlInter+i); ++i) {
		// IntersectionGeometry-Daten auslesen
		xmlInterDoc	 = xmlReadMemory(*(xmlInter+i), strlen(*(xmlInter+i)), NULL, NULL, 0);
		strRegion	 = getNodeValue(xmlInterDoc, "//id/region");
		strId		 = getNodeValue(xmlInterDoc, "//id/id");
		interLong	 = getNodeValue(xmlInterDoc, "//refPoint/long");
		interLat	 = getNodeValue(xmlInterDoc, "//refPoint/lat");
		strElevation = getNodeValue(xmlInterDoc, "//refPoint/elevation");
		strLaneWidth = getNodeValue(xmlInterDoc, "//laneWidth");
		// IntersectionReferenceID oder refPoint nicht gefunden -> nächste Intersection
		if (strRegion == NULL || strId == NULL || interLong == NULL 
			|| interLat == NULL || strLaneWidth == NULL) {
			xmlFreeDoc(xmlInterDoc);
			freeArray(strLaneWidth);
			freeArray(strElevation);
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
		elevation	 = (strElevation == NULL) ? 0 :
						(int)strtol(*(strElevation),NULL,10);
		microDegree	 = get100thMicroDegree(elevation);
		interOffset	 = microDegree*INTER_AREA;
		interMaxLong = (int)strtol(*(interLong),NULL,10) + interOffset;
		interMaxLat	 = (int)strtol(*(interLat),NULL,10) + interOffset;
		interMinLong = (int)strtol(*(interLong),NULL,10) - interOffset;
		interMinLat  = (int)strtol(*(interLat),NULL,10) - interOffset;
		
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
		update		= 1;
		segPartSkip	= SEG_SKIP;
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
			laneID	 = (uint8_t)strtol(*(strLaneId),NULL,10);
			segID	 = 0;
			xmlNodes = getWellFormedXML(temp);
			freeArray(temp);
			for (int k = 0; *(xmlNodes+k); ++k) {
				xmlNodeDoc	 = xmlReadMemory(*(xmlNodes+k), strlen(*(xmlNodes+k)),
											 NULL, NULL, 0);
				strNodeWidth = getNodeValue(xmlNodeDoc, "//dWidth");
				nodeWidth = (strNodeWidth != NULL) ?
					(uint16_t)strtol(*(strNodeWidth),NULL,10) + laneWidth :
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
							"VALUES ('%u', '%u', '%u', '%i', '%i') "
							"ON DUPLICATE KEY UPDATE longitude = '%i', "
							"latitude = '%i'", region, id, laneID, 
							nodeLong, nodeLat, nodeLong, nodeLat);
							
					if (mysql_query(dbCon, query))
						sqlError(dbCon);
					freeArray(strNodeLong);
					freeArray(strNodeLat);
				}
				else
				{
					strOffsetX	= getNodeValue(xmlNodeDoc, "//x");
					strOffsetY	= getNodeValue(xmlNodeDoc, "//y");
					if (strOffsetX == NULL || strOffsetY == NULL) {
						xmlFreeDoc(xmlNodeDoc);
						freeArray(strOffsetX);
						freeArray(strOffsetY);
						break;
					}
					offsetX	= (int)strtol(*(strOffsetX), NULL, 0);
					offsetY	= (int)strtol(*(strOffsetY), NULL, 0);
					setSegments(stmt, &segID, &maxLong, &maxLat, &minLong, &minLat,
								microDegree, nodeWidth, segPartSkip, 
								nodeLong, nodeLat, offsetX, offsetY);
					// neuer Referenzpunkt für den nächsten Node
					segPartSkip	= 0;
					nodeLong	+= (int)(microDegree*offsetX);
					nodeLat		+= (int)(microDegree*offsetY);
					freeArray(strOffsetX);
					freeArray(strOffsetY);
				}
				freeArray(strNodeWidth);
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
		freeArray(strElevation);
	}
	// Benachrichtigung der Clients
	if (update == 1) {
		clientPtr	= clients;
		s2c.prio	= 2;
		strcat(clientNotify, "</mapUpdate>");
		clientNotify[strlen(clientNotify)] = '\0';
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
	int res;
	xmlDocPtr ptrSPAT;
	char ** stateRaw, ** stateXML, ** moy, ** mSec;
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
		s2c.prio	= 2;
		sprintf(s2c.message, "%s", *(stateXML+i));
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
	degNodeGap /= segments;
	segments -= skip;
	// 0° <= Segment-Winkel < 90°
	if (offsetX > 0 && offsetY >= 0) {
		// 0° <= Segment-Winkel < 45°
		if ((cos = offsetX/nodeGap) > M_SQRT1_2) {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			// Grenzen für jedes Teilstück bestimmen
			*maxLat  = nodeLat + (int)ceil(offsetC/2.0) + skip * offsetB;
			*maxLong = nodeLong + skip * offsetA;
			while (*segID < segments) {
				*segID	+= 1;
				*maxLat	 = *maxLat + offsetB;
				*minLat	 = *maxLat - (offsetB + offsetC);
				*minLong = *maxLong;
				*maxLong = *minLong + offsetA;
				mysql_stmt_execute(stmt);
			}
		}
		// 45° <= Segment-Winkel < 90°
		else {
			// Winkel zwischen Lane-Segment-Ausrichtung und Ordinate
			cos = offsetY/nodeGap;
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*maxLong = nodeLong + (int)ceil(offsetC/2.0) + skip * offsetB;
			*maxLat	 = nodeLat + skip * offsetA;
			while (*segID < segments) {
				*segID	+= 1;
				*maxLong = *maxLong + offsetB;
				*minLong = *maxLong - (offsetB + offsetC);
				*minLat	 = *maxLat;
				*maxLat	 = *minLat + offsetA;
				mysql_stmt_execute(stmt);
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
			while (*segID < segments) {
				*segID	+= 1;
				*minLat	 = *maxLat;
				*maxLat	 = *minLat + offsetA;
				*minLong = *minLong - offsetB;
				*maxLong = *minLong + offsetB + offsetC;
				mysql_stmt_execute(stmt);
			}
		}
		// 135° <= Segment-Winkel < 180°
		else {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLong = nodeLong - skip * offsetA;
			*maxLat	 = nodeLat + (int)ceil(offsetC/2.0) + skip * offsetB;
			while (*segID < segments) {
				*segID	+= 1;
				*maxLong = *minLong;
				*minLong = *maxLong - offsetA;
				*maxLat	 = *maxLat + offsetB;
				*minLat	 = *maxLat - (offsetB + offsetC);
				mysql_stmt_execute(stmt);
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
			while (*segID < segments) {
				*segID	+= 1;
				*maxLong = *minLong;
				*minLong = *maxLong - offsetA;
				*minLat	 = *minLat - offsetB;
				*maxLat	 = *minLat + offsetB + offsetC;
				mysql_stmt_execute(stmt);
			}
				
		}
		// 225° <= Segment-Winkel < 270°
		else {
			cos = abs(offsetY)/nodeGap;
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLong = nodeLong - ((int)ceil(offsetC/2.0) + skip * offsetB);
			*minLat	 = nodeLat - skip * offsetA;
			while (*segID < segments) {
				*segID	+= 1;
				*minLong = *minLong - offsetB;
				*maxLong = *minLong + offsetB + offsetC;
				*maxLat	 = *minLat;
				*minLat	 = *maxLat - offsetA;
				mysql_stmt_execute(stmt);
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
			while (*segID < segments) {
				*segID	+= 1;
				*maxLong = *maxLong + offsetB;
				*minLong = *maxLong - (offsetB + offsetC);
				*maxLat	 = *minLat;
				*minLat	 = *maxLat - offsetA;
				mysql_stmt_execute(stmt);
			}
		}
		// 315° <= Segment-Winkel < 360°
		else {
			setOffsets(&offsetA, &offsetB, &offsetC, cos, degLaneWidth, degNodeGap);
			*minLat	 = nodeLat - ((int)ceil(offsetC/2.0) + skip * offsetB);
			*maxLong = nodeLong + skip * offsetA;
			while (*segID < segments) {
				*segID	+= 1;
				*minLong = *maxLong;
				*maxLong = *minLong + offsetA;
				*minLat	 = *minLat - offsetB;
				*maxLat	 = *minLat + offsetB + offsetC;
				mysql_stmt_execute(stmt);
			}
		}
	}
}

void setParamBind(MYSQL_BIND * bind, uint16_t * region, uint16_t * id, 
				  uint8_t * laneID, uint8_t * nodeID, uint8_t * segID, 
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
	
	bind[3].buffer_type	 = MYSQL_TYPE_TINY;
	bind[3].buffer		 = (char *)nodeID;
	bind[3].is_null 	 = 0;
	bind[3].length		 = 0;
	
	bind[4].buffer_type	 = MYSQL_TYPE_TINY;
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

void setPrepStmt(char * query, MYSQL_STMT * stmt, MYSQL * dbCon) {
	stmt = mysql_stmt_init(dbCon);
	if (!stmt)
	{
	  fprintf(stderr, " mysql_stmt_init(), out of memory\n");
	  exit(0);
	}
/*	if (mysql_stmt_prepare(stmt, q, strlen(q)))
	{
	  fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
	  fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
	  exit(0);
	}
	*/
	fprintf(stdout, " prepare, INSERT successful\n");
	
}

void setOffsets(uint8_t * offsetA, uint8_t * offsetB, uint8_t *offsetC,
				double cos, uint16_t degLaneWidth, uint16_t degNodeGap) {
	
	*offsetA = (uint8_t)ceil(cos * degNodeGap);
	*offsetB = (uint8_t)ceil(sqrt((degNodeGap*degNodeGap)-(*offsetA * *offsetA)));
	*offsetC = (uint8_t)ceil(degLaneWidth/cos);
}

double get100thMicroDegree(int elevation) {
	// Radiant für 1 cm
	double degree = asin(100000000.0/(WGS84_RAD + elevation * 10));
	// Umrechnung in 1/100 µGrad
	return (360/(2*M_PI))*degree;
}
