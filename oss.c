//*****************************************************
//Program Name: oss
//Author: Cory Mckiel
//Date Created: Oct 19, 2020
//Last Modified: Oct 20, 2020
//Program Description:
//      oss creates children and manages them to do some
//      dummy task.
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
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILENAMESIZE 64

//struct for message queue.
typedef struct {
    long mtype;
} mymsg_t;


//help declaration
int help(char*);

//done_flag lets master know when either the
//timer is up or ctrl-c has been invoked.
//Volatile so that compiler knows to check
//every time rather than optimize. sig_atomic_t to ensure
//it only gets changed by one thing at a time.
static volatile sig_atomic_t done_flag = 0;

//Sets a flag to indicate when program should exit gracefully.
static void setdoneflag(int s)
{
    fprintf(stderr, "\nInterrupt Received.\n");
    done_flag = 1;
}

//Set the signal handler to listen to the timer
//and ctrl-c.
static int setupinterrupt()
{
    struct sigaction act;
    act.sa_handler = setdoneflag;
    act.sa_flags = 0;
    return (sigemptyset(&act.sa_mask) || sigaction(SIGALRM, &act, NULL) || sigaction(SIGINT, &act, NULL));
}

//Set up the interrupt timer for the time specified.
static int setuptimer(int time)
{
    struct itimerval value;
    if ( time <= 1000 ) {
        value.it_interval.tv_sec = time;
        value.it_interval.tv_usec = 0;
    } else {
        return -1;
    }
    value.it_value = value.it_interval;
    return (setitimer(ITIMER_REAL, &value, NULL));
}


//*****************************************************
///////////////////////MAIN////////////////////////////
//*****************************************************
int main(int argc, char *argv[])
{
//*****************************************************
//BEGIN: Command line processing.

    //Used throughout the program for iterating.
    int i = 0; 

    //max num of concurrent children.
    int mx_chdrn = 5;
    //max real time allocated to this process.
    int ptime = 20;
    //Log file name and pointer.
    char logfile[FILENAMESIZE] = "log.out";
    FILE *fp = NULL;

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

    fp = fopen(logfile, "a");

//END: Command line processing.
//*****************************************************
//BEGIN: Interrupt setup.

    if ( setupinterrupt() == -1 ) {
        fprintf(stderr, "%s: Error: Failed to set up interrupt.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    if ( setuptimer(ptime) == -1 ) {
        fprintf(stderr, "%s: Error: Failed to set up timer.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

//END: Interrupt setup.
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

//END: Setting up shared memory.
//*****************************************************
//BEGIN: Setting up message queue.

    mymsg_t *msg;
    int msgid;
    key_t msgkey;

    //Generate key deterministically so that children
    //can do the same and attach to message queue.
    if ( (msgkey = ftok("./", 922)) == -1 ) {
        fprintf(stderr, "%s: Error: ftok() failed to generated key.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    if ( (msgid = msgget(msgkey, 0666 | IPC_CREAT)) == -1) {
        fprintf(stderr, "%s: Error: msgget() failed to create message queue.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    if ( (msg = (mymsg_t*)malloc(sizeof(mymsg_t))) == NULL ) {
        fprintf(stderr, "%s: Error: malloc() failed to allocate message.\n%s\n", argv[0], strerror(errno));
        return 1;
    }

    //Arbitrary.
    msg->mtype = 1;
    
    if ( msgsnd(msgid, msg, 0, 0) == -1 ) {
        fprintf(stderr, "%s: Error: msgsnd() failed to send message.\n%s\n", argv[0], strerror(errno));
        free(msg);
        return 1;
    }

//END: Setting up message queue.
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
    //Number of children that have finished.
    int done_child = 0;
    //Flag for end of simulated time.
    int time_is_up = 0;
    int sec = 0;
    int nsec = 0;

    do {
        //If an interrupt occured, break.
        if ( done_flag ) {
            break;
        }

        //If there are less children right now than the
        //simultaneous max,
        if ( (child_count < mx_chdrn) && (child_count_total < 100) ) {
            //Check if there is still simulated time left.
            if ( 1 /*Meant to take this out.*/ ) {        
                //Create a child.
                if ( (childpid[child_count_total] = fork()) < 0 ) {
                    fprintf(stderr, "%s: Error: fork() failed to create child.\n%s\n", argv[0], strerror(errno));
                    return 1;
                } else if ( childpid[child_count_total] == 0 ) {
                    //Inside child,
                    //Build the argv.
                    char *arg_vector[] = {"./user", NULL};
                    //exec.
                    execv(arg_vector[0], arg_vector);
                } else {
                    //Inside parent,
                    fprintf(fp, "Master: Creating new child pid %d at my time %d.%d\n", childpid[child_count_total],sec, nsec);
                    //increment the simultaneous child count.
                    child_count++;
                    //increment the total child count.
                    child_count_total++;
                    //increment the logical child_id.
                    child_id++;
                }
            } else {
                break;
            }
        }

        //If I grabbed a message from the queue.
        if (msgrcv(msgid, msg, 0, 0, 0) == 0) {

            //CRITICAL SECTION:
            //
            //if shm_pid is not 0, it contains
            //the pid of child who is done.
            
            if ( *(shmp + 2) != 0 ) {
                fprintf(fp, "Master: Child pid %d is terminating at system clock time %d.%d\n", *(shmp + 2), *shmp, *(shmp+1));
                
                waitpid(*(shmp + 2), NULL, 0);
                //Set shm_pid back to 0.
                *(shmp + 2) = 0;
                //Decrement the current number of children.
                child_count--;
                //Increase the number of finished children.
                done_child++;
            }
    
            //Increment the clock.
            if (*(shmp+1) + 100 >= 1000000000) {
                *shmp += 1;
                *(shmp+1) = 0;
            } else {
                *(shmp+1) += 100;
            }
          
            //Save the clock.
            sec = *shmp;
            nsec = *(shmp+1);
 
            //Check if over 2 sim seconds.
            if (*shmp >= 2) {
                time_is_up = 1;
            }
 
            //Make sure to send the msg back into the queue
            //so someone else can go into critical section.
            if ( msgsnd(msgid, msg, 0, 0) == -1 ) {
                fprintf(stderr, "%s: Error: msgsnd() failed to send message.\n%s\n", argv[0], strerror(errno));
                free(msg);
                return 1;
            }

        }

    } while (done_child != 100 && !time_is_up);

//END: Creating children.
//*****************************************************
//BEGIN: Finishing up.

    //If an interrupt occured, kill the children.
    if ( done_flag ) {
        for ( i = 0; i < child_count_total; i++ ) {
            kill(childpid[i], SIGINT);
        }
    }

    //Wait for all children.
    while ( wait(NULL) > 0 );

    fprintf(stderr, "oss: Finished at time %d.%d\n", *shmp, *(shmp+1));

    //Clean up shared memory.
    shmdt(shmp);
    shmctl(shmid, IPC_RMID, 0);

    //Close message queue.
    msgctl(msgid, IPC_RMID, NULL);

    free(msg);

    fclose(fp);

    return 0;
}

int help(char *progname)
{
    printf("%s: Usage: %s [-c x] [-l filename] [-t z]\n", progname, progname);
    printf("Where: x is max number of concurrent children. Default 5\n");
    printf("filename is name of output file. Default: log.out\n");
    printf("z is real time in seconds before program should terminate. Default: 20\n");
    return 0;
}
