#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    long mtype;
} mymsg_t;

int main(int argc, char *argv[])
{

//*****************************************************
////BEGIN: Setting up shared memory.

    int i = 0;
    
    key_t key;
    int shmid;
    int *shmp;

    //Generate key deterministically so that children
    //can do the same and attach to shared memory.
    if ( (key = ftok("./", 912)) == -1 ) {
        fprintf(stderr, "%s: Error: ftok() failed to generated key.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    //Calculate the size of shared memory.
    //One slot for seconds, one for nanoseconds,
    //and one for child pid. Total three.
    int shm_size = 3;

    //Create and get the id of the shared memory segment.
    if ( (shmid = shmget(key, shm_size, 0666)) < 0 ) {
        fprintf(stderr, "%s: Error: Failed to allocate shared memory.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    //Attach to shared memory.
    if ( (shmp = (int*)shmat(shmid, NULL, 0)) < 0 ) {
        fprintf(stderr, "%s: Error: Failed to attach to shared memory.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    for (i = 0; i < shm_size; i++) {
       printf("User: shmp at %d: %d\n", i, *(shmp + i)); 
    }



//END: Setting up shared memory.
//*****************************************************
//BEGIN: Setting up message queue.

    mymsg_t msg;
    int msgid;
    key_t msgkey;

    //Generate key deterministically so that children
    //can do the same and attach to message queue.
    if ( (msgkey = ftok("./", 922)) == -1 ) {
        fprintf(stderr, "%s: Error: ftok() failed to generated key.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    if ( (msgid = msgget(msgkey, 0666)) == -1) {
        fprintf(stderr, "%s: Error: msgget() failed to attach to message queue.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    /*
    if ( (msg = (mymsg_t*)malloc(sizeof(mymsg_t))) == NULL ) {
        fprintf(stderr, "%s: Error: malloc() failed to allocate message.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    //Arbitrary.
    //msg->mtype = 1;
    
    if ( msgsnd(msgid, msg, 0, 0) == -1 ) {
        fprintf(stderr, "%s: Error: msgsnd() failed to send message.\n%s\n", argv[0], strerror(errno));
        free(msg);
        return 1;
    }
    free(msg);*/

    if ( msgrcv(msgid, &msg, 0, 0, 0) == -1 ) {
        fprintf(stderr, "%s: Error: msgrcv() failed to receive message.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

//END: Setting up message queue.
//*****************************************************
//BEGIN: Finishing up.

    printf("Hello from user.\n");

    shmdt(shmp);

    //Close message queue.
    msgctl(msgid, IPC_RMID, NULL);

    return 0;
}
