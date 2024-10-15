#include "usyscall.h"
#include "phase2.h"


// GLOBAL VARIABLES
// array of function pointers
// int funct(char c, char * str, long l);
// int (*fptr)(char,char*,long);
// int (*many_pointersA[10])(char,char*,long);

// process table -- and pcb?
   // use % PID from phase 1 to map PIDs to appropriate slots

typedef struct mbox {

} Mbox;

typedef struct node {
    // what is the thing inside of it
    struct node * next;
} Node;

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

// declare systemCallVec
// make blocked queues for:
   // producers
   // consumers
   // mail slot

// PRODUCERS:
// block when they attempt to send a message, but the mailbox
// has already consumed its maximum number of mail slots
// must deliver their messages in the same order they arrived

// Consumers block when there are no queued messages

void phase2_init() {

}

int MboxCreate(int numSlots, int slotSize) {
    return 0;
}

int MboxRelease(int mailboxID) {
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
