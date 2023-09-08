#include "phase1.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct Process{
    int pid; //process id
    Process * children;
    Process * parent;
    int returnStatus;
    int priority;
};
//-----Global Vars---------//
Process ProcList[MAXPROC];

void phase1_init(void){

}


void startprocesses(void){
    USLOSS_Context_Switch()
}

int fork1(char *name, int(*startFunc)(char *), char *arg, int stacksize, int priority){

}

int join(int *status){

}

void quit(int status, int switchToPID){

}

/* 

// to be implemented in 1b
void zap(int pid){

}

int isZapped(void){

}
*/

int getpid(void){

}

void dumpProcesses(void){

}

/**
// to be implemented in 1b
void blockMe(int newStatus){    

}

int unblockProc(int pid){

}

int readCurStartTime(void){

}

int currentTime(void){

}

int readtime(void){

}

void timeSlice(void){

}

*/


