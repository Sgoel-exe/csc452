/*
* Authors: 	Shrey Goel, Muhtasim Al-Farabi
* Class: 	CS 452
* File: 	phase2.c
* Project: 	Phase 2
* @ Description: This file contains the implementation of the phase 2 project. It contains the implementation of the following functions:
*               MboxCreate, MboxRelease, MboxSend, MboxCondSend, MboxRecv, MboxCondRecv, waitDevice, diskHandler, termHandler, syscallHandler,
                clockHandler, phase2_clockHandler, phase2_check_io.
                We Simulate a mailbox system for IPC (InterProcessCommunication). We use a queue to store the mailslots and shadow processes.
                This helps with the implementation of the blocking and unblocking of processes.
                We also have IO handlers for the clock, disk, and terminal. These handlers are used to unblock processes that are blocked on IO.
                We also have a syscall handler that handles the system calls. Currently system calls are not really processed fully (see NULLSYS in spec).
*/

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
#define CLOCK 0
#define DISK 1
#define TERM 3
typedef struct MailBox MailBox;
typedef struct MailSlot MailSlot;
typedef struct ShadowProc ShadowProc;

//------Queue------//
typedef struct Queue
{
    int size;
    void *head;
    void *tail;
    int type; // 0 = MailSlot, 1 = ShadowProc
} Queue;

//------The Mail Box------//
typedef struct MailBox
{
    int mboxID;
    int slotSize;   // Size of Max message that a slot can have
    int totalSlots; // Total number of slots in the mailbox
    int currentNumOfSlots;
    int status;  // 1 = Active, -3 = Release, -1 = Inactive
    Queue slots; // Queue of mailslots
    Queue pros;  // Queue of producers
    Queue cons;  // Queue of consumers
} MailBox;

//------The Mail Slot------//
typedef struct MailSlot
{
    int mboxID;
    int status;
    char message[MAX_MESSAGE];
    int messageSize;
    MailSlot *next;
} MailSlot;

//------The Process------//
typedef struct ShadowProc
{
    int pid;
    int index;
    void *msg_ptr;
    int msg_size;
    MailSlot *mailSlot;
    ShadowProc *next;
} ShadowProc;

//------Function Prototypes------//
void nullsys(USLOSS_Sysargs *args);
void phase2_clockHandler(void);
void diskHandler(int dev, void *arg);
void termHandler(int dev, void *arg);
void syscallHandler(int dev, void *arg);
void clockHandler(int dev, void *arg);
void Queue_init(Queue *queue, int type);
void Queue_push(Queue *queue, void *item);
void *Queue_pop(Queue *queue);
void *Queue_front(Queue *queue);
int findNextMBoxID();
int findNextSlotID();
int createMailSlot(int mboxID, char *message, int messageSize);
void cleanSlot(int index);
void cleanProc(int index, int whichProc);
void cleanMail(int index);
//------Global Variables------//
MailBox mailBoxes[MAXMBOX];
MailSlot mailSlots[MAXSLOTS];
ShadowProc conProcs[MAXPROC];
ShadowProc proProcs[MAXPROC];
int nextMBoxID = 0; // Next available mailbox ID
int nextSlotID = 0; // Next available slot ID
int globalSlotUsed = 0;
int io = 0;

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

/**
* @brief: This function initializes the mailboxes, mailslots, shadow processes, and system call vector.
                  Also sets up the interrupt handlers.
* @param: None
* @return: None
*/
void phase2_init(void)
{

    // Initialize the mailboxes
    for (int i = 0; i < MAXMBOX; i++)
    {
        mailBoxes[i].mboxID = -1;
        mailBoxes[i].slotSize = -1;
        mailBoxes[i].totalSlots = -1;
        mailBoxes[i].currentNumOfSlots = 0;
        mailBoxes[i].status = EMPTY;
        Queue_init(&mailBoxes[i].slots, 0);
        Queue_init(&mailBoxes[i].pros, 1);
        Queue_init(&mailBoxes[i].cons, 1);
    }

    // Initialize the mailslots
    for (int i = 0; i < MAXSLOTS; i++)
    {
        mailSlots[i].mboxID = -1;
        mailSlots[i].status = EMPTY;
        mailSlots[i].messageSize = -1;
        mailSlots[i].message[0] = '\0';
        mailSlots[i].next = NULL;
    }

    // Initialize the shadow processes
    for (int i = 0; i < MAXPROC; i++)
    {
        // Consumers
        conProcs[i].pid = -1;
        conProcs[i].index = i;
        conProcs[i].msg_ptr = NULL;
        conProcs[i].msg_size = -1;
        conProcs[i].mailSlot = NULL;
        conProcs[i].next = NULL;
        // Producers
        proProcs[i].pid = -1;
        proProcs[i].index = i;
        proProcs[i].msg_ptr = NULL;
        proProcs[i].msg_size = -1;
        proProcs[i].mailSlot = NULL;
        proProcs[i].next = NULL;
    }

    // Initialize the system call vector
    for (int i = 0; i < MAXSYSCALLS; i++)
    {
        systemCallVec[i] = nullsys;
    }

    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;

    // Create IO mailboxes // Fix Copilot code
    for (int i = 0; i < 7; i++)
    {
        MboxCreate(0, sizeof(int));
        // 0-7 interrupt handlres
    }
}

