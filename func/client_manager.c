#include <client_manager.h>

const char * moveTypes[] = {"unknown", "feet", "bike", "motor"};

sighandler_t mySignal(int sigNr, sighandler_t signalHandler) {
	struct sigaction sigOld, sigNew;
	
	sigNew.sa_handler	= signalHandler;
	sigemptyset(&sigNew.sa_mask);
	sigNew.sa_flags		= SA_RESTART;

	if (sigaction(sigNr, &sigNew, &sigOld) < 0)
		return SIG_ERR;

	return sigOld.sa_handler;
}

int setClientInit(int sock, clientStruct * client) {
	int (*func) (char *, clientStruct *);
	char * reqAuth, * reqServ, * reqLoc, logText[LOG_BUF];
	int result = 1;
	reqAuth = getFileContent(REQ_AUTH);
	// Fehler beim Lesen der Auth-Request File
	if (reqAuth == NULL) {
		sprintf(logText,"Error while getting REQ_AUTH (%s) content\n", REQ_AUTH);
		setLogText(logText, LOG_SERVER);
	
	} else {
		// Sonst: Sende Authentifizierungs-Anforderung
		func = setClientAuth;
		if (getClientResponse(sock, MAX_LOGIN, func, reqAuth, client)) {
			sprintf(logText, "[%u]   Max. failed login requests (%i) reached\n",
					client->pid, MAX_LOGIN);
			setLogText(logText, LOG_CLIENT);
		
		} else {
			reqServ = getFileContent(REQ_SERV);
			// Fehler beim Lesen der Loc-Request File
			if (reqServ == NULL) {
				sprintf(logText, "Error while getting REQ_SERV (%s) content\n", REQ_SERV);
				setLogText(logText, LOG_SERVER);
			
			} else {
				func = setClientServices;
				if (getClientResponse(sock, MAX_SERV, func, reqAuth, client)) {
					sprintf(logText, "[%s]   Max. failed service requests (%i) reached\n",
					client->name, MAX_SERV);
					setLogText(logText, LOG_CLIENT);
				} else {
					reqLoc = getFileContent(REQ_LOC);
					// Fehler beim Lesen der Loc-Request File
					if (reqLoc == NULL) {
						sprintf(logText, "Error while getting REQ_LOC (%s) content\n", REQ_LOC);
						setLogText(logText, LOG_SERVER);
					
					} else {
						// Sonst: Sende Lokalisierungs-Anforderung
						func = setClientLocation;
						if (getClientResponse(sock, MAX_LOC, func, reqLoc, client)) {
							sprintf(logText, "[%s]   Max. failed location requests (%i) reached\n",
									client->name, MAX_LOC);
							setLogText(logText, LOG_CLIENT);
							
						} else
							result = 0;
					} // eo request location
					free(reqLoc);
				}
			} // eo request services
			free(reqServ);
		}
	} // eo request authorisation
	free(reqAuth);

	return result;
}

int getClientResponse(	int sock, int retries, 
						int (* func) (char *, clientStruct *), 
						char * message, clientStruct * client) {
	char * resp;
	int result;
	long unsigned msgSize;
	
	if (message == NULL)
		return 1;
	
	result	= 0;
	msgSize = strlen(message);
	
	// sende den Request bis zu `retries` Mal
	do {
		// TESTING
		//printf ("# # #   SENDE NACHRICHT AN CLIENT   # # #\n%s\n",
		//		message);
		setSocketContent(sock, message, msgSize);
		resp = getSocketContent(sock);
		if (resp == NULL) {
			free(resp);
			result = 1;
			break;
		}
		// TESTING
		printf ("# # #   NACHRICHT VON CLIENT EMPFANGEN   # # #\n%s\n",resp);
		result = func(resp, client);
		free(resp);
	} while (result && --retries);

	return result;
}

int setClientResponse(char * msg, clientStruct * client) {
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	// Client-Antwort enthält Positionsdaten
	if (xmlContains(xmlMsg, "//refPoint/long")) {
		xmlFreeDoc(xmlMsg);
		return setClientLocation(msg, client);
	}
	// Client-Antwort enthält Services
	if (xmlContains(xmlMsg, "//service")) {
		xmlFreeDoc(xmlMsg);
		return setClientServices(msg, client);
	}
	// Client hat weder mit Positionsdaten noch mit Services geantwortet
	xmlFreeDoc(xmlMsg);
	return 1;
}

