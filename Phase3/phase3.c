#include "phase3.h"
#include "phase2.h"
#include <usloss.h>
#include "phase1.h"
#include <usyscall.h>
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


//-------------------Structures-------------------//


//-------------------Function Prototypes-------------------//
void nullsys(USLOSS_Sysargs *args);
int spawn_K(USLOSS_Sysargs *args);
int wait_K(USLOSS_Sysargs *args);
void terminate_K(USLOSS_Sysargs *args);

//-------------------Global Variables-------------------//
// void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);



//-------------------Function Implementations-------------------//

void phase3_init(){
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
    USLOSS_Halt(1);
}

int spawn_K(USLOSS_Sysargs *args){
    //SYS_SPAWN = args->number;
    int (*func)(char*) = args->arg1;
    char *arg = args->arg2;
    int stack_size = (int)(long)args->arg3;
    int priority= (int)(long)args->arg4;
    char *name = args->arg5;
    args->arg1 = 1;
    USLOSS_Console("spawn_K(): Spawn called.  name: %s, func: 0x%x, arg: %s, stack_size: %d, priority: %d\n", name, func, arg, stack_size, priority);
}

int wait_K(USLOSS_Sysargs *args){
    //SYS_WAIT = args->number;
    int pid = (int)(long)args->arg1;
    int status = (int)(long)args->arg2;
    args->arg1 = 1;
    USLOSS_Console("wait_K(): Wait called.  pid: %d, status: 0x%x\n", pid, status);
}

void terminate_K(USLOSS_Sysargs *args){
    //SYS_TERMINATE = args->number;
    int status = (int)(long)args->arg1;
    USLOSS_Console("terminate_K(): Terminate called.  status: %d\n", status);
}