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
void spawn_K(USLOSS_Sysargs *args);
void wait_K(USLOSS_Sysargs *args);
void terminate_K(USLOSS_Sysargs *args);
void sem_create_K(USLOSS_Sysargs *args);
void semP_K(USLOSS_Sysargs *args);
void semV_K(USLOSS_Sysargs *args);
void get_time_of_day_K(USLOSS_Sysargs *args);
void cpu_time_K(USLOSS_Sysargs *args);
void getPID_K(USLOSS_Sysargs *args);
Process* Queue_pop(Queue *queue);
Process* Queue_front(Queue *queue);
void Queue_init(Queue *queue, int type);

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
    systemCallVec[SYS_SEMCREATE] = sem_create_K;
    systemCallVec[SYS_SEMP] = semP_K;
    systemCallVec[SYS_SEMV] = semV_K;
    systemCallVec[SYS_GETTIMEOFDAY] = get_time_of_day_K;
    systemCallVec[SYS_GETPROCINFO] = cpu_time_K;
    systemCallVec[SYS_GETPID] = getPID_K;

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

void spawn_K(USLOSS_Sysargs *args){ //Add ERROR CHECKING and set arg4 accordingly.
    //SYS_SPAWN = args->number;
    kernel_mode("spawn_K()");
    int (*func)(char*) = args->arg1;
    char *arg = args->arg2;
    int stack_size = (int)(long)args->arg3;
    int priority= (int)(long)args->arg4;
    char *name = args->arg5;
    int pid = fork1(name, spawn_T, arg, stack_size, priority);
    if(pid == -1){
        args->arg1 = (void*)(long)-1;
        // terminate_K(args);
        return ;
    }

    Process *childProc = &ProcTable[pid % MAXPROC];
    Queue_push(&ProcTable[getpid() % MAXPROC].childQueue, childProc);

    if(childProc->pid == -1){
        process_init(pid);
        //MboxSend(childProc->mailBoxID, NULL, 0);
    }
    childProc->startFunc = func;
    childProc->args = arg;
    childProc->parent = &ProcTable[getpid() % MAXPROC];
    args->arg1 = (void*)(long)pid;
    MboxCondSend(childProc->mailBoxID, NULL, 0);
    
    // change_to_user_mode();
    // terminate_K(args);
    // USLOSS_Console("spawn_K(): Spawn called.  name: %s, func: 0x%x, arg: %s, stack_size: %d, priority: %d\n", name, func, arg, stack_size, priority);
}

void wait_K(USLOSS_Sysargs *args){
    //SYS_WAIT = args->number;
    kernel_mode("wait_K()");
    int pid = (int)(long)args->arg1;
    int status = (int)(long)args->arg2;
    pid = join(&status);
    if(pid == -2){
        args->arg4 = (void*)(long)-2;
        return ;
        // terminate_K(args);
    }
    args->arg1 = (void*)(long)pid;
    args->arg2 = (void*)(long)status;
    args->arg4 = (void*)(long)0;
    // USLOSS_Console("wait_K(): Wait called.  pid: %d, status: 0x%x\n", pid, status);
    change_to_user_mode();
    // terminate_K(args);
    // USLOSS_Console("wait_K(): Wait called.  pid: %d, status: 0x%x\n", pid, status);
}

void terminate_K(USLOSS_Sysargs *args){
    //SYS_TERMINATE = args->number;
    // USLOSS_Console("terminate_K(): called.  status = %d, pid = %d\n", (int)(long)args->arg1, getpid());
    kernel_mode("terminate_K()");
    int status = (int)(long)args->arg1;
    int joinRet = 0;
    int currPid = getpid();
    int temp;
    while(ProcTable[currPid % MAXPROC].childQueue.size > 0){
        Queue_pop(&ProcTable[currPid % MAXPROC].childQueue);
        joinRet = join(&temp);
        // USLOSS_Console("terminate_K(): Current pid: %d, joined with PID: %d", currPid,joinRet);
    }
    // Process* parent = ProcTable[currPid % MAXPROC].parent;
    // if(parent != NULL){
    //     Queue_remove(&parent->childQueue, currPid);
    // }
    quit(status);
    args->arg1 = (void*)(long)status;
    change_to_user_mode();
}

