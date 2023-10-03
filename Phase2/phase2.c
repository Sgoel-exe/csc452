#include "phase2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <usloss.h>
#include <phase1.h>

//------Queue------//
typedef struct Queue{
    int size;
    void *head;
    void *tail;
    int type; // 0 = mSlot, 1 = proc
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


//------Global Variables------//
MailBox mailboxes[MAXMBOX];
MailSlot mailSlots[MAXSLOTS];
ShadowProc shadowProcs[MAXPROC];
int nextMBoxID = 0;  //Next available mailbox ID
int nextSlotID = 0;  //Next available slot ID

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

//------Function Prototypes------//


void phase2_init(void){
    //Initialize the mailboxes
    for(int i = 0; i < MAXMBOX; i++){
        mailboxes[i].mboxID = -1;
        mailboxes[i].slotSize = -1;
        mailboxes[i].numSlots = -1;
        mailboxes[i].status = -1;
        Queue_init(&mailboxes[i].slots, 0);
        Queue_init(&mailboxes[i].senders, 1);
        Queue_init(&mailboxes[i].recievers, 1);
    }

    //Initialize the mailslots
    for(int i = 0; i < MAXSLOTS; i++){
        mailSlots[i].mboxID = -1;
        mailSlots[i].status = -1;
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

    //Create IO mailboxes // Fix Copilot code
    for(int i = 0; i < 7; i++){
        MboxCreate(0, 0);
    }

}

void Queue_init(Queue * queue, int type){
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    queue->type = type;
}

void nullsys(USLOSS_Sysargs *args){
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
}

void phase2_start_service_processes(void){

}

int phase2_check_io(void){
    return 0;
}

void phase2_clockHandler(void){

}

int MboxCreate(int slots, int slot_size){
    return 0;
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

}

void disableInterrupts(void){

}

