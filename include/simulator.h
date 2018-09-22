/*	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#																			#
 * 	#				HfTL SAFARI PROTOTYP - SIMULATOR HEADER						#
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
#include <dirent.h>

#include <basic.h>
#include <socket.h>

/** \brief	Pfad für MAP-Nachrichten */
#define MAP_PATH	LIB_SAFARI"xml/map/"

/** \brief	Pfad für SPaT-Nachrichten */
#define SPAT_PATH 	LIB_SAFARI"xml/spat/"

/** \brief Listen-Element für SPat- und MAP-Nachrichten (Lesevorgang) */
typedef struct xmlListElement {
	struct xmlListElement *next;
	int offset;
	xmlDocPtr ptr;
} xmlListElement;

/** \brief Listen-Kopf für einfach verkettete Nachrichten-Liste */
typedef struct {
	xmlListElement *first;
	size_t size;
} xmlListHead;

/** \brief Erzeugt eine Liste mit Pointern der xml-Dateien im übergebenen Pfad
 * 
 * \param[in] pathname	Nachrichtenverzeichnis (SPaT oder MAP / universal)
 * 
 * \return	xmlDocListHead->xmlDocListElement->ptr wenn OK, sonst NULL
 */
xmlListHead *getxmlptrlist(char *pathname);

/** \brief	Speicher einer Nachrichten-Liste freigeben
 * 
 * \param[in] head	Pointer auf das Header-Element
 *
*/
void freeList(xmlListHead * head);

/** \brief	String aus dem XML-Dokument erzeugen und an Socket senden
 * 
 * \param[in]	doc			XML-Dokument
 * \param[in]	msgSocket	Unix Domain Socket
 *
*/
void setMessage(int msgSocket, xmlDocPtr doc);
