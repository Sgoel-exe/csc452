#include "phase2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <usloss.h>
#include <phase1.h>
#include <assert.h>

//------GLOBAL VARIABLES------//
#define EMPTY -1
#define ACTIVE 0
#define ZOMBIE 1
#define INACTIVE 2
typedef struct MailBox MailBox;
typedef struct MailSlot MailSlot;
typedef struct ShadowProc ShadowProc;


//------Queue------//
typedef struct Queue{
    int size;
    void *head;
    void *tail;
    int type; // 0 = MailSlot, 1 = ShadowProc
}Queue;

//------The Mail Box------//
typedef struct MailBox{
    int mboxID;
    int slotSize;
    int numSlots;
    int status;
    Queue slots;
    Queue senders;
    Queue recievers;
}MailBox;

//------The Mail Slot------//
typedef struct MailSlot{
    int mboxID;
    int status;
    char message[MAX_MESSAGE];
    int messageSize;
    MailSlot* next;
}MailSlot;

//------The Process------//
typedef struct ShadowProc{
    int pid;
    char * message;
    int messageSize;
    MailSlot * slot; 
    ShadowProc* next;
}ShadowProc;

//------Function Prototypes------//
void nullsys(USLOSS_Sysargs *args);
void phase2_clockHandler(void);
void diskHandler(int dev, void *arg);
void termHandler(int dev, void *arg);
void syscallHandler(int dev, void *arg);
void enableInterrupts(void);
void disableInterrupts(void);
void Queue_init(Queue * queue, int type);
void Queue_push(Queue * queue, void * item);
void Queue_pop(Queue * queue);
void * Queue_front(Queue * queue);
int findNextMBoxID();
int findNextSlotID();
void createMailSlot(int mboxID, char * message, int messageSize);

//------Global Variables------//
MailBox mailboxes[MAXMBOX];
MailSlot mailSlots[MAXSLOTS];
ShadowProc shadowProcs[MAXPROC];
int nextMBoxID = 0;  //Next available mailbox ID
int nextSlotID = 0;  //Next available slot ID

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

void phase2_init(void){

    //Initialize the mailboxes
    for(int i = 0; i < MAXMBOX; i++){
        mailboxes[i].mboxID = -1;
        mailboxes[i].slotSize = -1;
        mailboxes[i].numSlots = -1;
        mailboxes[i].status = EMPTY;
        Queue_init(&mailboxes[i].slots, 0);
        Queue_init(&mailboxes[i].senders, 1);
        Queue_init(&mailboxes[i].recievers, 1);
    }

    //Initialize the mailslots
    for(int i = 0; i < MAXSLOTS; i++){
        mailSlots[i].mboxID = -1;
        mailSlots[i].status = EMPTY;
        mailSlots[i].messageSize = -1;
        mailSlots[i].message[0] = '\0';
        mailSlots[i].next = NULL;
    }

    //Initialize the shadow processes
    for(int i = 0; i < MAXPROC; i++){
        shadowProcs[i].pid = -1;
        shadowProcs[i].messageSize = -1;
        shadowProcs[i].message = NULL;
        shadowProcs[i].slot = NULL;
        shadowProcs[i].next = NULL;
    }

    //Initialize the system call vector
    for(int i = 0; i < MAXSYSCALLS; i++){
        systemCallVec[i] = nullsys;
    }

    USLOSS_IntVec[USLOSS_CLOCK_INT] = phase2_clockHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;

    //Create IO mailboxes // Fix Copilot code
    for(int i = 0; i < 7; i++){
        MboxCreate(0, sizeof(int));
        //0-7 interrupt handlres
    }
}


void nullsys(USLOSS_Sysargs *args){
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
}

void phase2_start_service_processes(void){
    // phase3_start_service_processes();
    // phase4_start_service_processes();
    // phase5_start_service_processes();
}

int phase2_check_io(void){
    return 0;
}

int MboxCreate(int slots, int slot_size){
    if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || slots > MAX_MESSAGE) {
        return -1;
    }
    //Index IS the mailbox ID
    if(nextMBoxID == MAXMBOX || mailboxes[nextMBoxID].status != EMPTY){
        nextMBoxID = findNextMBoxID();
        if(nextMBoxID == -1){
            return -1;
        }
    }
    mailboxes[nextMBoxID].mboxID = nextMBoxID;
    mailboxes[nextMBoxID].slotSize = slot_size;
    mailboxes[nextMBoxID].numSlots = slots;
    mailboxes[nextMBoxID].status = ACTIVE;

    nextMBoxID++;
     
    return nextMBoxID - 1;
}

