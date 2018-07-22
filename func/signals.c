#include <signals.h>

static sighandler_t mySignal(int sigNr, sighandler_t signalHandler) {
	struct sigaction newSig, oldSig;
	newSig.sa_handler	= signalHandler;
	sigemptyset(&newSig.sa_mask);
	newSig.sa_flags		= SA_RESTART;
	
	if (sigaction(sigNr, &newSig, &oldSig) < 0)
		return SIG_ERR;
	return oldSig.sa_handler;
}

static void noZombie(int sigNr) {
	pid_t pid;
	int ret;
	while ((pid = waitpid(-1, &ret, WNOHANG)) > 0)
		printf("Child-Serbver mit pid=%d hat sich beendet\n", pid);
	return;
}
