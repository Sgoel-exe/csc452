/**
 * @file phase4.c
 * 
 * @author Shrey Goel, Muhtasim Al-Farabi
 * 
 * @brief This file contains the implementation of the functions for Phase 4. It implements the syscall functions so that the syscall vector can call them. 
 * It implements the functions for the process heap and the process table. We implemented a heap to manage the processes for each entry in the process table.
 * We also have structs that represent the process table and the process heap. Most functions here
 * take in USLOSS_Sysargs *args as an argument. We unpack them and then use inside the functions.
 */
#include <phase4.h>
#include <usyscall.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdio.h>
#include <string.h>  
#include <stdlib.h>

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

typedef struct diskReqQ diskReqQ;

struct diskReqQ{
    int pid;
    int track;
    int first;
    int sectors;
    void *diskBuffer;
    int op;
    int mboxID;
    diskReqQ * next;
};

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

//Daemon Functions
static int clockDriver(char *arg);
static int termDriver(char *args);
static int diskDriver(char *args);
//Disk Helper Functions
int diskSeek(int unit, int track);
void diskQueue(int unit, int pid);
//Utility Functions
void init_proc(int pid);
void initHeap(procHeap * heap);
void addHeap(procHeap * heap, procStruct * proc);
procStruct * removeHeap(procHeap * heap);
procStruct * peekHeap(procHeap * heap);

// CONSTANTS
#define DISK_MBOX 0 // 1 mailbox for disk
#define DISK_Q_MBOX 1 // 1 mailbox for disk queue
#define DISK_MUT 2 // 1 mutex each
#define DISK_MUT_TRACK 3 // 1 mutex per track
#define DISK_NUM_TRACKS 4
#define DISK_MBOX_SIZE 5 // 4 mailboxes for disk
#define DISK0 0
#define DISK1 1

//-------------Global Variables------------------//
procStruct procTable[MAXPROC];
procHeap slHeap;
static int clockDriverPID;

//Terminal Stuff
int terminalLines[USLOSS_TERM_UNITS][MAXLINE];
int terminalLineMbox[USLOSS_TERM_UNITS];
int terminalReadMbox[USLOSS_TERM_UNITS];
int terminalWrite[USLOSS_TERM_UNITS];
int writeLock[USLOSS_TERM_UNITS];

//Disk Stuff
diskReqQ diskTable[2][MAXPROC];
int disk0[DISK_MBOX_SIZE];
int disk1[DISK_MBOX_SIZE];
int CurrentDisk;
diskReqQ * disk0Req = NULL;
diskReqQ * disk1Req = NULL;

//-------------Function Implementations------------------//

/**
 * @brief Initializes the Phase 4 of the project.
 *
 * This function is responsible for setting up any necessary data structures and 
 * configurations needed for the execution of Phase 4. It does not take any arguments 
 * and does not return any value.
 *
 * @return void
 */
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

    //Terminal Stuff
    for(int i = 0; i < USLOSS_TERM_UNITS; i++){
        int usless = USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)(long)0x2);
        usless++; // Avoid warning
        memset(terminalLines[i], '\0', sizeof(terminalLines[i]));
        terminalLineMbox[i] = 0;
        terminalReadMbox[i] = MboxCreate(10,MAXLINE);
        terminalWrite[i] = MboxCreate(1,0);
        writeLock[i] = MboxCreate(1,0);
    }

    //Disk Stuff
    for(int i = 0; i < MAXPROC; i++){
        diskTable[DISK0][i].pid = -1;
        diskTable[DISK0][i].next = NULL;
        diskTable[DISK0][i].mboxID = MboxCreate(1,0);   
        diskTable[DISK1][i].pid = -1;
        diskTable[DISK1][i].next = NULL;
        diskTable[DISK1][i].mboxID = MboxCreate(1,0);
    }

    for(int i = 0; i < DISK_MBOX_SIZE - 1; i++){
        disk0[i] = MboxCreate(1,0);
        disk1[i] = MboxCreate(1,0);
    }
    disk0Req = NULL;
    disk1Req = NULL;

}

