#include "usyscall.h"
#include "phase1.h"
#include "phase2.h"
#include <string.h>
#include <stdio.h>


/* STRUCT DEFINITIONS */
typedef struct node {
    void * thing;
    struct node * next;
} Node;

typedef struct pcb {
    int pid; // process id
    struct pcb * next;
} pcb;

typedef struct mslot {
    char msg[MAX_MESSAGE];
    int          is_alive;
    struct mslot *   next;
} mslot;

typedef struct mbox {
    int     is_alive;
    int     numSlots;
    int   maxMsgSize;

    mslot *   mslots;    // queue of mslot structs
    pcb * consumers;    // queue of consumer processes
    pcb * producers;    // queue of producer processes

} mbox;
/****************************************************/


/* GLOBAL VARIABLES */
static pcb proc_table[MAXPROC];
static mslot all_mslots[MAXSLOTS];
static mbox all_mboxes[MAXMBOX];
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
/****************************************************/
   
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

static void nullsys() {

    // check for kernel mode and disable interrupts
    check_kernel_mode("nullsys");
    unsigned int old_psr = disable_interrupts();

    // print out error message
    /*int syscallNum = systemCallVec[USLOSS_SYSCALL_INT];*/
    /*int syscallNum = (int) USLOSS_IntVec[USLOSS_SYSCALL_INT];*/
    int syscallNum = 0;
    printf("nullsys(): Program called an unimplemented syscall.  syscall no: %d  PSR: %d\n", syscallNum, USLOSS_PsrGet());

    // halt simulation
    USLOSS_Halt(1);

    // restore interrupts
    restore_interrupts(old_psr);
}

/* INIT FUNCTIONS */
void phase2_init() {

    // check kernel mode and disable interrupts
    check_kernel_mode("phase2_init");
    unsigned int old_psr = disable_interrupts();
    
    // memset all the arrays to 0
    for (int i=0; i < MAXPROC; i++) memset(&proc_table[i], 0, sizeof(struct pcb));
    for (int i=0; i < MAXSLOTS; i++) memset(&all_mslots[i], 0, sizeof(struct mslot));
    for (int i=0; i < MAXMBOX; i++) memset(&all_mboxes[i], 0, sizeof(struct mbox));

    // intialize array of function pointers
    for (int i=0; i < MAXSYSCALLS; i++) systemCallVec[i] = nullsys;

    // restore interrupts
    restore_interrupts(old_psr);
}

// uncessary function -- add function call to compile properly
void phase2_start_service_processes() { }
/****************************************************/


/* HELPER FUNCTIONS */
int find_next_empty_mbox() {
    for (int i=0; i < MAXMBOX; i++) {
        if (!all_mboxes[i].is_alive) return i;
    } return -1;
}

int find_next_empty_mslot() {
    for (int i=0; i < MAXSLOTS; i++) {
        if (!all_mslots[i].is_alive) return i;
    } return -1;
}

// remove and return the tail -- list will always have at least one node
pcb * popTailProc(pcb** head) {
    // list will never be empty
    pcb * proc_toReturn = NULL;
    if (!(*head)->next) {
        // list with one node
        proc_toReturn = *head;
        *head = NULL;
    } else {
        // list with more than one node
        pcb * cur = *head;
        while (cur->next->next) { cur = cur->next; }

        proc_toReturn = cur->next;
        cur->next = NULL;
    } return proc_toReturn;
}

mslot * popTailMslot(mslot** head) {
    // list will never be empty
    mslot * mslot_toReturn = NULL;
    if (!(*head)->next) {
        // list with one node
        mslot_toReturn = *head;
        *head = NULL;
    } else {
        // list with more than one node
        mslot * cur = *head;
        while (cur->next->next) { cur = cur->next; }

        mslot_toReturn = cur->next;
        cur->next = NULL;
    } return mslot_toReturn;
}

int sendHelp(int mboxID, void *msg, int msgSize, int block) {

    // disable interrupts
    unsigned int old_psr = disable_interrupts();

    // retrieve mslot ID, mslot, and mbox
    int mslotID = find_next_empty_mslot();
    mslot * mslot = &all_mslots[mslotID];
    mbox * mbox = &all_mboxes[mboxID];
    
    // check preemptive return conditions
    if (mslotID < 0) return -2;                                                 // invalid mslot ID
    if (!(mboxID >= 0 && mboxID < MAXMBOX) || !mbox->is_alive) return -1;        // invalid mbox ID

    // if allowed mslots are depleted, block until one opens
    if (!mbox->numSlots) {
        int pid = getpid();
        pcb proc = proc_table[pid % MAXPROC];

        // add proc to head of queue 
        proc.next = mbox->producers;
        mbox->producers = &proc;

        // block or fall through
        if (block) {
            blockMe();
        } else {
            return -2;
        }
    }

    // this executes when the current producer wakes up due to a mbox release without any new slots opening
    if (!mbox->is_alive) {
        return -1;
    } else if (!mbox->numSlots) {
        // consume message
        pcb* proc_toBeUnblocked = popTailProc(&mbox->consumers);
        unblockProc(proc_toBeUnblocked->pid);
    } else {
        // copy the message into the proper mailslot and decrement the available mslot counter
        strcpy(mslot->msg, (char *) msg);
        mslot->is_alive = 1;   
        mbox->numSlots--;

        // add mslot to mbox mslots -- end of queue (head of list)
        mslot->next = mbox->mslots;
        mbox->mslots = mslot;

        // if there is a consumer waiting to consume a message, wake up the tail of the consumer queue
        if (mbox->consumers) {
            pcb* proc_toBeUnblocked = popTailProc(&mbox->consumers);
            unblockProc(proc_toBeUnblocked->pid);
        }
    }

    // restore interrupts
    restore_interrupts(old_psr);
    return 0; // success
}

