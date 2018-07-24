#include <client_manager.h>

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
	char * reqAuth, * reqServ, * reqLoc;
	int result = 1;
	reqAuth = getFileContent(REQ_AUTH);
	// Fehler beim Lesen der Auth-Request File
	if (reqAuth == NULL)
		printf("Error while getting REQ-AUTH (%s) Content\n", REQ_AUTH);
	else {
		printf("\n# # #   Sende Authentifizierungs-Anforderung   # # #\n%s", reqAuth);
		// Sonst: Sende Authentifizierungs-Anforderung
		func = setClientAuth;
		if (getClientResponse(sock, 10, func, reqAuth, client))
			printf("10 ungültige Login-Versuche -> Abbruch!\n");
		else {
			reqServ = getFileContent(REQ_SERV);
			// Fehler beim Lesen der Loc-Request File
			if (reqServ == NULL)
				printf("Error while getting REQ-SERV (%s) Content\n", REQ_SERV);
			else {
				printf("Authentifizierung OK - angemeldet als user: %s\n", client->name);
				printf("\n# # #   Sende Service-Anforderung   # # #\n%s", reqServ);
				func = setClientServices;
				if (getClientResponse(sock, 10, func, reqAuth, client))
					printf("Client fordert keine Services an -> Abbruch\n");
				else {
					printf("getClientResponse(setClientServices) - OK\n");
					reqLoc = getFileContent(REQ_LOC);
					// Fehler beim Lesen der Loc-Request File
					if (reqLoc == NULL)
						printf("Error while getting REQ-LOC (%s) Content\n", REQ_LOC);
					else {
						// Sonst: Sende Lokalisierungs-Anforderung
						printf("\n# # #   Sende Standort-Anforderung   # # #\n%s", reqLoc);
						func = setClientLocation;
						if (getClientResponse(sock, 10, func, reqLoc, client))
							printf("Client sendet keine Positionsdaten -> Abbruch\n");
						else
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
	int result = 0;
	long unsigned msgSize = strlen(message);
	
	if (message == NULL)
		return 1;
	
	// sende den Request bis zu `retries` Mal
	do {
		setSocketContent(sock, message, msgSize);
		resp = getSocketContent(sock);
		if (resp == NULL) {
			free(resp);
			result = 1;
			break;
		}
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
		return result;
	}

	sprintf(query,	"SELECT name FROM users WHERE name = '%s' AND "
					"pass = AES_ENCRYPT('%s', SHA2('safari', 512))"
					, *(nodeUser), *(nodePass));
	
	if (mysql_query(dbCon, query))
		sqlError(dbCon);
	if ((dbResult = mysql_store_result(dbCon)) == NULL)
		sqlError(dbCon);
	
	if (mysql_num_rows(dbResult) != 1)
		client->name = "unknown";
	else {
		client->name = mysql_fetch_row(dbResult)[0];
		result = 0;
		sprintf(logText, "User '%s' logged in successfully", client->name);
		setLogText(logText);
	}
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(nodeUser);
	freeArray(nodePass);
	return result;
}

int setClientServices(char * msg, clientStruct * client) {
	uint8_t servReqID, servRowID;
	MYSQL_RES * dbResult;
	MYSQL_ROW row;
	char query[Q_BUF];
	MYSQL * dbCon	 = sqlConnect("safari");
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	char ** services = getNodeValue(xmlMsg, "//service");
	char ** cityID	 = getNodeValue(xmlMsg, "//cityID");
	// Ungültiger Service-Response
	if (services == NULL || cityID == NULL) {
		freeArray(services);
		xmlFreeDoc(xmlMsg);
		return 1;
	}
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
			if (servRowID == servReqID)
				client->serviceMask += servReqID;
		}
	}
	if (client->serviceMask == 0)
		client->serviceMask = 128;
	
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(cityID);
	freeArray(services);

	return 0;
}