int setClientLocation(char * msg, clientStruct * client) {
	char ** longitude, ** latitude;
	int moy, mSec;
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	// Ungültiger Location-Response
	if (!(xmlContains(xmlMsg, "//refPoint/long") && xmlContains(xmlMsg, "//refPoint/lat"))) {
		xmlFreeDoc(xmlMsg);
		return 1;
	}
	longitude = getNodeValue(xmlMsg, "//refPoint/long");
	latitude  = getNodeValue(xmlMsg, "//refPoint/lat");
	getTimestamp(&moy, &mSec);
	client->update.mSec	  = mSec;
	client->update.moy	  = moy;
	client->pos.latitude  = strtol(*(latitude), NULL, 10);
	client->pos.longitude = strtol(*(longitude), NULL, 10);
	freeArray(longitude);
	freeArray(latitude);
	xmlFreeDoc(xmlMsg);
	return 0;
}

int setClientAuth(char * msg, clientStruct * client) {
	int result;
	char query[Q_BUF], logText[LOG_BUF], ** user, ** pass;
	xmlDocPtr xmlMsg;
	MYSQL * dbCon;
	MYSQL_RES * dbResult;

	result = 1;
	dbCon  = sqlConnect("safari");
	xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	// Ungültiger Auth-Response
	if (!(xmlContains(xmlMsg, "//data/user") && xmlContains(xmlMsg, "//data/pass"))) {
		xmlFreeDoc(xmlMsg);
		mysql_close(dbCon);
		return result;
	}
	user = getNodeValue(xmlMsg, "//data/user");
	pass = getNodeValue(xmlMsg, "//data/pass");
	sprintf(logText, "[%u]   Receiving login data\n", client->pid);
	setLogText(logText, LOG_CLIENT);

	sprintf(query,	"SELECT name FROM users WHERE name = '%s' AND "
					"pass = AES_ENCRYPT('%s', SHA2('safari', 512))",
					*(user), *(pass));
	
	if (mysql_query(dbCon, query))
		sqlError(dbCon);
	if ((dbResult = mysql_store_result(dbCon)) == NULL)
		sqlError(dbCon);
	
	if (mysql_num_rows(dbResult) != 1)
		sprintf(logText, "[%u]   Login failed\n", client->pid);
	else {
		strcpy(client->name, mysql_fetch_row(dbResult)[0]);
		sprintf(logText, "[%s]   User '%s' successfully logged in\n",
				client->name, client->name);
		result = 0;
	}
	setLogText(logText, LOG_CLIENT);
	
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(user);
	freeArray(pass);
	return result;
}

