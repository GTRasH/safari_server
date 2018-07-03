/* uds_client.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mysql.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <func.h>

#define BUF 1024
#define UDS_FILE "/tmp/safari_sim.sock"

xmlDocPtr getMessage(int createSocket, int *response);

int main (int argc, char **argv) {
	int createSocket, response;
	struct sockaddr_un address;
	xmlDocPtr doc;
  
	MYSQL *con = mysql_init(NULL);
  
	printf("MySQL client version: %s\n", mysql_get_client_info());

	if((createSocket=socket (PF_LOCAL, SOCK_STREAM, 0)) > 0)
		printf ("Socket wurde angelegt\n");
	address.sun_family = AF_LOCAL;
	strcpy(address.sun_path, UDS_FILE);

	if (connect(createSocket, (struct sockaddr *) &address, sizeof(address)) == 0)
		printf ("Verbindung mit dem Server hergestellt\n");

	do {
		doc = getMessage(createSocket, &response);
		
		char *msgId = getNodeValue(doc, "//messageId");
		
		printf("Nachricht mit messageId %s erhalten", msgId);
				
		// xmlFreeDoc(doc);
		
	} while (response > 0);
	
	return EXIT_SUCCESS;
}

xmlDocPtr getMessage(int createSocket, int *response) {
	int msgSize;
	char *buffer = malloc(BUF), *tmp;
	xmlDocPtr message;
	// Empfang der Nachrichtengröße
	recv(createSocket, buffer, BUF, 0);
	// Buffer anpassen und Nachricht empfangen
	if ((msgSize = (int)strtol(buffer, &tmp, 10)) == 0)
		return NULL;

	buffer	= realloc(buffer, msgSize);
	recv(createSocket, buffer, msgSize, 0);
	message = xmlReadMemory(buffer, strlen(buffer), NULL, NULL, 0);
	free(buffer);
	*response = (strcmp(tmp, "quit") != 0) ? 1 : 0;
	return message;
}
