#include "usyscall.h"
#include "phase1.h"
#include "phase2.h"
#include <string.h>
#include <stdio.h>

// process table -- and pcb?
   // use % PID from phase 1 to map PIDs to appropriate slots

// Questions
  // how do we know when a mail box has consumed its maximum number of mail slots?
  // can we leave start_services_processes blank? It wasn't in the header file.
  // do we have to disable interrupts?
  // is it permissible to reuse pids?
  // what in mboxrelease should return -1 when the consumers are unblocked?
  // how to wake up blocked processes when releasing a mailbox?

// STRUCT DEFINITIONS
typedef struct node {
    void * thing;
    struct node * next;
} Node;

typedef struct pcb {
    int pid; // process id
} pcb;

typedef struct mslot {
    char msg[MAX_MESSAGE];
    int is_alive;
} mslot;

typedef struct mbox {
    int is_alive;
    int numSlots;
    int maxMsgSize;

    Node * mslots; // queue of mslot structs
    Node * consumers; // queue of consumer processes
    Node * producers; // queue of producer processes

} mbox;


// GLOBAL VARIABLES
static pcb proc_table[MAXPROC];
static mslot all_mslots[MAXSLOTS];
static mbox all_mboxes[MAXMBOX];
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
   

// PRODUCERS:
// block when they attempt to send a message, but the mailbox
// has already consumed its maximum number of mail slots
// must deliver their messages in the same order they arrived
// is it permissible to reuse pids?
// when releasing a mail box, how do we wake up the blocked processes?
// what are service processes? Why do we need to create them?

// Consumers block when there are no queued messages

// disable interrupts
unsigned int disable_interrupts() {
    
    // store the current psr
    unsigned int old_psr = USLOSS_PsrGet();

    // disable interrupts
    int psr_status = USLOSS_PsrSet(old_psr & ~USLOSS_PSR_CURRENT_INT);

    // TODO: use psr_status to check for errors?
    
    return old_psr;
}

// restore interrupts
void restore_interrupts(unsigned int old_psr) {

    // restore interrupts to their previous state
    int psr_status = USLOSS_PsrSet(old_psr);

    // TODO: use psr stsatus to check for errors?
}

// check for kernel mode
void check_kernel_mode(char * arg) {
    unsigned int psr = USLOSS_PsrGet();
    if ((psr & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in %s is being called from user mode\n", arg);
        USLOSS_Halt(1);
    }
}

static void nullsys(int syscallNum) {

    // check for kernel mode and disable interrupts
    check_kernel_mode("nullsys");
    unsigned int old_psr = disable_interrupts();

    // print out error message
    printf("nullsys(): Program called an unimplemented syscall.  syscall no: %d  PSR: %d\n", syscallNum, USLOSS_PsrGet());

    // halt simulation
    USLOSS_Halt(1);

    // restore interrupts
    restore_interrupts(old_psr);
}

void phase2_init() {

    // check kernel mode and disable interrupts
    check_kernel_mode("phase2_init");
    unsigned int old_psr = disable_interrupts();
    
    // memset all the arrays to 0
    for (int i=0; i < MAXPROC; i++) memset(&proc_table[i], 0, sizeof(struct pcb));
    for (int i=0; i < MAXSLOTS; i++) memset(&all_mslots[i], 0, sizeof(struct mslot));
    for (int i=0; i < MAXMBOX; i++) memset(&all_mboxes[i], 0, sizeof(struct mbox));

    // intialize array of function pointers

    

    // restore interrupts
    restore_interrupts(old_psr);
}

void phase2_start_service_processes() {

    // check kernel mode and disable interrupts
    check_kernel_mode("phase2_start_service_processes");
    unsigned int old_psr = disable_interrupts();

    // stuff here

    // restore interrupts
    restore_interrupts(old_psr);
}

int find_next_empty_mbox() {
    for (int i=0; i < MAXMBOX; i++) {
        if (!all_mboxes[i].is_alive) return i;
    } return -1;
}

int MboxCreate(int numSlots, int slotSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCreate");
    unsigned int old_psr = disable_interrupts();

    int mbox_id = find_next_empty_mbox();
    if (numSlots < 0 || slotSize < 0) return -1;

    if (mbox_id >= 0) {
        mbox mbox = all_mboxes[mbox_id];
        mbox.is_alive = 1;
        mbox.numSlots = numSlots;
        mbox.maxMsgSize = slotSize;
    }
    // deal with assigning tail pointer?

    // restore interrupts and return
    restore_interrupts(old_psr);
    return mbox_id;
}

int MboxRelease(int mboxID) {
    
    // check kernel mode and disable interrupts
    check_kernel_mode("MboxRelease");
    unsigned int old_psr = disable_interrupts();

    mbox mbox = all_mboxes[mboxID];
    if (!mbox.is_alive) return -1;

    // release mslots
    /*mslot * cur_mslot = mbox->mslots->head;*/
    /*while ((cur_mslot = cur_mslot->next)) cur_mslot->is_alive = 0;*/

    // set everything to 0
    memset(&mbox, 0, sizeof(struct mbox));


    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

int find_next_empty_mslot() {
    for (int i=0; i < MAXSLOTS; i++) {
        if (!all_mslots[i].is_alive) return i;
    } return -1;
}

// remove and return the tail -- list will always have at least one node
void* popTail(Node** head) {
    void* thing_toReturn = NULL;
    if (!(*head)->next) {
        // list with one node
        thing_toReturn = (*head)->thing;
        *head = NULL;
    } else {
        // list with more than one node
        Node * cur = *head;
        while (cur->next->next) { cur = cur->next; }

        thing_toReturn = cur->next->thing;
        cur->next = NULL;
    } return thing_toReturn;
}


// newer (fixed?) version
//
// there's a high likelyhood that some of these commented chunks will
// be copied into helper functions that will be used by this function 
// as well as mboxCondSend()
int MboxSendAlt(int mboxID, void *msg, int messageSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxSend");
    unsigned int old_psr = disable_interrupts();

    // retrieve mslot ID, mslot, and mbox
    int mslotID = find_next_empty_mslot();
    mslot mslot = all_mslots[mslotID];
    mbox mbox = all_mboxes[mboxID];
    
    // check preemptive return conditions
    if (mslotID < 0) return -2;                                                 // invalid mslot ID
    if (!(mboxID >= 0 && mboxID < MAXMBOX) || !mbox.is_alive) return -1;        // invalid mbox ID

    // if allowed mslots are depleted, block until one opens
    if (!mbox.numSlots) {
        // get process from proc table
        pcb proc = proc_table[getpid() % MAXPROC];

        // create proc node and add to head of producer queue
        Node proc_node = {&proc, mbox.producers};
        mbox.producers = &proc_node;

        // block producer process
        blockMe();
    }

    // this executes when the current producer wakes up due to a mbox release without any new slots opening
    if (!mbox.numSlots) return -1;

    // copy the message into the proper mailslot and decrement the available mslot counter
    strcpy(mslot.msg, (char *) msg);
    mslot.is_alive = 1;   
    mbox.numSlots--;

    // add mslot to mbox mslots
    Node mslot_node = {&mslot, mbox.mslots};
    mbox.mslots = &mslot_node;

    // if there is a consumer waiting to consume a message, wake up the tail of the consumer queue
    if (mbox.consumers) {
        pcb* proc_toBeUnblocked = popTail(&mbox.consumers);
        unblockProc(proc_toBeUnblocked->pid);
    }

    // restore interrupts
    restore_interrupts(old_psr);

    return 0; // success
}



// might be deleting this if the above function (newer version) works
int MboxSend(int mboxID, void *msg, int messageSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxSend");
    unsigned int old_psr = disable_interrupts();

    // retrieve mailbox and mslot id
    mbox mbox = all_mboxes[mboxID];
    int mslotID = find_next_empty_mslot();

    // check for invalid slot id
    if (mslotID < 0) return -2;

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX) || !mbox.is_alive) return -1;

    // if no remaining slots, add to producer queue and block
    if (!mbox.numSlots) {
        int pid = getpid();
        pcb proc = proc_table[pid % MAXPROC];
        
        // create new proc node and add producer proc to head
        Node proc_node = {&proc, mbox.producers};
        mbox.producers = &proc_node;
        
        blockMe();
    } else {
        // get next mslot
        mslot mslot = all_mslots[mslotID];

        // add message to mailslot if there is an available slot
        strcpy(mslot.msg, (char *) msg);
        mslot.is_alive = 1;
        
        mbox.numSlots--;

        // add mslot to mbox mslots
        Node mslot_node = {&mslot, mbox.mslots};
        mbox.mslots = &mslot_node;

    }

    // if there is a consumer waiting to consume a message, wake up
    // the tail of the consumer queue
    if (mbox.consumers) {
        pcb* proc_toBeUnblocked = popTail(&mbox.consumers);
        unblockProc(proc_toBeUnblocked->pid);
    }

    // restore interrupts
    restore_interrupts(old_psr);
    return 0; // success
}

