
#include <phase4.h>
#include <usyscall.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdio.h>

//-------------Structs------------------//
typedef struct procStruct{
    int pid;
    int mboxID;
    int semID;
    int wakeTime;
}procStruct;

typedef struct procHeap{
    int size;
    procStruct * HeapProc[MAXPROC];
}procHeap;


//-------------Function Prototypes------------------//
void phase4_init();
// void startProcesses();
void phase4_start_service_processes();
void sleep_sys(USLOSS_Sysargs *args);
int kernSleep(int seconds);
void term_write_sys(USLOSS_Sysargs *args);
int kernTermWrite(char *buffer, int bufferSize, int unitID,int *numCharsRead);
void term_read_sys(USLOSS_Sysargs *args);
int kernTermRead (char *buffer, int bufferSize, int unitID,int *numCharsRead);
void disk_size_sys(USLOSS_Sysargs *args);
int kernDiskSize (int unit, int *sector, int *track, int *disk);
void disk_write_sys(USLOSS_Sysargs *args);
int kernDiskWrite(void *diskBuffer, int unit, int track, int first,int sectors, int *status);
void disk_read_sys(USLOSS_Sysargs *args);
int kernDiskRead (void *diskBuffer, int unit, int track, int first,int sectors, int *status);

static int clockDriver(char *arg);
static int termDriver(char *args);

void init_proc(int pid);
void initHeap(procHeap * heap);
void addHeap(procHeap * heap, procStruct * proc);
void heapify(procHeap * heap);
procStruct * removeHeap(procHeap * heap);
procStruct * peekHeap(procHeap * heap);


//-------------Global Variables------------------//
procStruct procTable[MAXPROC];
procHeap slHeap;

int MainMbox;
static int clockDriverPID;

//Terminal Stuff
int charRecvMailBox[USLOSS_TERM_UNITS];
int charSendMailBox[USLOSS_TERM_UNITS];
int lineReadMailBox[USLOSS_TERM_UNITS];
int lineWriteMailBox[USLOSS_TERM_UNITS];
int pidMailbox[USLOSS_TERM_UNITS];
int termInterrupt[USLOSS_TERM_UNITS];
int termProcTable[USLOSS_TERM_UNITS][3];

//-------------Function Implementations------------------//
void phase4_init(){
    for(int i = 0; i < MAXPROC; i++){
        procTable[i].pid = -1;
        procTable[i].mboxID = -1;
        procTable[i].semID = -1;
        procTable[i].wakeTime = -1;
    }
    initHeap(&slHeap);
    clockDriverPID = -1;
    systemCallVec[SYS_SLEEP] = sleep_sys;
    systemCallVec[SYS_DISKREAD] = disk_read_sys;
    systemCallVec[SYS_DISKWRITE] = disk_write_sys;
    systemCallVec[SYS_DISKSIZE] = disk_size_sys;
    systemCallVec[SYS_TERMREAD] = term_read_sys; 
    systemCallVec[SYS_TERMWRITE] = term_write_sys;

    MainMbox = MboxCreate(0,0);

    //Terminal Stuff
    for(int i = 0; i < USLOSS_TERM_UNITS; i++){
        charRecvMailBox[i] = MboxCreate(1,MAXLINE);
        charSendMailBox[i] = MboxCreate(1,MAXLINE);
        lineReadMailBox[i] = MboxCreate(10,MAXLINE);
        lineWriteMailBox[i] = MboxCreate(10,MAXLINE);
        pidMailbox[i] = MboxCreate(1,sizeof(int));
    }

    for(int i = 0; i < USLOSS_TERM_UNITS; i++){
        // termProcTable[i][0] = fork1("termReader", termDriver, (void *)i, USLOSS_MIN_STACK, 2);
    }

    // MboxRecv(MainMbox, NULL, 0);
}

// void startProcesses(){
//     USLOSS_Console("startProcesses(): called\n");
// }

// void startClock(){
//     int status;
//     clockDriverPID = fork1("clockDriver", clockDriver, NULL, USLOSS_MIN_STACK, 2);
//     if(clockDriverPID < 0){
//         USLOSS_Console("startProcesses(): Can't create clockDriver\n");
//         USLOSS_Halt(1);
//     }
//     MboxRecv(MainMbox, NULL, 0);

//     // zap(clockDriverPID);
//     // join(&status);
// }

void phase4_start_service_processes(){
    clockDriverPID = fork1("clockDriver", clockDriver, NULL, USLOSS_MIN_STACK, 2);
    int status;
    if(clockDriverPID < 0){
        USLOSS_Console("startProcesses(): Can't create clockDriver\n");
        USLOSS_Halt(1);
    }
    for(int i = 0; i < USLOSS_TERM_UNITS; i++){
        termProcTable[i][0] = fork1("termReader", termDriver, (void *)i, USLOSS_MIN_STACK, 2);
    }

    // zap(clockDriverPID);
    // join(&status);
} 

void sleep_sys(USLOSS_Sysargs *args){
    int seconds = (int)(long)args->arg1;
    int result = kernSleep(seconds);
    args->arg4 = (void *)(long)result;
}

int kernSleep(int seconds){
    // if(clockDriverPID == -1){
    //     startClock();
    // }
    int procPID = getpid();
    if(seconds < 0){
        return -1;
    }
    if(procTable[procPID % MAXPROC].pid == -1){
        init_proc(procPID);
    }
    // if(procPID == 4){
    //     blockMe(21);
    //     unblockProc(procPID);
    //     return 0;
    // }
    procTable[procPID % MAXPROC].wakeTime =currentTime() + seconds*1000000;
    addHeap(&slHeap, &procTable[procPID % MAXPROC]);
    // clockDriver("PlaceHolder");
    MboxRecv(procTable[procPID % MAXPROC].mboxID, NULL, 0);
    // blockMe(21);
    return 0;
}


