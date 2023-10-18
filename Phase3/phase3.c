#include "phase3.h"
#include "phase2.h"
#include <usloss.h>
#include "phase1.h"
#include <usyscall.h>
#include "phase3_usermode.h"
#include <stdio.h>
#include <assert.h>
/**
 * We are trying to make 2 copies of the same funciton, one will be the kernel mode,
 * opne will be the user mode (trampoline type functiosn??). Basically like phase1, but
 * we use mailboxes with semaphores and other funcitons that we made in phase2 and 1 to handle 
 * syscalls.  
 * 
 * We need to create a PROC TABLE
 * We need to call fork and stuff
 * We need to block/unblock things using mailboxes and semaphores
*/


//-------------------Constantt Defines-------------------//



//-------------------Structures type definations-------------------//
typedef struct Process Process;
typedef struct Queue Queue;
typedef struct Semaphore Semaphore;


//-------------------Structures-------------------//
struct Queue
{
    int size;
    Process *head;
    Process *tail;
    int type; 
};

struct Process{
    int pid;
    int priority;
    int status;
    int (*startFunc)(char*);
    char *args;
    int mailBoxID;
    Process *parent;
    Process *nextSibling;
    Queue childQueue;
};

struct Semaphore{
    int id;
    int value;
    int startingValue;
    Queue blockedProcesses;
    int privMBoxID;
    int mutexMBoxID;
};


//-------------------Function Prototypes-------------------//
void nullsys(USLOSS_Sysargs *args);
int spawn_K(USLOSS_Sysargs *args);
int wait_K(USLOSS_Sysargs *args);
void terminate_K(USLOSS_Sysargs *args);

int spawn_T(char *args);

//-------------------Global Variables-------------------//
// void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
Process ProcTable[MAXPROC];
Semaphore SemTable[MAXSEMS];
int totalSemUsed;



//-------------------Function Implementations-------------------//

void phase3_init(){
    // Initialize the process table
    for(int i = 0; i < MAXPROC; i++){
        ProcTable[i].pid = -1;
        ProcTable[i].priority = -1;
        ProcTable[i].status = -1;
        ProcTable[i].startFunc = NULL;
        ProcTable[i].args = NULL;
        ProcTable[i].mailBoxID = -1;
        ProcTable[i].parent = NULL;
        ProcTable[i].nextSibling = NULL;
        Queue_init(&ProcTable[i].childQueue, 1);
    }
    // Initialize the semaphore table
    for(int i = 0; i < MAXSEMS; i++){
        SemTable[i].id = -1;
        SemTable[i].value = -1;
        SemTable[i].startingValue = -1;
        Queue_init(&SemTable[i].blockedProcesses, 1);
        SemTable[i].privMBoxID = -1;
        SemTable[i].mutexMBoxID = -1;
    }
    totalSemUsed = 0;
    // Initialize the system call vector
    for(int i = 0; i < MAXSYSCALLS; i++){
        systemCallVec[i] = nullsys;
    }
    // Initialize the system call vector with the correct functions
    systemCallVec[SYS_SPAWN] = spawn_K;
    systemCallVec[SYS_WAIT] = wait_K;
    systemCallVec[SYS_TERMINATE] = terminate_K;


}

void phase3_start_service_processes(void){
    // phase4_start_service_processes();
    // phase5_start_service_processes();
}

void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%.2x\n", args->number, USLOSS_PsrGet());
    terminate_K(args);
}

int spawn_K(USLOSS_Sysargs *args){ //Add ERROR CHECKING and set arg4 accordingly.
    //SYS_SPAWN = args->number;
    int (*func)(char*) = args->arg1;
    char *arg = args->arg2;
    int stack_size = (int)(long)args->arg3;
    int priority= (int)(long)args->arg4;
    char *name = args->arg5;
    int pid = fork1(name, spawn_T, arg, stack_size, priority);

    if(pid == -1){
        args->arg1 = (void*)(long)-1;
        return -1;
    }

    Process *childProc = &ProcTable[pid % MAXPROC];
    Queue_push(&ProcTable[getpid() % MAXPROC].childQueue, childProc);

    if(childProc->pid == -1){
        process_init(pid);
        //MboxSend(childProc->mailBoxID, NULL, 0);
    }
    childProc->startFunc = func;
    childProc->parent = &ProcTable[getpid() % MAXPROC];

    MboxCondSend(childProc->mailBoxID, NULL, 0);
    args->arg1 = (void*)(long)pid;
    change_to_user_mode();
    return pid;
    // USLOSS_Console("spawn_K(): Spawn called.  name: %s, func: 0x%x, arg: %s, stack_size: %d, priority: %d\n", name, func, arg, stack_size, priority);
}

