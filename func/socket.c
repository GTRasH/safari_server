#include <socket.h>

int getClientUDS(void) {
	int uds;
	struct sockaddr_un address;
	
	if ((uds = socket(PF_LOCAL, SOCK_STREAM, 0)) > 0)
		printf("Socket wurde angelegt\n");

	address.sun_family = AF_LOCAL;
	strcpy(address.sun_path, UDS_FILE);

	if (connect(uds, (struct sockaddr *) &address, sizeof(address)) == 0)
		printf("Verbindung mit dem Server hergestellt\n");
		
	return uds;
}


int getServerUDS(struct sockaddr_un * address, socklen_t * addrlen) {
	int uds;
	
	if ((uds = socket(PF_LOCAL, SOCK_STREAM, 0)) > 0)
		printf("Socket wurde angelegt\n");
		
	*addrlen = sizeof(struct sockaddr_in);
	unlink(UDS_FILE);
	address->sun_family = AF_LOCAL;
	strcpy(address->sun_path, UDS_FILE);

	if (connect(uds, (struct sockaddr *) &address, sizeof(address)) == 0)
		printf("Verbindung mit dem Server hergestellt\n");
		
	return uds;
}