/**
 * @brief : This function is called when a process calls an unimplemented system call.
 * @parame: USLOSS_Sysargs *args - The system arguments
 * @return: None
 */
void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%.2x\n", args->number, USLOSS_PsrGet());
    USLOSS_Halt(1);
}

/**
 * @brief This is the wrapper for the start2 function. It is called by the startup code.
 */
void phase2_start_service_processes(void)
{
}

/**
 * @brief This is the fucntion that creates mailboxes. It takes in the number of slots and the size of the slots.
 *          It tries to find the next available mailbox ID. If it cannot find one, it returns -1. Otherwise, it creates
 *         the mailbox and returns the mailbox ID.
 * @param slots - The number of slots in the mailbox
 * @param slot_size - The size of the slots in the mailbox
 * @return -1 if the mailbox could not be created, the mailbox ID otherwise
 */
int MboxCreate(int slots, int slot_size)
{
    // Error checks
    if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || slot_size > MAX_MESSAGE)
    {
        return -1;
    }
    // Index is the mailbox ID
    if (nextMBoxID == MAXMBOX || mailBoxes[nextMBoxID].status != EMPTY)
    {
        nextMBoxID = findNextMBoxID();
        if (nextMBoxID == -1)
        {
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

/**
 * @brief Tries to find the next available mailbox ID. If it cannot find one, it returns -1. Otherwise, it returns the mailbox ID.
 * @param None
 * @return -1 if the mailbox could not be created, the mailbox ID otherwise
*/

int findNextMBoxID()
{
    for (int i = 7; i < MAXMBOX; i++)
    {
        if (mailBoxes[i].status == EMPTY)
        {

            return i;
        }
    }

    return -1;
}

/**
 * @brief This function creates a mailslot. It takes in the mailbox ID, the message, and the size of the message.
 *          It tries to find the next available slot ID. If it cannot find one, it returns -1. Otherwise, it creates
 *        the mailslot and returns the slot ID.
 * @param mboxID - The mailbox ID
 * @param message - The message to be sent
 * @param messageSize - The size of the message
 * 
 * @return -1 if the mailslot could not be created, -5 if there are no available slots, the slot ID otherwise
*/

int createMailSlot(int mboxID, char *message, int messageSize)
{
    // Error checks
    if (mailBoxes[mboxID].currentNumOfSlots == mailBoxes[mboxID].totalSlots)
    {
        return -1;
    }
    if (nextSlotID == MAXSLOTS || mailSlots[nextSlotID].status != EMPTY)
    {
        nextSlotID = findNextSlotID();
        if (nextSlotID == -1)
        {

            return -5;
        }
    }
    if (messageSize > mailBoxes[mboxID].slotSize)
    {
        return -1;
    }
    // Create the mailslot
    mailSlots[nextSlotID].mboxID = mboxID;
    mailSlots[nextSlotID].status = ACTIVE;
    mailSlots[nextSlotID].messageSize = messageSize;
    strncpy(mailSlots[nextSlotID].message, message, messageSize);
    mailSlots[nextSlotID].next = NULL;
    nextSlotID++;
    return nextSlotID - 1;
}

/**
 * @brief This function finds the next available slot ID. If it cannot find one, it returns -1. Otherwise, it returns the slot ID.
 * @param None
 * @return -1 if the mailslot could not be created, the slot ID otherwise
*/

int findNextSlotID()
{
    for (int i = 0; i < MAXSLOTS; i++)
    {
        if (mailSlots[i].status == EMPTY)
        {

            return i;
        }
    }

    return -1;
}

/**
 * @brief This function creates a shawdow process. It takes in the process ID, the message, the size of the message, and which process it is.
 * @param pid - The process ID
 * @param msg_ptr - The message to be sent
 * @param msg_size - The size of the message
 * @param whichProc - 1 if it is a producer, 0 if it is a consumer
 * @return -1 if the shadow process could not be created, the index of the shadow process otherwise
*/

int createShadowProc(int pid, void **msg_ptr, int msg_size, int whichProc)
{
    // Producers
    if (whichProc == 1)
    {

        int index = pid % MAXPROC;
        proProcs[index].pid = pid;
        proProcs[index].msg_ptr = msg_ptr;
        proProcs[index].msg_size = msg_size;
        proProcs[index].mailSlot = NULL;
        return index;
    }
    // Consumers
    else
    {
        int index = pid % MAXPROC;
        conProcs[index].pid = pid;
        conProcs[index].msg_ptr = msg_ptr;
        conProcs[index].msg_size = msg_size;
        conProcs[index].mailSlot = NULL;
        return index;
    } // Success
}
/**
 * @brief This function releases a mailslot. It takes in the slot ID and sets the status of the mailslot to RELEASE,
 *      unblocks the processes that are blocked on the mailslot, and cleans the mailslot.
 * @param index - The slot ID
 * @return -1 if the mailslot could not be released, 0 otherwise
*/
int MboxRelease(int mbox_id)
{
    // Error checking
    if (mbox_id < 0 || mbox_id >= MAXMBOX || mailBoxes[mbox_id].status == INACTIVE)
    {
        return -1;
    }
    // Release the mailbox
    while (mailBoxes[mbox_id].slots.size > 0)
    {
        MailSlot *mailSlot = Queue_pop(&mailBoxes[mbox_id].slots);
        mailBoxes[mbox_id].currentNumOfSlots--;
        globalSlotUsed--;
        cleanSlot(mailSlot->mboxID);
    }
    mailBoxes[mbox_id].status = RELEASE;
    
    // Release the processes
    while (mailBoxes[mbox_id].cons.size > 0)
    {
        ShadowProc *consumerProc = Queue_pop(&mailBoxes[mbox_id].cons);
        unblockProc(consumerProc->pid);
        cleanProc(consumerProc->index, 0);
    }
    while (mailBoxes[mbox_id].pros.size > 0)
    {
        ShadowProc *producerProc = Queue_pop(&mailBoxes[mbox_id].pros);
        unblockProc(producerProc->pid);
        cleanProc(producerProc->index, 1);
    }
    cleanMail(mbox_id);
    return 0;
}

/**
 * @brief This function is called in both MboxSend and MboxCondSend. It takes in the mailbox ID, the message, 
 *          the size of the message, and whether or not it can block. First, it handles the errors such as 
 *          invalid mailbox ID, invalid message size, and invalid message pointer. Then, it checks if there are
 *         any consumers waiting on the mailbox. If there are, it unblocks the consumer and returns 0, else it 
 *          will block the producer if it can block. If it cannot block, it returns -2. Then it pushes to the
 *          producer queue.
 * 
 * @param mbox_id - The mailbox ID
 * @param msg_ptr - The message to be sent
 * @param msg_size - The size of the message
 * @param canBlock - 1 if it can block, 0 if it cannot block
 * 
 * @return -1 if error, -2 if it cannot block or out of slots, -3 if it is zapped or the mailbox is inactive/released, 0 otherwise
 *          
 *          
*/

int SendHelper(int mbox_id, void *msg_ptr, int msg_size, int canBlock)
{
    // Error checking
    if (mbox_id < 0 || mbox_id >= MAXMBOX || mailBoxes[mbox_id].status == INACTIVE)
    {
        return -1;
    }
    if (msg_size < 0 || msg_size > MAX_MESSAGE || msg_size > mailBoxes[mbox_id].slotSize)
    {
        return -1;
    }
    if (msg_size > 0 && msg_ptr == NULL)
    {
        return -1;
    }
    if (globalSlotUsed == MAXSLOTS)
    {
        return -2;
    }
    // If there are consumers waiting and zero slot mailbox or there are slots available
    if (mailBoxes[mbox_id].cons.size > 0 && (mailBoxes[mbox_id].totalSlots == 0 || mailBoxes[mbox_id].currentNumOfSlots < mailBoxes[mbox_id].totalSlots))
    {
        ShadowProc *consumerProc = Queue_pop(&mailBoxes[mbox_id].cons);
        memcpy(consumerProc->msg_ptr, msg_ptr, msg_size);
        consumerProc->msg_size = msg_size;
        unblockProc(consumerProc->pid);
        return 0;
    }
    // If there are no consumers waiting and no slots available
    if (mailBoxes[mbox_id].currentNumOfSlots == mailBoxes[mbox_id].totalSlots)
    {
        if (canBlock == CANTBLOCK)
        {
            return -2;
        }

        int result = createShadowProc(getpid(), msg_ptr, msg_size, 1);
        Queue_push(&mailBoxes[mbox_id].pros, &proProcs[result]);
        blockMe(MAILBLOCK);

        if (isZapped() || mailBoxes[mbox_id].status != ACTIVE)
        {
            return -3;
        }

        return 0;
    }

    int mailSlotID = createMailSlot(mbox_id, msg_ptr, msg_size);
    mailBoxes[mbox_id].currentNumOfSlots++;
    globalSlotUsed++;
    Queue_push(&mailBoxes[mbox_id].slots, &mailSlots[mailSlotID]);
    return 0;
}

/**
 * @brief This function calls the SendHelper function with the canBlock parameter set to 1.
 * @param mbox_id - The mailbox ID
 * @param msg_ptr - The message to be sent
 * @param msg_size - The size of the message
 * 
 * @return returns what the SendHelper function returns
*/

int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    return SendHelper(mbox_id, msg_ptr, msg_size, CANBLOCK);
}

/**
 * @brief This function calls the SendHelper function with the canBlock parameter set to 0. It is supposed to be non-blocking.
 * @param mbox_id - The mailbox ID
 * @param msg_ptr - The message to be sent
 * @param msg_size - The size of the message
 * 
 * @return returns what the SendHelper function returns
*/
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    return SendHelper(mbox_id, msg_ptr, msg_size, CANTBLOCK);
}