int wait_K(USLOSS_Sysargs *args){
    //SYS_WAIT = args->number;
    int pid = (int)(long)args->arg1;
    int status = (int)(long)args->arg2;
    pid = join(&status);
    if(pid == -2){
        args->arg4 = (void*)(long)-2;
        return -2;
    }
    args->arg1 = (void*)(long)pid;
    args->arg2 = (void*)(long)status;
    args->arg4 = (void*)(long)0;
    // USLOSS_Console("wait_K(): Wait called.  pid: %d, status: 0x%x\n", pid, status);
    change_to_user_mode();
    return 0;
    

    // USLOSS_Console("wait_K(): Wait called.  pid: %d, status: 0x%x\n", pid, status);
}

void terminate_K(USLOSS_Sysargs *args){
    //SYS_TERMINATE = args->number;
    // USLOSS_Console("terminate_K(): called.  status = %d, pid = %d\n", (int)(long)args->arg1, getpid());
    int status = (int)(long)args->arg1;
    int pid = getpid();
    if(ProcTable[pid % MAXPROC].childQueue.size > 0){
        while(pid != -2){
            pid = join(&status);
        }
    }
    quit(status);
    args->arg1 = (void*)(long)status;
    change_to_user_mode();
}


void process_init(int pid){
    int i = pid % MAXPROC;
    ProcTable[i].pid = pid;
    ProcTable[i].status = 1;
    ProcTable[i].startFunc = NULL;
    ProcTable[i].args = NULL;
    ProcTable[i].mailBoxID = MboxCreate(0, 0);
    ProcTable[i].parent = NULL;
    ProcTable[i].nextSibling = NULL;
    Queue_init(&ProcTable[i].childQueue, 1);
}

void change_to_user_mode(){
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
    // assert(result == USLOSS_DEV_OK);
}

//---------------------------Trampoline Functions---------------------------//
int spawn_T(char *args){
    if(isZapped()){
        terminate_K(1);
    }
    Process *p = &ProcTable[getpid() % MAXPROC];
    if(p->pid == -1){
        process_init(getpid());
        MboxRecv(p->mailBoxID, NULL, 0);
    }

    change_to_user_mode();
    int status = p->startFunc(p->args);
    Terminate(status);
    return 0;
}

//---------------------------QUEUE FUNCTIONS---------------------------//

/**
 * @brief This function initializes the queue. It takes in the queue and the type of queue (0 = MailSlot, 1 = ShadowProc).
 * @param queue - The queue to be initialized
 * @param type - The type of queue
 * @return None
 * 
*/
void Queue_init(Queue *queue, int type)
{
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    queue->type = type;
}

/**
 * @brief This function pushes an item to the queue. It takes in the queue and the item to be pushed.
 * @param queue - The queue to be pushed to
 * @param item - The item to be pushed
 * @return None
*/

void Queue_push(Queue *queue, Process *item)
{   // MailSlot
    if (queue->size == 0)
        {
            queue->head = item;
            queue->tail = item;
        }
        else
        {
            queue->tail->nextSibling = item;
            queue->tail = item;
        }
    queue->size++;
}

/**
 * @brief This function pops an item from the queue. It takes in the queue and returns the item that was popped.
 * @param queue - The queue to be popped from
 * @return The item that was popped
 * 
 * 
*/

Process *Queue_pop(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL;
    }
    Process *temp = queue->head;
    queue->head = queue->head->nextSibling;
    queue->size--;
    return temp;
}

/**
 * @brief This function returns the front of the queue. It takes in the queue and returns the front of the queue.
 * @param queue - The queue to be returned
 * @return The front of the queue
 * 
*/

Process *Queue_front(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL;
    }
    return queue->head;
}