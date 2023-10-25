/**
 * @file phase3.c
 * 
 * @author Muhtasim Al-Farabi, Shrey Goel
 * 
 * @brief This file contains the implementation of the functions for Phase 3. It implements the syscall functions so that the syscall vector can call them. 
 * It implements the functions for the semaphore table and the process table. We implemented a queue to hold the block processes for each entry in the sem table.
 * We also have structs that represent the process table and the semaphore table. Most functions here
 * take in USLOSS_Sysargs *args as an argument. We unpack them and then use inside the functions.
*/

#include "phase3.h"
#include "phase2.h"
#include <usloss.h>
#include "phase1.h"
#include <usyscall.h>
#include "phase3_usermode.h"
#include <stdio.h>
#include <assert.h>

//-------------------Structures type definations-------------------//
typedef struct Process Process;
typedef struct Queue Queue;
typedef struct Semaphore Semaphore;


//-------------------Structures-------------------//
/**
 * @brief This struct represents a queue. It contains the size, head, tail, and type of queue
*/
struct Queue
{
    int size;
    Process *head;
    Process *tail;
    int type; 
};

/**
 * @brief This struct represents a process. It contains the pid, priority, status, startFunc, args, mailBoxID, parent, nextSibling, and childQueue.
*/

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

/**
 * @brief This struct represents a semaphore. It contains the id, value, startingValue, blockedProcesses, privMBoxID, and mutexMBoxID.
*/

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
void Queue_push(Queue *queue, Process *item);
void process_init(int pid);
void change_to_user_mode();
void kernel_mode(char *func);
int sem_create(int value);
int spawn_T(char *args);

//-------------------Global Variables-------------------//

Process ProcTable[MAXPROC];
Semaphore SemTable[MAXSEMS];
int totalSemUsed;

//-------------------Function Implementations-------------------//
/**
 * @brief This function initializes the process table, semaphore table, and the system call vector. It also initializes the system call vector with the correct functions.
*/
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

/**
 * @brief This function starts the service processes for Phase 3.
*/

void phase3_start_service_processes(void){
}

/**
 * @brief This function is the nullsys function. It is called when a syscall is called that is not implemented. It prints out the syscall number and the PSR.
 * 
 * @param args - The arguments passed in to the syscall
*/

void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%.2x\n", args->number, USLOSS_PsrGet());
    terminate_K(args);
}

/**
 * @brief This function takes in the arguments passed in to spawn and calls fork1 to create a new process. It then adds the child to the parent's child queue. It also initializes the child process.
 * 
 * @param args - The arguments passed in to spawn
*/

void spawn_K(USLOSS_Sysargs *args){ 
    kernel_mode("spawn_K()");
    //unpack
    int (*func)(char*) = args->arg1;
    char *arg = args->arg2;
    int stack_size = (int)(long)args->arg3;
    int priority= (int)(long)args->arg4;
    char *name = args->arg5;
    int pid = fork1(name, spawn_T, arg, stack_size, priority);
    if(pid == -1){
        args->arg1 = (void*)(long)-1;
        return ;
    }
    // set the child process
    Process *childProc = &ProcTable[pid % MAXPROC];
    Queue_push(&ProcTable[getpid() % MAXPROC].childQueue, childProc);

    if(childProc->pid == -1){
        process_init(pid);
    }
    childProc->startFunc = func;
    childProc->args = arg;
    childProc->parent = &ProcTable[getpid() % MAXPROC];
    args->arg1 = (void*)(long)pid;
    // non blocking send
    MboxCondSend(childProc->mailBoxID, NULL, 0);
}

/**
 * @brief This function takes in the arguments passed in to wait and calls join to wait for a child process to terminate. It sets the right arguments (passed in as references) and then changes to user mode.
 * 
 * @param args - The arguments passed in to wait
*/

void wait_K(USLOSS_Sysargs *args){
    kernel_mode("wait_K()");
    int pid = (int)(long)args->arg1;
    int status = (int)(long)args->arg2;
    pid = join(&status);
    if(pid == -2){
        args->arg4 = (void*)(long)-2;
        return ;
    }
    args->arg1 = (void*)(long)pid;
    args->arg2 = (void*)(long)status;
    args->arg4 = (void*)(long)0;
    change_to_user_mode();
}

/**
 * @brief This function takes in the arguments passed in to terminate and calls quit to terminate the process. It also waits for all the children to terminate. It then changes to user mode.
 * 
 * @param args - The arguments passed in to terminate
*/

void terminate_K(USLOSS_Sysargs *args){
    kernel_mode("terminate_K()");
    int status = (int)(long)args->arg1;
    int currPid = getpid();
    int temp;
    // join all children
    while(ProcTable[currPid % MAXPROC].childQueue.size > 0){
        Queue_pop(&ProcTable[currPid % MAXPROC].childQueue);
        join(&temp);
    }
    quit(status);
    args->arg1 = (void*)(long)status;
    change_to_user_mode();
}

/**
 * @brief This function takes in the arguments passed in to sem_create and calls sem_create to create a new semaphore. It then sets the right arguments.
 * 
 * @param args - The arguments passed in to sem_create
*/
void sem_create_K(USLOSS_Sysargs *args){
    kernel_mode("sem_create_K()");
    if(totalSemUsed == MAXSEMS){
        args->arg4 = (void*)(long)-1;
        return ;
    }
    int value = (int)(long)args->arg1;
    if(value < 0){
        args->arg4 = (void*)(long)-1;
        return;
    }
    
    int semID = sem_create(value);
    if(semID == -1){
        args->arg4 = (void*)(long)-1;
        return;
    }
    args->arg1 = (void*)(long)semID;
    args->arg4 = (void*)(long)0;
}

