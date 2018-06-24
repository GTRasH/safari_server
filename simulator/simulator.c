/* uds_server.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include <data_elements.h>
#include <map.h>

#define BUF 1024      // muss später der SPaT/MAP Nachricht angepasst werden
#define UDS_FILE "/tmp/safari_sim.sock"

int main (void) {
	int create_socket, new_socket, cnt = 0;  // int Rückgabe-Werte
	socklen_t addrlen;
	char *buffer = malloc (BUF), msg[50];          // alloziert BUF Byte Speicher im Adressraum
	ssize_t size;
	struct sockaddr_un address;	//  struct sockaddr_un {
								//      uint8_t     sun_len;        -> Länge Struktur - Nicht POSIX   
								//      sa_family_t sun_family;     -> PF_UNIX, AF_UNIX, PF_LOCAL...
								//      char        sun_path[104];  -> Pfadname
								//  };

	MapData *map = malloc(sizeof(MapData));

	printf("size without intersection %lu\n", sizeof(map));
	IntersectionGeometry *intersection = malloc(sizeof(IntersectionGeometry));
	
	intersection->revision = 123;

	map->intersections[0] = intersection;
  
	printf("size with 1 intersection %lu\n", sizeof(map));
  
	printf("revision intersection 1 = %i \n", map->intersections[0]->revision);
  
	if((create_socket=socket (AF_LOCAL, SOCK_STREAM, 0)) > 0)   // 
		printf ("Socket wurde angelegt\n");
  
	unlink(UDS_FILE);                   // bind schlägt fehl wenn Datei bereits vorhanden
  
	address.sun_family = AF_LOCAL;      // Address Family LOCAL
  
	strcpy(address.sun_path, UDS_FILE); // Pfad des Socket-Deskriptors in die Struktur kopieren
  
	if (bind(create_socket, 
			(struct sockaddr *) &address,
			sizeof (address)) != 0) {
				printf( "Der Port ist nicht frei – belegt?!?!\n");
	}

	listen (create_socket, 2);  // Socket im passiven Modus und Mitteilung an System,
                              // dass Server bereit ist Verbindungen entgegen zu nehmen.
                              // Zweiter Parameter für Warteschlange -> Nur ein Client gleichzeitig
	addrlen = sizeof (struct sockaddr_in);
  
	while (1) {
		new_socket = accept(create_socket, (struct sockaddr *) &address, &addrlen);
		if (new_socket > 0)
			printf ("Ein Client ist verbunden ...\n");
     
		do {
			// Nachricht zurück setzen und generieren
			sprintf(msg, "Nachricht #%d generiert", ++cnt);
			strcpy(buffer, msg);
			// Nachricht übergeben  
			send (new_socket, buffer, strlen (buffer), 0);
			// warte auf Bestätigung
			size = recv (new_socket, buffer, BUF, 0);
			// terminierende Null anhängen
			if (size > 0)
				buffer[size] = '\0';
        
			printf("Verarbeitung %i bestätigt\n", cnt);

			sleep(1);
        
		} while (strcmp (buffer, "complete"));
     
		close (new_socket);
	}
	close (create_socket);
	return EXIT_SUCCESS;
}
