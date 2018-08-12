#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LIB_SAFARI "/usr/local/share/safari_server/"
#define LOG_CLIENT 	LIB_SAFARI"/var/log/client"
#define LOG_SERVER	LIB_SAFARI"/var/log/server"
#define LOG_BUF 256

#define TERM_NULL "\0"

/** \brief Fehlerfunktion
 * 
 * \param[in] *message	Fehlermeldung
 * \param[in] abend		Programmabbruch
 * 
 * \return	exit(1) wenn abend == 1
 */
void setError(char *message, int abend);

/** \brief Cast eines n-stelligen int in einen n-stelligen char
 * 
 * \param[in] value = zu wandelnder Wert
 * 
 * \return	char *
 */
char * int2string(int value);

/** \brief Speicherfreigabe eines zweidimensionalen Arrays
 * 
 * \param[in] ** array	= zu leerendes Array
  * 
 * \return	void
 */
void freeArray(char ** array);

/** \brief Liefert den Inhalt einer lesbaren Datei als String
 * 
 * \param[in] * filename	Name der zu lesenden Datei
  * 
 * \return	String wenn OK, sonst exit(1)
 */
char * getFileContent(const char * fileName);

void setLogText(char * text, const char * logFile);

char ** getSplitString(char * a_str, const char a_delim);

void getTimestamp(int * moy, int * mSec);
