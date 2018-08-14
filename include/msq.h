#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>

#include <mqueue.h>

#include <basic.h>

/* Magische Nummer */
#define KEY 1234L
/* Begrenzung der Nachricht */
#define MSQ_LEN 8192
/* Zugriffsrechte */
#define PERM 0666
#undef signal

#define MSQ_REG_QUEUE "/mq_register"
#define MSQ_MSG_QUEUE "/mq_messages"