int recvHelp(int mboxID, void *msg, int maxMsgSize, int block) {

    // disable interrupts
    unsigned int old_psr = disable_interrupts();

    // get mbox and size of msg
    mbox * mbox = &all_mboxes[mboxID];
    int msgSize = strlen((char *) msg);

    // check for invalid mailbox id, dead mailbox, and invalid msg size
    if (!(mboxID >= 0 && mboxID < MAXMBOX)
            || !mbox->is_alive
            || msgSize >= maxMsgSize)
        return -1;

    // if there are no messages waiting in mslots, then add CRP to consumer queue & block
    if (!mbox->mslots) {
        
        // call helper to enqueue and block
        int pid = getpid();
        pcb proc = proc_table[pid % MAXPROC];

        // create new proc node and add producer proc to head
        proc.next = mbox->producers;
        mbox->producers = &proc;

        // block or fall through
        if (block) {
            blockMe();
        } else {
            return -2;
        }
    }

    // this executes when the current consumer wakes up due to a mbox release without any new slots gaining messages
    if (!mbox->is_alive) return -1;

    // consume message and increment available mslots in the mbox
    if (mbox->numSlots) {
        mslot * msg_toReceive = popTailMslot(&mbox->mslots);
        strcpy(msg, msg_toReceive->msg);
        mbox->numSlots++;
    }
        
    // if a producer is waiting, wake it
    if (mbox->producers) {
        pcb* proc_toBeUnblocked = popTailProc(&mbox->producers);
        unblockProc(proc_toBeUnblocked->pid);
    }

    // restore interrupts
    restore_interrupts(old_psr);
    return 0; // success
}

/****************************************************/


/* MBOX FUNCTIONS */

int MboxCreate(int numSlots, int slotSize) {

    // check kernel mode and disable interrupts
    check_kernel_mode("MboxCreate");
    unsigned int old_psr = disable_interrupts();

    int mbox_id = find_next_empty_mbox();
    if (numSlots < 0 || slotSize < 0) return -1;
    
    mbox * mbox = &all_mboxes[mbox_id];
    if (mbox_id >= 0) {
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

    mbox * mbox = &all_mboxes[mboxID];
    if (!mbox->is_alive) return -1;

    // release consumers
    while (mbox->consumers) {
        pcb* proc_toBeUnblocked = popTailProc(&mbox->consumers);
        unblockProc(proc_toBeUnblocked->pid);
    }

    // release producers
    while (mbox->producers) {
        pcb* proc_toBeUnblocked = popTailProc(&mbox->producers);
        unblockProc(proc_toBeUnblocked->pid);
    }

    // release mslots
    while (mbox->mslots) {
        mslot* mslot_toBeReleased = popTailMslot(&mbox->mslots);
        memset(mslot_toBeReleased->msg, 0, sizeof(mslot_toBeReleased->msg));
        mslot_toBeReleased->is_alive = 0;
    }

    // set everything to 0
    memset(mbox, 0, sizeof(struct mbox));

    // restore interrupts
    restore_interrupts(old_psr);

    return 0;
}


// send a message to a process via a mailslot in a mailbox
int MboxSend(int mboxID, void *msg, int messageSize) {

    // check for kernel mode
    check_kernel_mode("MboxSend");

    return sendHelp(mboxID, msg, messageSize, 1);

}

// send a message but fall through if no slot is available
int MboxCondSend(int mboxID, void *msg, int msgSize) {
    // WILL NEVER BLOCK

    // check for kernel mode
    check_kernel_mode("MboxCondSend");

    return sendHelp(mboxID, msg, msgSize, 0);
}

// receive a message from a mailslot in a mailbox into a process
int MboxRecv(int mboxID, void *msg, int maxMsgSize) {

    // check for kernel mode
    check_kernel_mode("MboxRecv");

    return recvHelp(mboxID, msg, maxMsgSize, 1);
}

// receive a message but fall through if no message is available
int MboxCondRecv(int mboxID, void *msg, int maxMsgSize) {
    // WILL NEVER BLOCK
    
    // check for kernel mode
    check_kernel_mode("MboxCondRecv");

    return recvHelp(mboxID, msg, maxMsgSize, 0);
}
/****************************************************/

void waitDevice(int type, int unit, int *status) {

    // check for kernel mode
    check_kernel_mode("waitDevice");

    // disable interrupts
    unsigned int old_psr = disable_interrupts();

    // error checking
    

    // restore interrupts
    restore_interrupts(old_psr);

}


