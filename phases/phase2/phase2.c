// QUESTIONS:
// do we disable interrupts in every function again?
// what fields from the pcb struct do we actually need?

#include <string.h>
#include <stdio.h>

#include "phase1.h"
#include "phase2.h"
#include <usloss.h>


// struct definitions
typedef struct pcb {
    // TODO: what do we need here?
    // must keep what's necessary for queueing blocked processes
    // remove everything else

    // both not strictly necessary but good for visualization
    // msg_ptr could be in or out, depending on if process is consumer or producer
           char *msg_ptr;
    struct pcb *next_producer;
    struct pcb *next_consumer;
           int            pid;

} pcb;

typedef struct mslot {
    char msg[MAX_MESSAGE];
    struct mslot *next_slot;
    int is_alive;
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

void dumpMbox(int mbox_id);
void         check_kernel_mode             (const char        *func        );
void         enable_interrupts             (                               );
unsigned int disable_interrupts            (                               );
void         restore_interrupts            (      unsigned int old_psr     );
unsigned int check_and_disable             (const char        *func        );
void         phase2_start_service_processes();
int          sendHelp(int mbox_id, void *msg_ptr, int msg_size, int block);
int          recvHelp(int mbox_id, void *msg_ptr, int msg_max_size, int block);

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

// check for kernel mode -- save and disable interrupts
unsigned int check_and_disable(const char *func) {
    check_kernel_mode(func);
    return disable_interrupts();
}

static void trivial_clock_handler(int dev, void *arg) {
    dispatcher();
}

static void term_handler(int dev, void *arg) {
    // retreieve terminal number from arg
    int term_no = (int)(long) arg;
}

static void disk_handler(int dev, void *arg) {
    // retrieve disk number from arg
    int disk_no = (int)(long) arg;
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

    // allocate mailboxes for interrupt handlers
    for (int i = 0; i < 7; i++) MboxCreate(1, sizeof(int));

    // send messages to wake up the device driver and when an interrupt occurs

    USLOSS_IntVec[USLOSS_CLOCK_INT] = trivial_clock_handler;

    restore_interrupts(old_psr);
}



void phase2_start_service_processes() {
}


// returns id of mailbox, or -1 if no more mailboxes, or -1 if invalid args
int MboxCreate(int slots, int slot_size) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // error checking
    if (slot_size < 0 || slot_size > MAXSLOTS || slot_size < 0 || slot_size > MAX_MESSAGE) {
        return -1;
    }

    // find the next available mailbox
    int mbox_id = -1;
    for (int i = 0; i < MAXMBOX; i++) {
        if (!mboxes[i].is_alive) { 
            mbox_id = i;
            break;
        }
    }

    // check if there are no mailboxes available
    if (mbox_id == -1) return -1;

    // initialize mbox
    Mbox *mbox = &mboxes[mbox_id];
    mbox->is_alive   = 1;
    mbox->numSlots   = slots;
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
        cur = cur->next_slot;
    }

    // unblock producers
    pcb *producer = mbox->producers;
    while (producer) {
        unblockProc(producer->pid);
        producer = producer->next_producer;
    }

    // unblock consumers
    pcb *consumer = mbox->consumers;
    while (consumer) {
        unblockProc(consumer->pid);
        consumer = consumer->next_consumer;
    }

    // set to zeros
    memset(mbox, 0, sizeof(Mbox));


    restore_interrupts(old_psr);

    return 0;
}


// return 0 if successful, -1 if invalid args
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    return sendHelp(mbox_id, msg_ptr, msg_size, 1);
}

// returns size of received msg if successful, -1 if invalid args
int MboxRecv(int mbox_id, void *msg_ptr, int max_msg_size) {
    return recvHelp(mbox_id, msg_ptr, max_msg_size, 1);
}

// returns 0 if successful, 1 if mailbox full, -1 if illegal args
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    return sendHelp(mbox_id, msg_ptr, msg_size, 0);
}

// returns 0 if successful, 1 if no msg available, -1 if illegal args
int MboxCondRecv(int mbox_id, void *msg_ptr, int max_msg_size) {
    return recvHelp(mbox_id, msg_ptr, max_msg_size, 0);
}

// get a ptr to current process in proc table
pcb *get_cur_proc() {
    int pid = getpid();
    return &shadow_proc_table[pid % MAXPROC];
}