/**
 * @@brief This function is called in both MboxRecv and MboxCondRecv. It takes in the mailbox ID, the message,
 *         the size of the message, and whether or not it can block. First, it handles the errors such as   
 *        invalid mailbox ID, invalid message size, and invalid message pointer. Then, it checks if there are
 *        any producers waiting on the mailbox. If there are, it unblocks the producer and returns the size of the message,
 *       else it will block the consumer if it can block. If it cannot block, it returns -2. Them it pushed to the consumer queue.
 * 
 * @param mbox_id - The mailbox ID
 * @param msg_ptr - The message to be sent
 * @param msg_size - The size of the message
 * @param canBlock - 1 if it can block, 0 if it cannot block
 * 
 * @return -1 if error, -2 if it cannot block or out of slots, -3 if it is zapped or the mailbox is inactive/released, the size of the message otherwise
*/

int RecvHelper(int mbox_id, void *msg_ptr, int msg_size, int canBlock)
{
    // ERROR CHECKING
    if (mbox_id < 0 || mbox_id >= MAXMBOX || mailBoxes[mbox_id].status == INACTIVE)
    {
        return -1;
    }
    if (msg_size < 0 || msg_size > MAX_MESSAGE)
    {
        return -1;
    }
    if (msg_size > 0 && msg_ptr == NULL)
    {
        return -1;
    }
    // 0 slot mailboxes
    if (mailBoxes[mbox_id].totalSlots == 0)
    {
        int index = createShadowProc(getpid(), msg_ptr, msg_size, 0);
        // If there are producers waiting
        if (mailBoxes[mbox_id].pros.size > 0)
        {
            ShadowProc *producer = Queue_pop(&mailBoxes[mbox_id].pros);
            memcpy(conProcs[index].msg_ptr, producer->msg_ptr, producer->msg_size);
            conProcs[index].msg_size = producer->msg_size;
            unblockProc(producer->pid);
        }
        // non-blocking
        else if (canBlock == CANBLOCK)
        {
            Queue_push(&mailBoxes[mbox_id].cons, &conProcs[index]);
            blockMe(MAILBLOCK);

            if (isZapped() || mailBoxes[mbox_id].status != ACTIVE)
            {
                return -3;
            }
        }

        return conProcs[index].msg_size;
    }
    MailSlot *mailSlot;
    // no slots have been created yet
    if (mailBoxes[mbox_id].slots.size == 0)
    {
        int index = createShadowProc(getpid(), msg_ptr, msg_size, 0);

        if (mailBoxes[mbox_id].totalSlots == 0 && mailBoxes[mbox_id].pros.size > 0)
        {
            ShadowProc *producer = Queue_pop(&mailBoxes[mbox_id].pros);
            memcpy(conProcs[index].msg_ptr, producer->msg_ptr, producer->msg_size);
            conProcs[index].msg_size = producer->msg_size;
            unblockProc(producer->pid);
            return conProcs[index].msg_size;
        }
        // non-blocking
        
        if (canBlock == CANTBLOCK)
        {
            if (mailBoxes[mbox_id].status != ACTIVE)
            {
                return -1;
            }
            return -2;
        }

        Queue_push(&mailBoxes[mbox_id].cons, &conProcs[index]);
        blockMe(MAILBLOCK);
        if (conProcs[index].msg_size > msg_size)
        {
            return -1;
        }
        if (isZapped() || mailBoxes[mbox_id].status != ACTIVE)
        {
            return -3;
        }

        return conProcs[index].msg_size;
    }
    // slots available
    else
    {
        mailSlot = Queue_pop(&mailBoxes[mbox_id].slots);
        mailBoxes[mbox_id].currentNumOfSlots--;
        globalSlotUsed--;
    }
    if (mailSlot == NULL || mailSlot->status == EMPTY || msg_size < mailSlot->messageSize)
    {
        return -1;
    }
    int size = mailSlot->messageSize;
    memcpy(msg_ptr, mailSlot->message, size);
    mailSlot->status = EMPTY;
    // If there are producers waiting
    if (mailBoxes[mbox_id].pros.size > 0)
    {
        ShadowProc *producer = Queue_pop(&mailBoxes[mbox_id].pros);
        int i = createMailSlot(mbox_id, producer->msg_ptr, producer->msg_size);
        mailBoxes[mbox_id].currentNumOfSlots++;
        globalSlotUsed++;
        Queue_push(&mailBoxes[mbox_id].slots, &mailSlots[i]);
        unblockProc(producer->pid);
    }
    return size;
}

