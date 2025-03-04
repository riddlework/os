// QUESTIONS:
// do we disable interrupts in every function again?
// what fields from the pcb struct do we actually need?

#include <string.h>

#include "phase1.h"
#include "phase2.h"
#include <usloss.h>


// struct definitions
typedef struct pcb {
    // TODO: what do we need here?
    // must keep what's necessary for queueing blocked processes
    // remove everything else

    // both not strictly necessary but good for visualization
    struct pcb *next_producer;
    struct pcb *next_consumer;
           int            pid;

} pcb;

typedef struct mslot {
    char msg[MAX_MESSAGE];
    struct mslot *next;

} Mslot;

typedef struct mbox {
    Mslot *first_mslot;
    int numSlots; // number of available slots in this mailbox

    int is_alive;
    int maxMsgSize; // the maximum size a message can be in this mailbox's slots

    // queues
    pcb *producers;
    pcb *consumers;
} Mbox;




// function stubs

void         check_kernel_mode             (const char        *func        );
void         enable_interrupts             (                               );
unsigned int disable_interrupts            (                               );
void         restore_interrupts            (      unsigned int old_psr     );
unsigned int check_and_disable             (const char        *func        );
void         phase2_init                   ();
void         phase2_start_service_processes();
int          MboxCreate                    (int slots, int slot_size);
int          MboxRelease(int mbox_id);
int          MboxSend(int mbox_id, void *msg_ptr, int msg_size);
int          MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size);
int          MboxCondSend(int mbox_id, void *msg_ptr, int msg_size);
int          MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size);
int          MboxSendHelp(int mbox_id, void *msg_ptr, int msg_size, int cond_flag);
int          MboxRecvHelp(int mbox_id, void *msg_ptr, int msg_max_size);

// global variables
Mbox mboxes[MAXMBOX];
Mslot mslots[MAXSLOTS];
pcb shadow_proc_table[MAXPROC];


// verifies that the program is currently running in kernel mode and halts if not 
void check_kernel_mode(const char *func) {
    unsigned int cur_psr = USLOSS_PsrGet();
    if (!(cur_psr & USLOSS_PSR_CURRENT_MODE)) {
        // not in kernel mode, halt simulation
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }
}

// updates psr to turn interrupts on
void enable_interrupts() {
    // enable interrupts
    int psr_status = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }
}

// updates psr to turn interrupts off
// return the old status so that interrupts may be reset later
unsigned int disable_interrupts() {
    // retrieve current psr
    unsigned int old_psr = USLOSS_PsrGet();

    // flip the current interrupt bit
    int psr_status = USLOSS_PsrSet(old_psr & ~USLOSS_PSR_CURRENT_INT);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }

    // return previous psr
    return old_psr;
}

// updates psr status to reinstate old interrupt status
void restore_interrupts(unsigned int old_psr) {
    int psr_status = USLOSS_PsrSet(old_psr);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }
}


void phase2_init() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // initialize mboxes to 0s
    for (int i = 0; i < MAXMBOX; i++) memset(&mboxes[i], 0, sizeof(Mbox));

    // initialize mslots to 0s
    for (int i = 0; i < MAXSLOTS; i++) memset(&mslots[i], 0, sizeof(Mslot));

    // initialize shadow_proc_table to 0s
    for (int i = 0; i < MAXPROC; i++) memset(&shadow_proc_table[i], 0, sizeof(pcb));

    restore_interrupts(old_psr);
}



// returns id of mailbox, or -1 if no more mailboxes, or -1 if invalid args
int MboxCreate(int slots, int slot_size) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // error checking
    if (slot_size < 0 || slot_size > MAXSLOTS || slot_size < 0 || slot_size > MAX_MESSAGE) {
        return -1;
    }

    int mbox_id = -1;

    for (int i = 0; i < MAXMBOX; i++) {
        Mbox *mbox = &mboxes[i];
        if (!mbox->is_alive) mbox_id = i;
    }

    // check if there are no mailboxes available
    if (mbox_id == -1) return -1;

    // initialize mbox
    Mbox *mbox = &mboxes[mbox_id];
    mbox->is_alive = 1;
    mbox->numSlots = slots;
    mbox->maxMsgSize = slot_size;

    restore_interrupts(old_psr);

    return mbox_id;
}

// returns 0 if successful, -1 if invalid arg
int MboxRelease(int mbox_id) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    Mbox *mbox = &mboxes[mbox_id];

    // error checking
    if (!mbox->is_alive) return -1;

    // mark as terminated
    mbox->is_alive = 0;

    // free slots consumed by the mailbox
    Mslot *prev = NULL;
    Mslot *cur = mbox->first_mslot;
    while (cur) {
        if (prev) memset(prev, 0, sizeof(Mslot));

        prev = cur;
        cur = cur->next;
    }

    // unblock producers
    pcb *producer = mbox->producers;
    while ((producer = producer->next_producer)) unblockProc(producer->pid);

    // unblock consumers
    pcb *consumer = mbox->consumers;
    while ((consumer = consumer->next_consumer)) unblockProc(consumer->pid);

    restore_interrupts(old_psr);

    return 0;
}


// return 0 if successful, -1 if invalid args
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {

    // find the next mslot to be filled
       // if there are no more mslots, return -2

    // check that the mbox with the given id is alive (check bounds?)
    // check that the length of message matches reported size

    // if there is a consumer queued, deliver directly
    // if there is an available mailslot, queue in mailslot
    // otherwise, block
    int mslot_id = -2;
    
    for (int i = 0; i < MAXSLOTS; i++) {
    }
    return 0;
}

// returns size of received msg if successful, -1 if invalid args
int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}

// returns 0 if successful, 1 if mailbox full, -1 if illegal args
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    return 0;
}

// returns 0 if successful, 1 if no msg available, -1 if illegal args
int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}

int MboxSendHelp(int mbox_id, void *msg_ptr, int msg_size, int cond_flag) {

}

int MboxRecvHelp(int mbox_id, void *msg_ptr, int msg_size, int cond_flag) {
}