int setClientServices(char * msg, clientStruct * client) {
	uint8_t servReqID, servRowID, typeID;
	MYSQL * dbCon;
	MYSQL_RES * dbResult;
	MYSQL_ROW row;
	xmlDocPtr xmlMsg;
	char query[Q_BUF], logText[LOG_BUF], servName[LOG_BUF], * tmp,
		 ** services, ** region, ** type;

	dbCon		= sqlConnect("safari");
	xmlMsg		= xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	servName[0]	= '\0';
	// Ungültiger Service-Response
	if (!(xmlContains(xmlMsg, "//service") && xmlContains(xmlMsg, "//cityID") &&
		  xmlContains(xmlMsg, "//movementType"))) {
		xmlFreeDoc(xmlMsg);
		mysql_close(dbCon);
		return 1;
	}
	services = getNodeValue(xmlMsg, "//service");
	region	 = getNodeValue(xmlMsg, "//cityID");
	type	 = getNodeValue(xmlMsg, "//movementType");
	// TESTING - Log-Update
	sprintf(logText,"[%s]   Receiving vehicle type and service "
					"request\n", client->name);
	setLogText(logText, LOG_CLIENT);
	// Prüfe die angeforderten Dienste gegen die für den Anbieter hinterlegten
	sprintf(query,	"SELECT serviceid FROM services "
					"WHERE cityid = '%s'", *(region));
	
	mysql_query(dbCon, query);
	dbResult = mysql_store_result(dbCon);

	client->serviceMask = 0;
	
	while ((row = mysql_fetch_row(dbResult))) {
		for (int i = 0; *(services+i) != NULL; i++) {
			servReqID = (uint8_t)strtol(*(services+i), NULL, 10);
			servRowID = (uint8_t)strtol(row[0], NULL, 10);
			if (servRowID == servReqID) {
				client->serviceMask += servReqID;
				tmp = getServiceName(servReqID);
				if (strlen(servName) == 0)
					strcpy(servName, tmp);
				else {
					strcat(servName, " | ");
					strcat(servName, tmp);
				}
				free(tmp);
			}
		}
	}
	// Mindestens ein Dienst wurde bestätigt
	if (client->serviceMask != 0) {
		sprintf(logText,"[%s]   Verified services: %s\n", client->name, servName);
		client->region	= (uint16_t)strtol(*(region),NULL,10);
	} else {
		client->region		= 0;	// Region ID 0 nur für Testzwecke
		client->serviceMask = 128;
		sprintf(logText,"[%s]   None requested services was verified\n", client->name);
	}
	setLogText(logText, LOG_CLIENT);
	// Setzen des Fortbewegungsmittels
	typeID = (uint8_t)strtol(*(type), NULL, 10);
	if (typeID != 1 && typeID != 2 && typeID != 3) {
		sprintf(logText,"[%s]   Unknown movement type received\n", client->name);
		setLogText(logText, LOG_CLIENT);
	} else {
		sprintf(logText,"[%s]   Selected movement type: %s\n", client->name, moveTypes[typeID]);
		setLogText(logText, LOG_CLIENT);
		client->type			= typeID;
		client->update.timeGap *= typeID;
	}
	
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(type);
	freeArray(region);
	freeArray(services);

	return 0;
}

clientStruct * setClientStruct(unsigned int pid) {
	clientStruct * str	= malloc(sizeof(clientStruct));
	str->name[0]		= '\0';
	str->serviceMask	= 128;
	str->pos.latitude	= 0;
	str->pos.longitude	= 0;
	str->pid			= pid;
	str->type			= unkown;
	str->update.moy		= 0;
	str->update.mSec	= 0;
	str->update.timeGap = 1000;
	return str;
}

char * getServiceName(uint8_t servID) {
	char * name = NULL;
	switch (servID) {
		case 1:
			name = calloc(11, sizeof(char));
			strcpy(name, "unregister");
			return name;
		case 2:
			name = calloc(6, sizeof(char));
			strcpy(name, "GLOSA");
			return name;
		case 4:
			name = calloc(15, sizeof(char));
			strcpy(name, "placeholder #1");
			return name;
		case 8:
			name = calloc(15, sizeof(char));
			strcpy(name, "placeholder #2");
			return name;
		case 16:
			name = calloc(15, sizeof(char));
			strcpy(name, "placeholder #3");
			return name;
		case 32:
			name = calloc(15, sizeof(char));
			strcpy(name, "placeholder #4");
			return name;
		case 64:
			name = calloc(15, sizeof(char));
			strcpy(name, "placeholder #5");
			return name;
		default :
			name = calloc(10, sizeof(char));
			strcpy(name, "undefined");
			return name;
	}
}

void getHash(uint16_t region, uint16_t id, uint32_t * refID, uint8_t * hash) {
	*refID	 = region;
	*refID <<= 16;
	*refID  += id;
	*hash	 = *refID % MAX_HASH;
}