/**
 * @brief This function calls the RecvHelper function with the canBlock parameter set to 1.
 * @param mbox_id - The mailbox ID
 * @param msg_ptr - The message to be received
 * @param msg_size - The size of the message that can be received
 * 
 * @return returns what the RecvHelper function returns
*/

int MboxRecv(int mbox_id, void *msg_ptr, int msg_size)
{
    return RecvHelper(mbox_id, msg_ptr, msg_size, CANBLOCK);
}

/**
 * @brief This function calls the RecvHelper function with the canBlock parameter set to 0. Non-blocking.
 * @param mbox_id - The mailbox ID
 * @param msg_ptr - The message to be received
 * @param msg_size - The size of the message that can be received
 * 
 * @return returns what the RecvHelper function returns
*/

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_size)
{
    return RecvHelper(mbox_id, msg_ptr, msg_size, CANTBLOCK);
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
    if (type == 0)
    {
        queue->head = (MailSlot *)queue->head;
        queue->tail = (MailSlot *)queue->tail;
    }
    else
    {
        queue->head = (ShadowProc *)queue->head;
        queue->tail = (ShadowProc *)queue->tail;
    }
    queue->type = type;
}

/**
 * @brief This function pushes an item to the queue. It takes in the queue and the item to be pushed.
 * @param queue - The queue to be pushed to
 * @param item - The item to be pushed
 * @return None
*/

