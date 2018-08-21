#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#define LIB_SAFARI "/usr/local/share/safari_server/"
#define LOG_CLIENT 	LIB_SAFARI"/var/log/client"
#define LOG_SERVER	LIB_SAFARI"/var/log/server"
#define LOG_BUF 256

#define TERM_NULL "\0"

/** \brief	Mittlerer Erdradius nach WGS-84 in cm */
#define WGS84_RAD 637100080

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

/** \brief Schreibt einen Eintrag in die Server- oder Client-Log
 * 
 * \param[in] * text		Log-Eintrag
 * \param[in] * logFile		LOG_SERVER oder LOG_CLIENT
 * 
 */
void setLogText(char * text, const char * logFile);

/** \brief Teilt einen String am Delimeter in n Teil-Strings
 * 
 * \param[in] * a_str		Zu teilender String
 * \param[in] * a_delim		Delimeter
 * 
 * \return Array der Teil-Strings
 */
char ** getSplitString(char * a_str, const char a_delim);

/** \brief 	Berechnet die Minuten das laufenden Jahres und die Millisekunden
 * 			der aktuellen Minuten ausgehend von der Systemzeit.
 * 
 * \param[out] moy		minute of the year
 * \param[out] mSec		ms
 */
void getTimestamp(int * moy, int * mSec);

/** \brief	Berechnet 1/100 µGrad in Abhängigkeit der Höhe
 * 
 * \param[in] elevation	Geographische Höhe der Kreuzung
 * 
 * \return 1/100 µGrad
*/
double get100thMicroDegree(int elevation);
