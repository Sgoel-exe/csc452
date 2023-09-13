#include "phase1.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Type Definitions
#define EMPTY 0
#define READY 1
#define RUNNING 2
#define QUIT 3
#define JOIN 4
#define BLOCKED 5
#define ZAPPED 6
#define CURRENT 7

typedef struct Process Process;


/**
 * @brief The process table
 *
 * The process table is an array of process slots. Each slot contains all the information about a process, including its
 * process ID (PID), its process status, its priority, pointers to its parent and children, its context, and its stack.
 * The process table is initialized by the function phase1_init(). The process table is manipulated by the functions
 * fork1(), join(), quit().
 *
 */

struct Process
{
    int pid; // process id
    int Status;
    int priority;
    char name[MAXNAME];
    USLOSS_Context state;
    int (*startFunc)(char *);
    char startArgs[MAXARG];
    char *stack;
    int stackSize;
    int returnVal;
    Process *parent;
    Process *children;
    Process *nextSibling;
    int childRet;
    int childStatus;
};


//-----Global Vars---------//
Process ProcList[MAXPROC];
// int CurrentPID;
Process *CurrentProc;
int nextPID = 1;
int prevProc = -1;

//-----Function Prototypes---------//
void trampoline(void);
int sentinel(char *);
int initFunc(char *);
int testMain_trampoline(char *);

/**
 * @brief Initializes the process table and forks the sentinel process. It also forks the testcasemain process.
 *
 *
 */

void phase1_init(void)
{
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
    // memset ProcList
    memset(ProcList, 0, sizeof(ProcList));
    // Initialize the proccess list
    for (int i = 0; i < MAXPROC; i++)
    {
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
    // Initialize PCB for init
    int index = nextPID ;
    ProcList[index].pid = 1;
    ProcList[index].Status = READY;
    ProcList[index].priority = 6;
    strcpy(ProcList[index].name, "init");
    // Assign halter to startfunc
    ProcList[index].startFunc = initFunc;
    ProcList[index].startArgs[0] = 'FUCKTHISCLASS';
    ProcList[index].stack = malloc(USLOSS_MIN_STACK);
    ProcList[index].stackSize = USLOSS_MIN_STACK;
    ProcList[index].returnVal = -1;
    ProcList[index].children = NULL;
    ProcList[index].parent = NULL;
    ProcList[index].nextSibling = NULL;
    // Initialize usloss context
    USLOSS_ContextInit(&(ProcList[index].state), ProcList[index].stack, ProcList[index].stackSize, NULL, trampoline);
    // CurrentPID = ProcList[0].pid;
    CurrentProc = &(ProcList[index]);
    // Call Fork1 to start sentinell
    ++nextPID;
    int res = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 7);
    USLOSS_Console("init(): next pid %d\n", nextPID);
    if (res < 0)
    {
        USLOSS_Console("Fork1(): Unable to fork sentinel, Stopping Proces.");
        USLOSS_Halt(1);
    }

    res = fork1("testcase_main", testMain_trampoline, NULL, USLOSS_MIN_STACK, 3);
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using fork1() to create it.\n");
    USLOSS_Console("init(): next pid %d\n", nextPID);
    TEMP_switchTo(res);
}

int initFunc(char *IDK)
{
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.");
    USLOSS_Halt(0);
}

void startProcesses(void)
{
    // Context switch to init
    USLOSS_Console("startProcesses(): How da heck we got here\n");
    CurrentProc = &(ProcList[0]);
    // CurrentPID = ProcList[0].pid;
    USLOSS_Halt(0);
}