void Queue_push(Queue *queue, void *item)
{   // MailSlot
    if (queue->type == 0)
    {
        MailSlot *mailSlot = (MailSlot *)item;
        if (queue->size == 0)
        {
            queue->head = mailSlot;
            queue->tail = mailSlot;
        }
        else
        {
            ((MailSlot *)queue->tail)->next = mailSlot;
            queue->tail = mailSlot;
        }
    }
    // ShadowProc
    else
    {
        ShadowProc *shadowProc = (ShadowProc *)item;
        if (queue->size == 0)
        {
            queue->head = shadowProc;
            queue->tail = shadowProc;
        }
        else
        {
            ((ShadowProc *)queue->tail)->next = shadowProc;
            queue->tail = shadowProc;
        }
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

void *Queue_pop(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL;
    }
    // MailSlot
    if (queue->type == 0)
    {
        MailSlot *mailSlot = (MailSlot *)queue->head;
        queue->head = mailSlot->next;
        mailSlot->next = NULL;
        queue->size--;
        return mailSlot;
    }
    // ShadowProc
    else
    {
        ShadowProc *shadowProc = (ShadowProc *)queue->head;
        queue->head = shadowProc->next;
        shadowProc->next = NULL;
        queue->size--;
        return shadowProc;
    }
}

/**
 * @brief This function returns the front of the queue. It takes in the queue and returns the front of the queue.
 * @param queue - The queue to be returned
 * @return The front of the queue
 * 
*/

void *Queue_front(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL;
    }
    if (queue->type == 0)
    {
        return (MailSlot *)queue->head;
    }
    else
    {
        return (ShadowProc *)queue->head;
    }
}

