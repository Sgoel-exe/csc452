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
 * between processes, and terminating processes. There are also functions that are used to check the mode of the process, timeslicing, zapping, blocking, and unblocking. The mastermind behind the process management system is the dispatcher() function. It is responsible for switching between processes. The dispatcher() function is called by the functions in this file.
 *
 *
 * @authors Shrey Goel, Muhtasim Al-Farabi, Russ
 */

//-----Include Files---------//
#include "phase1.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

//-----Definitions---------//
#define EMPTY 0
#define READY 1
#define RUNNING 2
#define QUIT 3
#define JOIN 4
#define JOINBLOCK 5
#define ZAPPED 6
#define CURRENT 7
#define ZAPBLOCK 8
#define SLICED 9
#define BLOCKED 10

typedef struct Process Process;

/*
 * Process table entry data structure.
 */
struct Process
{
    int pid;                  // process id
    int Status;               // EMPTY, READY, RUNNING, QUIT, JOIN, JOINBLOCK, ZAPPED, CURRENT
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
    int childRet;             // return value of the child
    int childStatus;          // status of the child
    long int startTime;
    long int totalTime;
    int isZapped;              // 0 if not zapped, 1 if zapped
    int calledZapped[MAXPROC]; // Saves PID of processes that called zap on this process
    int blockReason;           // 0 is unblocked, else blocked for reasons
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
Process *CurrentProc;
int nextPID = 1;
int prevProc = -1;

// DEBUG var
int debug1 = 0;

//-----Function Prototypes---------//

void trampoline(void);
int sentinel(char *);
int initFunc(char *);
int testMain_trampoline(char *);
void userMode(char *);
void freeProc(Process *);
void dispatcher(int);
void clockHandler(int, void *);
void timeSlice(void);
void DisableInterupts(void);
void EnableInterupts(void);
void switchToIndex(int);

//-----Function Implementations---------//
/**
 * @brief Initializes the process table and forks the sentinel process. It also forks the testcasemain process.
 */
void phase1_init(void)
{
    DisableInterupts();
    // for future phases
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
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
        ProcList[i].startTime = -1;
        ProcList[i].totalTime = 0;
        ProcList[i].isZapped = 0;
        for (int j = 0; j < MAXPROC; j++)
        {
            ProcList[i].calledZapped[j] = -1;
        }
        ProcList[i].blockReason = 0;
    }
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
    // Call Fork1 to start sentinel
    int res = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 7);
    if (res < 0)
    {
        USLOSS_Console("Fork1(): Unable to fork sentinel, Stopping Proces.");
        USLOSS_Halt(1);
    }

    // Call Fork1 to start testcase_main
    res = fork1("testcase_main", testMain_trampoline, NULL, USLOSS_MIN_STACK, 3);
    EnableInterupts();
    dispatcher(0);
    USLOSS_Console("Achivement Unlocked: How did we get here?\n");
}

/**
 * @brief This function is called in the init process. It is a temporary hack to call the testcase_main() function.
 *
 * @param idk This is a placeholder for the argument that is passed to the testcase_main() function.
 */
int initFunc(char *idk)
{
    USLOSS_Console("testcase_main() returned, simulation will now halt.");
    USLOSS_Halt(0);
    return 0;
}

/**
 * @brief Currently does nothing. Might need it for phase1b
 */
void startProcesses(void)
{
    CurrentProc = &(ProcList[0]);
    dispatcher(0);
    USLOSS_Halt(0);
}

/**
 * @brief This function enables kernel interupts.
 */

void EnableInterupts()
{
    int result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    assert(result == USLOSS_DEV_OK);
}

/**
 * @brief This function disables kernel interupts.
 */

void DisableInterupts()
{
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT);
    assert(result == USLOSS_DEV_OK);
}

/**
 * @brief Finds the next empty index in the process table.
 *
 * @return int The index of the next empty slot in the process table. If no empty slot is found, -1 is returned.
 */