/**
 * @brief Starts the service processes for Phase 4 of the project.
 *
 * This function is responsible for initializing and starting the service processes 
 * required for the operation of Phase 4. It does not take any arguments and does not 
 * return any value.
 *
 * @return void
 */
void phase4_start_service_processes(){
    clockDriverPID = fork1("clockDriver", clockDriver, NULL, USLOSS_MIN_STACK, 2);
    if(clockDriverPID < 0){
        USLOSS_Console("startProcesses(): Can't create clockDriver\n");
        USLOSS_Halt(1);
    }

    for(int i = 0; i < USLOSS_TERM_UNITS; i++){
        char terminalBuffer[10];
        char terminalName[64];
        sprintf(terminalBuffer, "%d", i);
        sprintf(terminalName, "termDriver %d", i);
        int usless_pid = fork1(terminalName, termDriver, terminalBuffer, USLOSS_MIN_STACK, 2);
        usless_pid++; // Avoid warning
    }

    for(int i = 0; i < USLOSS_DISK_UNITS; i++){
        char diskBuffer[10];
        char diskName[64];
        sprintf(diskBuffer, "%d", i);
        sprintf(diskName, "diskDriver %d", i);
        int usless_pid = fork1(diskName, diskDriver, diskBuffer, USLOSS_MIN_STACK, 2);
        usless_pid++; // Avoid warning
    }
} 


/**
 * @brief Puts the current process to sleep for a specified number of seconds.
 *
 * This function is a system call handler for the sleep operation. It takes a pointer to a 
 * USLOSS_Sysargs structure as an argument, which contains the arguments passed to the system call.
 * The number of seconds to sleep is expected to be in the `arg1` field of the USLOSS_Sysargs structure.
 *
 * @param args A pointer to a USLOSS_Sysargs structure containing the arguments passed to the system call.
 * @return void
 */
void sleep_sys(USLOSS_Sysargs *args){
    int seconds = (int)(long)args->arg1;
    int result = kernSleep(seconds);
    args->arg4 = (void *)(long)result;
}

/**
 * @brief Puts the current process to sleep for a specified number of seconds.
 *
 * This function is used to pause the execution of the current process for a certain number of seconds.
 * It takes an integer as an argument, which represents the number of seconds the process should sleep.
 * If the number of seconds is less than 0, the function returns -1 indicating an error.
 *
 * @param seconds The number of seconds the process should sleep.
 * @return int Returns 0 if the sleep operation was successful, -1 otherwise.
 */
int kernSleep(int seconds){
    int procPID = getpid();
    if(seconds < 0){
        return -1;
    }
    if(procTable[procPID % MAXPROC].pid == -1){
        init_proc(procPID);
    }
    procTable[procPID % MAXPROC].wakeTime =currentTime() + seconds*1000000;
    addHeap(&slHeap, &procTable[procPID % MAXPROC]);
    MboxRecv(procTable[procPID % MAXPROC].mboxID, NULL, 0);
    return 0;
}

/**
 * @brief Handles the terminal write system call.
 *
 * This function is a system call handler for the terminal write operation. It takes a pointer to a 
 * USLOSS_Sysargs structure as an argument, which contains the arguments passed to the system call.
 * The data to write and the number of bytes to write are expected to be in the `arg1` and `arg2` fields 
 * of the USLOSS_Sysargs structure, respectively.
 *
 * @param args A pointer to a USLOSS_Sysargs structure containing the arguments passed to the system call.
 * @return void
 */
void term_write_sys(USLOSS_Sysargs *args){
    char *buffer = (char *)args->arg1;
    int bufferSize = (int)(long)args->arg2;
    int unitID = (int)(long)args->arg3;
    int numCharsRead = 0;
    int result = kernTermWrite(buffer, bufferSize, unitID, &numCharsRead);
    args->arg4 = (void *)(long)result;
    args->arg5 = (void *)(long)numCharsRead;
}

