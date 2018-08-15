#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>

#include <basic.h>

/**	\brief	Message Queue Schlüssel */
#define KEY 1234L

/** \brief	Begrenzung der Nachricht */
#define MSQ_LEN 8192

/** \brief	Zugriffsrechte für Message Queue */
#define PERM 0666
#undef signal

/** \brief	Wartezeit für Message Queue Abfrage in µs */
#define MSQ_POLL 10000