int findNextIndex()
{
    DisableInterupts();
    int retIndex = -1;
    int i;
    int searchPid = nextPID;

    for (i = (searchPid % MAXPROC); i < MAXPROC; i++)
    {
        if (ProcList[i].Status == EMPTY)
        {
            retIndex = i;
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
                break;
            }
            else
            {
                nextPID++;
            }
        }
    }
    EnableInterupts();
    return retIndex;
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
    DisableInterupts();
    if (stacksize < USLOSS_MIN_STACK)
    {
        EnableInterupts();
        return -2;
    }
    if (strcmp(name, "sentinel") != 0 && priority > 5)
    {
        EnableInterupts();
        return -1;
    }

    if (priority < 1 || startFunc == NULL || name == NULL || strlen(name) > MAXNAME)
    {
        EnableInterupts();
        return -1;
    }

    int procIndex = findNextIndex();
    if (procIndex == -1)
    {
        EnableInterupts();
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
    ProcList[procIndex].childStatus = EMPTY;
    ProcList[procIndex].parent->childStatus = READY;
    ProcList[procIndex].parent->childRet = -1;
    ProcList[procIndex].startTime = 0;
    ProcList[procIndex].totalTime = 0;
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
    if (strcmp(name, "sentinel") == 0)
    {
        return ProcList[procIndex].pid;
    }
    EnableInterupts();
    dispatcher(3);
    return ProcList[procIndex].pid;
}

/**
 * @brief This function is a wrapper for the startFunc of the process. It is made for USLOSS_ContextInit().
 */
void trampoline()
{
    int res = USLOSS_PsrSet(USLOSS_PSR_CURRENT_MODE | USLOSS_PSR_CURRENT_INT);
    res = CurrentProc->startFunc(CurrentProc->startArgs);
    quit(res);
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
    // USLOSS_Console("Phase 1A TEMPORARY HACK: testcase_main() returned, simulation will now halt.\n");
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
        if (phase2_check_io() == 0)
        {
            USLOSS_Console("DEADLOCK DETECTED!  All of the processes have blocked, but I/O is not ongoing.\n");
            USLOSS_Halt(1);
        }
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
    userMode("join");
    DisableInterupts();
    // Error checking
    if (CurrentProc->children == NULL)
    {
        return -2;
    }
    // while have a child, remove child from children list if child has quit
    while (CurrentProc->children != NULL)
    {
        Process *child = CurrentProc->children;
        int pid = -2;
        Process *prev = NULL;
        while (child != NULL)
        {
            if (child->Status == QUIT)
            {
                if (prev == NULL)
                {
                    CurrentProc->children = child->nextSibling;
                }
                else
                {
                    prev->nextSibling = child->nextSibling;
                }

                break;
            }
            if (child->Status == READY)
            {
                CurrentProc->Status = JOINBLOCK;
            }
            prev = child;
            child = child->nextSibling;
        }
        if (child == NULL)
        {
            CurrentProc->Status = JOINBLOCK;
            EnableInterupts();
            dispatcher(5);
        }
        else
        {
            // returning child's info to parent
            *status = child->returnVal;
            pid = child->pid;
            freeProc(child);
            CurrentProc->Status = READY;
            EnableInterupts();
            dispatcher(5);
            return pid;
        }
    }
    return -2;
}

/**
 * @brief This function resets the entry in the process table for the process.
 *
 * @param p This is a pointer to the process that is to be freed.
 */
