#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include <basic.h>

typedef void (*sighandler_t)(int);

static sighandler_t mySignal(int sigNr, sighandler_t signalHandler);

static void noZombie(int sigNr);
