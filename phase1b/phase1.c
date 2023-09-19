/**
 * @file phase1.c
 *
 * @brief This file contains the phase1a functions.
 *
 * @course CSC 452
 *
 * @details The functions in this file are used to create and manage processes. The functions are fork1(), join(), quit(),
 * getpid(), dumpProcesses(), and startProcesses(). The functions are called by the testcases. These function are the
 * building blocks for a rudimentary process management system. They are responsible for creating processes, switching
 * between processes, and terminating processes.
 *
 *
 * @authors Shrey Goel, Muhtasim Al-Farabi
 */

//-----Include Files---------//
#include "phase1.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//-----Definitions---------//
#define EMPTY 0
#define READY 1
#define RUNNING 2
#define QUIT 3
#define JOIN 4
#define BLOCKED 5
#define ZAPPED 6
#define CURRENT 7

typedef struct Process Process;

/*
 * Process table entry data structure.
 */
struct Process
{
    int pid;                  // process id
    int Status;               // EMPTY, READY, RUNNING, QUIT, JOIN, BLOCKED, ZAPPED, CURRENT
    int priority;             // 1 - 7, where 6, and 7 are for sentinel and init
    char name[MAXNAME];       // process name
    USLOSS_Context state;     // process state for USLOSS
    int (*startFunc)(char *); // function where process begins
    char startArgs[MAXARG];   // args passed to startFunc
    char *stack;              // location of stack
    int stackSize;            // size of stack
    int returnVal;            // value returned by process
    Process *parent;          // parent process
    Process *children;        // children processes
    Process *nextSibling;     // next sibling process (Linked list)
    Process *ReadyChild;      // next child process that is ready to run
    int childRet;             // return value of the child
    int childStatus;          // status of the child
};

//-----Global Vars---------//
/**
 * @brief The process table
 *
 * The process table is an array of process slots. Each slot contains all the information about a process, including its
 * process ID (PID), its process status, its priority, pointers to its parent and children, its context, and its stack.
 * The process table is initialized by the function phase1_init(). The process table is manipulated by the functions
 * fork1(), join(), quit().
 */
Process ProcList[MAXPROC];

Process *ReadyProcessHead;

Process *CurrentProc;
int nextPID = 1;
int prevProc = -1;

//-----Function Prototypes---------//
void trampoline(void);
int sentinel(char *);
int initFunc(char *);
int testMain_trampoline(char *);
void userMode(char *);
void freeProc(Process *);

//-----Function Implementations---------//

/**
 * @brief Initializes the process table and forks the sentinel process. It also forks the testcasemain process.
 */
void phase1_init(void)
{
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();

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
        ProcList[i].ReadyChild = NULL;
        ProcList[i].childRet = -1;
        ProcList[i].childStatus = EMPTY;
    }

    ReadyProcessHead = NULL; // Initialize the ready process Linked-list

    // Initialize PCB for init
    int index = nextPID;
    ProcList[index].pid = 1;
    ProcList[index].Status = READY;
    ProcList[index].priority = 6;
    strcpy(ProcList[index].name, "init");
    ProcList[index].startFunc = initFunc;
    strcpy(ProcList[index].startArgs, "init");
    ProcList[index].stack = malloc(USLOSS_MIN_STACK);
    ProcList[index].stackSize = USLOSS_MIN_STACK;
    USLOSS_ContextInit(&(ProcList[index].state), ProcList[index].stack, ProcList[index].stackSize, NULL, trampoline);

    CurrentProc = &(ProcList[index]);
    // Call Fork1 to start sentinell
    int res = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 7);
    if (res < 0)
    {
        USLOSS_Console("Fork1(): Unable to fork sentinel, Stopping Proces.");
        USLOSS_Halt(1);
    }

    // Call Fork1 to start testcase_main
    res = fork1("testcase_main", testMain_trampoline, NULL, USLOSS_MIN_STACK, 3);
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using fork1() to create it.\n");
    TEMP_switchTo(res);
}

/**
 * @brief This function is called in the init process. It is a temporary hack to call the testcase_main() function.
 */
int initFunc(char *IDK)
{
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.");
    USLOSS_Halt(0);
    return 0;
}

/**
 * @brief Currently does nothing. Might need it for phase1b
 */
void startProcesses(void)
{

    CurrentProc = &(ProcList[0]);
    USLOSS_Halt(0);
}

/**
 * @brief Finds the next empty index in the process table.
 *
 * @return int The index of the next empty slot in the process table. If no empty slot is found, -1 is returned.
 */
int findNextIndex()
{
    int retIndex = -1;
    int i;
    int searchPid = nextPID;

    for (i = (searchPid % MAXPROC); i < MAXPROC; i++)
    {
        if (ProcList[i].Status == EMPTY)
        {
            retIndex = i;
            // nextPID++;
            break;
        }
        else
        {
            nextPID++;
        }
    }

    if (retIndex == -1)
    {
        for (i = 0; i < (searchPid % MAXPROC); i++)
        {
            if (ProcList[i].Status == EMPTY)
            {
                retIndex = i;
                // nextPID++;
                break;
            }
            else
            {
                nextPID++;
            }
        }
    }

    return retIndex;
}

