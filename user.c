//*****************************************************
//Program Name: user
//Author: Cory Mckiel
//Date Created: Oct 19, 2020
//Last Modified: Oct 20, 2020
//Program Description:
//      user does a dummy task.
//
//Needed Files:
//      oss.c
//Compilation Instructions:
//      Option 1: Using the supplied Makefile.
//          Type: make
//      Option 2: Using gcc.
//          Type: gcc -Wall -g oss.c -o oss
//                gcc -Wall -g user.c -o user
//*****************************************************

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

//Struct for message queue.
typedef struct {
    long mtype;
} mymsg_t;

int main(int argc, char *argv[])
{

//*****************************************************
////BEGIN: Setting up shared memory.
    
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

//END: Setting up message queue.
//*****************************************************
//BEGIN: Determining runtime.

    //Generate a number between 10,000 and 1,000,000.
    int runtime = ((rand() % 100) + 1) * 10000;
    
    if ( msgrcv(msgid, &msg, 0, 0, 0) == -1 ) {
        fprintf(stderr, "%s: Error: msgrcv() failed to receive message.\n%s\n", argv[0], strerror(errno));
        return 1;
    }
    //CRITICAL SECTION:

    //Save the time.
    int sec = *(shmp);
    int nsec = *(shmp+1);

    //Determine what time the child should stop.
    if ( nsec + runtime >= 1000000000 ) {
        sec += 1;
        nsec = (nsec + runtime) - 1000000000;
    } else {
        nsec += runtime;
    }
    
    //END CRITICAL SECTION.
    if ( msgsnd(msgid, &msg, 0, 0) == -1 ) {
        fprintf(stderr, "%s: Error: msgsnd() failed to send message.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    //Do the 'work'. 
    int workdone = 0;
    do {
        if ( msgrcv(msgid, &msg, 0, 0, 0) == -1 ) {
            fprintf(stderr, "%s: Error: msgrcv() failed to receive message.\n%s\n", argv[0], strerror(errno));
            return 1;
        }

        //CRITICAL SECTION:
        
        //Check to see if the time has passed.
        if ( sec < *(shmp) ) {
            workdone = 1;
        } else if ( sec == *(shmp) ) {
            if ( nsec < *(shmp+1) ) {
                workdone = 1;
            }
        }
        
        //END CRITICAL SECTION.
        if ( msgsnd(msgid, &msg, 0, 0) == -1 ) {
            fprintf(stderr, "%s: Error: msgsnd() failed to send message.\n%s\n", argv[0], strerror(errno));
            return 1;
        }
    
    } while (!workdone);


    //Wait your turn to tell oss you are finished.
    int finished = 0;
    do {
        //Wait for message.
        if ( msgrcv(msgid, &msg, 0, 0, 0) == -1 ) {
            fprintf(stderr, "%s: Error: msgrcv() failed to receive message.\n%s\n", argv[0], strerror(errno));
            return 1;
        }
        //We now have the message.
        //CRITICAL SECTION.

        //If shm_pid is 0, put my pid inside.
        if ( *(shmp + 2) == 0 ) {
            *(shmp + 2) = getpid();
            finished = 1;
        }
        
        //Make sure to send the msg back into the queue
        //so someone else can go into critical section.
        if ( msgsnd(msgid, &msg, 0, 0) == -1 ) {
            fprintf(stderr, "%s: Error: msgsnd() failed to send message.\n%s\n", argv[0], strerror(errno));
            return 1;
        }

    } while (!finished);


//END: Setting up message queue.
//*****************************************************
//BEGIN: Finishing up.

    shmdt(shmp);
    return 0;
}