/**
 * @brief Writes data to a terminal.
 *
 * This function is used to write a specified number of characters from a buffer to a terminal.
 * It takes a buffer, the size of the buffer, the terminal unit number, and a pointer to an integer 
 * that will store the number of characters actually written.
 *
 * @param buffer A pointer to the buffer containing the data to be written.
 * @param bufferSize The size of the buffer.
 * @param unit The terminal unit number to which the data should be written.
 * @param numCharsRead A pointer to an integer that will store the number of characters actually written.
 * @return int Returns 0 if the write operation was successful, -1 otherwise.
 */
int  kernTermWrite(char *buffer, int bufferSize, int unit,int *numCharsRead){
    if(unit < 0 || unit >= USLOSS_TERM_UNITS || bufferSize < 0){
        return -1;
    }
    MboxSend(writeLock[unit], NULL, 0);
    for(int i = 0; i < bufferSize; i++){
        MboxRecv(terminalWrite[unit], NULL, 0);
        //Supplement code
        int cr_val = 0x1;
        cr_val |= 0x2;
        cr_val |= 0x4;
        cr_val |= (buffer[i] << 8);

        int useless = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *)(long)cr_val);
        useless++; // Avoid warning
    }
    MboxRecv(writeLock[unit], NULL, 0);
    return bufferSize;
}

/**
 * @brief Handles the terminal read system call.
 *
 * This function is a system call handler for the terminal read operation. It takes a pointer to a 
 * USLOSS_Sysargs structure as an argument, which contains the arguments passed to the system call.
 * The buffer to read into and the maximum number of bytes to read are expected to be in the `arg1` 
 * and `arg2` fields of the USLOSS_Sysargs structure, respectively.
 *
 * @param args A pointer to a USLOSS_Sysargs structure containing the arguments passed to the system call.
 * @return void
 */
void term_read_sys(USLOSS_Sysargs *args){
    char *buffer = (char *)args->arg1;
    int bufferSize = (int)(long)args->arg2;
    int unitID = (int)(long)args->arg3;
    int numCharsRead = 0;
    int result = kernTermRead(buffer, bufferSize, unitID, &numCharsRead);
    args->arg4 = (void *)(long)result;
    args->arg2 = (void *)(long)numCharsRead;
}

/**
 * @brief Reads data from a terminal.
 *
 * This function is used to read a specified number of characters from a terminal into a buffer.
 * It takes a buffer, the size of the buffer, the terminal unit number, and a pointer to an integer 
 * that will store the number of characters actually read.
 *
 * @param buffer A pointer to the buffer where the read data should be stored.
 * @param bufferSize The size of the buffer.
 * @param unit The terminal unit number from which the data should be read.
 * @param numCharsRead A pointer to an integer that will store the number of characters actually read.
 * @return int Returns 0 if the read operation was successful, -1 otherwise.
 */
int  kernTermRead (char *buffer, int bufferSize, int unit,int *numCharsRead){
    if(unit < 0 || unit > USLOSS_TERM_UNITS -1 || bufferSize <= 0){
        return -1;
    }
    char line[MAXLINE];

    int ret = MboxRecv(terminalReadMbox[unit], &line, MAXLINE);
    if(ret > bufferSize){
        ret = bufferSize;
        
    }
    memcpy(buffer, line, ret);
    *numCharsRead = ret;
    return 0;
}

/**
 * @brief Handles the disk size system call.
 *
 * This function is a system call handler for the disk size operation. It takes a pointer to a 
 * USLOSS_Sysargs structure as an argument, which contains the arguments passed to the system call.
 * The disk unit number is expected to be in the `arg1` field of the USLOSS_Sysargs structure.
 * The function fills the `arg2`, `arg3`, and `arg4` fields of the USLOSS_Sysargs structure with 
 * the sector size, the number of sectors, and the number of tracks on the disk, respectively.
 *
 * @param args A pointer to a USLOSS_Sysargs structure containing the arguments passed to the system call.
 * @return void
 */
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