interStruct ** getInterStructTable(uint16_t region) {
	uint32_t refID;
	uint16_t id;
	uint8_t hash;
	interStruct ** table, * inter, * interPtr;
	MYSQL_RES * resInter;
	MYSQL_ROW rowInter;

	MYSQL * db = sqlConnect("safari");
	char query[Q_BUF];
	// Hash-Table initialisieren
	table = malloc(MAX_HASH * sizeof(interStruct *));
	for (int i = 0; i < MAX_HASH; i++)
		table[i] = NULL;

	// Intersections des Anbieters abfragen
	sprintf (query, "SELECT id, maxLong, maxLat, minLong, minLat "
					"FROM intersections "
					"WHERE region = '%i'", region);
	if (mysql_query (db, query))
		sqlError(db);
	if ((resInter = mysql_store_result(db)) == NULL) {
		mysql_free_result(resInter);
		sqlError(db);
	}
	while ((rowInter = mysql_fetch_row(resInter))) {
		id = (uint16_t)strtol(rowInter[0], NULL, 10);
		getHash(region, id, &refID, &hash);
		// Neues Intersection-Element anlegen
		if ((inter = getInterStruct(db, region, id)) == NULL) {
			mysql_free_result(resInter);
			mysql_close(db);
			return NULL;
		}
		inter->refID			= refID;
		inter->borders.maxLong	= (int)strtol(rowInter[1], NULL, 10);
		inter->borders.maxLat	= (int)strtol(rowInter[2], NULL, 10);
		inter->borders.minLong	= (int)strtol(rowInter[3], NULL, 10);
		inter->borders.minLat	= (int)strtol(rowInter[4], NULL, 10);
		// Kollisions-Abfrage
		if (table[hash] == NULL)
			table[hash] = inter;
		else {
			interPtr = table[hash];
			while (interPtr != NULL)
				interPtr = interPtr->next;
			interPtr->next = inter;
		}
	}
	mysql_free_result(resInter);
	mysql_close(db);
	return table;
}

interStruct * getInterStruct(MYSQL * db, uint16_t region, uint16_t id) {
	MYSQL_RES * resLane;
	MYSQL_RES * resSeg;
	MYSQL_ROW rowLane;
	MYSQL_ROW rowSeg;
	
	interStruct * inter;
	laneStruct 	* lane, * lanePtr;
	segStruct	* seg, * segPtr;

	char query[Q_BUF];
	inter		 = malloc(sizeof(interStruct));
	inter->next	 = NULL;
	inter->lanes = NULL;
	
	// Fahrspuren (Lanes) der Intersection abfragen
	sprintf (query, "SELECT laneID, longitude, latitude "
					"FROM lanes "
					"WHERE region = '%u' && id = '%u'",
					region, id);
	if (mysql_query (db, query))
		sqlError(db);
	if ((resLane = mysql_store_result(db)) == NULL) {
		mysql_free_result(resLane);
		return NULL;
	}
	while ((rowLane = mysql_fetch_row(resLane))) {
		// neues Lane-Element anlegen
		lane				= malloc(sizeof(laneStruct));
		lane->next			= NULL;
		lane->segments		= NULL;
		lane->laneID		= (int)strtol(rowLane[0],NULL,10);
		lane->pos.longitude	= (int)strtol(rowLane[1],NULL,10);
		lane->pos.latitude	= (int)strtol(rowLane[2],NULL,10);
		if (inter->lanes == NULL) {
			inter->lanes = lane;
			lanePtr		 = lane;
		} else {
			lanePtr->next = lane;
			lanePtr		  = lanePtr->next;
		}
		// Lane-Segmente abfragen
		sprintf (query, "SELECT maxLong, maxLat, minLong, minLat "
						"FROM segments "
						"WHERE region = '%u' && id = '%u' && laneID = '%i'",
						region, id, lane->laneID);
		if (mysql_query (db, query))
			sqlError(db);
		if ((resSeg = mysql_store_result(db)) == NULL) {
			mysql_free_result(resLane);
			mysql_free_result(resSeg);
			sqlError(db);
			return NULL;
		}
		while ((rowSeg = mysql_fetch_row(resSeg))) {
			// Neues Lane-Segment anlegen
			seg					 = malloc(sizeof(segStruct));
			seg->next			 = NULL;
			seg->borders.maxLong = (int)strtol(rowSeg[0],NULL,10);
			seg->borders.maxLat	 = (int)strtol(rowSeg[1],NULL,10);
			seg->borders.minLong = (int)strtol(rowSeg[2],NULL,10);
			seg->borders.minLat	 = (int)strtol(rowSeg[3],NULL,10);
			if (lane->segments == NULL) {
				lane->segments  = seg;
				segPtr			= seg;
			} else {
				segPtr->next = seg;
				segPtr		 = segPtr->next;
			}
		}
		mysql_free_result(resSeg);
	}
	mysql_free_result(resLane);
	
	return inter;
}

