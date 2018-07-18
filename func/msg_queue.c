#include <msg_queue.h>

unsigned int * setSeqSMS(char * tmpFile) {
	int shmId;
	void * shmData;
	char * shmIdStr;
	unsigned int * seqSMS;
	FILE * fd;
	fd = fopen(tmpFile, "w");

	if ((shmId = shmget(IPC_PRIVATE, 
						sizeof(unsigned int), 
						IPC_CREAT | SHM_R | SHM_W)) == -1) {
		fclose(fd);
		setError("Error while shmget()", 1);
	}
	
	shmIdStr = int2string(shmId);
	
	fwrite(shmIdStr, strlen(shmIdStr), 1, fd);
	fclose(fd);
	free(shmIdStr);
	
	/* Shared-Memory-Segment anbinden */
	shmData = shmat(shmId, NULL, 0);
	if (shmData == (void *) -1)
		setError("Error while shmat()", 1);

	seqSMS = shmData;
	
	return seqSMS;
}

unsigned int * getSeqSMS(char * tmpFile) {
	int shmId;
	void * shmData;
	unsigned int * seqSMS;
	char shmIdStr[12];
	FILE * fd;
	
	fd = fopen(tmpFile, "r");
	
	if (fd == NULL)
		setError("Error while open SeqNo SMS", 1);
	
	fread(shmIdStr, 12, 1, fd);
	
	shmId = (int)strtol(shmIdStr, NULL, 10);

	fclose(fd);
	
	/* Shared-Memory-Verbindung zu Server */
	if ((shmData = shmat(shmId, NULL, 0)) == (void *) -1)
		setError("Error while shmat() for existing SMS", 1);
	
	seqSMS = shmData;

	return seqSMS;
}