/**
 * @brief Retrieves the size of a disk.
 *
 * This function is used to get the size of a specified disk unit. It takes the disk unit number 
 * and pointers to integers that will store the sector size, the number of tracks, and the number 
 * of sectors on the disk.
 *
 * @param unit The disk unit number.
 * @param sector A pointer to an integer that will store the sector size.
 * @param track A pointer to an integer that will store the number of tracks.
 * @param disk A pointer to an integer that will store the number of sectors.
 * @return int Returns 0 if the operation was successful, -1 otherwise.
 */
int kernDiskSize (int unit, int *sector, int *track, int *disk){
    int tempTrack = -1;
    if(unit < 0 || unit > USLOSS_DISK_UNITS -1){
        return -1;
    }
    tempTrack = unit == 0 ? disk0[DISK_MUT_TRACK] : disk1[DISK_MUT_TRACK];
    MboxRecv(tempTrack, NULL, 0);
    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;
    *disk = CurrentDisk;
    return 0;
}

/**
 * @brief Handles the disk write system call.
 *
 * This function is a system call handler for the disk write operation. It takes a pointer to a 
 * USLOSS_Sysargs structure as an argument, which contains the arguments passed to the system call.
 * The buffer to write from, the number of sectors to write, the track number, the first sector number, 
 * and the disk unit number are expected to be in the `arg1`, `arg2`, `arg3`, `arg4`, and `arg5` fields 
 * of the USLOSS_Sysargs structure, respectively.
 *
 * @param args A pointer to a USLOSS_Sysargs structure containing the arguments passed to the system call.
 * @return void
 */
void disk_write_sys(USLOSS_Sysargs *args){
    void *diskBuffer = (void *)args->arg1;
    int unit = (int)(long)args->arg5;
    int track = (int)(long)args->arg3;
    int first = (int)(long)args->arg4;
    int sectors = (int)(long)args->arg2;
    int status = 0;
    int result = kernDiskWrite(diskBuffer, unit, track, first, sectors, &status);
    args->arg1 = (void *)(long)status;
    args->arg4 = (void *)(long)result;
}

/**
 * @brief Writes data to a disk.
 *
 * This function is used to write a specified number of sectors from a buffer to a disk.
 * It takes a buffer, the disk unit number, the track number, the first sector number, 
 * the number of sectors to write, and a pointer to an integer that will store the status 
 * of the write operation.
 *
 * @param diskBuffer A pointer to the buffer containing the data to be written.
 * @param unit The disk unit number to which the data should be written.
 * @param track The track number where the writing should start.
 * @param first The first sector number where the writing should start.
 * @param sectors The number of sectors to write.
 * @param status A pointer to an integer that will store the status of the write operation.
 * @return int Returns 0 if the write operation was successful, -1 otherwise.
 */
