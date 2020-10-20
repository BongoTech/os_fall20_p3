//*****************************************************
//Program Name: oss
//Author: Cory Mckiel
//Date Created: Oct 19, 2020
//Last Modified: Oct 19, 2020
//Program Description:
//
//Needed Files:
//      user.c
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
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILENAMESIZE 64

//help declaration
int help(char*);

//*****************************************************
///////////////////////MAIN////////////////////////////
//*****************************************************
int main(int argc, char *argv[])
{
    //*****************************************************
    //BEGIN: Command line processing.

    //Used throughout the program for iterating.
    int i = 0; 

    int mx_chdrn = 5;
    int ptime = 20;
    char logfile[FILENAMESIZE] = "log.out";

    int option;
    while ( (option = getopt(argc, argv, ":c:l:t:h")) != -1 ) {
        switch ( option ) {
            case 'h':
                help(argv[0]);
                return 0;
            case 'c':
                mx_chdrn = atoi(optarg);
                break;
            case 'l':
                strncpy(logfile, optarg, FILENAMESIZE-1);
                break;
            case 't':
                ptime = atoi(optarg);
                break;
            case ':':
                fprintf(stderr, "%s: Error: Missing argument value.\n%s\n", argv[0], strerror(errno));
                return 1;
            case '?':
                fprintf(stderr, "%s: Error: Unknown argument.\n%s\n", argv[0], strerror(errno));
                return 1;
        }
    }

    printf("Max children: %d\n", mx_chdrn);
    printf("Logfile name: %s\n", logfile);
    printf("Time to run: %d\n", ptime);

    //END: Command line processing.
    //*****************************************************
    //BEGIN: Setting up shared memory.

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
    if ( (shmid = shmget(key, shm_size, IPC_CREAT|0666)) < 0 ) {
        fprintf(stderr, "%s: Error: Failed to allocate shared memory.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    //Attach to shared memory.
    if ( (shmp = (int*)shmat(shmid, NULL, 0)) < 0 ) {
        fprintf(stderr, "%s: Error: Failed to attach to shared memory.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    for (i = 0; i < shm_size; i++) {
        *(shmp + i) = 0;
    }
    for (i = 0; i < shm_size; i++) {
       printf("shmp at %d: %d\n", i, *(shmp + i)); 
    }



//END: Setting up shared memory.
//*****************************************************
//BEGIN: Creating children.

    //Array containing pids of all children.
    //Used for killing them after interrupt.
    pid_t childpid[100];
    //The number of children at a given time.
    int child_count = 0;
    //The number of children created so far.
    int child_count_total = 0;
    //The logical id given to a child.
    int child_id = 1;

    do {
        /*If an interrupt occured, break.
        if ( done_flag ) {
            break;
        }*/

        //If there are less children right now than the
        //simultaneous max,
        if ( child_count < mx_chdrn ) {
            //Check if there is still simulated time left.
            if ( 1 /*TODO: Put the simulated time check here.*/ ) {        
                //Create a child.
                if ( (childpid[child_count_total] = fork()) < 0 ) {
                    fprintf(stderr, "%s: Error: fork() failed to create child.\n%s\n", argv[0], strerror(errno));
                    return 1;
                } else if ( childpid[child_count_total] == 0 ) {
                    //Inside child,
                    //Build the argv.
                    //char arg1[PALINSIZE];
                    //char arg2[16];
                    //char arg3[16];
                    //sscanf(buffer, "%s", arg1);
                    //sprintf(arg2, "%d", max_lifetime_children);
                    //sprintf(arg3, "%d", child_id);
                    char *arg_vector[] = {"./user", NULL};
                    //exec.
                    execv(arg_vector[0], arg_vector);
                } else {
                    //Inside parent,
                    //increment the simultaneous child count.
                    child_count++;
                    //increment the total child count.
                    child_count_total++;
                    //increment the logical child_id.
                    child_id++;
                }
            } else {
                //Simulated time ran out.
                break;
            }
        }

        //See if a child finished.
        if ( waitpid(-1, NULL, WNOHANG) > 0 ) {
            child_count--;
        }

    //TODO: Fix this check for more than one child.
    } while ( (child_count_total < 1) );

//END: Creating children.
//*****************************************************
//BEGIN: Finishing up.

    //Wait for all children.
    while ( wait(NULL) > 0 );

    //Clean up shared memory.
    shmdt(shmp);
    shmctl(shmid, IPC_RMID, 0);

    return 0;
}

int help(char *progname)
{
    printf("hElp mE..\n");
    return 0;
}