void sem_create_K(USLOSS_Sysargs *args){
    kernel_mode("sem_create_K()");
    if(totalSemUsed == MAXSEMS){
        args->arg4 = (void*)(long)-1;
        return ;
        // terminate_K(args);
    }
    int value = (int)(long)args->arg1;
    if(value < 0){
        args->arg4 = (void*)(long)-1;
        return ;
        // terminate_K(args);
    }
    
    int semID = sem_create(value);
    if(semID == -1){
        args->arg4 = (void*)(long)-1;
        return ;
        // terminate_K(args);
    }
    args->arg1 = (void*)(long)semID;
    args->arg4 = (void*)(long)0;
    // return 0;
    // terminate_K(args);
}

int sem_create(int value){
    kernel_mode("sem_create()");
    for(int i = 0; i < MAXSEMS;i++){
        if(SemTable[i].id == -1){
            SemTable[i].id = i;
            SemTable[i].value = value;
            SemTable[i].startingValue = value;
            SemTable[i].privMBoxID = MboxCreate(value, 0); //QUestionable change
            SemTable[i].mutexMBoxID = MboxCreate(1, 0);
            totalSemUsed++;
            return i;
        }
    }
    return -1;
}

void semP_K(USLOSS_Sysargs *args){
    kernel_mode("semP_K()");
    int semID = (int)(long)args->arg1;
    if(semID < 0 || semID >= MAXSEMS){
        args->arg4 = (void*)(long)-1;
        return ;
        // terminate_K(args);
    }


    MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);

    if(SemTable[semID].value == 0){
        Queue_push(&SemTable[semID].blockedProcesses, &ProcTable[getpid() % MAXPROC]);
        MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
        int result = MboxRecv(SemTable[semID].privMBoxID, NULL, 0);
        
        if(SemTable[semID].id < 0){
            args->arg4 = (void*)(long)-1;
            return ;
            // terminate_K(args);
        }

        MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);

        if(result < 0){
            USLOSS_Console("semP_K(): MboxReceive failed.  result = %d\n", result);
        }
    }
    else{
        SemTable[semID].value--;
        int result = MboxCondRecv(SemTable[semID].privMBoxID, NULL, 0);
        // USLOSS_Console("semP_K(): semID = %d\n", semID);
        if(result < 0){
            // USLOSS_Console("semP_K(): MboxReceive failed.  result = %d\n", result);
        }
    }

    MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
    args->arg4 = (void*)(long)0;
    // return 0;
    // terminate_K(args);
}

void semV_K(USLOSS_Sysargs *args){
    kernel_mode("semV_K()");
    int semID = (int)(long)args->arg1;
    if(semID < 0 || semID >= MAXSEMS){
        args->arg4 = (void*)(long)-1;
        return ;
        // terminate_K(args);
    }
    MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);
    if(SemTable[semID].blockedProcesses.size > 0){
        Process *p = Queue_pop(&SemTable[semID].blockedProcesses);
        MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
        MboxSend(SemTable[semID].privMBoxID, NULL, 0);
        MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);
    }
    else{
        SemTable[semID].value++;
        // MboxSend(SemTable[semID].privMBoxID, NULL, 0);
    }
    MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
    args->arg4 = (void*)(long)0;
}

void get_time_of_day_K(USLOSS_Sysargs *args){
    kernel_mode("get_time_of_day_K()");
    args->arg1 = (void *)(long)currentTime();
    // change_to_user_mode();
}

void cpu_time_K(USLOSS_Sysargs *args){
    kernel_mode("cpu_time_K()");
    args->arg1 = (void *)(long)readtime();
    // change_to_user_mode();
}

void getPID_K(USLOSS_Sysargs *args){
    kernel_mode("getPID_K()");
    args->arg1 = (void *)(long)getpid();
    // USLOSS_Console("getPID_K(): called.  pid = %d\n", getpid());
    // change_to_user_mode();
}


void process_init(int pid){
    kernel_mode("process_init()");
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

void kernel_mode(char *func){
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("%s: called while in user mode, by process %d Halting...\n", func, getpid());
        USLOSS_Halt(1);
    }
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

Process* Queue_pop(Queue *queue)
{
    if(queue == NULL){
        return NULL;
    }
    if (queue->size == 0)
    {
        return NULL;
    }
    Process * temp = queue->head;
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

Process* Queue_front(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL;
    }
    return queue->head;
}

Queue_remove(Queue *queue, int pid){
    if(queue->size == 0){
        return;
    }
    Process *curr = queue->head;
    Process *prev = NULL;
    while(curr != NULL){
        if(curr->pid == pid){
            if(prev == NULL){
                queue->head = curr->nextSibling;
            }
            else{
                prev->nextSibling = curr->nextSibling;
            }
            queue->size--;
            return;
        }
        prev = curr;
        curr = curr->nextSibling;
    }
}