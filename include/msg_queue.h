#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <basic.h>

#define SEQ_SPAT "/tmp/seq_spat"

/** \brief 	Richtet ein Shared-Memory-Segment (SMS) für eine
 * 			Nachrichten-Sequenz-Nummer ein
 * 
 * \param[in] *tmpFile	Datei für die Shared-Memory-ID
 * 
 * \return	exit(1) bei Fehler, sonst Zeiger auf SMS
 */
unsigned int * setSeqSMS(char * tmpFile);

/** \brief 	Ruft ein vorhandenes Shared-Memory-Segment (SMS) zum Lesen
 * 			einer Nachrichten-Sequenz-Nummer auf
 * 
 * \param[in] *tmpFile	Datei für die Shared-Memory-ID
 * 
 * \return	exit(1) bei Fehler, sonst Zeiger auf SMS
 */
unsigned int * getSeqSMS(char * tmpFile);
