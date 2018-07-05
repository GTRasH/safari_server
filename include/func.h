#include <libxml/parser.h>
#include <libxml/xpath.h>

/** \brief Fehlerfunktion
 * 
 * \param[in] *message = Fehlermeldung
 * 
 * \return	EXIT_FAILURE
 */
int error(char *message);

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