char * getClientMessage(interStruct ** interTable, char * msg, clientStruct * client) {
	xmlDocPtr xmlMsg, xmlState;
	interStruct * interPtr;
	laneStruct	* lanePtr;
	segStruct	* segPtr;
	char ** interStates, ** strRegion, ** strID, ** strLaneID, clientMsg[MSQ_LEN];
	uint32_t refID;
	uint16_t region, id;
	uint8_t laneID, hash, laneMatch = 0;
	xmlMsg		= xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	// # # #   SPaT-Nachricht   # # #
	if (xmlContains(xmlMsg, "//IntersectionState")) {
		interStates = getTree(xmlMsg, "//IntersectionState");
		sprintf(clientMsg, "%s%s", XML_TAG, SPAT_TAG_START);
		// IntersectionStates abarbeiten
		for (int i = 0; *(interStates + i); i++) {
			// Werte auslesen
			xmlState  = xmlReadMemory(*(interStates+i), strlen(*(interStates+i)),NULL,NULL,0);
			strRegion = getNodeValue(xmlState, "//id/region");
			strID	  = getNodeValue(xmlState, "//id/id");
			strLaneID = getNodeValue(xmlState, "//LaneID");
			if (strID == NULL || strRegion == NULL || strLaneID == NULL) {
				freeArray(strRegion);
				freeArray(strLaneID);
				freeArray(strID);
				xmlFreeDoc(xmlState);
				continue;
			}
			region = (uint16_t)strtol(*(strRegion),NULL,10);
			id	   = (uint16_t)strtol(*(strID),NULL,10);
			laneID = (uint8_t)strtol(*(strLaneID),NULL,10);
			freeArray(strRegion);
			freeArray(strLaneID);
			freeArray(strID);
			xmlFreeDoc(xmlState);
			// Intersection-Liste aufrufen
			getHash(region, id, &refID, &hash);
			interPtr = interTable[hash];
			// Kreuzungen abarbeiten
			while (interPtr != NULL) {
				// Kreuzung in der Hash-Table gefunden
				if (interPtr->refID == refID) {
					// User befindet sich im Kreuzungsbereich
					if (client->pos.latitude <= interPtr->borders.maxLat &&
						client->pos.latitude >= interPtr->borders.minLat &&
						client->pos.longitude <= interPtr->borders.maxLong &&
						client->pos.longitude >= interPtr->borders.minLong)
						{
						// Lanes abarbeiten
						lanePtr = interPtr->lanes;
						while (lanePtr != NULL) {
							// LaneID in der Liste != LaneID in der Nachricht
							if (lanePtr->laneID != laneID) {
								lanePtr = lanePtr->next;
								continue;
							} else {
								// Lane-Segmente prüfen
								segPtr = lanePtr->segments;
								while (segPtr != NULL) {
									// User befindet sich in einem Lane-Segment
									if (client->pos.latitude <= segPtr->borders.maxLat &&
										client->pos.latitude >= segPtr->borders.minLat &&
										client->pos.longitude <= segPtr->borders.maxLong &&
										client->pos.longitude >= segPtr->borders.minLong)
										{
											laneMatch = 1;
											// Nachricht vorbereiten
											sprintf(clientMsg + strlen(clientMsg), "%s", *(interStates+i));
											break;
										}
									segPtr = segPtr->next;
								}
								lanePtr = lanePtr->next;
							}
						}
					}
				} // eo if (interPtr->refID == refID)
				interPtr = interPtr->next;
			} // eo while (interPtr != NULL)
		} // eo for (int i = 0; *(interStates + i); i++)
		freeArray(interStates);
		xmlFreeDoc(xmlMsg);
		// Nachricht an den Client zurückgeben
		if (laneMatch == 0)
			return NULL;
		else {
			sprintf(clientMsg + strlen(clientMsg), "%s", SPAT_TAG_END);
			char * ret = malloc(strlen(clientMsg)+1);
			strcat(clientMsg, TERM_NULL);
			strcpy(ret, clientMsg);
			return ret;
		}
	} // eo if (interStates != NULL)
	if (xmlContains(xmlMsg, "/mapUpdate")) {
		// Nachricht zur Aktualisierung bestimter Intersection-Daten eingegangen
		strRegion = getNodeValue(xmlMsg, "/mapUpdate/region");
		strID	  = getNodeValue(xmlMsg, "/mapUpdate/id");
		if (strRegion != NULL && strID != NULL)
			setInterUpdate(interTable, strRegion, strID);

		freeArray(strRegion);
		freeArray(strID);
		xmlFreeDoc(xmlMsg);
	}
	return NULL;
}