int sendHelp(int mbox_id, void *msg_ptr, int msg_size, int block) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // YOU ARE A PRODUCER PROCESS

    // you have
    // mbox_id -- the id of the mailbox to send the message to
        // if the mbox_id is invalid, i.e., out of bounds, throw error
        // if the mbox_id is valid but the mbox has been terminated, throw error
    // msg_ptr -- a pointer to the message you are sending
        // a void *, cast to char *
    // msg_size -- the size of the message you are sending
       // if msg_ptr is NULL and msg_size != 0, throw error
       // each mbox has a max size that its messages can be, should use this
       // TODO: should we only copy over msg_size bytes?
       // or should we just throw an error if strlen(msg_ptr) != msg_size?

    // you are a producer process

    // check that the mbox_id is valid (bounded)
    if (mbox_id < 0 || mbox_id >= MAXMBOX) return -1;

    // retrieve a reference to the desired mailbox
    Mbox *mbox = &mboxes[mbox_id];

    // check that the desired mbox is alive
    if (!mbox->is_alive) return -1;

    // check that the message matches its metadata
    char *msg = (char *) msg_ptr;
    // TODO: might cause an error when msg_ptr is NULL and msg_size is 0
    if ((!msg && msg_size) || msg_size > mbox->maxMsgSize || (msg && strlen(msg)+1 != msg_size)) {
        // +1 to accomodate for null terminator
        // check that msg exists before trying to use strlen on it
        return -1;
    }



    if (mbox->consumers) {
        // IF CONSUMER, DELIVER MSG DIRECTLY

        // write the message to the consumer -- if the ptr is not NULL
        if (msg) strcpy(mbox->consumers->msg_ptr, msg);

        // dequeue and unblock the consumer
        int pid_toUnblock = mbox->consumers->pid;
        mbox->consumers = mbox->consumers->next_consumer;
        unblockProc(pid_toUnblock);

    } else if (mbox->numSlots) {
        // IF AVAILABLE MSLOTS QUEUE MESSAGE

        // find the next empty mslot
        int mslot_id = -2;
        for (int i = 0; i < MAXSLOTS; i++) {
            if (!mslots[i].is_alive) {
                mslot_id = i;
                break;
            }
        }

        // check if the system has run out of global mslots
        // if so, msg could not be queued, throw error
        if (mslot_id == -2) return -2;

        // fill newly allocated mslot
        Mslot *mslot = &mslots[mslot_id];
        mslot->is_alive = 1;
        strcpy(mslot->msg, msg);

        // add mslot to mbox mslot queue
        Mslot *cur = mbox->first_mslot;
        if (!cur) {
            mbox->first_mslot = mslot;
        } else {
            while (cur->next_slot) cur = cur->next_slot;
            cur->next_slot = mslot;
        }

        // decrement the number of available mslots for the mbox
        mbox->numSlots--;

    } else if (block) {
        // BLOCK -- WAITING ON MSLOT TO BECOME AVAILABLE
        
        // add self to producer queue and block
        pcb *self = get_cur_proc();
        self->pid = getpid(); // set pid

        pcb *cur_proc = mbox->producers;
        if (!cur_proc) {
            mbox->producers = self;
        } else {
            while (cur_proc->next_producer) cur_proc = cur_proc->next_producer;
            cur_proc->next_producer = self;
        }
        blockMe();

        // AFTER BLOCK: (a) AVAILABLE MSLOT OR (b) MBOX RELEASED

        // check if the mailbox has been released
        if (!mbox->is_alive) return -1;

        // QUEUE THE MESSAGE

        // find the next empty mslot
        int mslot_id = -2;
        for (int i = 0; i < MAXSLOTS; i++) {
            if (!mslots[i].is_alive) {
                mslot_id = i;
                break;
            }
        }

        // check if the system has run out of global mslots
        // if so, msg could not be queued, throw error
        if (mslot_id == -2) return -2;

        // fill newly allocated mslot
        Mslot *mslot = &mslots[mslot_id];
        mslot->is_alive = 1;
        strcpy(mslot->msg, msg);


        // queue the mslot
        Mslot *cur_slot = mbox->first_mslot;
        if (!cur_slot) {
            mbox->first_mslot = mslot;
        } else {
            while (cur_slot->next_slot) cur_slot = cur_slot->next_slot;
            cur_slot->next_slot = mslot;
        }

        // decrement the number of available mslots for the mbox
        mbox->numSlots--;
    } else {
        return -2;
    }

    restore_interrupts(old_psr);
    return 0;


}