void term_write_sys(USLOSS_Sysargs *args){
    char *buffer = (char *)args->arg1;
    int bufferSize = (int)(long)args->arg2;
    int unitID = (int)(long)args->arg3;
    int numCharsRead = 0;
    int result = kernTermWrite(buffer, bufferSize, unitID, &numCharsRead);
    args->arg4 = (void *)(long)result;
    args->arg5 = (void *)(long)numCharsRead;
}
int  kernTermWrite(char *buffer, int bufferSize, int unitID,int *numCharsRead){
    USLOSS_Console("kernTermWrite: called\n");
    return 0;
}

void term_read_sys(USLOSS_Sysargs *args){
    char *buffer = (char *)args->arg1;
    int bufferSize = (int)(long)args->arg2;
    int unitID = (int)(long)args->arg3;
    int numCharsRead = 0;
    int result = kernTermRead(buffer, bufferSize, unitID, &numCharsRead);
    args->arg4 = (void *)(long)result;
    args->arg5 = (void *)(long)numCharsRead;
}
int  kernTermRead (char *buffer, int bufferSize, int unitID,int *numCharsRead){
    USLOSS_Console("kernTermRead: called\n");
    return 0;
}

void disk_size_sys(USLOSS_Sysargs *args){
    int unit = (int)(long)args->arg1;
    int sector = 0;
    int track = 0;
    int disk = 0;
    int result = kernDiskSize(unit, &sector, &track, &disk);
    args->arg1 = (void *)(long)sector;
    args->arg2 = (void *)(long)track;
    args->arg3 = (void *)(long)disk;
    args->arg4 = (void *)(long)result;
}
int kernDiskSize (int unit, int *sector, int *track, int *disk){
    USLOSS_Console("kernDiskSize: called\n");
    return 0;
}

void disk_write_sys(USLOSS_Sysargs *args){
    void *diskBuffer = (void *)args->arg1;
    int unit = (int)(long)args->arg2;
    int track = (int)(long)args->arg3;
    int first = (int)(long)args->arg4;
    int sectors = (int)(long)args->arg5;
    int status = 0;
    int result = kernDiskWrite(diskBuffer, unit, track, first, sectors, &status);
    args->arg1 = (void *)(long)status;
    args->arg4 = (void *)(long)result;
}
int kernDiskWrite(void *diskBuffer, int unit, int track, int first,int sectors, int *status){
    USLOSS_Console("kernDiskWrite: called\n");
    return 0;
}

void disk_read_sys(USLOSS_Sysargs *args){
    void *diskBuffer = (void *)args->arg1;
    int unit = (int)(long)args->arg2;
    int track = (int)(long)args->arg3;
    int first = (int)(long)args->arg4;
    int sectors = (int)(long)args->arg5;
    int status = 0;
    int result = kernDiskRead(diskBuffer, unit, track, first, sectors, &status);
    args->arg1 = (void *)(long)status;
    args->arg4 = (void *)(long)result;
}
int kernDiskRead (void *diskBuffer, int unit, int track, int first,int sectors, int *status){
    USLOSS_Console("kernDiskRead: called\n");
    return 0;
}

//-------------Daemon Functions------------------//
static int clockDriver(char *arg){
    int res;
    int status;
    // MboxSend(MainMbox, NULL, 0);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    while(!isZapped()){
        waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        procStruct * proc;
        while(slHeap.size > 0 && peekHeap(&slHeap)->wakeTime <= currentTime()){
            proc = removeHeap(&slHeap);
            MboxSend(proc->mboxID, NULL, 0);
            //unblockProc(proc->pid);
        }
    }
    // dumpProcesses();
    return 0;
}


static int termDriver(char *args){
    // USLOSS_Console("termDriver: called\n");
}
//-------------Utility Functions------------------//
void init_proc(int pid){
    procTable[pid].pid = pid;
    procTable[pid].mboxID = MboxCreate(0,0);
    procTable[pid].wakeTime = -1;
}

void initHeap(procHeap * heap){
    heap->size = 0;
    for(int i = 0; i < MAXPROC; i++){
        heap->HeapProc[i] = NULL;
    }
}

void addHeap(procHeap * heap, procStruct * proc){
    int i,parent;
    // USLOSS_Console("addHeap: Procces PID: %d\n", proc->pid);
    for(i = heap->size; i > 0; i = parent){
        parent = (i-1)/2;
        if(heap->HeapProc[parent]->wakeTime <= proc->wakeTime){
            break;
        }
        heap->HeapProc[i] = heap->HeapProc[parent];
    }
    heap->HeapProc[i] = proc;
    heap->size++;
}

procStruct * removeHeap(procHeap * heap){
    if(heap->size == 0){
        return NULL;
    }
    procStruct * ret = heap->HeapProc[0];
    heap->HeapProc[0] = heap->HeapProc[heap->size - 1];
    heap->size--;
    int i = 0, min = 0;
    int left, right;
    while(i*2 <= heap->size){
        left = i*2 + 1;
        right = i*2 + 2;
        if(left < heap->size && heap->HeapProc[left]->wakeTime < heap->HeapProc[min]->wakeTime){
            min = left;
        }
        if(right < heap->size && heap->HeapProc[right]->wakeTime < heap->HeapProc[min]->wakeTime){
            min = right;
        }
        if(min == i){
            break;
        }
        procStruct * temp = heap->HeapProc[i];
        heap->HeapProc[i] = heap->HeapProc[min];
        heap->HeapProc[min] = temp;
        i = min;
    }
    // USLOSS_Console("removeHeap: Procces PID: %d\n", ret->pid);
    return ret;

}

procStruct * peekHeap(procHeap * heap){
    return heap->HeapProc[0];
}