// receive a message from a mailslot in a mailbox into the process
// which is consuming it
int MboxRecv(int mboxID, void *msg, int maxMsgSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxRecv");
    unsigned int old_psr = disable_interrupts();

    mbox mbox = all_mboxes[mboxID];
    int msgSize = strlen((char *) msg);

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX)
            || !mbox.is_alive
            || msgSize >= maxMsgSize)
        return -1;


    // check if there are any messages to be consumed
    if (mbox.mslots) {
        // consume message and increment available mslots in the mbox
        mslot* msg_toReceive = popTail(&mbox.mslots);
        msg = &msg_toReceive->msg;
        mbox.numSlots++;
        
        // if a producer is waiting, wake it to write into the newly empty mslot
        if (mbox.producers) {
            pcb* proc_toBeUnblocked = popTail(&mbox.producers);
            unblockProc(proc_toBeUnblocked->pid);
        }

    } else {
        // add consumer to consumer queue and block
        int pid = getpid();
        pcb proc = proc_table[pid % MAXPROC];
        
        // create new proc node and add consumer proc to head
        Node proc_node = {&proc, mbox.consumers};
        mbox.consumers = &proc_node;
        
        blockMe();
    }

    // zero-slot mailboxes?




    // if msg size is zero, set msg to NULL
    if (msgSize == 0) msg = NULL;

    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

int MboxCondSend(int mboxID, void *msg, int msgSize) {
    // WILL NEVER BLOCK

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCondSend");
    unsigned int old_psr = disable_interrupts();

    mbox mbox = all_mboxes[mboxID];

    // check if the consumer queue is empty
    if (mbox.consumers) blockMe(); // and no space available check

    if (find_next_empty_mslot() < 0) return -2;

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX) || !mbox.is_alive) return -2;
    
    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

int MboxCondRecv(int mboxID, void *msg, int maxMsgSize) {
    // WILL NEVER BLOCK
    
    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCondRecv");

    unsigned int old_psr = disable_interrupts();
    mbox mbox = all_mboxes[mboxID];
    int msgSize = strlen((char *) msg);

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX)
            || !mbox.is_alive
            || msgSize >= maxMsgSize)
        return -1;

    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

void waitDevice(int type, int unit, int *status) {

}

void wakeupByDevice(int type, int unit, int status) {

}

