#include <socket.h>

int getClientUDS(void) {
	int socketFD;
	struct sockaddr_un address;
	
	if ((socketFD = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0)
		setError("Unix Domain Socket für Message Manager konnte nicht angelegt werden\n", 1);

	address.sun_family = AF_LOCAL;
	strcpy(address.sun_path, UDS_FILE);

	if (connect(socketFD, (struct sockaddr *) &address, sizeof(address)) == -1)
		return -1;
		
	printf("- Verbindung mit dem Simulator hergestellt\n");
	return socketFD;
}

int getServerUDS(struct sockaddr_un * address, socklen_t * addrlen) {
	int socketFD;
	
	if ((socketFD = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0)
		setError("Unix Domain Socket für Simulator konnte nicht angelegt werden\n", 1);
		
	*addrlen = sizeof(struct sockaddr_in);
	unlink(UDS_FILE);
	address->sun_family = AF_LOCAL;
	strcpy(address->sun_path, UDS_FILE);

	if (connect(socketFD, (struct sockaddr *) &address, sizeof(address)) == 0)
		printf("Verbindung mit dem Server hergestellt\n");
		
	return socketFD;
}

int getSocket(int af, int type, int protocol) {
	socket_t sock;
	const int y = 1;
	/* Erzeuge das Socket */
	if ((sock = socket(af, type, protocol)) < 0)
		setError("Error while socket() : ", 1);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
	return sock;
}

void setSocketConnect(socket_t *sock, char *serv_addr, unsigned short port) {
	struct sockaddr_in server;
	struct hostent *host_info;
	unsigned long addr;
	memset( &server, 0, sizeof (server));
	if ((addr = inet_addr( serv_addr )) != INADDR_NONE) {
		/* argv[1] ist eine nummerische IP-Adresse */
		memcpy((char *)&server.sin_addr, &addr, sizeof(addr));
	}
	else {
		host_info = gethostbyname( serv_addr );
		if (NULL == host_info) 
			setError("Unknown server", 1); 
		memcpy((char *)&server.sin_addr, host_info->h_addr, host_info->h_length);
	}
	server.sin_family = AF_INET;
	server.sin_port = htons( port );
	/* Baue die Verbindung zum Server auf */
	if (connect( *sock, (struct sockaddr *)&server, sizeof( server)) < 0)
		setError("Unable to connect to server", 1);
}

void setSocketBind(socket_t *sock, unsigned long adress, unsigned short port) {
	struct sockaddr_in server;
	
	memset(&server, 0, sizeof (server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(adress);
	server.sin_port = htons(port);
	if (bind(*sock, (struct sockaddr*)&server, sizeof(server)) < 0 )
		setError("Error while bind() : ", 1);
}

void setSocketListen(socket_t *sock ) {
	if(listen(*sock, 5) == -1 )
		setError("Error while listen() : ", 1);
}

void setSocketAccept(socket_t * socket, socket_t * new_socket) {
	struct sockaddr_in client;
	unsigned int len;
	char logText[LOG_BUF];
	
	len = sizeof(client);
	*new_socket = accept(*socket,(struct sockaddr *)&client, &len);
	if (*new_socket  == -1) 
		setError("Error while accept() : ", 0);
	else {
		sprintf(logText, "[accept]   Client connected from %s\n",
				inet_ntoa(client.sin_addr));
		setLogText(logText, LOG_CLIENT);
	}
}

void setSocketContent(socket_t sock, char * message, long unsigned length) {
	char * msgPtr;
	int sentByte, offset, modSize, bufSize;
	offset	= 0;
	msgPtr	= message;
	modSize	= length % MSG_BUF;
	// Übertragung des Strings in MSG_BUF großen Blöcken
	while (offset < length) {
		// variable Buffer Größe zum Senden des letzten Chunks
		bufSize  = ((offset+modSize) == length) ? modSize : MSG_BUF;
		sentByte = send(sock, msgPtr, bufSize, 0);
		offset	 += sentByte;
		msgPtr	 = message + offset;
	}
	// Nachrichtenlänge ist ein Vielfaches von MSG_BUF
	if (modSize == 0)
		send(sock, MSG_EMPTY, 1, 0);
}

char * getSocketContent(socket_t sock) {
	long unsigned recvByte;
	char buf[MSG_BUF+1], msgBuf[MSG_MAX];
	char * msg	= NULL;
	msgBuf[0]	= '\0';
	// Socket auslesen
	do {
		recvByte = recv(sock, buf, MSG_BUF, MSG_EOR);
		buf[recvByte] = '\0';
		sprintf(msgBuf + strlen(msgBuf), "%s", buf);

	} while (recvByte == MSG_BUF);
	msg = malloc(strlen(msgBuf)+1);
	strcpy(msg, msgBuf);
	return msg;
}
