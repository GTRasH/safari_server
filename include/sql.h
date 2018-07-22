#include <mysql.h>

#include <basic.h>

/** \brief Fehlerfunktion für MYSQL
 * 
 * \param[in] *con = MYSQL Objekt
 * 
 * \return	exit(1)
 */
void sqlError(MYSQL *con);

/** \brief DB-Connector
 * 
 * \param[in] *db = DB-Name
 * 
 * \return	MYSQL
 */
MYSQL * sqlConnect(char * db);