void freeProc(Process *p)
{
    DisableInterupts();
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
    p->childRet = -1;
    p->childStatus = EMPTY;
    p->startTime = -1;
    p->totalTime = 0;
    p->isZapped = 0;
    for (int j = 0; j < MAXPROC; j++)
    {
        p->calledZapped[j] = -1;
    }
    p->blockReason = 0;
    EnableInterupts();
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

    DisableInterupts();
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
    if (CurrentProc->parent != NULL)
    {
        if (CurrentProc->parent->Status == JOINBLOCK)
        {
            CurrentProc->parent->Status = READY;
        }
    }

    // Unblock all processes that called zap on this process
    int i = 0;
    while (CurrentProc->calledZapped[i] != -1)
    {
        int index = CurrentProc->calledZapped[i] % MAXPROC;
        if (ProcList[index].Status == ZAPBLOCK)
        {
            ProcList[index].Status = READY;
        }
        i += 1;
    }
    EnableInterupts();
    dispatcher(1);
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
    DisableInterupts();
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
            case ZAPBLOCK:
                USLOSS_Console("Blocked(waiting for zap target to quit)\n");
                break;
            case JOINBLOCK:
                USLOSS_Console("Blocked(waiting for child to quit)\n");
                break;
            case BLOCKED:
                USLOSS_Console("Blocked(%d)\n", ProcList[i].blockReason);
                break;
            case READY:
                USLOSS_Console("Runnable\n");
                break;
            default:
                USLOSS_Console("Status: %d\n", ProcList[i].Status);
                break;
            }
        }
    }
    EnableInterupts();
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
    else
    {
        EnableInterupts();
    }
}

/**
 * @brief This function is called by a process to zap another process. The function changes the status of the process to ZAPBLOCKED.
 *
 * @param pid This is the PID of the process that is to be zapped.
 */
void zap(int pid)
{
    DisableInterupts();
    if (CurrentProc->pid == pid)
    {
        USLOSS_Console("ERROR: Attempt to zap() itself.\n");
        USLOSS_Halt(1);
    }
    if (pid <= 0)
    {
        USLOSS_Console("ERROR: Attempt to zap() a PID which is <=0.  other_pid = %d\n", pid);
        USLOSS_Halt(1);
    }
    if (pid == 1)
    {
        USLOSS_Console("ERROR: Attempt to zap() init.\n", CurrentProc->pid);
        USLOSS_Halt(1);
    }
    int index = pid % MAXPROC;
    if (ProcList[index].Status == QUIT)
    {
        USLOSS_Console("ERROR: Attempt to zap() a process that is already in the process of dying.\n");
        USLOSS_Halt(1);
    }
    if (ProcList[index].Status == EMPTY || ProcList[index].pid != pid)
    {
        USLOSS_Console("ERROR: Attempt to zap() a non-existent process.\n");
        USLOSS_Halt(1);
    }

    ProcList[index].isZapped = 1;
    int i = 0;
    while (ProcList[index].calledZapped[i] != -1)
    {
        i += 1;
    }
    ProcList[index].calledZapped[i] = CurrentProc->pid;

    CurrentProc->Status = ZAPBLOCK;
    EnableInterupts();
    dispatcher(0);
}

/**
 * @brief This function checks if the current process has been zapped.
 *
 */

int isZapped(void)
{
    return CurrentProc->isZapped;
}

/**
 * @brief This function tries to blocks itself for a given reason
 *
 * @param newStatus This is the reason for blocking
 */

void blockMe(int newStatus)
{
    DisableInterupts();
    if (newStatus <= 10)
    {
        USLOSS_Console("Error: Block reason not valid\n");
        USLOSS_Halt(1);
    }
    CurrentProc->blockReason = newStatus;
    CurrentProc->Status = BLOCKED;
    EnableInterupts();
    dispatcher(6);
}
/**
 * @brief This function tries to unblocks a process with a given PID.
 *
 * @param pid This is the PID of the process that is to be unblocked
 */
int unblockProc(int pid)
{
    DisableInterupts();
    int index = pid % MAXPROC;
    if (ProcList[index].blockReason != 0)
    {
        ProcList[index].blockReason = 0;
        ProcList[index].Status = READY;
        EnableInterupts();
        dispatcher(6);
        return 0;
    }
    EnableInterupts();
    return -2;
}

