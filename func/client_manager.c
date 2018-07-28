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

void noZombie(int sigNr) {
	pid_t pid;
	int ret;
	while ((pid = waitpid(-1, &ret, WNOHANG)) > 0)
		printf("Child-Server mit pid=%d hat sich beendet\n", pid);
	return;
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
		printf ("# # #   SENDE NACHRICHT AN CLIENT   # # #\n%s\n",
				message);
		setSocketContent(sock, message, msgSize);
		resp = getSocketContent(sock);
		if (resp == NULL) {
			free(resp);
			result = 1;
			break;
		}
		// TESTING
		printf ("# # #   NACHRICHT VON CLIENT EMPFANGEN   # # #\n%s\n",
				resp);
		result = func(resp, client);
		free(resp);
	} while (result && --retries);

	return result;
}

int setClientResponse(char * msg, clientStruct * client) {
	char ** test;
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	// Client-Antwort enthält Positionsdaten
	test = getNodeValue(xmlMsg, "//refPoint/long");
	if (test != NULL) {
		xmlFreeDoc(xmlMsg);
		freeArray(test);
		return setClientLocation(msg, client);
	}
	freeArray(test);
	// Client-Antwort enthält Services
	test = getNodeValue(xmlMsg, "//service");
	if (test != NULL) {
		xmlFreeDoc(xmlMsg);
		freeArray(test);
		return setClientServices(msg, client);
	}
	// Client hat weder mit Positionsdaten noch mit Services geantwortet
	xmlFreeDoc(xmlMsg);
	freeArray(test);
	return 1;
}

int setClientLocation(char * msg, clientStruct * client) {
	xmlDocPtr xmlMsg  = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	char ** longitude = getNodeValue(xmlMsg, "//refPoint/long");
	char ** latitude  = getNodeValue(xmlMsg, "//refPoint/lat");
	// Ungültiger Location-Response
	if (longitude == NULL || latitude == NULL) {
		freeArray(longitude);
		freeArray(latitude);
		xmlFreeDoc(xmlMsg);
		return 1;
	}
	client->pos.latitude  = strtol(*(latitude), NULL, 10);
	client->pos.longitude = strtol(*(longitude), NULL, 10);
	
	freeArray(longitude);
	freeArray(latitude);
	xmlFreeDoc(xmlMsg);
	return 0;
}

int setClientAuth(char * msg, clientStruct * client) {
	MYSQL_RES * dbResult;
	char query[Q_BUF], logText[LOG_BUF];
	
	int result		 = 1;
	MYSQL * dbCon	 = sqlConnect("safari");
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	char ** nodeUser = getNodeValue(xmlMsg, "//data/user");
	char ** nodePass = getNodeValue(xmlMsg, "//data/pass");
	// Ungültiger Auth-Response
	if (nodeUser == NULL || nodePass == NULL) {
		freeArray(nodeUser);
		freeArray(nodePass);
		xmlFreeDoc(xmlMsg);
		mysql_close(dbCon);
		return result;
	}
	
	sprintf(logText, "[%u]   Receiving login data\n", client->pid);
	setLogText(logText, LOG_CLIENT);

	sprintf(query,	"SELECT name FROM users WHERE name = '%s' AND "
					"pass = AES_ENCRYPT('%s', SHA2('safari', 512))"
					, *(nodeUser), *(nodePass));
	
	if (mysql_query(dbCon, query))
		sqlError(dbCon);
	if ((dbResult = mysql_store_result(dbCon)) == NULL)
		sqlError(dbCon);
	
	if (mysql_num_rows(dbResult) != 1)
		sprintf(logText, "[%u]   Login failed\n", client->pid);
	else {
		sprintf(logText, "[%s]   User '%s' successfully logged in\n",
				client->name, client->name);
		strcpy(client->name, mysql_fetch_row(dbResult)[0]);
		result = 0;
	}
	setLogText(logText, LOG_CLIENT);
	
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(nodeUser);
	freeArray(nodePass);
	return result;
}

int setClientServices(char * msg, clientStruct * client) {
	uint8_t servReqID, servRowID, typeID;
	MYSQL_RES * dbResult;
	MYSQL_ROW row;
	char query[Q_BUF], logText[LOG_BUF], serviceName[LOG_BUF], * tmp;
	MYSQL * dbCon	 = sqlConnect("safari");
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	char ** services = getNodeValue(xmlMsg, "//service");
	char ** cityID	 = getNodeValue(xmlMsg, "//cityID");
	char ** type	 = getNodeValue(xmlMsg, "//movementType");
	serviceName[0]	 = '\0';
	// Ungültiger Service-Response
	if (services == NULL || cityID == NULL || type == NULL) {
		freeArray(services);
		freeArray(type);
		freeArray(cityID);
		xmlFreeDoc(xmlMsg);
		mysql_close(dbCon);
		return 1;
	}
	// TESTING - Log-Update
	sprintf(logText,"[%s]   Receiving vehicle type and service "
					"request\n", client->name);
	setLogText(logText, LOG_CLIENT);
	// Prüfe die angeforderten Dienste gegen die für die Stadt Hinterlegten
	sprintf(query,	"SELECT serviceid FROM services "
					"WHERE cityid = '%s'", *(cityID));
	
	if (mysql_query(dbCon, query))
		sqlError(dbCon);
	if ((dbResult = mysql_store_result(dbCon)) == NULL)
		sqlError(dbCon);
	
	client->serviceMask = 0;
	
	while ((row = mysql_fetch_row(dbResult))) {
		for (int i = 0; *(services+i) != NULL; i++) {
			servReqID = (uint8_t)strtol(*(services+i), NULL, 10);
			servRowID = (uint8_t)strtol(row[0], NULL, 10);
			if (servRowID == servReqID) {
				client->serviceMask += servReqID;
				tmp = getServiceName(servReqID);
				if (strlen(serviceName) == 0)
					strcpy(serviceName, tmp);
				else {
					strcat(serviceName, " | ");
					strcat(serviceName, tmp);
				}
				free(tmp);
			}
		}
	}
	// Mindestens ein Dienst wurde bestätigt
	if (client->serviceMask != 0)
		sprintf(logText,"[%s]   Verified services: %s\n",
						client->name, serviceName);
	else {
		client->serviceMask = 128;
		sprintf(logText,"[%s]   None requested services was verified\n",
						client->name);
	}
	setLogText(logText, LOG_CLIENT);
	
	// Setzen des Fortbewegungsmittels
	typeID = (uint8_t)strtol(*(type), NULL, 10);
	if (typeID != 1 && typeID != 2 && typeID != 3) {
		sprintf(logText,"[%s]   Unknown movement type received\n",
				client->name);
		setLogText(logText, LOG_CLIENT);
	} else {
		sprintf(logText,"[%s]   Selected movement type: %s\n",
				client->name, moveTypes[typeID]);
		setLogText(logText, LOG_CLIENT);
		client->type = typeID;
	}
	
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(type);
	freeArray(cityID);
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
