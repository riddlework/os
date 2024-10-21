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
typedef struct pcb {
    int pid; // process id

} pcb;

typedef struct queue {
    void * head;
    void * tail;
} queue;

typedef struct mslot {
    char msg[MAX_MESSAGE];
    int is_alive;
    struct mslot * next;
} mslot;

typedef struct proc_node {
    // what is the thing inside of it
    pcb * proc;
    struct proc_node * next;
} proc_node;

typedef struct mbox {
    int is_alive;
    int numSlots;
    int maxMsgSize;

    queue * mslots;
    queue * consumers;
    queue * producers;
    // max buffered messages?

    // runnable processes
} mbox;


// GLOBAL VARIABLES
static pcb * proc_table[MAXPROC];
static mslot * all_mslots[MAXSLOTS];
static mbox * all_mboxes[MAXMBOX];
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
    
    // memset all the arrays to 0?
    memset(proc_table, 0, sizeof(proc_table));
    memset(all_mslots, 0, sizeof(all_mslots));
    memset(all_mboxes, 0, sizeof(all_mboxes));

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
        if (!all_mboxes[i]->is_alive) return i;
    } return -1;
}

int MboxCreate(int numSlots, int slotSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCreate");
    unsigned int old_psr = disable_interrupts();

    int mbox_id = find_next_empty_mbox();
    if (numSlots < 0 || slotSize < 0) return -1;

    if (mbox_id >= 0) {
        mbox * mbox = all_mboxes[mbox_id];
        mbox->is_alive = 1;
        mbox->numSlots = numSlots;
        mbox->maxMsgSize = slotSize;
    }

    // restore interrupts and return
    restore_interrupts(old_psr);
    return mbox_id;
}

int MboxRelease(int mboxID) {
    
    // check kernel mode and disable interrupts
    check_kernel_mode("MboxRelease");
    unsigned int old_psr = disable_interrupts();

    mbox * mbox = all_mboxes[mboxID];
    if (!mbox->is_alive) return -1;

    // release mslots
    mslot * cur_mslot = mbox->mslots->head;
    while ((cur_mslot = cur_mslot->next)) cur_mslot->is_alive = 0;

    // set everything to 0
    memset(mbox, 0, sizeof(struct mbox));


    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

int find_next_empty_mslot() {
    for (int i=0; i < MAXSLOTS; i++) {
        if (!all_mslots[i]->is_alive) return i;
    } return -1;
}

int MboxSend(int mboxID, void *msg, int messageSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxSend");
    unsigned int old_psr = disable_interrupts();

    // retrieve mailbox and mslot id
    mbox * mbox = all_mboxes[mboxID];
    int mslotID = find_next_empty_mslot();

    // check for invalid slot id
    if (mslotID < 0) return -2;

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX) || !mbox->is_alive) return -1;

    // if no remaining slots, add to producer queue and block
    if (!mbox->numSlots) {
        int pid = getpid();
        pcb * proc = proc_table[pid % MAXPROC];
        
        // create new queue node and add producer proc to queue head
        proc_node proc_node = {proc, mbox->producers->head};
        mbox->producers->head = &proc_node;
        
        blockMe();
    } else {
        // get next mslot
        mslot * mslot = all_mslots[mslotID];

        // add message to mailslot if there is an available slot
        strcpy(mslot->msg, (char *) msg);
        mslot->is_alive = 1;
        
        mbox->numSlots--;

        // add mslot to mbox queue
        mslot->next = mbox->mslots->head;
        mbox->mslots->head = mslot;
    }

    // if there is a consumer waiting to consume a message, wake up
    // the tail of the consumer queue
    queue * consumer_queue = mbox->consumers;
    if (consumer_queue) {
        proc_node * tail = consumer_queue->tail;
        pcb * proc_toBeUnblocked = tail->proc;

        // iterate through queue to remove tail
        // MIGHT CAUSE BUGS
        proc_node * cur_node = consumer_queue->head;
        while (cur_node->next->next) { cur_node = cur_node->next; }
        cur_node->next = NULL;
        consumer_queue->tail = cur_node;

        // unblock process
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

    mbox * mbox = all_mboxes[mboxID];
    int msgSize = strlen((char *) msg);


    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX)
            || !mbox->is_alive
            || msgSize >= maxMsgSize)
        return -1;

    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

int MboxCondSend(int mboxID, void *msg, int msgSize) {
    // WILL NEVER BLOCK

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCondSend");
    unsigned int old_psr = disable_interrupts();

    mbox * mbox = all_mboxes[mboxID];

    // check if the consumer queue is empty
    if (mbox->consumers) blockMe(); // and no space available check

    if (find_next_empty_mslot() < 0) return -2;

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX) || !mbox->is_alive) return -2;
    
    // restore interrupts
    restore_interrupts(old_psr);
    return 0;
}

int MboxCondRecv(int mboxID, void *msg, int maxMsgSize) {
    // WILL NEVER BLOCK
    
    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCondRecv");

    unsigned int old_psr = disable_interrupts();
    mbox * mbox = all_mboxes[mboxID];
    int msgSize = strlen((char *) msg);

    // check for invalid mailbox id
    if (!(mboxID >= 0 && mboxID < MAXMBOX)
            || !mbox->is_alive
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

