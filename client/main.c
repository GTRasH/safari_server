/* client.c */ 

#include <basic.h>
#include <socket.h>

#define RESP_AUTH1 "./../xml/client/resp_auth_nok.xml"
#define RESP_AUTH2 "./../xml/client/resp_auth_ok.xml"
#define RESP_LOC "./../xml/client/resp_loc.xml"
#define RESP_SERV "./../xml/client/resp_serv.xml"

int main (void) {
	socket_t sock;
	char * fileAuthOK  = getFileContent(RESP_AUTH2);
	char * fileLoc	= getFileContent(RESP_LOC);
	char * fileServ	= getFileContent(RESP_SERV);
	char * respAuth, * respServ, * respLoc;
	long unsigned	fileSizeAuthOK	= strlen(fileAuthOK),
					fileSizeServ	= strlen(fileServ),
					fileSizeLoc		= strlen(fileLoc);

	sock = getSocket(AF_INET, SOCK_STREAM, 0);
	// setSocketConnect(&sock, "red-dev.de", 15000);
	setSocketConnect(&sock, "localhost", 15000);
	
	
	respAuth = getSocketContent(sock);	// Auth Request
	printf("strlen(message) %lu\n%s", strlen(respAuth), respAuth);
	free(respAuth);	
	setSocketContent(sock, fileAuthOK, fileSizeAuthOK);	// Auth Response
	
	
	
	respServ = getSocketContent(sock);	// Serv Request
	printf("strlen(message) %lu\n%s", strlen(respServ), respServ);
	free(respServ);	
	setSocketContent(sock, fileServ, fileSizeServ);	// Serv Response	
	
	
	
	respLoc = getSocketContent(sock);	// Loc Request
	printf("strlen(message) %lu\n%s", strlen(respLoc), respLoc);
	free(respLoc);	
	setSocketContent(sock, fileLoc, fileSizeLoc);	// Loc Response
	
	
	free(fileAuthOK);
	free(fileServ);
	free(fileLoc);
	close(sock);
	return EXIT_SUCCESS;
}
