#include "phase1.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//Type Definitions
#define EMPTY 0
#define READY  1
#define RUNNING 2
#define QUIT 3
#define JOIN 4
#define BLOCKED 5
#define ZAPPED 6
#define CURRENT 7



typedef struct Process Process;

struct Process{
    int pid; //process id
    int Status; 
    int priority;
    char name[MAXNAME];
    USLOSS_Context state;
    int (* startFunc) (char *);
    char startArgs[MAXARG];
    char *stack;
    int stackSize;
    int returnVal;
    Process * children;
    Process * parent;
    Process * nextSibling;
    int childRet;
    int childStatus;
};
//-----Global Vars---------//
Process ProcList[MAXPROC];
int CurrentPID;
int nextPID = 1;
int sentinel(char *);
int prevProc =- 1;

void phase1_init(void){
    //memset ProcList
    memset(ProcList, 0, sizeof(ProcList));
    //initialize PCB for init
    //Initialize the proccess list
    for(int i = 0; i < MAXPROC; i++){
        ProcList[i].pid = -1;
        ProcList[i].Status = EMPTY;
        ProcList[i].priority = -1;
        ProcList[i].name[0] = '\0';
        ProcList[i].startFunc = NULL;
        ProcList[i].startArgs[0] = '\0';
        ProcList[i].stack = NULL;
        ProcList[i].stackSize = -1;
        ProcList[i].returnVal = -1;
        ProcList[i].children = NULL;
        ProcList[i].parent = NULL;
        ProcList[i].nextSibling = NULL;
        ProcList[i].childRet = -1;
        ProcList[i].childStatus = EMPTY;
    }

    // CurrentPID = 1;
    //Initialize PCB for init
    ProcList[0].pid = 1;
    ProcList[0].Status = RUNNING;
    ProcList[0].priority = 6;
    strcpy(ProcList[0].name, "init");
    // ProcList[0].startFunc = NULL;
    // ProcList[0].startArgs[0] = '\0';
    ProcList[0].stack = malloc(USLOSS_MIN_STACK);
    ProcList[0].stackSize = USLOSS_MIN_STACK;
    // ProcList[0].returnVal = -1;
    // ProcList[0].children = NULL;
    // ProcList[0].parent = NULL;
    // ProcList[0].nextSibling = NULL;
    CurrentPID = ProcList[0].pid;
    
    //USLOSS_ContextInit(&(ProcList[0].state), &(ProcList[0].stack), ProcList[0].stackSize, NULL, NULL);
    //Call Fork1 to start sentinell
    int res = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 6);
    if(res < 0){
        USLOSS_Console("Fork1(): Unable to fork sentinel, Stopping Proces.");
        USLOSS_Halt(1);
    }
    //CurrentPID = res;
    //FOrk to testCase_main
    res = fork1("testcase_main", testcase_main, NULL, USLOSS_MIN_STACK, 6);
    //context switch to test case main
    USLOSS_ContextSwitch(&(ProcList[0].state), &(ProcList[res-1].state));
    CurrentPID = res;
    return;
    }


void startProcesses(void){
    //Context swith to init
    USLOSS_ContextSwitch(NULL, &(ProcList[0].state));
    CurrentPID = ProcList[0].pid;
}

int fork1(char *name, int(*startFunc)(char *), char *arg, int stacksize, int priority){
    int procIndex = -1;
    //Check for errors
    if(strlen(name) >= (MAXNAME-1)){
        USLOSS_Console("Fork1(): Name of the process is too long, Stopping Proces.");
        USLOSS_Halt(1);
    }

    //Find next empty PCB
    for(int i = 0; i <MAXPROC;i++){
        if(ProcList[(nextPID + i)% MAXPROC].Status == EMPTY){
            procIndex = nextPID + i;
            break;
        }
        if(i == MAXPROC -1){
            return -1;
        }
    }
    //Initialize PCB
    nextPID = procIndex + 1;
    Process * newProc = &ProcList[procIndex];
    newProc->pid = procIndex + 1;
    newProc->Status = EMPTY;
    newProc->priority = priority;
    *(newProc->name) = (char *)malloc(MAXNAME);
    strcpy(newProc->name, name);
    *(newProc->startArgs) = (char *)malloc(MAXARG);
    
    if(arg != NULL){
        strcpy(newProc->startArgs, arg);
    }

    newProc->startFunc = startFunc;
    newProc->stack = malloc(stacksize);
    newProc->stackSize = stacksize;
    newProc->returnVal = -1;
    newProc->children = NULL;
    newProc->parent = &(ProcList[CurrentPID-1]);
    newProc->nextSibling = NULL;
    newProc->childRet = -1;
    newProc->childStatus = EMPTY;

    newProc->parent->childStatus = READY;
    //Initialize usloss context
    USLOSS_ContextInit(&(newProc->state), newProc->stack, newProc->stackSize, NULL, newProc->startFunc);
    return newProc->pid;
}

//Create sentinel process
int sentinel(char *arg){
    while(1){
        phase2_check_io();
        USLOSS_WaitInt();
    }
}

int join(int *status){
    //Check if any children are quit
    Process * current = &ProcList[CurrentPID-1];
    Process * child = current->children;
    if(child == NULL){
        return -2;
    }
    while(child != NULL){
        if(child->Status == QUIT){
            *status = child->returnVal;
            int retPid = child->pid;
            freeProc(*child);
            return retPid;
        }
        child = child->nextSibling;
    }
    //If no children are quit, block the process
    current->Status = JOIN;
    //USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, status);
    return ;
}

void freeProc(Process p){
    p.pid = -1;
    p.Status = EMPTY;
    p.priority = -1;
    p.name[0] = '\0';
    p.startFunc = NULL;
    p.startArgs[0] = '\0';
    p.stack = NULL;
    p.stackSize = -1;
    p.returnVal = -1;
    p.children = NULL;
    p.parent = NULL;
    p.nextSibling = NULL;
}

void quit(int status, int switchToPID){
    if(CurrentPID == 1){
        USLOSS_Console("Quit(): Cannot quit the init process, Stopping Proces.");
        USLOSS_Halt(1);
    }
    ProcList[CurrentPID-1].Status = QUIT;
    ProcList[CurrentPID-1].parent->childRet = status;
    ProcList[CurrentPID-1].parent->childStatus = QUIT;
    //Context swith to switchToPID
    USLOSS_ContextSwitch(&(ProcList[CurrentPID-1].state), &(ProcList[switchToPID-1].state));

}

void TEMP_switchTo(int newpid){
prevProc = CurrentPID;
CurrentPID = newpid;
USLOSS_ContextSwitch(&(ProcList[prevProc-1].state), &(ProcList[CurrentPID-1].state));
}

/* 

// to be implemented in 1b
void zap(int pid){

}

int isZapped(void){

}
*/

int getpid(void){
return CurrentPID;
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