int emptySlotAvailable() 
{
    int slot = -1;
    int i;
    int startPid = nextPID;

    // find the next available slot in the table between startPid and MAXPROC
    for (i = (startPid % MAXPROC); i < MAXPROC; i++)
    {
        if (ProcList[i].Status == EMPTY)
        {
                slot = i;
                // nextPID++;
                break;
        }
        else
        {
                nextPID++;
        }
    }

        // if there was no free slot in the previous loop
        
    if (slot == -1)
    {
            // try to find a free slot between 0 and startPid
        for (i = 0; i < (startPid % MAXPROC); i++)
        {
            if (ProcList[i].Status == EMPTY)
            {
                slot = i;
                // nextPID++;
                break;
            }
            else
            {
                nextPID++;
            }
        }
    }
    
    return slot;
}

int getNextPID()
{
    return nextPID;
}

void ProcPrinter(int pid){
    //Print Details about the proccess ath given pid
    USLOSS_Console("PID: %d\t", ProcList[pid % MAXPROC].pid);
    USLOSS_Console("Status: %d\t", ProcList[pid % MAXPROC].Status);
    USLOSS_Console("Priority: %d\t", ProcList[pid % MAXPROC].priority);
    USLOSS_Console("Name: %s\n", ProcList[pid % MAXPROC].name);
}

int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority)
{
    // Check for errors
    if(stacksize < USLOSS_MIN_STACK){
        return -2;
    }
    

    if (priority<1 || startFunc==NULL || name==NULL || strlen(name)>MAXNAME)
    {
        return -1;
    }
    

    int procIndex = emptySlotAvailable();
    if(procIndex == -1){
        return -1;
    }
    ProcList[procIndex].pid = nextPID;
    ProcList[procIndex].Status = READY;
    ProcList[procIndex].priority = priority;

    *(ProcList[procIndex].name) = (char *)malloc(MAXNAME);
    strcpy(ProcList[procIndex].name, name);
    
    *(ProcList[procIndex].startArgs) = (char *)malloc(MAXARG);
    if (arg != NULL)
    {
        strcpy(ProcList[procIndex].startArgs, arg);
    }
    ProcList[procIndex].startFunc = startFunc;
    ProcList[procIndex].stack = malloc(stacksize);
    ProcList[procIndex].stackSize = stacksize;
    ProcList[procIndex].returnVal = -1;
    ProcList[procIndex].children = NULL;
    ProcList[procIndex].parent = CurrentProc;
    ProcList[procIndex].nextSibling = NULL;
    ProcList[procIndex].childRet = -1;
    ProcList[procIndex].childStatus = EMPTY;
    ProcList[procIndex].parent->childStatus = READY;
    USLOSS_ContextInit(&(ProcList[procIndex].state), ProcList[procIndex].stack, ProcList[procIndex].stackSize, NULL, trampoline);

    // int currentIndex = CurrentPID - 1;
    if(CurrentProc->children == NULL){
        CurrentProc->children = &(ProcList[procIndex]);
    }
    else{
        //Add to head
        Process *temp = CurrentProc->children;
        CurrentProc->children = &(ProcList[procIndex]);
        ProcList[procIndex].nextSibling = temp;
    }

    return ProcList[procIndex].pid;
}

void trampoline()
{
    int psr_retval = USLOSS_PsrSet(USLOSS_PSR_CURRENT_MODE | USLOSS_PSR_CURRENT_INT);
    int res = CurrentProc->startFunc(CurrentProc->startArgs);
}

int testMain_trampoline(char *DontNeedThis)
{
    int res = testcase_main();
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
    USLOSS_Halt(0);
    return 0;
}

// Create sentinel process
int sentinel(char *arg)
{
    while (1)
    {
        phase2_check_io();
        USLOSS_WaitInt();
    }
}