//---------------------------HANDLERS---------------------------//
/**
 * @brief This function waits for a device. It takes in the type of device, the unit of the device, and the status of the device.
 *        Receives the status of the device and stores it in the status parameter.
 * @param type - The type of device
 * @param unit - The unit of the device
 * @param status - The status of the device
 * @return None
*/
void waitDevice(int type, int unit, int *status)
{
    int res;

    if (type == USLOSS_CLOCK_DEV)
    {
        res = CLOCK;
        if (unit != 0)
        {
            USLOSS_Console("waitDevice(): Invalid unit %d for clock device. Halting...\n", unit);
            USLOSS_Halt(1);
        }
    }
    else if (type == USLOSS_DISK_DEV)
    {
        res = DISK;
        if (unit < 0 || unit >= 2)
        {
            USLOSS_Console("waitDevice(): Invalid unit %d for disk device. Halting...\n", unit);
            USLOSS_Halt(1);
        }
    }
    else if (type == USLOSS_TERM_DEV)
    {
        res = TERM;
        if (unit < 0 || unit >= 4)
        {
            USLOSS_Console("waitDevice(): Invalid unit %d for terminal device. Halting...\n", unit);
            USLOSS_Halt(1);
        }
    }
    else
    {
        USLOSS_Console("waitDevice(): Invalid type %d. Halting...\n", type);
        USLOSS_Halt(1);
    }

    io++;
    MboxRecv(res + unit, status, 0);
    io--;
}

/**
 * @brief This function handles the disk. It takes in the device and the argument.
 *      It calls the USLOSS_DeviceInput function to get the status of the device.
 *      Does a non-blocking send to the DISK mailbox.
 * 
 * @param dev - The device
 * @param arg - The argument
 * @return None
*/
void diskHandler(int dev, void *arg)
{
    if (dev != USLOSS_DISK_DEV)
    {
        USLOSS_Console("diskHandler(): called with invalid device %d.\n", dev);
        return;
    }
    long unit = (long)arg;
    int status;
    int valid = USLOSS_DeviceInput(dev, unit, &status);
    if (valid == USLOSS_DEV_INVALID)
    {
        USLOSS_Console("diskHandler(): Invalid input from device %d, unit %d.\n", dev, unit);
        USLOSS_Halt(1);
    }

    MboxCondSend(DISK + unit, &status, sizeof(int));
}

/**
 * @brief This function handles the ternimal. It takes in the device and the argument.
 *      It calls the USLOSS_DeviceInput function to get the status of the device.
 *      Does a non-blocking send to the TERMINAL mailbox.
 * 
 * @param dev - The device
 * @param arg - The argument
 * @return None
*/