int recvHelp(int mbox_id, void *msg_ptr, int max_msg_size, int block) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // YOU ARE A CONSUMER PROCESS

    // you have
    // mbox_id -- the id of the mbox you are receiving a message from
    // msg_ptr -- the pointer to fill with recv'ed message
    // max_msg_size -- the size of the buffer
       // you can recieve up to this size, but might receive less

    // check that the mbox_id is valid (bounded)
    if (mbox_id < 0 || mbox_id >= MAXMBOX) return -1;

    // retrieve a reference to the desired mailbox
    Mbox *mbox = &mboxes[mbox_id];

    // check that the desired mbox is alive
    if (!mbox->is_alive) return -1;

    // declare return value
    int msg_size;
    if (mbox->first_mslot) {
        // if there is a message queued, consume it
        char *msg = strdup(mbox->first_mslot->msg);
        msg_size = strlen(msg) + 1; // account for null terminator

        // check if the message is too large for the buffer
        // or the buffer is null and the msg is not null
        if (msg_size > max_msg_size || (!msg_ptr && msg_size)) return -1;

        // copy the message into the buffer -- if not null
        if (msg_ptr) strcpy(msg_ptr, msg);

        // free the mslot
        Mslot *mslot_toFree = mbox->first_mslot;
        mbox->first_mslot = mbox->first_mslot->next_slot;
        memset(mslot_toFree, 0, sizeof(Mslot));

        // increment the mbox's number of available mslots
        mbox->numSlots++;

        // unblock a producer if one is waiting in send
        if (mbox->producers) {
            int pid_toUnblock = mbox->producers->pid;
            mbox->producers = mbox->producers->next_producer;
            unblockProc(pid_toUnblock);
        }
    } else if (block) {
        // BLOCK -- WAITING ON MSG TO BE QUEUED

        pcb *self = get_cur_proc();
        self->pid = getpid(); // set pid

        // init msg ptr
        char buf[max_msg_size];
        self->msg_ptr = buf;

        // add self to consumer queue and block
        pcb *cur_proc = mbox->consumers;
        if (!cur_proc) {
            mbox->consumers = self;
        } else {
            while (cur_proc->next_consumer) cur_proc = cur_proc->next_consumer;
            cur_proc->next_consumer = get_cur_proc();
        }
        blockMe();

        // AFTER BLOCK -- (a) MBOX RELEASED OR (b) MSG HAS BEEN DIRECTLY DELIVERED

        // check if the mbox was released
        if (!mbox->is_alive) return -1;

        // consume msg
        pcb *consumer_proc = &shadow_proc_table[getpid() % MAXPROC];
        char *msg = strdup(consumer_proc->msg_ptr);
        msg_size = strlen(msg) + 1; // account for null terminator

        // check if the message is too large for the buffer
        // or the buffer is null and the msg is not null
        if (msg_size > max_msg_size || (!msg_ptr && msg_size)) return -1;

        // copy the message into the buffer -- if not null
        if (msg_ptr) strcpy(msg_ptr, msg);

    } else {
        return -2;
    }

    restore_interrupts(old_psr);
    return msg_size;
}


void dumpMbox(int mbox_id) {
    printf("MBOX %d\n", mbox_id);
    Mbox * mbox = &mboxes[mbox_id];

    printf("MSLOTS\n");

    Mslot *mslot = mbox->first_mslot;
    int i = 0;
    while (mslot) {
        printf("mslot_id = %d; msg = %s\n", i, mslot->msg);
        i++;
        mslot = mslot->next_slot;
    }

    printf("PRODUCERS\n");
    pcb *producer = mbox->producers;
    while (producer) {
        printf("pid = %d; msg = %s\n", producer->pid, producer->msg_ptr ? producer->msg_ptr : "NULL");
        producer = producer->next_producer;
    }

    printf("CONSUMERS\n");
    pcb *consumer = mbox->consumers;
    while (consumer) {
        printf("pid = %d; msg = %s\n", consumer->pid, consumer->msg_ptr ? consumer->msg_ptr : "NULL");
        consumer = consumer->next_consumer;
    }
    
}
