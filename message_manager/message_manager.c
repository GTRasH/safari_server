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

#include <mysql/mysql.h>

#define BUF 1024
#define UDS_FILE "/tmp/safari_sim.sock"

int main (int argc, char **argv) {
  int create_socket;
  char *buffer = malloc (BUF);
  struct sockaddr_un address;
  int size;
  char ready[] = "completed";
  
  MYSQL *con = mysql_init(NULL);
  
  printf ("\e[2J");
  
  printf("MySQL client version: %s\n", mysql_get_client_info());
  /*
  if((create_socket=socket (PF_LOCAL, SOCK_STREAM, 0)) > 0)
    printf ("Socket wurde angelegt\n");
  address.sun_family = AF_LOCAL;
  strcpy(address.sun_path, UDS_FILE);
  if (connect ( create_socket,
                (struct sockaddr *) &address,
                sizeof (address)) == 0)
    printf ("Verbindung mit dem Server hergestellt\n");

  while (1) {
      size = recv(create_socket, buffer, BUF, 0);
      // terminierende Null anhängen
      if (size > 0)
         buffer[size] = '\0';
      printf ("Nachricht erhalten: %s\n", buffer);
      printf ("%s\n", ready);
      strcpy(buffer, ready);
      send(create_socket, buffer, strlen (buffer), 0);
  }
  */
  return EXIT_SUCCESS;
}