void termHandler(int dev, void *arg)
{
    if (dev != USLOSS_TERM_DEV)
    {
        USLOSS_Console("termHandler(): called with invalid device %d.\n", dev);
        return;
    }

    long unit = (long)arg;
    int status;
    int valid = USLOSS_DeviceInput(dev, unit, &status);
    if (valid == USLOSS_DEV_INVALID)
    {
        USLOSS_Console("diskHandler(): Invalid input from device %d, unit %d. Halting...\n", dev, unit);
        USLOSS_Halt(1);
    }

    MboxCondSend(TERM + unit, &status, sizeof(int));
}

/**
 * @brief This function handles the system calls. It takes in the device and the argument.
 *       It calls the right system call based on the system call number using the systemCallVec.
 * 
 * @param dev - The device
 * @param arg - The argument
 * @return None
*/

void syscallHandler(int dev, void *arg)
{
    if (dev != USLOSS_SYSCALL_INT)
    {
        USLOSS_Console("syscallHandler(): called with invalid interrupt %d.\n", dev);
        return;
    }
    USLOSS_Sysargs *args = (USLOSS_Sysargs *)arg;
    if (args->number < 0 || args->number >= MAXSYSCALLS)
    {
        USLOSS_Console("syscallHandler(): Invalid syscall number %d\n", args->number);
        USLOSS_Halt(1);
    }
    systemCallVec[args->number](args);
}

/**
 * @brief This function handles the clock. It takes in the device and the argument.
 *       It calls phase2 clockHandler if the time interval is greater than 90,000.
 * 
 * @param dev - The device
 * @param arg - The argument
*/

void clockHandler(int dev, void *arg)
{
    if (dev != USLOSS_CLOCK_DEV)
    {
        USLOSS_Console("clockHandler(): called with invalid device %d.\n", dev);
        return;
    }
    if (currentTime() - readCurStartTime() >= 100000)
    {
        phase2_clockHandler();
    }
}

/**
 * @brief This function handles the clock. It takes in the device and the argument.
 *      It calls the USLOSS_DeviceInput function to get the status of the device.
 *      Does a non-blocking send to the CLOCK mailbox.
*/

void phase2_clockHandler(void)
{
    int status;
    int res = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status);
    res++;
    MboxCondSend(CLOCK, &status, sizeof(int));
}

/**
 * @brief This function returns the number of current devices blocked on IO
 * 
 * @return The number of current devices blocked on IO
*/

int phase2_check_io(void)
{
    return io;
}

//---------------------------Mr Clean---------------------------//

/**
 * @brief This function cleans the process. It takes in the index of the process and whether or not it is a producer or consumer.
 * 
 * @param index - The index of the process
 * @param whichProc - 1 if it is a producer, 0 if it is a consumer
 * 
 * @return None
 * 
*/
void cleanProc(int index, int whichProc)
{
    if (whichProc)
    {
        proProcs[index].pid = -1;
        proProcs[index].index = index;
        proProcs[index].msg_ptr = NULL;
        proProcs[index].msg_size = -1;
        proProcs[index].mailSlot = NULL;
        proProcs[index].next = NULL;
    }
    else
    {
        conProcs[index].pid = -1;
        conProcs[index].index = index;
        conProcs[index].msg_ptr = NULL;
        conProcs[index].msg_size = -1;
        conProcs[index].mailSlot = NULL;
        conProcs[index].next = NULL;
    }
}

/**
 * @brief This function cleans the mailslot. It takes in the index of the mailslot.
 * 
 * @param index - The index of the mailslot
 * 
 * @return None
*/

void cleanSlot(int index)
{
    mailBoxes[mailSlots[index].mboxID].currentNumOfSlots--;
    globalSlotUsed--;
    mailSlots[index].mboxID = -1;
    mailSlots[index].status = EMPTY;
    mailSlots[index].messageSize = -1;
    mailSlots[index].message[0] = '\0';
    mailSlots[index].next = NULL;
}

/**
 * @brief This function cleans the mailbox. It takes in the index of the mailbox.
 * 
 * @param index - The index of the mailbox
 * 
 * @return None
*/

void cleanMail(int index)
{
    mailBoxes[index].mboxID = -1;
    mailBoxes[index].slotSize = -1;
    mailBoxes[index].totalSlots = -1;
    mailBoxes[index].currentNumOfSlots = 0;
    mailBoxes[index].status = EMPTY;
    Queue_init(&mailBoxes[index].slots, 0);
    Queue_init(&mailBoxes[index].pros, 1);
    Queue_init(&mailBoxes[index].cons, 1);
}
