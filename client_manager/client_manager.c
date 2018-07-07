/*	#########################################################
 * 	#			HfTL SAFARI PROTOTYP - SIMULATOR			#
 * 	#														#
 * 	#	Author:	Sebastian Neutsch							#
 * 	#	Mail:	sebastian.neutsch@t-online.de				#
 * 	#
 * 	#
 * 	#########################################################
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <string.h>

#include <func.h>

int main(void) {
	int socketCreate, socketNew;
	socklen_t addrlen;
	char *buffer = malloc(BUF);
	ssize_t size;
	struct sockaddr_in address;
	const int y = 1;
	
	if ((socketCreate = socket(AF_INET, SOCK_STREAM, 0)) > 0)
		printf("Socket wurde angelegt\n");
		
	setsockopt(socketCreate, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
	
	address.sin_family		= AF_INET;
	address.sin_addr.s_addr	= INADDR_ANY;
	address.sin_port		= htons(15000);
	
	if (bind(socketCreate, (struct sockaddr *) &address, sizeof(address)) != 0)
		printf("Port ist belegt!\n");
		
	listen(socketCreate, 5);
	addrlen = sizeof(struct sockaddr_in);
	
	while (1) {
		socketNew = accept(socketCreate, (struct sockaddr *) &address, &addrlen);
		
		if (socketNew > 0)
			printf("Ein Client (%s) ist verbunden ... \n", inet_ntoa(address.sin_addr));
			
		
		// fork oder thread ...
		
		close(socketNew);
	}
	return EXIT_SUCCESS;
}