int kernDiskWrite(void *diskBuffer, int unit, int track, int first,int sectors, int *status){
    if(unit != 1 && unit != 0){
        return -1;
    }
    int numTracks = unit == 0 ? 16 : 32;
    if(track < 0){
        return -1;
    }
    if(track >= numTracks){
        *status = USLOSS_DEV_ERROR;
        return 0;
    }
    if(first < 0 || first >= USLOSS_DISK_TRACK_SIZE){
        return -1;
    }
    int pid = getpid();
    int CurrentMbox = unit == 0 ? disk0[DISK_MBOX] : disk1[DISK_MBOX];
    int CurrentQMbox = unit == 0 ? disk0[DISK_Q_MBOX] : disk1[DISK_Q_MBOX];
    int currentTable = unit;
    int index = pid % MAXPROC;
    diskTable[currentTable][index].pid = pid;
    diskTable[currentTable][index].track = track;
    diskTable[currentTable][index].first = first;
    diskTable[currentTable][index].sectors = sectors;
    // memcpy(diskTable[index].diskBuffer, diskBuffer, sectors*USLOSS_DISK_SECTOR_SIZE+1);
    // USLOSS_Console("kernDiskWrite:Unit %d buffer: %s\n", unit, diskBuffer);
    diskTable[currentTable][index].diskBuffer = diskBuffer;
    diskTable[currentTable][index].op = USLOSS_DISK_WRITE;

    MboxSend(CurrentQMbox, NULL, 0);
    diskQueue(unit, pid);
    MboxRecv(CurrentQMbox, NULL, 0);
    MboxCondSend(CurrentMbox, NULL, 0);
    MboxRecv(diskTable[currentTable][index].mboxID, NULL, 0);
    status = 0;
    return 0;
}

/**
 * @brief Handles the disk read system call.
 *
 * This function is a system call handler for the disk read operation. It takes a pointer to a 
 * USLOSS_Sysargs structure as an argument, which contains the arguments passed to the system call.
 * The buffer to read into, the number of sectors to read, the track number, the first sector number, 
 * and the disk unit number are expected to be in the `arg1`, `arg2`, `arg3`, `arg4`, and `arg5` fields 
 * of the USLOSS_Sysargs structure, respectively.
 *
 * @param args A pointer to a USLOSS_Sysargs structure containing the arguments passed to the system call.
 * @return void
 */
void disk_read_sys(USLOSS_Sysargs *args){
    void *diskBuffer = (void *)args->arg1;
    int unit = (int)(long)args->arg5;
    int track = (int)(long)args->arg3;
    int first = (int)(long)args->arg4;
    int sectors = (int)(long)args->arg2;
    int status = 0;
    int result = kernDiskRead(diskBuffer, unit, track, first, sectors, &status);
    args->arg1 = (void *)(long)status;
    args->arg4 = (void *)(long)result;
}

/**
 * @brief Reads data from a disk.
 *
 * This function is used to read a specified number of sectors from a disk into a buffer.
 * It takes a buffer, the disk unit number, the track number, the first sector number, 
 * the number of sectors to read, and a pointer to an integer that will store the status 
 * of the read operation.
 *
 * @param diskBuffer A pointer to the buffer where the read data should be stored.
 * @param unit The disk unit number from which the data should be read.
 * @param track The track number where the reading should start.
 * @param first The first sector number where the reading should start.
 * @param sectors The number of sectors to read.
 * @param status A pointer to an integer that will store the status of the read operation.
 * @return int Returns 0 if the read operation was successful, -1 otherwise.
 */
int kernDiskRead (void *diskBuffer, int unit, int track, int first,int sectors, int *status){
    int pid = getpid();
    int CurrentMbox = unit == 0 ? disk0[DISK_MBOX] : disk1[DISK_MBOX];
    int CurrentQMbox = unit == 0 ? disk0[DISK_Q_MBOX] : disk1[DISK_Q_MBOX];
    int currentTable = unit;
    int index = pid % MAXPROC;
    diskTable[currentTable][index].pid = pid;
    diskTable[currentTable][index].track = track;
    diskTable[currentTable][index].first = first;
    diskTable[currentTable][index].sectors = sectors;
    // memcpy(diskBuffer, diskTable[currentTable][index].diskBuffer, sectors*USLOSS_DISK_SECTOR_SIZE+1);
    // USLOSS_Console("kernDiskRead:Unit %d buffer: %s\n", unit, diskBuffer);
    diskTable[currentTable][index].diskBuffer = diskBuffer;
    // USLOSS_Console("kernDiskRead: %s\n", &(diskBuffer[0]));
    diskTable[currentTable][index].op = USLOSS_DISK_READ;

    MboxSend(CurrentQMbox, NULL, 0);
    diskQueue(unit, pid);
    MboxRecv(CurrentQMbox, NULL, 0);
    MboxCondSend(CurrentMbox, NULL, 0);
    MboxRecv(diskTable[currentTable][index].mboxID, NULL, 0);
    status = 0;
    return 0;
}

