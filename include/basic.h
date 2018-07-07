#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

/** \brief Speicherfreigabe eines zweidimensionalen Arrays
 * 
 * \param[in] ** array	= zu leerendes Array
  * 
 * \return	void
 */
void freeArray(char ** array);
