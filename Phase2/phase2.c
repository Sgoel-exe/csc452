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
    int totalSlots;
    int currentNumOfSlots;
    int status;
    Queue slots;
    Queue pros;
    Queue cons;
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
MailBox mailBoxes[MAXMBOX];
MailSlot mailSlots[MAXSLOTS];
ShadowProc conProcs[MAXPROC];
ShadowProc proProcs[MAXPROC];
int nextMBoxID = 0;  //Next available mailbox ID
int nextSlotID = 0;  //Next available slot ID

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

void phase2_init(void){

    //Initialize the mailboxes
    for(int i = 0; i < MAXMBOX; i++){
        mailBoxes[i].mboxID = -1;
        mailBoxes[i].slotSize = -1;
        mailBoxes[i].totalSlots = -1;
        mailBoxes[i].currentNumOfSlots = 0;
        mailBoxes[i].status = EMPTY;
        Queue_init(&mailBoxes[i].slots, 0);
        Queue_init(&mailBoxes[i].pros, 1);
        Queue_init(&mailBoxes[i].cons, 1);
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
        //Consumers
        conProcs[i].pid = -1;
        conProcs[i].index = i;
        conProcs[i].msg_ptr = NULL;
        conProcs[i].msg_size = -1;
        conProcs[i].mailSlot = NULL;
        conProcs[i].next = NULL;
        //Producers
        proProcs[i].pid = -1;
        proProcs[i].index = i;
        proProcs[i].msg_ptr = NULL;
        proProcs[i].msg_size = -1;
        proProcs[i].mailSlot = NULL;
        proProcs[i].next = NULL;
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
    if(nextMBoxID == MAXMBOX || mailBoxes[nextMBoxID].status != EMPTY){
        nextMBoxID = findNextMBoxID();
        if(nextMBoxID == -1){
            return -1;
        }
    }
    mailBoxes[nextMBoxID].mboxID = nextMBoxID;
    mailBoxes[nextMBoxID].slotSize = slot_size;
    mailBoxes[nextMBoxID].totalSlots = slots;
    mailBoxes[nextMBoxID].status = ACTIVE;

    nextMBoxID++;
     
    return nextMBoxID - 1;
}

int findNextMBoxID(){
    for(int i = 7; i < MAXMBOX; i++){
        if(mailBoxes[i].status == EMPTY){
             
            return i;
        }
    }
     
    return -1;
}

int createMailSlot(int mboxID, char * message, int messageSize){
    if(mailBoxes[mboxID].currentNumOfSlots == mailBoxes[mboxID].totalSlots){
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

int createShadowProc(int pid, void **msg_ptr, int msg_size, int whichProc){
    //Producers
    if(whichProc == 1){

        int index = pid % MAXPROC;
        if(proProcs[index].pid != -1){
            return -1; //Already exists
        }
        proProcs[index].pid = pid;
        proProcs[index].msg_ptr = msg_ptr;
        proProcs[index].msg_size = msg_size;
        proProcs[index].mailSlot = NULL;
        return index;
    }
    //Consumers
    else{
        int index = pid % MAXPROC;
        if(conProcs[index].pid != -1){
            return -1; //Already exists
        }
        conProcs[index].pid = pid;
        conProcs[index].msg_ptr = msg_ptr;
        conProcs[index].msg_size = msg_size;
        conProcs[index].mailSlot = NULL;
        return index;
    } //Success
}

int MboxRelease(int mbox_id){
    return 0;
}


int SendHelper(int mbox_id, void *msg_ptr, int msg_size, int canBlock){
    //IS mailbox release?
    if(mailBoxes[mbox_id].status == RELEASE){
        return RELEASE;
    }
    //Error checking
    if(mbox_id < 0 || mbox_id >= MAXMBOX || mailBoxes[mbox_id].status == INACTIVE){
        return -1;
    }
    if(msg_size < 0 || msg_size > MAX_MESSAGE || msg_size > mailBoxes[mbox_id].slotSize){
        return -1;
    }
    if(msg_size > 0 && msg_ptr == NULL){
        return -1;
    }
    if(nextSlotID == MAXSLOTS){
        return -2;
    }
    //IF we have a reciver that is waiting, We can send the message
    //Else we need to block the sender
    //What if The proccess is the sender and the reciver? We can not block it per say
    if(mailBoxes[mbox_id].cons.size > 0){
        ShadowProc * consumerProc = Queue_pop(&mailBoxes[mbox_id].cons);
        // MailSlot * mailSlot = Queue_pop(&mailBoxes[mbox_id].slots);
        // consumerProc->mailSlot = mailSlot;
        memcpy(consumerProc->msg_ptr, msg_ptr, msg_size);
        consumerProc->msg_size = msg_size;
        // mailBoxes[mbox_id].currentNumOfSlots--;
        unblockProc(consumerProc->pid);
        return 0;
    }
    if(mailBoxes[mbox_id].currentNumOfSlots == mailBoxes[mbox_id].totalSlots){
        if(!canBlock){
            return -2;
        }

        int result = createShadowProc(getpid(), msg_ptr, msg_size, 1);
        Queue_push(&mailBoxes[mbox_id].pros, &proProcs[result]);
        blockMe(MAILBLOCK);

        if(isZapped()){
            return -3;
        }

        return 0;
    }

    int mailSlotID = createMailSlot(mbox_id, msg_ptr, msg_size);
    Queue_push(&mailBoxes[mbox_id].slots, &mailSlots[mailSlotID]);
    return 0;
}


int MboxSend(int mbox_id, void *msg_ptr, int msg_size){
  return SendHelper(mbox_id, msg_ptr, msg_size, CANBLOCK);
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){
    return SendHelper(mbox_id, msg_ptr, msg_size, CANTBLOCK);
}


int RecvHelper(int mbox_id, void *msg_ptr, int msg_size, int canBlock){
    //IS mailbox release?
    if(mailBoxes[mbox_id].status == RELEASE){
        return RELEASE;
    }
    //ERROR CHECKING
    if(mbox_id < 0 || mbox_id >= MAXMBOX || mailBoxes[mbox_id].status == INACTIVE){
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
    
    //0 slot mailboxes
    if(mailBoxes[mbox_id].totalSlots == 0){
         int index = createShadowProc(getpid(), msg_ptr, msg_size, 0);
         
         if(mailBoxes[mbox_id].pros.size > 0){
            ShadowProc * producer = Queue_pop(&mailBoxes[mbox_id].pros);
            memcpy(conProcs[index].msg_ptr, producer->msg_ptr, producer->msg_size);
            conProcs[index].msg_size = producer->msg_size;
            unblockProc(producer->pid);
         }
         else if (canBlock == CANBLOCK){
            Queue_push(&mailBoxes[mbox_id].cons, &conProcs[index]);
            blockMe(MAILBLOCK);

            if(isZapped()){
                return -3;
            }
         }
         
         return conProcs[index].msg_size;
    }
    MailSlot * mailSlot;
    if(mailBoxes[mbox_id].slots.size == 0){
        int index = createShadowProc(getpid(), msg_ptr, msg_size, 0);

        if(mailBoxes[mbox_id].totalSlots == 0 && mailBoxes[mbox_id].pros.size > 0){
            ShadowProc * producer = Queue_pop(&mailBoxes[mbox_id].pros);
            memcpy(conProcs[index].msg_ptr, producer->msg_ptr, producer->msg_size);
            conProcs[index].msg_size = producer->msg_size;
            unblockProc(producer->pid);
            return conProcs[index].msg_size;
        }
        if (!canBlock){
            return -2;
        }

        Queue_push(&mailBoxes[mbox_id].cons, &conProcs[index]);
        blockMe(MAILBLOCK);

        if(isZapped()){
            return -3;
        }

        return conProcs[index].msg_size;
    }
    else{
        mailSlot = Queue_pop(&mailBoxes[mbox_id].slots);
    }
    if(mailSlot == 0 || mailSlot->status == EMPTY || msg_size < mailSlot->messageSize){
        return -1;
    }

    int size = mailSlot->messageSize;
    memcpy(msg_ptr, mailSlot->message, size);
    cleanSlot(mailSlot->mboxID);

    if(mailBoxes[mbox_id].pros.size > 0){
        ShadowProc * producer = Queue_pop(&mailBoxes[mbox_id].pros);

        int i = createMailSlot(mbox_id, producer->msg_ptr, producer->msg_size);
        Queue_push(&mailBoxes[mbox_id].slots, &mailSlots[i]);
        unblockProc(producer->pid);
    }

    return size;
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
// void cleanProc(int index){
//     shadowProcs[index].pid = -1;
//     shadowProcs[index].msg_ptr = NULL;
//     shadowProcs[index].msg_size = -1;
//     shadowProcs[index].mailSlot = NULL;
//     shadowProcs[index].next = NULL;
// }

void cleanSlot(int index){
    mailBoxes[mailSlots[index].mboxID].currentNumOfSlots--;
    mailSlots[index].mboxID = -1;
    mailSlots[index].status = EMPTY;
    mailSlots[index].messageSize = -1;
    mailSlots[index].message[0] = '\0';
    mailSlots[index].next = NULL;
}