/**
 * @brief Returns the next PID to be used.
 */
int getNextPID()
{
    return nextPID;
}

/**
 * @brief Prints the details of the process at the given pid (test function for phase1b)
 */
void ProcPrinter(int pid)
{
    // Print Details about the proccess ath given pid
    USLOSS_Console("PID: %d\t", ProcList[pid % MAXPROC].pid);
    USLOSS_Console("Status: %d\t", ProcList[pid % MAXPROC].Status);
    USLOSS_Console("Priority: %d\t", ProcList[pid % MAXPROC].priority);
    USLOSS_Console("Name: %s\n", ProcList[pid % MAXPROC].name);
}

void readyListAdd(Process *entry)
{
    if (ReadyProcessHead == NULL)
    {
        ReadyProcessHead = entry;
    }
    else
    {
        // Add sorted by priority
        Process *curr = ReadyProcessHead;
        while (curr->ReadyChild != NULL)
        {
            if (curr->ReadyChild->priority <= entry->priority)
            {
                break;
            }
            curr = curr->ReadyChild;
        }
        if (curr->ReadyChild != NULL) // This is wierd MAybe we switch this to == NULL
        {
            curr->ReadyChild = entry;
        }
        else
        {
            Process *temp = curr->ReadyChild;
            curr->ReadyChild = entry;
            curr->ReadyChild->ReadyChild = temp;
        }
    }
}


void readyListDel(Process* entry){
    Process* curr = ReadyProcessHead;
    entry->Status = CURRENT;
    while(curr->ReadyChild != NULL && curr->ReadyChild != entry){
        curr = curr->ReadyChild;
    }
    if(curr->ReadyChild == NULL){
        if(curr->pid == entry->pid){
            curr = NULL;
        }
        else{
            USLOSS_Console("ERROR: readyListDel(): Process not found in ready list.\n");
            //USLOSS_Halt(1);
        }
    }
    else{
        curr->ReadyChild = curr->ReadyChild->ReadyChild;
    }

}


/**
 * @brief Creates a new process and adds it to the process table. The process is added to the parent's children list.
 *
 * @param name The name of the process
 * @param startFunc The function where the process begins
 * @param arg The arguments passed to the startFunc
 * @param stacksize The size of the stack
 * @param priority The priority of the process
 *
 * @return int The PID of the new process. If the process table is full, -1 is returned.
 * If the priority is greater than 5, -1 is returned. If the stacksize is less than USLOSS_MIN_STACK, -2 is returned.
 */
int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority)
{
    userMode("fork1");
    // Check for errors
    if (stacksize < USLOSS_MIN_STACK)
    {
        return -2;
    }
    if (strcmp(name, "sentinel") != 0 && priority > 5)
    {
        return -1;
    }

    if (priority < 1 || startFunc == NULL || name == NULL || strlen(name) > MAXNAME)
    {
        return -1;
    }

    int procIndex = findNextIndex();
    if (procIndex == -1)
    {
        return -1;
    }

    // Initialize PCB for new process
    ProcList[procIndex].pid = nextPID;
    ProcList[procIndex].Status = READY;
    ProcList[procIndex].priority = priority;
    strcpy(ProcList[procIndex].name, name);
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

    // Add to parent's children list
    if (CurrentProc->children == NULL)
    {
        CurrentProc->children = &(ProcList[procIndex]);
    }
    else
    {
        // Add to head
        Process *temp = CurrentProc->children;
        CurrentProc->children = &(ProcList[procIndex]);
        ProcList[procIndex].nextSibling = temp;
    }
    ++nextPID;
    return ProcList[procIndex].pid;
}

/**
 * @brief This function is a wrapper for the startFunc of the process. It is made for USLOSS_ContextInit().
 */
void trampoline()
{
    int res = USLOSS_PsrSet(USLOSS_PSR_CURRENT_MODE | USLOSS_PSR_CURRENT_INT);
    CurrentProc->startFunc(CurrentProc->startArgs);
    res += 1;
}

/**
 * @brief This function is a wrapper for the testcase_main() function. It is made for USLOSS_ContextInit().
 *
 * @param PlaceHolder This is a placeholder for the argument that is passed to the testcase_main() function.
 *
 * @return int This function returns 0.
 */
int testMain_trampoline(char *PlaceHolder)
{
    testcase_main();
    USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
    USLOSS_Halt(0);
    return 0;
}

/**
 * @brief This function is the sentinel process. It is the first process to be created. Will be used in phase2 and beyond.
 *
 * @param arg This is a placeholder for the argument that is passed to the sentinel() function.
 *
 */
int sentinel(char *arg)
{
    while (1)
    {
        phase2_check_io();
        USLOSS_WaitInt();
    }
}