// Code provided to us by phase1b instructions
void clockHandler(int dev, void *arg)
{
    if (debug1)
    {
        USLOSS_Console("clockHandler(): PSR = %d\n", USLOSS_PsrGet());
        USLOSS_Console("clockHandler(): currentTime = %d\n", currentTime());
    }
    /* make sure to call this first, before timeSlice(), since we want to do
     * the Phase 2 related work even if process(es) are chewing up lots of
     * CPU.
     */
    phase2_clockHandler();
    // call the dispatcher if the time slice has expired
    timeSlice();
    /* when we return from the handler, USLOSS automatically re-enables
     * interrupts and disables kernel mode (unless we were previously in
     * kernel code). Or I think so. I haven’t double-checked yet. TODO
     */
}

/**
 * @brief This function returns the current CPU time in microseconds.
 */

int readCurStartTime(void)
{
    // Return current CPU time in microseconds
    CurrentProc->startTime = currentTime();
    return CurrentProc->startTime;
}

// Code provided to us by phase1b instructions
int currentTime(void)
{
    int retval = 0;
    int result = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &retval);
    assert(result == USLOSS_DEV_OK);
    return retval;
}

/**
 * @brief This function returns the total time that the process has been running in the CPU.
 */

int readtime(void)
{
    return CurrentProc->totalTime;
}

/**
 * @brief This function times slices a process if it has been running for more than 80 miliseconds.
 */
void timeSlice(void)
{
    if (currentTime() - CurrentProc->startTime >= 80000)
    {
        CurrentProc->totalTime += currentTime() - CurrentProc->startTime;
        readCurStartTime();
        CurrentProc->Status = SLICED;
        dispatcher(CurrentProc->pid);
    }
}

/**
 * @brief This function switches the current process to another process by priority.
 *
 * @param dev For development purposes
 */

void dispatcher(int dev)
{
    userMode("dispatcher");
    DisableInterupts();
    int min_priorty = 10;
    int index = -1;
    // Find the process with the lowest priority
    for (int i = 0; i < MAXPROC; i++)
    {
        if (i == 1)
        {
            continue;
        }

        if (ProcList[i].Status == READY || ProcList[i].Status == JOIN || ProcList[i].Status == RUNNING)
        {
            // maybe be useful for optimization
            // if (ProcList[i].priority == min_priorty)
            // {
            //     if (ProcList[i].isZapped == 1)
            //     {
            //         index = ProcList[i].pid;
            //         continue;
            //     }
            // }
            if (ProcList[i].priority < min_priorty && ProcList[i].priority != -1)
            {
                min_priorty = ProcList[i].priority;
                index = ProcList[i].pid;
            }
        }
    }

    // zapped processes first
    for (int i = index; i < MAXPROC; i++)
    {
        if (ProcList[i].isZapped == 1 && (ProcList[i].Status == READY || ProcList[i].Status == JOIN || ProcList[i].Status == RUNNING))
        {
            if (ProcList[i].priority == min_priorty)
            {
                index = ProcList[i].pid;
            }
        }
    }
    // check for sentinel
    if (index == 2)
    {
        for (int i = index; i < MAXPROC; i++)
        {
            if (ProcList[i].Status == SLICED && ProcList[i].pid != dev)
            {
                if (ProcList[i].priority < min_priorty)
                {
                    min_priorty = ProcList[i].priority;
                    index = ProcList[i].pid;
                }
                index = ProcList[i].pid;
            }
        }
    }
    if (index == -1)
    {
        return;
    }
    else
    {
        if (CurrentProc->Status == RUNNING)
        {
            CurrentProc->Status = READY;
        }
        switchToIndex(index);
    }
}

/**
 * @brief This function switches the current process to another process by PID.
 * 
 * @param newpid This is the PID of the process that the current process will switch to.
*/

void switchToIndex(int newpid)
{
    int indexFrom = (CurrentProc->pid % MAXPROC);
    int indexTo = newpid % MAXPROC;
    CurrentProc = &(ProcList[indexTo]);
    CurrentProc->Status = RUNNING;
    EnableInterupts();
    readCurStartTime();
    USLOSS_ContextSwitch(&(ProcList[indexFrom].state), &(ProcList[indexTo].state));
}
