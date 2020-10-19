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


    return 0;
}

int help(char *progname)
{
    printf("hElp mE..\n");
    return 0;
}