int join(int *status)
{
    Process *current = CurrentProc; // current = parent
    Process *curr = current->children;
    if (curr == NULL)
    {
        //USLOSS_Console("Join(): No Children to report for process id %d.\n", CurrentPID);
        return -2;
    }

    //Check Head is quit
    if (curr->Status == QUIT)
    {
        //USLOSS_Console("Join(): Child %d has quit.\n", child->pid);
        *status = curr->returnVal;
        int retPid = curr->pid;
        current->children = curr->nextSibling;
        freeProc(curr);
        return retPid;
    }

    Process *prev = curr;
    curr = curr->nextSibling;
    while (curr != NULL)
    {
        if (curr->Status == QUIT)
        {
            //USLOSS_Console("Join(): Child %d has quit.\n", child->pid);
            *status = curr->returnVal;
            int retPid = curr->pid;
            prev->nextSibling = curr->nextSibling;
            freeProc(curr);
            return retPid;
        }
        prev = curr;
        curr = curr->nextSibling;
    }
    // If no children are quit, block the process
    // current->Status = JOIN;
    USLOSS_Console("Join(): Why did we reach here?. \n");
    USLOSS_Halt(1);
    return -2;
}

void freeProc(Process *p)
{
    p->pid = -1;
    p->Status = EMPTY;
    p->priority = -1;
    p->name[0] = '\0';
    p->startFunc = NULL;
    p->startArgs[0] = '\0';
    free(p->stack);
    p->stackSize = -1;
    p->returnVal = -1;
    p->children = NULL;
    p->parent = NULL;
    p->nextSibling = NULL;
}

void quit(int status, int switchToPID)
{
    if (CurrentProc->pid == 1)
    {
        USLOSS_Console("Quit(): Cannot quit the init process, Stopping Proces.");
        USLOSS_Halt(1);
    }

    if(CurrentProc->children != NULL){
        USLOSS_Console("Quit(): Cannot quit a process %d with children, Stopping Proces.", CurrentProc->pid);
        USLOSS_Halt(1);
    }
    // Print out process name and current pid
    // ProcList[prevPID - 1].Status = QUIT;
    // ProcList[prevPID - 1].parent->childRet = status;
    // ProcList[prevPID - 1].returnVal = status;
    // ProcList[prevPID - 1].parent->childStatus = QUIT;
    
    //Change Currents status
    CurrentProc->Status = QUIT;
    CurrentProc->returnVal = status;
    //Change currents parents child status
    CurrentProc->parent->childStatus = QUIT;
    CurrentProc->parent->childRet = status;
    // Context swith to switchToPID
    TEMP_switchTo(switchToPID);
}

void TEMP_switchTo(int newpid)
{
    // prevProc = CurrentPID;
    // ProcList[prevProc - 1].Status = READY;
    // CurrentPID = newpid;
    // ProcList[CurrentPID - 1].Status = RUNNING;
    //CurrentProc->Status = READY;
    int indexFrom = (CurrentProc->pid % MAXPROC);
    int indexTo = newpid % MAXPROC;
    CurrentProc = &(ProcList[indexTo]);
    CurrentProc->Status = RUNNING;
    // dumpProcesses();
    //USLOSS_Console("TEMP_switchTo(): Switching from %d to %d\n", indexFrom, indexTo);
    USLOSS_ContextSwitch(&(ProcList[indexFrom].state), &(ProcList[indexTo].state));
}

int getpid(void)
{
    return CurrentProc->pid;
}

void dumpProcesses(void){
    USLOSS_Console(" PID  PPID  NAME              PRIORITY  STATE\n");
    for (int i = 0; i < MAXPROC; i++)
    {
        if (ProcList[i].Status != EMPTY)
        {
            USLOSS_Console("%4d  %4d  %-17s %-10d", ProcList[i].pid, ProcList[i].parent==NULL?0:ProcList[i].parent->pid, ProcList[i].name, ProcList[i].priority);
            
            if(ProcList[i].Status == QUIT){
                USLOSS_Console("Terminated(%d)\n", ProcList[i].returnVal);
            }
            else if(ProcList[i].Status == RUNNING){
                USLOSS_Console("Running\n");
            }
            else{
                USLOSS_Console("Runnable\n");
            }
        }
    }

}

/**
// to be implemented in 1b
void zap(int pid){

}

int isZapped(void){

}
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