//-------------Daemon Functions------------------//

/**
 * @brief The clock driver function.
 *
 * This function is a driver for the clock. It is typically run as a separate process, 
 * and it is responsible for handling clock interrupts and performing any necessary 
 * actions. It takes a single argument, which can be used to pass data to the driver.
 *
 * @param arg A pointer to data that can be used by the driver.
 * @return int Returns 0 if the driver function completed successfully, -1 otherwise.
 */
static int clockDriver(char *arg){
    int status;
    int useless = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    useless++; // Avoid warning
    while(!isZapped()){
        waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        procStruct * proc;
        while(slHeap.size > 0 && peekHeap(&slHeap)->wakeTime <= currentTime()){
            proc = removeHeap(&slHeap);
            MboxSend(proc->mboxID, NULL, 0);
        }
    }
    return 0;
}

/**
 * @brief The terminal driver function.
 *
 * This function is a driver for the terminal. It is typically run as a separate process, 
 * and it is responsible for handling terminal interrupts and performing any necessary 
 * actions. It takes a single argument, which can be used to pass data to the driver.
 *
 * @param args A pointer telling us terminal unit.
 * @return int Returns 0 if the driver function completed successfully, -1 otherwise.
 */
static int termDriver(char *args){
    int status;
    int unit = atoi(args);
    while(!isZapped()){
        int Xmit = USLOSS_TERM_STAT_XMIT(status);
        if(Xmit == USLOSS_DEV_READY){
            MboxCondSend(terminalWrite[unit], NULL, 0);
        }
        else if(Xmit == USLOSS_DEV_ERROR){
            USLOSS_Console("termDriver: Error\n");
            USLOSS_Halt(1);
        }

        waitDevice(USLOSS_TERM_DEV, unit, &status);
        int stat = USLOSS_TERM_STAT_RECV(status);

        if(stat == USLOSS_DEV_BUSY){
            char ch = USLOSS_TERM_STAT_CHAR(status);
            if(ch == '\n' || terminalLineMbox[unit] == MAXLINE){
                if(terminalLineMbox[unit] != MAXLINE){
                    terminalLines[unit][terminalLineMbox[unit]] = ch;
                    terminalLineMbox[unit]++;
                }
                char retLine[terminalLineMbox[unit]];
                memset(retLine, '\0', sizeof(retLine));
                for(int i = 0; i < terminalLineMbox[unit]; i++){
                    retLine[i] = terminalLines[unit][i];
                }
                MboxSend(terminalReadMbox[unit], retLine, terminalLineMbox[unit]);
                
                memset(terminalLines[unit], '\0', sizeof(terminalLines[unit]));
                terminalLineMbox[unit] = 0;
            }else{
                terminalLines[unit][terminalLineMbox[unit]] = ch;
                terminalLineMbox[unit]++;
            }
        }else if(stat == USLOSS_DEV_ERROR){
            USLOSS_Console("termDriver: Error\n");
            USLOSS_Halt(1);
        }
    }
    return 0;
}

/**
 * @brief The disk driver function.
 *
 * This function is a driver for the disk. It is typically run as a separate process, 
 * and it is responsible for handling disk interrupts and performing any necessary 
 * actions. It takes a single argument, which can be used to pass data to the driver.
 *
 * @param args A pointer telling us the disk unit.
 * @return int Returns 0 if the driver function completed successfully, -1 otherwise.
 */
