/*	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#																			#
 * 	#				HfTL SAFARI PROTOTYP - XML HEADER							#
 * 	#																			#
 * 	#	Author:		Sebastian Neutsch											#
 * 	#	Mail:		sebastian.neutsch@t-online.de								#
 *  #	Copyright:	2018 Sebastian Neutsch / Deutsche Telekom AG				#
 * 	#																			#
 * 	#																			#
 * 	#	This file is part of safari_server.										#
 *	#																			#
 *	#	safari_server is free software: you can redistribute it and/or modify	#
 *	#	it under the terms of the GNU General Public License as published by	#
 *	#	the Free Software Foundation, either version 3 of the License, or		#
 *	#	(at your option) any later version.										#
 *	#																			#
 *	#	safari_server is distributed in the hope that it will be useful,		#
 *	#	but WITHOUT ANY WARRANTY; without even the implied warranty of			#
 *	#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			#
 *	#	GNU General Public License for more details.							#
 *	#																			#
 *	#	You should have received a copy of the GNU General Public License		#
 *	#	along with safari_server. If not, see <https://www.gnu.org/licenses/>.	#
 *	#																			#
 * 	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
*/
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>

#include <basic.h>

#define XML_TAG "<?xml version=\"1.0\"?>\n"
#define XML_TAG_LEN 23

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

/** \brief Erzeugt einen Pointer auf ein XML-Dokument
 * 
 * \param[in] *docname = Dateipfad
 * 
 * \return	xmlDocPtr
 * 			NULL im Fehlerfall
 */
xmlDocPtr getdoc(char *docname);

/** \brief	Ergänzt die Strings im Array mit <?xml version="1.0"?>
 * 
 * \param[in] trees		Bäume
 * 
 * \return	XML-String (beginnend mit <?xml version="1.0"?>)
 */
char ** getWellFormedXML(char ** trees);

/** \brief	Liefert ein String-Array. Jeder String ist ein Baum ab 'tag'.
 * 
 * \param[in] message	Pointer auf ein XML-Dokument
 * \param[in] tag		zu suchender Tag-Name (XPath-Expression)
 * 
 * \return	String
 */
char ** getTree(xmlDocPtr message, char * tag);

/** \brief	Prüft, ob ein bestimmter Node im XML-Dokument enthalten ist
 * 
 * \param[in]	doc			XML-Dokument
 * \param[in]	expression	gesuchter Node
 *
 * \return	Anzahl der Vorkommnisse 
*/
int xmlContains(xmlDocPtr doc, char * expression);
