/* client.c */ 

#include <basic.h>
#include <socket.h>

#define RESP_AUTH1	LIB_SAFARI"/xml/client/resp_auth_nok.xml"
#define RESP_AUTH2	LIB_SAFARI"/xml/client/resp_auth_ok.xml"
#define RESP_LOC	LIB_SAFARI"/xml/client/resp_loc.xml"
#define RESP_SERV	LIB_SAFARI"/xml/client/resp_serv.xml"

int main (int argc, char * argv[]) {
	socket_t sock;
	char * fileAuthOK  = getFileContent(RESP_AUTH2);
	char * fileLoc	= getFileContent(RESP_LOC);
	char * fileServ	= getFileContent(RESP_SERV);
	char * respAuth, * respServ, * respLoc, *respMSQ;
	long unsigned	fileSizeAuthOK	= strlen(fileAuthOK),
					fileSizeServ	= strlen(fileServ),
					fileSizeLoc		= strlen(fileLoc);
					
	int count = 0;

	sock = getSocket(AF_INET, SOCK_STREAM, 0);
	
	if (argc > 1 && strcmp(argv[1], "remote") == 0)
		setSocketConnect(&sock, "red-dev.de", 15000);
	else
		setSocketConnect(&sock, "localhost", 15000);

	respAuth = getSocketContent(sock);	// Auth Request
	printf("\n# # #   Authentifizierungsanforderung empfangen   # # #\n%s\n", respAuth);
	
	printf("\n# # #   Authentifizierung gesendet   # # #\n");
//	printf("strlen(message) %lu\n%s", strlen(respAuth), respAuth);
	free(respAuth);	
	setSocketContent(sock, fileAuthOK, fileSizeAuthOK);	// Auth Response

	respServ = getSocketContent(sock);	// Serv Request
	printf("\n# # #   (Eigentlich) Serviceanforderung empfangen   # # #\n%s\n", respServ);
	printf("\n# # #   Service-Antwort gesendet   # # #\n%s\n", fileServ);
//	printf("strlen(message) %lu\n%s", strlen(respServ), respServ);
	free(respServ);	
	setSocketContent(sock, fileServ, fileSizeServ);	// Serv Response	

	respLoc = getSocketContent(sock);	// Loc Request
	printf("\n# # #   Standortanforderung (oder irgendwas) empfangen   # # #\n%s\n", respLoc);
	printf("\n# # #   Standort gesendet   # # #\n");
//	printf("strlen(message) %lu\n%s", strlen(respLoc), respLoc);
	setSocketContent(sock, fileLoc, fileSizeLoc);	// Loc Response
	
	while(1) {
		respMSQ = getSocketContent(sock);	// Loc Request
		printf("\n# # #   Nachricht # %i von SAFARI   # # #\n%s\n", ++count, respMSQ);
		free(respMSQ);
		setSocketContent(sock, fileLoc, fileSizeLoc);
	}
	
	free(respLoc);
	free(fileAuthOK);
	free(fileServ);
	free(fileLoc);
	close(sock);
	return EXIT_SUCCESS;
}