/**
 * @brief This function creates a new semaphore. It takes in the value of the semaphore and returns the id of the semaphore. It loops throguh the semaphore table and finds the first empty entry.
 * 
 * @param value - The value of the semaphore
*/

int sem_create(int value){
    kernel_mode("sem_create()");
    for(int i = 0; i < MAXSEMS;i++){
        if(SemTable[i].id == -1){
            SemTable[i].id = i;
            SemTable[i].value = value;
            SemTable[i].startingValue = value;
            SemTable[i].privMBoxID = MboxCreate(value, 0);
            SemTable[i].mutexMBoxID = MboxCreate(1, 0);
            totalSemUsed++;
            return i;
        }
    }
    return -1;
}
/**
 * @brief This function takes in the arguments passed in to semP. If the values of the sem is zero,
 * then it blocks the process, essentially working as a lock mechanism. If the value is non zero, then
 * it decrements the value and returns. 
 * 
 * @param args - The arguments passed in to semP
*/
void semP_K(USLOSS_Sysargs *args){
    kernel_mode("semP_K()");
    // error checks
    int semID = (int)(long)args->arg1;
    if(semID < 0 || semID >= MAXSEMS){
        args->arg4 = (void*)(long)-1;
        return;
    }

    //lock
    MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);
    // for zero value, block
    if(SemTable[semID].value == 0){
        Queue_push(&SemTable[semID].blockedProcesses, &ProcTable[getpid() % MAXPROC]);
        // unlock
        MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
        int result = MboxRecv(SemTable[semID].privMBoxID, NULL, 0);
        
        if(SemTable[semID].id < 0){
            args->arg4 = (void*)(long)-1;
            return;
        }
        // lock
        MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);

        if(result < 0){
            USLOSS_Console("semP_K(): MboxReceive failed.  result = %d\n", result);
        }
    }
    // else decrement the value
    else{
        SemTable[semID].value--;
        int result = MboxCondRecv(SemTable[semID].privMBoxID, NULL, 0);
        if(result < 0){
        }
    }
    // unlock
    MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
    args->arg4 = (void*)(long)0;
}

/**
 * @brief This function takes in the arguments passed in to semV. If there are blocked processes, then it unblocks the first process in the queue. If there are no blocked processes, then it increments the value.
 * 
 * @param args - The arguments passed in to semV
*/

void semV_K(USLOSS_Sysargs *args){
    kernel_mode("semV_K()");
    // error checks
    int semID = (int)(long)args->arg1;
    if(semID < 0 || semID >= MAXSEMS){
        args->arg4 = (void*)(long)-1;
        return;
    }
    // sending to mutexMBoxID to lock
    MboxSend(SemTable[semID].mutexMBoxID, NULL, 0);
    if(SemTable[semID].blockedProcesses.size > 0){
        Queue_pop(&SemTable[semID].blockedProcesses);
        // unlock
        MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0); 
        // send to privMBoxID
        MboxSend(SemTable[semID].privMBoxID, NULL, 0); 
        // lock
        MboxSend(SemTable[semID].mutexMBoxID, NULL, 0); 
    }
    else{
        SemTable[semID].value++;
    }
    // unlock
    MboxRecv(SemTable[semID].mutexMBoxID, NULL, 0);
    args->arg4 = (void*)(long)0;
}

/**
 * @brief This function takes in the arguments passed in to get_time_of_day and sets that argument to the current time.
 * 
 * @param args - The arguments passed in to get_time_of_day
*/

void get_time_of_day_K(USLOSS_Sysargs *args){
    kernel_mode("get_time_of_day_K()");
    args->arg1 = (void *)(long)currentTime();
}

/**
 * @brief This function takes in the arguments passed in to cpu_time and sets that argument to the built in readTime
 * 
 * @param args - The arguments passed in to cpu_time
*/

void cpu_time_K(USLOSS_Sysargs *args){
    kernel_mode("cpu_time_K()");
    args->arg1 = (void *)(long)readtime();
}

/**
 * @brief This function takes in the arguments passed in to getPID and sets that argument to the current pid.
 * 
 * @param args - The arguments passed in to getPID
*/

void getPID_K(USLOSS_Sysargs *args){
    kernel_mode("getPID_K()");
    args->arg1 = (void *)(long)getpid();
}

/**
 * @brief This function initializes the process table. It takes in the pid and initializes the process table entry. This is just a way to keep track of the processes.
 * 
 * @param pid - The pid of the process
*/


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

/**
 * @brief This function changes the mode to user mode.
 *
*/

void change_to_user_mode(){
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
    result++;
}

/**
 * @brief This function checks if the mode is in kernel mode. If it is not, then it halts.
*/

void kernel_mode(char *func){
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("%s: called while in user mode, by process %d Halting...\n", func, getpid());
        USLOSS_Halt(1);
    }
}



//---------------------------Trampoline Functions---------------------------//
/**
 * @brief This function is the trampoline function for the spawn function. It takes in the args and calls
 * the function that was passed in as an argument to spawn in user mode. It then terminates the process.
 * 
 * @param args - The arguments passed in to spawn
 * @return 0
*/
int spawn_T(char *args){
    // check if we have a process to run
    Process *p = &ProcTable[getpid() % MAXPROC];
    if(p->pid == -1){
        process_init(getpid());
        MboxRecv(p->mailBoxID, NULL, 0);
    }

    change_to_user_mode();
    // run the function in usermode
    int status = p->startFunc(p->args);
    // terminate after it returns
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