/**
 * @brief This function is called by the parent process to wait for a child process to quit. If the child has already quit,
 * the function returns immediately. Otherwise, the parent blocks until the child quits. The parent can call join() on a
 * child only once. If the parent calls join() on a child that has not quit, the parent is blocked until the child quits.
 *
 * @param status This is a pointer to an integer where the return value of the child is stored.
 *
 * @return int The PID of the child that quit. If the parent has no children, -2 is returned.
 */
int join(int *status)
{
    Process *current = CurrentProc; // current = parent
    Process *curr = current->children;
    if (curr == NULL)
    {
        return -2;
    }

    // Check Head is quit
    if (curr->Status == QUIT)
    {
        *status = curr->returnVal;
        int retPid = curr->pid;
        current->children = curr->nextSibling;
        freeProc(curr);
        return retPid;
    }

    // checking the rest of the children
    Process *prev = curr;
    curr = curr->nextSibling;
    while (curr != NULL)
    {
        if (curr->Status == QUIT)
        {
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
    USLOSS_Console("Join(): We should not be here. \n");
    USLOSS_Halt(1);
    return -2;
}

/**
 * @brief This function resets the entry in the process table for the process.
 *
 * @param p This is a pointer to the process that is to be freed.
 */
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
    p->ReadyChild = NULL;
    p->childRet = -1;
    p->childStatus = EMPTY;
}

/**
 * @brief This function is called by a process to quit. It changes the status of the process to QUIT and tells the parent
 * that the process has quit. It switches to the process that the current process is supposed to switch to.
 *
 * @param status This is the return value of the process.
 * @param switchToPID This is the PID of the process that the current process will switch to.
 */
void quit(int status)
{
    userMode("quit");

    // Error checking
    if (CurrentProc->pid == 1)
    {
        USLOSS_Console("Quit(): Cannot quit the init process, Stopping Proces.");
        USLOSS_Halt(1);
    }

    if (CurrentProc->children != NULL)
    {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n", CurrentProc->pid);
        USLOSS_Halt(1);
    }

    // Change Currents status
    CurrentProc->Status = QUIT;
    CurrentProc->returnVal = status;
    // Change currents parents child status
    CurrentProc->parent->childStatus = QUIT;
    CurrentProc->parent->childRet = status;
    // Context swith to switchToPID
    // TEMP_switchTo(switchToPID);
}

/**
 * @brief This function switches the current process to the process with the given PID. This is a temporary hack for phase1a.
 *
 * @param newpid This is the PID of the process that the current process will switch to.
 */
void TEMP_switchTo(int newpid)
{
    int indexFrom = (CurrentProc->pid % MAXPROC);
    int indexTo = newpid % MAXPROC;
    CurrentProc = &(ProcList[indexTo]);
    CurrentProc->Status = RUNNING;
    if (CurrentProc->parent != NULL)
    {
        CurrentProc->parent->Status = READY;
    }
    USLOSS_ContextSwitch(&(ProcList[indexFrom].state), &(ProcList[indexTo].state));
}

/**
 * @brief This function returns the PID of the current process.
 */
int getpid(void)
{
    return CurrentProc->pid;
}

/**
 * @brief This function prints the details of all the processes in the process table.
 */
void dumpProcesses(void)
{
    USLOSS_Console(" PID  PPID  NAME              PRIORITY  STATE\n");
    for (int i = 0; i < MAXPROC; i++)
    {
        if (ProcList[i].Status != EMPTY)
        {
            USLOSS_Console("%4d  %4d  %-17s %-10d", ProcList[i].pid, ProcList[i].parent == NULL ? 0 : ProcList[i].parent->pid, ProcList[i].name, ProcList[i].priority);

            switch (ProcList[i].Status)
            {
            case QUIT:
                USLOSS_Console("Terminated(%d)\n", ProcList[i].returnVal);
                break;
            case RUNNING:
                USLOSS_Console("Running\n");
                break;
            default:
                USLOSS_Console("Runnable\n");
                break;
            }
        }
    }
}

/**
 * @brief This function checks the mode of the process. If the process is in user mode, the function prints an error message.
 *
 * @param name This is the name of the function that was called in user mode.
 */
void userMode(char *name)
{
    if (!(USLOSS_PsrGet() & 1))
    {
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", name);
        USLOSS_Halt(1);
    }
}

// to be implemented in 1b
void zap(int pid)
{
}

int isZapped(void)
{
}
void blockMe(int newStatus)
{
}

int unblockProc(int pid)
{
    int index = pid % MAXPROC;
    if (ProcList[index].Status == BLOCKED)
    {
        ProcList[index].Status = READY;
        return 0;
    }
    return -2;
}

int readCurStartTime(void)
{
}

int currentTime(void)
{
}

int readtime(void)
{
}

void timeSlice(void)
{
}

void dispatcher(void)
{
    // EnableInterrupts();
}