static int diskDriver(char *args){
    int currentMbox, currentQMbox, currentMut, currentMutTrack;
    int unit;
    int status;
    diskReqQ** diskPtr = NULL;
    diskReqQ* diskReq = NULL;
    USLOSS_DeviceRequest req;

    if(*args == '0'){
        currentMbox = disk0[DISK_MBOX];
        currentQMbox = disk0[DISK_Q_MBOX];
        currentMut = disk0[DISK_MUT];
        currentMutTrack = disk0[DISK_MUT_TRACK];
        unit = 0;
        diskPtr = &disk0Req;
    }
    else{
        currentMbox = disk1[DISK_MBOX];
        currentQMbox = disk1[DISK_Q_MBOX];
        currentMut = disk1[DISK_MUT];
        currentMutTrack = disk1[DISK_MUT_TRACK];
        unit = 1;
        diskPtr = &disk1Req;
    }

    req.opr = USLOSS_DISK_TRACKS;
    req.reg1 = (void *)(long)&CurrentDisk;
    int useless = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &req);
    useless++; // Avoid warning
    waitDevice(USLOSS_DISK_DEV, unit, &status);
    if(unit == 0){
        disk0[DISK_NUM_TRACKS] = CurrentDisk;
    }
    else{
        disk1[DISK_NUM_TRACKS] = CurrentDisk;
    }
    MboxSend(currentMutTrack, NULL, 0);
    while(!isZapped()){
        MboxRecv(currentMbox, NULL, 0);
        while(*diskPtr != NULL){
            diskReq = *diskPtr;
            int mboxID = diskReq->mboxID;
            int track = diskReq->track;
            int first = diskReq->first;
            int sectors = diskReq->sectors;
            void *diskBuffer = diskReq->diskBuffer;
            int op = diskReq->op;
            diskSeek(unit, track);
            req.opr = op;
            req.reg1 = (void *)(long)first;
            req.reg2 = (void *)(long)diskBuffer;
            for(int i = 0; i < sectors; i++){
                int temp = (int)(long)req.reg1;
                if(temp == USLOSS_DISK_TRACK_SIZE){
                    req.reg1 = (void *)(long)0;
                    track++;
                    diskSeek(unit, track);
                }
                MboxSend(currentMut, NULL, 0);
                useless = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &req);
                useless++; // Avoid warning
                waitDevice(USLOSS_DISK_DEV, unit, &status);
                MboxRecv(currentMut, NULL, 0);
                req.reg1 = (void *)(long)(temp + 1);
                req.reg2 = (void *)(long)((char *)req.reg2 + USLOSS_DISK_SECTOR_SIZE);
            }
            MboxSend(currentQMbox, NULL, 0);
            *diskPtr = (*diskPtr)->next;
            diskReq->next = NULL;
            MboxRecv(currentQMbox, NULL, 0);
            MboxSend(mboxID, NULL, 0);
        }
        // MboxRecv(currentMutTrack, NULL, 0);
    }
    // MboxRecv(currentMutTrack, NULL, 0);
    return 0;
}

//-------------Disk Helper Functions---------------//
/**
 * @brief Seeks to a specific track on a disk.
 *
 * This function is used to move the disk's read/write head to a specific track. 
 * It takes the disk unit number and the track number as arguments.
 *
 * @param unit The disk unit number.
 * @param track The track number to seek to.
 * @return int Returns 0 if the seek operation was successful, -1 otherwise.
 */
int diskSeek(int unit, int track){
    int status;
    int CurrentMbox = unit == 0 ? disk0[DISK_MBOX] : disk1[DISK_MBOX];

    USLOSS_DeviceRequest req;
    req.opr = USLOSS_DISK_SEEK;
    req.reg1 = (void *)(long)track;
    MboxSend(CurrentMbox, NULL, 0);
    int useless = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &req);
    useless++; // Avoid warning
    waitDevice(USLOSS_DISK_DEV, unit, &status);
    MboxRecv(CurrentMbox, NULL, 0);
    return 0;
}

