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
				printf("\n# # #   Sende Service-Anforderung   # # #\n%s", reqServ);
				func = setClientServices;
				if (getClientResponse(sock, 10, func, reqServ, client))
					printf("Client fordert keine Services an -> Abbruch\n");
				else {
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
	int result;
	long unsigned messageSize = strlen(message);
	
	if (message == NULL)
		return 1;
	
	else {
		// sende den Request bis zu `retries` Mal
		do {
			setSocketContent(sock, message, messageSize);
			if ((resp = getSocketContent(sock)) == NULL) {
				result = 1;
				break;
			}
			// printf("%s", resp);
			result	= func(resp, client);
			free(resp);
		} while (result && --retries);
	}
	return result;
}

int setClientResponse(char * msg, clientStruct * client) {
	char ** test;
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	test = getNodeValue(xmlMsg, "//refPoint/long");
	if (test != NULL) {
		freeArray(test);
		return setClientLocation(msg, client);
	}
	freeArray(test);
	test = getNodeValue(xmlMsg, "//service");
	if (test != NULL) {
		freeArray(test);
		return setClientServices(msg, client);
	}
	freeArray(test);
	return 1;
}

int setClientLocation(char * msg, clientStruct * client) {
	char ** longitude, ** latitude;
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	longitude		 = getNodeValue(xmlMsg, "//refPoint/long");
	latitude		 = getNodeValue(xmlMsg, "//refPoint/lat");
	// Ungültiger Location-Response
	if (longitude == NULL || latitude == NULL)
		return 1;
		
	client->pos.latitude = strtol(*(latitude), NULL, 10);
	client->pos.longitude = strtol(*(longitude), NULL, 10);
	
	freeArray(longitude);
	freeArray(latitude);
	return 0;
}

int setClientAuth(char * msg, clientStruct * client) {
	char ** nodeUser, ** nodePass;
	int result = 1;
	MYSQL_RES * dbResult;
	char query[MSG_BUF];
	MYSQL * dbCon	 = sqlConnect("safari");
	xmlDocPtr xmlMsg = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	nodeUser		 = getNodeValue(xmlMsg, "//data/user");
	nodePass		 = getNodeValue(xmlMsg, "//data/pass");
	// Ungültiger Auth-Response
	if (nodeUser == NULL || nodePass == NULL)
		return 1;

	sprintf(query,	"SELECT name FROM users WHERE name = '%s' AND "
					"pass = AES_ENCRYPT('%s', SHA2('safari', 512))"
					, *nodeUser, *nodePass);
	
	if (mysql_query(dbCon, query))
		sqlError(dbCon);
	if ((dbResult = mysql_store_result(dbCon)) == NULL)
		sqlError(dbCon);
	
	if (mysql_num_rows(dbResult) != 1)
		client->name = "unknown";
	else {
		client->name = mysql_fetch_row(dbResult)[0];
		result = 0;
	}
	mysql_free_result(dbResult);
	mysql_close(dbCon);
	xmlFreeDoc(xmlMsg);
	freeArray(nodeUser);
	freeArray(nodePass);
	return result;
}

int setClientServices(char * msg, clientStruct * client) {
	char ** services;
	xmlDocPtr xml = xmlReadMemory(msg, strlen(msg), NULL, NULL, 0);
	services = getNodeValue(xml, "//service");
	printf("services:");
	for (int i = 0; *(services+i); i++)
		printf(" %s", *(services+i));
		
	printf("\n");
	return 0;
}
