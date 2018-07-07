#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>
#include <mysql.h>

#include <list.h>

#define BUF 1024
#define SIM_FILE "/tmp/safari_sim.sock"

/** \brief Fehlerfunktion
 * 
 * \param[in] *message = Fehlermeldung
 * 
 * \return	EXIT_FAILURE
 */
int error(char *message);

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

/** \brief Cast eines n-stelligen int in einen n-stelligen char
 * 
 * \param[in] value = zu wandelnder Wert
 * 
 * \return	char *
 */
char * int2string(int value);

/** \brief Setzen der Knoten-Werte innerhalb eines XML-Dokumentes
 * 
 * \param[in] *doc	 		= Pointer des XML-Objektes
 * \param[in] *expression	= TagName -> "//bezeichner"
 * \param[in] *value 		= zu setzender Wert
 * 
 * \return	1 wenn OK, 0 sonst
 */
int setNodeValue(xmlDocPtr doc, char * expression, char * value);

/** \brief Liefert ein Array mit Strings (Werte der "Expression"-Tags)
 * 
 * \param[in] *doc	 		= Pointer des XML-Objektes
 * \param[in] *expression	= TagName -> siehe XPath API
 * 
 * \return	char **, NULL sonst
 */
char ** getNodeValue(xmlDocPtr doc, char * expression);

/** \brief Aufruf aller Knoten mit einem bestimmten Namen
 * 
 * \param[in] *doc	 		= Pointer des XML-Objektes
 * \param[in] *expression	= TagName -> siehe XPath API
  * 
 * \return	xmlXPathObjectPtr wenn OK, NULL sonst
 */
xmlXPathObjectPtr getNodes(xmlDocPtr doc, char * expression);

/** \brief Speicherfreigabe eines zweidimensionalen Arrays
 * 
 * \param[in] ** array	= zu leerendes Array
  * 
 * \return	void
 */
void freeArray(char ** array);

/** \brief Speicherfreigabe der IntersectionGeometry-Table
 * 
 * \param[in] ** hashTable	= zu leerende Hash
  * 
 * \return	void
 */
void freeGeoTable(intersectGeo ** hashTable);

