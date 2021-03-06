/*	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#																			#
 * 	#				HfTL SAFARI PROTOTYP - CLIENT MANAGER FUNCTIONS				#
 * 	#																			#
 * 	#	Author:		Sebastian Neutsch											#
 * 	#	Mail:		sebastian.neutsch@t-online.de								#
 *  #	Copyright:	2018 Sebastian Neutsch / Deutsche Telekom AG				#
 * 	#																			#
 * 	#																			#
 * 	#	This file is part of safari_server.										#
 *	#																			#
 *	#	safari_server is free software: you can redistribute it and/or modify	#
 *	#	it under the terms of the GNU General Public License as published by	#
 *	#	the Free Software Foundation, either version 3 of the License, or		#
 *	#	(at your option) any later version.										#
 *	#																			#
 *	#	safari_server is distributed in the hope that it will be useful,		#
 *	#	but WITHOUT ANY WARRANTY; without even the implied warranty of			#
 *	#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			#
 *	#	GNU General Public License for more details.							#
 *	#																			#
 *	#	You should have received a copy of the GNU General Public License		#
 *	#	along with safari_server. If not, see <https://www.gnu.org/licenses/>.	#
 *	#																			#
 * 	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
*/
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
		// # # # Sende Authentifizierungs-Anforderung (Microservice S14 und S16) # # #
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
				// # # # Sende Service-Anforderung (Microservice S16 und S20) # # #
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
						// # # # Sende Standort-Anforderung (Microservice S20 und S22) # # #
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
		switch (typeID) {
			case feet:	client->maxSpeed = speedFeet;
						break;
			case bike:	client->maxSpeed = speedBike;
						break;
			case motor:	client->maxSpeed = speedMotor;
						break;
			default: 	; break;
		}
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
	str->maxSpeed		= speedUnkown;
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
	sprintf (query, "SELECT id, maxLong, maxLat, minLong, minLat, elevation "
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
		inter->elevation		= (int)strtol(rowInter[5], NULL, 10);
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
	partStruct 	* part, * partPtr;
	segStruct	* seg, * segPtr;
	// Lane-ID 255 ist reserviert! (SAEJ2735)
	uint8_t rowLaneID, laneID = 255;

	char query[Q_BUF];
	inter		 = malloc(sizeof(interStruct));
	inter->next	 = NULL;
	inter->lanes = NULL;
	
	// Fahrspuren (Lanes) der Intersection abfragen
	sprintf (query, "SELECT laneID, partID, longitude, latitude, distance, maxSpeed, maneuvers "
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
		rowLaneID = (int)strtol(rowLane[0],NULL,10);
		// neues Lane-Element anlegen
		if (rowLaneID != laneID) {
			laneID		 	= rowLaneID;
			lane		 	= malloc(sizeof(laneStruct));
			lane->next	 	= NULL;
			lane->parts	 	= NULL;
			lane->laneID 	= laneID;
			lane->maneuvers	= (uint16_t)strtol(rowLane[6],NULL,10);
		
			if (inter->lanes == NULL) {
				inter->lanes = lane;
				lanePtr		 = lane;
			} else {
				lanePtr->next = lane;
				lanePtr		  = lanePtr->next;
			}
		}
		// neuen Lane-Abschnitt anlegen
		part				= malloc(sizeof(partStruct));
		part->next			= NULL;
		part->segments		= NULL;
		part->pos.longitude	= (int)strtol(rowLane[2],NULL,10);
		part->pos.latitude	= (int)strtol(rowLane[3],NULL,10);
		part->distance		= (int)strtol(rowLane[4],NULL,10);
		part->maxSpeed		= (int)strtol(rowLane[5],NULL,10);
		
		if (lane->parts == NULL) {
			lane->parts = part;
			partPtr		= part;
		} else {
			partPtr->next = part;
			partPtr		  = partPtr->next;
		}
		// Segmente der Lane-Abschnitte abfragen
		sprintf (query, "SELECT maxLong, maxLat, minLong, minLat "
						"FROM segments "
						"WHERE region = '%u' && id = '%u'"
						" && laneID = '%i' && partID = '%s'",
						region, id, lane->laneID, rowLane[1]);

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
			if (part->segments == NULL) {
				part->segments  = seg;
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
	partStruct	* partPtr;
	segStruct	* segPtr;
	char ** interStates, ** strRegion, ** strID, ** strLaneID, msgSpat[MSQ_LEN],
		 msgMap[MSQ_LEN], msgClient[MSQ_LEN], ** strStamp, ** strMoy, * advise, * map;
	int moy, mSec;
	uint32_t refID;
	uint16_t region, id;
	uint8_t laneID, hash, laneMatch = 0;
	xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	// # # #   SPaT-Nachricht (Microservice S27)   # # #
	if (xmlContains(xmlMsg, "//IntersectionState")) {
		interStates = getTree(xmlMsg, "//IntersectionState");
		// Clientnachricht vorbereiten
		sprintf(msgClient, "%s%s", XML_TAG, SAFARI_START);
		sprintf(msgSpat, "%s", SPAT_START);
		sprintf(msgMap, "%s", MAP_START);
		// IntersectionStates abarbeiten
		for (int i = 0; *(interStates + i); i++) {
			xmlState = xmlReadMemory(*(interStates+i), strlen(*(interStates+i)),NULL,NULL,0);
			// IntersectionState enthält nicht die erforderlichen Werte
			if (!(xmlContains(xmlState, "//moy") && xmlContains(xmlState, "//timeStamp") &&
				  xmlContains(xmlState, "//id/region") && xmlContains(xmlState, "//id/id") &&
				  xmlContains(xmlState, "//LaneID"))) {

				xmlFreeDoc(xmlState);
				continue;
			}
			// Prüft das Alter der SPaT-Nachricht
			strMoy	 = getNodeValue(xmlState, "//moy");
			strStamp = getNodeValue(xmlState, "//timeStamp");
			moy		 = (int)strtol(*(strMoy), NULL, 10);
			mSec	 = (int)strtol(*(strStamp), NULL, 10);
			if (spatObsolete(moy, mSec)) {
				freeArray(strStamp);
				freeArray(strMoy);
				xmlFreeDoc(xmlState);
				continue;
			}
			// Auslesen der erforderlichen Daten
			strRegion = getNodeValue(xmlState, "//id/region");
			strID	  = getNodeValue(xmlState, "//id/id");
			strLaneID = getNodeValue(xmlState, "//LaneID");
			region 	  = (uint16_t)strtol(*(strRegion),NULL,10);
			id	   	  = (uint16_t)strtol(*(strID),NULL,10);
			laneID 	  = (uint8_t)strtol(*(strLaneID),NULL,10);
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
								// Lane-Abschnitte durchlaufen
								partPtr = lanePtr->parts;
								while (partPtr != NULL) {
									// Lane-Segmente prüfen
									segPtr = partPtr->segments;
									while (segPtr != NULL) {
										// User befindet sich in einem Lane-Segment
										if (client->pos.latitude <= segPtr->borders.maxLat &&
											client->pos.latitude >= segPtr->borders.minLat &&
											client->pos.longitude <= segPtr->borders.maxLong &&
											client->pos.longitude >= segPtr->borders.minLong)
											{
												advise = getState(*(interStates+i), lanePtr->laneID, client, 
																  partPtr, interPtr->elevation, moy, mSec);

												// Nachricht befüllen
												if (advise != NULL) {
													laneMatch = 1;
													sprintf(msgSpat + strlen(msgSpat), "%s", advise);
													map = getMapBody(lanePtr);
													sprintf(msgMap + strlen(msgMap), "%s", map);
													free(map);
													free(advise);
												}
												break;
											}
										segPtr = segPtr->next;
									}
									partPtr = partPtr->next;
								}
							} // eo (lanePtr->laneID == laneID)
							lanePtr = lanePtr->next;
						}
					}
				} // eo if (interPtr->refID == refID)
				interPtr = interPtr->next;
			} // eo while (interPtr != NULL)
			// Speicher freigeben
			freeArray(strRegion);
			freeArray(strLaneID);
			freeArray(strStamp);
			freeArray(strMoy);
			freeArray(strID);
			xmlFreeDoc(xmlState);
		} // eo for (int i = 0; *(interStates + i); i++)
		freeArray(interStates);
		xmlFreeDoc(xmlMsg);
		// Nachricht an den Client zurückgeben
		if (laneMatch == 0)
			return NULL;
		else {
			sprintf(msgSpat + strlen(msgSpat), "%s", SPAT_END);
			sprintf(msgMap + strlen(msgMap), "%s", MAP_END);
			sprintf(msgClient + strlen(msgClient), "%s%s%s", msgMap, msgSpat, SAFARI_END);
			char * ret = malloc(strlen(msgClient)+1);
			strcat(msgClient, TERM_NULL);
			strcpy(ret, msgClient);
			return ret;
		}
	} // eo if (interStates != NULL)
	
	// # # # Aktualisierung bestimmter Intersection-Daten eingegangen (Microservice S26) # # #
	if (xmlContains(xmlMsg, "/mapUpdate")) {
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
		sprintf (query, "SELECT maxLong, maxLat, minLong, minLat, elevation "
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
		inter->elevation		= (int)strtol(rowInter[4], NULL, 10);
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
	partStruct * partPtr, * partTmp;
	segStruct * segPtr, * segTmp;
	lanePtr = inter->lanes;
	while (lanePtr != NULL) {
		partPtr = lanePtr->parts;
		while (partPtr != NULL) {
			segPtr = partPtr->segments;
			while (segPtr != NULL) {
				segTmp = segPtr;
				segPtr = segPtr->next;
				free(segTmp);
			}
			partTmp = partPtr;
			partPtr = partPtr->next;
			free(partTmp);
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
	int timeGap;
	// Zeitabstand berechnen
	timeGap = getTimeGap(client->update.moy, client->update.mSec);
	
	if (timeGap > client->update.timeGap)
		return 1;

	return 0;
}

uint8_t spatObsolete(int moy, int mSec) {
	int timeGap;
	// Zeitabstand berechnen
	timeGap = getTimeGap(moy, mSec);
	
	if (timeGap > SPAT_OBSOLETE)
		return 1;

	return 0;
}

int getTimeGap(int moy, int mSec) {
	int sysMoy, sysMSec, timeGap;
	getTimestamp(&sysMoy, &sysMSec);
	timeGap = (moy == sysMoy) ?
					sysMSec - mSec :
					((sysMoy-moy) * MINUTE) - mSec + sysMSec;
					
	return timeGap;
}

char * getState(char * state, uint8_t laneID, clientStruct * client, 
				partStruct * part, int elevation, int moy, int mSec) {

	char buf[MSQ_LEN] = XML_TAG;
	xmlDocPtr xmlMsg, xmlAdvise;
	// XML-String bilden
	sprintf(buf + strlen(buf), "%s", state);
	xmlMsg = xmlReadMemory(buf, strlen(buf), NULL, NULL, 0);
	// Ampel nicht ROT oder GRÜN -> Keine Empfehlung
	if (!(xmlContains(xmlMsg, "//stop-And-Remain") || 
		  xmlContains(xmlMsg, "//permissive-Movement-Allowed"))) {
		xmlFreeDoc(xmlMsg);
		return NULL;
	}
	// Geschwindigkeitsempfehlung berechnen
	else {
		double offLong, offLat, microDegLong, microDegLat, timeLeft;
		int msgTime, minEnd, maxEnd, advise, bufSize, dist;
		char ** strMinEnd, ** strMaxEnd, * strAdvise, * strTime, * tmp, * msg, * strID;
		xmlChar * xmlBuf;
		// Status-Zeiten nicht enthalten
		if (!(xmlContains(xmlMsg, "//minEndTime") || xmlContains(xmlMsg, "//minEndTime"))) {
			xmlFreeDoc(xmlMsg);
			return NULL;
		}
		// Nachrichten-Zeit in 10-tel Sekunden ab der letzten Stunde umrechen
		msgTime		= (moy % 60) * 600 + (mSec/100);
		// Status-Zeiten auslesen
		strMinEnd 	= getNodeValue(xmlMsg, "//minEndTime");
		strMaxEnd 	= getNodeValue(xmlMsg, "//maxEndTime");
		minEnd		= (int)strtol(*(strMinEnd), NULL, 10);
		maxEnd		= (int)strtol(*(strMaxEnd), NULL, 10);
		freeArray(strMinEnd);
		freeArray(strMaxEnd);
		timeLeft	= (minEnd+maxEnd)/2 - msgTime;
		// Abstand zum letzten Referenzpunkt in 1/10 µGrad
		offLong	= (double)(part->pos.longitude - client->pos.longitude);
		offLat	= (double)(part->pos.latitude - client->pos.latitude);
		// Umrechnung in 0.1m
		get100thMicroDegree(elevation, client->pos.latitude, &microDegLat, &microDegLong);
		offLong	/= (microDegLong/10);
		offLat	/= (microDegLat/10);
		// Gesamtstrecke zur Stopp-Linie
		dist	 = (int)sqrt(offLong*offLong + offLat*offLat) + part->distance;
		// Geschwindigkeitsempfehlung in 0.1 m/s
		advise	 = getSpeedAdvise(client, dist, timeLeft, part->maxSpeed);
		// Rückgabe vorbereiten
		strcpy(buf, XML_TAG);
		strcat(buf, SPAT_BODY);
		strTime   = int2string(timeLeft);
		strAdvise = int2string(advise);
		strID	  = int2string((int)laneID);
		xmlAdvise = xmlReadMemory(buf, strlen(buf), NULL,NULL,0);
		setNodeValue(xmlAdvise, "//likelyTime", strTime);
		setNodeValue(xmlAdvise, "//LaneID", strID);
		setNodeValue(xmlAdvise, "//speed", strAdvise);
		free(strAdvise);
		free(strTime);
		free(strID);
		// XML in String schreiben, DTD entfernen und String zurückgeben
		xmlDocDumpMemory(xmlAdvise, &xmlBuf, &bufSize);
		bufSize = bufSize - XML_TAG_LEN + 1;
		tmp		= (char *)xmlBuf;
		msg		= malloc(bufSize+1);
		strcpy(msg, &tmp[XML_TAG_LEN-1]);
		strcat(msg, TERM_NULL);
		xmlFreeDoc(xmlAdvise);
		xmlFreeDoc(xmlMsg);
		xmlFree(xmlBuf);
		return msg;
	}
}

int getSpeedAdvise(clientStruct * client, double dist, double timeLeft, int maxSpeed) {
	int advise;
	advise	 = (int)dist/timeLeft;
	// 50 m/s für eine nicht verfügbare Empfehlung (SAEJ2735)
	if (advise > client->maxSpeed || advise > maxSpeed)
		advise = 500;
	return advise;
}

char * getMapBody(laneStruct * lane) {
	int bufSize;
	char * msg, * tmp, * maneuvers, * laneID, buf[MSQ_LEN] = XML_TAG;
	xmlDocPtr xml;
	xmlChar * xmlBuf;
	
	sprintf(buf + strlen(buf), "%s", MAP_BODY);
	
	xml = xmlReadMemory(buf, strlen(buf), NULL, NULL, 0);
	
	maneuvers = int2string((int)lane->maneuvers);
	laneID	  = int2string((int)lane->laneID);
	
	setNodeValue(xml, "//maneuvers", maneuvers);
	setNodeValue(xml, "//laneID", laneID);
	
	free(maneuvers);
	free(laneID);
	
	xmlDocDumpMemory(xml, &xmlBuf, &bufSize);
	bufSize = bufSize - XML_TAG_LEN + 1;
	
	tmp		= (char *)xmlBuf;
	msg		= malloc(bufSize+1);
	strcpy(msg, &tmp[XML_TAG_LEN-1]);
	strcat(msg, TERM_NULL);
	
	xmlFreeDoc(xml);
	xmlFree(xmlBuf);
	
	return msg;
}