/**
 * @brief Adds a process to the disk queue.
 *
 * This function is used to add a process to the queue of processes waiting for disk access. 
 * It takes the disk unit number and the process ID as arguments.
 *
 * @param unit The disk unit number.
 * @param pid The process ID of the process to be added to the queue.
 * @return void
 */
void diskQueue(int unit, int pid){
    diskReqQ * temp = NULL;
    if(unit == 0){
        if(disk0Req == NULL){
            disk0Req = &diskTable[DISK0][pid % MAXPROC];
            return;
        }
        else{
            temp = disk0Req;
        }
    }
    else{
        if(disk1Req == NULL){
            disk1Req = &diskTable[DISK1][pid % MAXPROC];
            return;
        }
        else{
            temp = disk1Req;
        }
    }
    int index = pid % MAXPROC;
    int currentTrack = temp->track;
    int lastTrack = diskTable[unit][index].track;
    if(currentTrack <= lastTrack){
        while(temp->next != NULL && temp->next->track <= currentTrack && temp->next->track >= lastTrack){
            temp = temp->next;
        }
        diskTable[unit][index].next = temp->next;
        temp->next = &diskTable[unit][index];
        return;
    }else{
        while(temp->next != NULL && temp->next->track >= currentTrack){
            temp = temp->next;
        }
        if(temp->next == NULL){
            temp->next = &diskTable[unit][index];
            return;
        }
        else{
            while(temp->next != NULL && temp->next->track <= lastTrack){
                temp = temp->next;
            }
            diskTable[unit][index].next = temp->next;
            temp->next = &diskTable[unit][index];
            return;
        }
    }
}
//-------------Utility Functions------------------//

/**
 * @brief Initializes a process.
 *
 * This function is used to initialize a process with a given process ID (pid). 
 * It sets up any necessary data structures and configurations for the process.
 *
 * @param pid The process ID of the process to be initialized.
 * @return void
 */
void init_proc(int pid){
    procTable[pid].pid = pid;
    procTable[pid].mboxID = MboxCreate(0,0);
    procTable[pid].wakeTime = -1;
}

/**
 * @brief Initializes a process heap.
 *
 * This function is used to initialize a process heap. It sets up any necessary 
 * data structures and configurations for the heap. It takes a pointer to a procHeap 
 * structure as an argument.
 *
 * @param heap A pointer to a procHeap structure representing the heap to be initialized.
 * @return void
 */
void initHeap(procHeap * heap){
    heap->size = 0;
    for(int i = 0; i < MAXPROC; i++){
        heap->HeapProc[i] = NULL;
    }
}

/**
 * @brief Adds a process to a process heap.
 *
 * This function is used to add a process to a process heap. It takes a pointer to a procHeap 
 * structure and a pointer to a procStruct structure as arguments. The procStruct represents 
 * the process to be added to the heap.
 *
 * @param heap A pointer to a procHeap structure representing the heap.
 * @param proc A pointer to a procStruct structure representing the process to be added.
 * @return void
 */
void addHeap(procHeap * heap, procStruct * proc){
    int i,parent;
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

/**
 * @brief Removes a process from a process heap.
 *
 * This function is used to remove a process from a process heap. It takes a pointer to a procHeap 
 * structure as an argument. The function returns a pointer to the procStruct structure representing 
 * the process that was removed from the heap.
 *
 * @param heap A pointer to a procHeap structure representing the heap.
 * @return procStruct* A pointer to the procStruct structure representing the process that was removed.
 */
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
    return ret;

}

/**
 * @brief Peeks at the top process in a process heap.
 *
 * This function is used to get a reference to the process at the top of a process heap without 
 * removing it. It takes a pointer to a procHeap structure as an argument. The function returns 
 * a pointer to the procStruct structure representing the process at the top of the heap.
 *
 * @param heap A pointer to a procHeap structure representing the heap.
 * @return procStruct* A pointer to the procStruct structure representing the process at the top of the heap.
 */
procStruct * peekHeap(procHeap * heap){
    return heap->HeapProc[0];
}