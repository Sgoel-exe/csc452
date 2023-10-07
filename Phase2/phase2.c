#include "phase2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <usloss.h>
#include <phase1.h>
#include <assert.h>

//------GLOBAL VARIABLES------//
#define RELEASE -3
#define EMPTY -1
#define ACTIVE 0
#define ZOMBIE 1
#define INACTIVE 2
#define MAILBLOCK 12
#define CANBLOCK 1
#define CANTBLOCK 0
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
    int currentNumOfSlots;
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
    int index;
    void *msg_ptr;
    int msg_size;
    MailSlot * mailSlot;
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
void * Queue_pop(Queue * queue);
void * Queue_front(Queue * queue);
int findNextMBoxID();
int findNextSlotID();
int createMailSlot(int mboxID, char * message, int messageSize);

//------Global Variables------//
MailBox mailboxes[MAXMBOX];
MailSlot mailSlots[MAXSLOTS];
ShadowProc shadowProcs[MAXPROC];
int nextMBoxID = 0;  //Next available mailbox ID
int nextSlotID = 0;  //Next available slot ID
int nextProcID = 0;  //Next available process ID

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

void phase2_init(void){

    //Initialize the mailboxes
    for(int i = 0; i < MAXMBOX; i++){
        mailboxes[i].mboxID = -1;
        mailboxes[i].slotSize = -1;
        mailboxes[i].numSlots = -1;
        mailboxes[i].currentNumOfSlots = 0;
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
        shadowProcs[i].msg_ptr = NULL;
        shadowProcs[i].msg_size = -1;
        shadowProcs[i].mailSlot = NULL;
        shadowProcs[i].index = i;
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

int createMailSlot(int mboxID, char * message, int messageSize){
    if(mailboxes[mboxID].currentNumOfSlots == mailboxes[mboxID].numSlots){
        return -1;
    }
    if(nextSlotID == MAXSLOTS || mailSlots[nextSlotID].status != EMPTY){
        nextSlotID = findNextSlotID();
        if(nextSlotID == -1){
             
            return -1;
        }
    }
    mailSlots[nextSlotID].mboxID = mboxID;
    mailSlots[nextSlotID].status = ACTIVE;
    mailSlots[nextSlotID].messageSize = messageSize;
    // USLOSS_Console("createMailSlot(): messageSize = %d,  Message: %s\n", messageSize, message);
    strncpy(mailSlots[nextSlotID].message, message, messageSize);
    mailSlots[nextSlotID].next = NULL;
    nextSlotID++;
    return nextSlotID - 1;
}

int findNextSlotID(){
    for(int i = 0; i < MAXSLOTS; i++){
        if(mailSlots[i].status == EMPTY){
             
            return i;
        }
    }
     
    return -1;
}

int createShadowProc(int pid, void **msg_ptr, int msg_size){
    if(pid < 0 || pid >= MAXPROC){
        return -1;
    }
    if(shadowProcs[nextProcID].pid != -1){
        nextProcID = findNextProcID();
        if(nextProcID == -1){
            return -1;
        }
    }
    shadowProcs[nextProcID].pid = pid;
    shadowProcs[nextProcID].msg_ptr = msg_ptr;
    shadowProcs[nextProcID].msg_size = msg_size;
    shadowProcs[nextProcID].next = NULL;
    nextProcID++;
    return nextProcID - 1;
}

int findNextProcID(){
    for(int i = 0; i < MAXPROC; i++){
        if(shadowProcs[i].pid == -1){
            return i;
        }
    }
    return -1;
}

int MboxRelease(int mbox_id){
    return 0;
}


int SendHelper(int mbox_id, void *msg_ptr, int msg_size, int canBlock){
    //IS mailbox release?
    if(mailboxes[mbox_id].status == RELEASE){
        return RELEASE;
    }
    //Error checking
    if(mbox_id < 0 || mbox_id >= MAXMBOX || mailboxes[mbox_id].status != ACTIVE){
        return -1;
    }
    if(msg_size < 0 || msg_size > MAX_MESSAGE){
        return -1;
    }
    if(msg_size > 0 && msg_ptr == NULL){
        return -1;
    }
    //IF we have a reciver that is waiting, We can send the message
    //Else we need to block the sender
    //What if The proccess is the sender and the reciver? We can not block it per say
    if(mailboxes[mbox_id].recievers.size > 0){
        ShadowProc * shadowProc = Queue_pop(&(mailboxes[mbox_id].recievers));
        //USLOSS_Console("SendHelper(): messageSize = %d,  Message: %s\n", msg_size, msg_ptr);
        strncpy(shadowProc->mailSlot->message, msg_ptr, msg_size);
        // USLOSS_Console("SendHelper(): messageSize = %d,  Message: %s\n", shadowProc->mailSlot->messageSize, shadowProc->mailSlot->message);
        shadowProc->mailSlot->messageSize = msg_size;
        shadowProc->msg_ptr = msg_ptr;
        shadowProc->msg_size = msg_size;

        mailboxes[mbox_id].currentNumOfSlots--;
        // USLOSS_Console("SendHelper(): Current Mail Slots: %d\n", mailboxes[mbox_id].currentNumOfSlots);
        unblockProc(shadowProc->pid);
        return 0;
    }
    else{
        int mailSlotID = createMailSlot(mbox_id, msg_ptr, msg_size);
        while(mailSlotID == -1){
            // USLOSS_Console("SendHelper(): Could not create mail slot\n");
            if(!canBlock){
                return -2;
            }
            blockMe(MAILBLOCK);
            mailSlotID = createMailSlot(mbox_id, msg_ptr, msg_size);  
        }
        mailboxes[mbox_id].currentNumOfSlots++;
        MailSlot * mailSlot = &mailSlots[mailSlotID];
        int shadowProcID = createShadowProc(getpid(), msg_ptr, msg_size);
        if(shadowProcID == -1){
            return -1;
        }
        ShadowProc * shadowProc = &shadowProcs[shadowProcID];
        shadowProc->mailSlot = mailSlot;
        // strncpy(shadowProc->mailSlot->message, msg_ptr, msg_size);
        shadowProc->msg_ptr = msg_ptr;
        shadowProc->msg_size = msg_size;
        Queue_push(&(mailboxes[mbox_id].senders), shadowProc);
        return 0;
    }
}


int MboxSend(int mbox_id, void *msg_ptr, int msg_size){
  return SendHelper(mbox_id, msg_ptr, msg_size, CANBLOCK);
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){
    return SendHelper(mbox_id, msg_ptr, msg_size, CANTBLOCK);
}


int RecvHelper(int mbox_id, void *msg_ptr, int msg_size, int canBlock){
    //IS mailbox release?
    if(mailboxes[mbox_id].status == RELEASE){
        return RELEASE;
    }
    //ERROR CHECKING
    if(mbox_id < 0 || mbox_id >= MAXMBOX || mailboxes[mbox_id].status != ACTIVE){
        return -1;
    }
    if(msg_size < 0 || msg_size > MAX_MESSAGE){
        return -1;
    }
        if(msg_size > 0 && msg_ptr == NULL){
        return -1;
    }
    //IF we have a message waiting, we can recieve it
    //Else we need to block the reciver until a message is sent
    //NOte we have 0 slot mail boxes and 0 length messages
    if(mailboxes[mbox_id].senders.size > 0){
        ShadowProc * shadowProc = Queue_pop(&(mailboxes[mbox_id].senders));
        // MailSlot * mailSlot = Queue_pop(&(mailboxes[mbox_id].slots));
        strncpy(msg_ptr, shadowProc->mailSlot->message, shadowProc->mailSlot->messageSize);
        shadowProc->mailSlot->status = EMPTY;
        int temp_size = shadowProc->mailSlot->messageSize;
        shadowProc->mailSlot->messageSize = -1;
        shadowProc->mailSlot->message[0] = '\0';
        shadowProc->msg_ptr = NULL;
        shadowProc->msg_size = -1;

        mailboxes[mbox_id].currentNumOfSlots--;
        // USLOSS_Console("RECVHelper(): Current Mail slots %d\n", mailboxes[mbox_id].currentNumOfSlots);
        unblockProc(shadowProc->pid);
        shadowProc->pid = -1;
        //Queue_push(&(mailboxes[mbox_id].recievers), shadowProc);
        return temp_size;
    }
    else{
        int shadowProcID = createShadowProc(getpid(), &msg_ptr, msg_size);
        if(shadowProcID == -1){
            USLOSS_Console("RecvHelper(): Could not create shadow process\n");
            return -1;
        }
        ShadowProc * shadowProc = &shadowProcs[shadowProcID];
        int mailSlotID = createMailSlot(mbox_id, msg_ptr, msg_size);
        while(mailSlotID == -1){
            USLOSS_Console("RecvHelper(): Could not create mail slot\n");
            if(!canBlock){
                return -2;
            }
            blockMe(MAILBLOCK);
            mailSlotID = createMailSlot(mbox_id, msg_ptr, msg_size);
        }
        mailboxes[mbox_id].currentNumOfSlots++;
        shadowProc->mailSlot = &mailSlots[mailSlotID];
        Queue_push(&(mailboxes[mbox_id].recievers), &shadowProcs[shadowProcID]);
        if(!canBlock){
            return -2;
        }
        blockMe(MAILBLOCK);
        strncpy(msg_ptr, shadowProc->mailSlot->message, shadowProc->mailSlot->messageSize);
        return shadowProc->mailSlot->messageSize;
    }

}


int MboxRecv(int mbox_id, void *msg_ptr, int msg_size){
    return RecvHelper(mbox_id, msg_ptr, msg_size, CANBLOCK);
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_size){
    return RecvHelper(mbox_id, msg_ptr, msg_size, CANTBLOCK);
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

void* Queue_pop(Queue * queue){
    if(queue->size == 0){
        return;
    }
    if(queue->type == 0){
        MailSlot * mailSlot = (MailSlot *) queue->head;
        queue->head = mailSlot->next;
        mailSlot->next = NULL;
        queue->size--;
        return mailSlot;
    }
    else{
        ShadowProc * shadowProc = (ShadowProc *) queue->head;
        queue->head = shadowProc->next;
        shadowProc->next = NULL;
        queue->size--;
        return shadowProc;
    }
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


//---------------------------Mr Clean---------------------------//
void cleanProc(int index){
    shadowProcs[index].pid = -1;
    shadowProcs[index].msg_ptr = NULL;
    shadowProcs[index].msg_size = -1;
    shadowProcs[index].mailSlot = NULL;
    shadowProcs[index].next = NULL;
}

void cleanSlot(int index){
    mailboxes[mailSlots[index].mboxID].currentNumOfSlots--;
    mailSlots[index].mboxID = -1;
    mailSlots[index].status = EMPTY;
    mailSlots[index].messageSize = -1;
    mailSlots[index].message[0] = '\0';
    mailSlots[index].next = NULL;
}