void setInterUpdate(interStruct ** interTable, char ** regions, char ** ids) {
	uint32_t refID;
	uint16_t region, id;
	uint8_t hash, refMatch = 0;
	interStruct * inter, * interPtr, * temp, * prev;
	char query[Q_BUF];
	MYSQL_RES * resInter;
	MYSQL_ROW rowInter;
	MYSQL * db = sqlConnect("safari");
	for (int i = 0; *(regions+i) && *(ids+i); i++) {
		region = (uint16_t)strtol(*(regions+i),NULL,10);
		id	   = (uint16_t)strtol(*(ids+i),NULL,10);
		getHash(region, id, &refID, &hash);
		
		// Intersections des Anbieters abfragen
		sprintf (query, "SELECT maxLong, maxLat, minLong, minLat "
						"FROM intersections "
						"WHERE region = '%u' && id = '%u'", region, id);
		if (mysql_query (db, query))
			sqlError(db);
		if ((resInter = mysql_store_result(db)) == NULL) {
			mysql_free_result(resInter);
			sqlError(db);
		}
		rowInter = mysql_fetch_row(resInter);
		if ((inter = getInterStruct(db, region, id)) == NULL) {
			mysql_free_result(resInter);
			continue;
		}
		inter->refID			= refID;
		inter->borders.maxLong	= (int)strtol(rowInter[0], NULL, 10);
		inter->borders.maxLat	= (int)strtol(rowInter[1], NULL, 10);
		inter->borders.minLong	= (int)strtol(rowInter[2], NULL, 10);
		inter->borders.minLat	= (int)strtol(rowInter[3], NULL, 10);
		
		// interTable[hash] ist noch leer
		if ((interPtr = interTable[hash]) == NULL)
			interTable[hash] = inter;
		// Suche nach der Intersection
		else {
			prev = NULL;
			while (interPtr != NULL) {
				if (interPtr->refID == refID) {
					refMatch = 1;
					temp	 = interPtr;
					// Erstes Element einhängen
					if (prev == NULL)
						interTable[hash] = inter;
					// oder mit dem Vorgänger verknüpfen
					else
						prev->next = inter;
					
					inter->next = interPtr->next;
					
					freeInterStruct(temp);
					break;
				}
				prev	 = interPtr;
				interPtr = interPtr->next;
			}
			// refIDs stimmten nicht überein -> Element anhängen
			if (refMatch == 0)
				interPtr->next = inter;
		}
		mysql_free_result(resInter);
	}
	mysql_close(db);
}

void freeInterStruct(interStruct * inter) {
	laneStruct * lanePtr, * laneTmp;
	segStruct * segPtr, * segTmp;
	lanePtr = inter->lanes;
	while (lanePtr != NULL) {
		segPtr = lanePtr->segments;
		while (segPtr != NULL) {
			segTmp = segPtr;
			segPtr = segPtr->next;
			free(segTmp);
		}
		laneTmp = lanePtr;
		lanePtr = lanePtr->next;
		free(laneTmp);
	}
	free(inter);
}

void freeInterTable(interStruct ** table) {
	for (int i = 0; i < MAX_HASH; i++)
		if (table[i] != NULL)
			freeInterStruct(table[i]);
	
	free(table);
}

uint8_t updateRequired(clientStruct * client) {
	int moy, mSec;
	getTimestamp(&moy, &mSec);
	if (client->update.moy == moy &&
		(mSec - client->update.mSec) > client->update.timeGap)
		return 1;
	else if (client->update.moy != moy &&
			  (60000 - client->update.mSec + mSec) > client->update.timeGap)
		return 1;

	return 0;
}