int findNextMBoxID(){
    for(int i = 7; i < MAXMBOX; i++){
        if(mailboxes[i].status == EMPTY){
             
            return i;
        }
    }
     
    return -1;
}

void createMailSlot(int mboxID, char * message, int messageSize){
    if(nextSlotID == MAXSLOTS || mailSlots[nextSlotID].status != EMPTY){
        nextSlotID = findNextSlotID();
        if(nextSlotID == -1){
             
            return NULL;
        }
    }
    mailSlots[nextSlotID].mboxID = mboxID;
    mailSlots[nextSlotID].status = 0;
    mailSlots[nextSlotID].messageSize = messageSize;
    strcpy(mailSlots[nextSlotID].message, message);
    mailSlots[nextSlotID].next = NULL;
    nextSlotID++;
    // return &mailSlots[nextSlotID - 1];
}

int findNextSlotID(){
    for(int i = 0; i < MAXSLOTS; i++){
        if(mailSlots[i].status == EMPTY){
             
            return i;
        }
    }
     
    return -1;
}

int MboxRelease(int mbox_id){
    return 0;
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size){
    return 0;
}

int MboxRecv(int mbox_id, void *msg_ptr, int msg_size){
    return 0;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){
    return 0;
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_size){
    return 0;
}

void waitDevice(int type, int unit, int *status){

}

void enableInterrupts(void){
    int result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    assert(result == USLOSS_DEV_OK);
}

void  disableInterrupts(void){
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT);
    assert(result == USLOSS_DEV_OK);
}

//---------------------------QUEUE FUNCTIONS---------------------------//
void Queue_init(Queue * queue, int type){
    queue->size = 0;
    if(type == 0){
        queue->head = (MailSlot *) queue->head;
        queue->tail = (MailSlot *) queue->tail;
    }
    else{
        queue->head = (ShadowProc *) queue->head;
        queue->tail = (ShadowProc *) queue->tail;
    }
    queue->type = type;
}

void Queue_push(Queue * queue, void * item){
    if(queue->type == 0){
        MailSlot * mailSlot = (MailSlot *) item;
        if(queue->size == 0){
            queue->head = mailSlot;
            queue->tail = mailSlot;
        }
        else{
            ((MailSlot *) queue->tail)->next = mailSlot;
            queue->tail = mailSlot;
        }
    }
    else{
        ShadowProc * shadowProc = (ShadowProc *) item;
        if(queue->size == 0){
            queue->head = shadowProc;
            queue->tail = shadowProc;
        }
        else{
            ((ShadowProc *) queue->tail)->next = shadowProc;
            queue->tail = shadowProc;
        }
    }
    queue->size++;
}

void Queue_pop(Queue * queue){
    if(queue->size == 0){
        return;
    }
    if(queue->type == 0){
        MailSlot * mailSlot = (MailSlot *) queue->head;
        queue->head = mailSlot->next;
        mailSlot->next = NULL;
    }
    else{
        ShadowProc * shadowProc = (ShadowProc *) queue->head;
        queue->head = shadowProc->next;
        shadowProc->next = NULL;
    }
    queue->size--;
}

void * Queue_front(Queue * queue){
    if(queue->size == 0){
        return NULL;
    }
    if(queue->type == 0){
        return (MailSlot *) queue->head;
    }
    else{
        return (ShadowProc *) queue->head;
    }
}


//---------------------------HANDLERS---------------------------//
void diskHandler(int dev, void *arg){
    USLOSS_Console("diskHandler(): called\n");
}

void termHandler(int dev, void *arg){
    USLOSS_Console("termHandler(): called\n");
}

void syscallHandler(int dev, void *arg){
    //USLOSS_Console("syscallHandler(): called\n");
    USLOSS_Sysargs *args = (USLOSS_Sysargs *) arg;
    if(args->number < 0 || args->number >= MAXSYSCALLS){
        USLOSS_Console("syscallHandler(): Invalid syscall %d. Halting...\n", args->number);
        USLOSS_Halt(1);
    }
    systemCallVec[args->number](args);
}

void phase2_clockHandler(void){

}