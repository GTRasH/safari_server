#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>

#include <basic.h>

/**	\brief	Message Queue Schlüssel */
#define KEY 1234L

/** \brief	Begrenzung der Nachricht */
#define MSQ_LEN 1280

/** \brief	Zugriffsrechte für Message Queue */
#define PERM 0666
#undef signal
