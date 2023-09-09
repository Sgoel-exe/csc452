#include "phase1.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Process Process;

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


void startProcesses(void){
    //USLOSS_ContextSwitch();
}

int fork1(char *name, int(*startFunc)(char *), char *arg, int stacksize, int priority){

return 0;
}

int join(int *status){
return 0;
}

void quit(int status, int switchToPID){
    
}

void TEMP_switchTo(int newpid){

}

/* 

// to be implemented in 1b
void zap(int pid){

}

int isZapped(void){

}
*/

int getpid(void){
return 0;
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


