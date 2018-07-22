#include <sql.h>

void sqlError(MYSQL *con) {
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);
}

MYSQL * sqlConnect(char * db) {
	MYSQL * con = mysql_init(NULL);
	if ( mysql_real_connect(con, "localhost", "root", "safari", 
							db, 0, NULL, 0) == NULL)
		sqlError(con);
		
	return con;
}
