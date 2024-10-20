#include "usyscall.h"
#include "phase1.h"
#include "phase2.h"

// process table -- and pcb?
   // use % PID from phase 1 to map PIDs to appropriate slots

// Questions
  // how do we know when a mail box has consumed its maximum number of mail slots?
  // can we leave start_services_processes blank? It wasn't in the header file.
  // do we have to disable interrupts?
  // is it permissible to reuse pids?

// STRUCT DEFINITIONS
typedef struct pcb {
    int pid; // process id

} pcb;

typedef struct mslot {
    char mesg[MAX_MESSAGE];
    struct mslot * next;
} mslot;

typedef struct proc_node {
    // what is the thing inside of it
    pcb * proc;
    struct proc_node * next;
} proc_node;

typedef struct mbox {
    int is_alive;
    mslot * mslots;
    proc_node * consumers;
    proc_node * producers;
    // max buffered messages?

    // runnable processes
} mbox;


typedef struct queue {
    proc_node * head;
    proc_node * tail;
} queue;

// GLOBAL VARIABLES
static pcb * proc_table[MAXPROC];
static mslot * all_mslots[MAXSLOTS];
static mbox * all_mboxes[MAXMBOX];
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

// pcb definition
   // table of phase1 stuff
   // table of phase2 stuff
   // array shadow process table
   // SHOULD have pcb struct?

// RUSS QUESTIONS:
   // are we overthinking the waking receivers problem? 4.1.1
      // just wake receivers in FIFO order
      // consumer queued flag wherever necessary?
   // what is the point of a zero-slot mailbox? What is a mailbox?
      // is it just a way of blocking?
   // how to declare systemCallVec?
   
   // should a mailbox be a struct?
     // yes -- what should be in the struct?
     // allocated or not?
     // parameters?
     // number of messages?
     // largest message?
     // queue of pending messages/processes
   // what's the difference between a mailbox and a mailslot?
      // a mail slot represent one message
      // every slot should have a buffer of the appropriate size;
   // can/should we access the pcb struct from phase1?

// PRODUCERS:
// block when they attempt to send a message, but the mailbox
// has already consumed its maximum number of mail slots
// must deliver their messages in the same order they arrived

// Consumers block when there are no queued messages

void phase2_init() {
    // disable interrupts?
    
    // memset all the arrays to 0?
    

    // restore interrupts?

}

void phase2_start_service_processes() {

}

int find_next_empty_mbox() {
    for (int i=0; i < MAXMBOX; i++) {
        if (!all_mboxes[i]->is_alive) return i;
    } return -1;
}

int MboxCreate(int numSlots, int slotSize) {
    int mbox_id = find_next_empty_mbox();
    if (numSlots < 0 || slotSize < 0) return -1;
    return mbox_id;
}

int MboxRelease(int mailboxID) {
    if (!all_mboxes[mailboxID]->is_alive) return -1;
    // do some shit
    return 0;
}

int MboxSend(int mailboxID, void *message, int messageSize) {
    // may block
    return 0;
}

int MboxRecv(int mailboxID, void *message, int maxMessageSize) {
    // may block
    return 0;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    // will never block
    return 0;
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    // will never block
    return 0;

}

void waitDevice(int type, int unit, int *status) {

}

void wakeupByDevice(int type, int unit, int status) {

}

void (*systemCallVec[])(USLOSS_Sysargs *args) {
    
}
