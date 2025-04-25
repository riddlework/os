#include "phase1.h"
#include "phase2.h"
#include "phase4.h"
#include <usloss.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "usyscall.h" // TODO: delete this later

// a macro for checking that the program is currently running in kernel mode
#define CHECKMODE { \
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) { \
        USLOSS_Console("ERROR: Someone attempted to call a function while in user mode!\n"); \
    } \
}

// change into user mode from kernel mode
#define SETUSERMODE { \
    USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE); \
}

// enable terminal recv interrupts
#define ENABLE_TERM_RECV_INT(unit) { \
    int err = USLOSS_DeviceOutput(USLOSS_TERM_DEV, (unit), (void *)(long)0x2); \
    if (err == USLOSS_DEV_INVALID) { \
        USLOSS_Console("ERROR: Failed to turn on read interrupts!\n"); \
        USLOSS_Halt(1); \
    } \
}

// enable terminal xmit interrupts
#define ENABLE_TERM_XMIT_INT(unit) { \
    int err = USLOSS_DeviceOutput(USLOSS_TERM_DEV, (unit), (void *)(long)(0x2 | 0x4)); \
    if (err == USLOSS_DEV_INVALID) { \
        USLOSS_Console("ERROR: Failed to turn on xmit interrupts!\n"); \
        USLOSS_Halt(1); \
    } \
}

// disable terminal xmit interrupts... same as enabling recv interrupts but makes for cleaner code
#define DISABLE_TERM_XMIT_INT(unit) { \
    int err = USLOSS_DeviceOutput(USLOSS_TERM_DEV, (unit), (void *)(long)0x2); \
    if (err == USLOSS_DEV_INVALID) { \
        USLOSS_Console("ERROR: Failed to turn off xmit interrupts!\n"); \
        USLOSS_Halt(1); \
    } \
}

// disable terminal xmit interrupts
// struct definitions
typedef struct pcb {
    int pid;
    int wakeup_cycle;
    struct pcb *next;
} pcb;

typedef struct rw_req {
           int         pid;
           char       *buf;
           int     bufSize;
           int     *lenOut;
           int cur_buf_idx;
    struct rw_req    *next;
} rw_req;

typedef struct term {
    char buf[MAXLINE+1];
    int mbox;
    int write_mbox;
    rw_req *read_queue;
    rw_req *write_queue;
} Terminal;


/* FUNCTION STUBS */
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase4_start_service_processes();
void put_into_sleep_queue(pcb *proc);
void put_into_term_queue(rw_req *req, rw_req **queue);

// system calls
void kern_sleep     (USLOSS_Sysargs *arg);
void kern_term_read (USLOSS_Sysargs *arg);
void kern_term_write(USLOSS_Sysargs *arg);
void kern_disk_read (USLOSS_Sysargs *arg);
void kern_disk_write(USLOSS_Sysargs *arg);
void kern_disk_size (USLOSS_Sysargs *arg);

// daemons
int sleepd(void *arg);
int termd(void *arg);

// globals
int mutex;
pcb *sleep_queue;
int num_cycles_since_start;
Terminal terms[4];


void gain_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO GAIN THE MUTEX!\n", func);*/
    MboxSend(mutex, NULL, 0);
    /*USLOSS_Console("%s HAS GAINED THE MUTEX!\n", func);*/
}

void release_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO RELEASE THE MUTEX!\n", func);*/
    MboxRecv(mutex, NULL, 0);
    /*USLOSS_Console("%s HAS RELEASED THE MUTEX!\n", func);*/
}

void phase4_init() {
    // require kernel mode
    CHECKMODE;

    // initialize the mutex using a semaphore
    mutex = MboxCreate(1, 0);

    // gain the mutex
    gain_mutex(__func__);
    
    // load the system call vec
    systemCallVec[SYS_SLEEP]     =      kern_sleep;
    systemCallVec[SYS_TERMREAD]  =  kern_term_read;
    systemCallVec[SYS_TERMWRITE] = kern_term_write;
    systemCallVec[SYS_DISKREAD]  =  kern_disk_read; 
    systemCallVec[SYS_DISKWRITE] = kern_disk_write; 
    systemCallVec[SYS_DISKSIZE]  =  kern_disk_size; 

    // initialize terminals
    for (int i = 0; i < 4; i++) {
        Terminal *term = &terms[i];

        memset(term, 0, sizeof(Terminal));

        // zero out buffer
        explicit_bzero(term->buf, MAXLINE+1);

        // initialize mailbox
        term->mbox = MboxCreate(10, MAXLINE+1);

        // initialize write mbox
        term->write_mbox = MboxCreate(1, 0);

        // enable terminal recv interrupts
        for (int unit = 0; unit < 4; unit++) ENABLE_TERM_RECV_INT(unit);
    }

    // read the disk sizes here and save them as global varaibles
    // queuing and sequencing disk requests

    // release the mutex
    release_mutex(__func__);
}

void phase4_start_service_processes() {
    // spork the sleep daemon
    spork("sleepd", sleepd, NULL, USLOSS_MIN_STACK, 5);

    // spork the four terminal daemons - one for each terminal
    for (int i = 0; i < 4; i++) spork("termd", termd, (void *)(long)i, USLOSS_MIN_STACK, 5);
}

/* DAEMONS */

/* sleep daemon */
int sleepd(void *arg) {

    while (1) {
        // call wait device to wait for a clock interrupt
        int status;
        waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        num_cycles_since_start++;

        // wakeup any cycles whose wakeup time has arrived/passed
        while (sleep_queue && num_cycles_since_start >= sleep_queue->wakeup_cycle) {
            
            // gain mutex here since the sleep queue is a shared 
            gain_mutex(__func__);
            int pid_toUnblock = sleep_queue->pid;
            sleep_queue = sleep_queue->next;
            release_mutex(__func__);

            unblockProc(pid_toUnblock);
        }
    }
}

/* terminal daemon */
int termd(void *arg) {

    int       unit = (int)(long)arg;
    Terminal *term =   &terms[unit];
    int    buf_idx =              0;

    // term write writes to the control register
    while (1) {
        // wait for a terminal interrupt to occur -- retrieve its status
        int status;
        waitDevice(USLOSS_TERM_DEV, unit, &status);

        // unpack the different parts of the status
        char         ch = USLOSS_TERM_STAT_CHAR(status); // char recvd, if any
        int xmit_status = USLOSS_TERM_STAT_XMIT(status);
        int recv_status = USLOSS_TERM_STAT_RECV(status);


        // read from the terminal
        if (recv_status == USLOSS_DEV_BUSY) {
            // a character has been received in the status register

            // buffer the character
            term->buf[buf_idx++] = ch;

            if (buf_idx == MAXLINE || ch == '\n') {
                // reset buf index
                buf_idx = 0;

                // conditionally send the buffer to the terminal mailbox
                // +1 for null terminator
                gain_mutex(__func__);

                MboxCondSend(term->mbox, term->buf, strlen(term->buf)+1);
                release_mutex(__func__);

                // zero out buffer
                explicit_bzero(term->buf, MAXLINE+1);

                // if there is a process on the read queue, deliver to it
                if (term->read_queue) {

                    // gain the mutex
                    gain_mutex(__func__);

                    // dequeue the request process
                    rw_req *req = term->read_queue;
                    term->read_queue = term->read_queue->next;

                    // deliver the chars from the terminal line
                    MboxCondRecv(term->mbox, req->buf, MAXLINE);

                    // record the length read
                    int buf_len = strlen(req->buf);
                    *req->lenOut = (buf_len < req->bufSize) ? buf_len : req->bufSize;

                    // release the mutex before unblocking the process
                    release_mutex(__func__);

                    // unblock the process
                    unblockProc(req->pid);
                }
            }
        }

        if (xmit_status == USLOSS_DEV_READY) {
            // ready to write a character out
            
            if (term->write_queue) {
                gain_mutex(__func__);
                rw_req *req = term->write_queue;

                if (req->cur_buf_idx < req->bufSize) {
                    // read the next character from the buffer

                    /*USLOSS_Console("%d %d %d \n", unit, req->cur_buf_idx, req->bufSize);*/
                    char ch_to_write = req->buf[req->cur_buf_idx++];

                    /*USLOSS_Console("%c\n", ch_to_write);*/
                    // put together a control word to write to the control reg
                    int cr_val = 0;
                    cr_val = USLOSS_TERM_CTRL_CHAR(cr_val, ch_to_write);
                    cr_val = USLOSS_TERM_CTRL_XMIT_INT(cr_val);
                    cr_val = USLOSS_TERM_CTRL_RECV_INT(cr_val);
                    cr_val = USLOSS_TERM_CTRL_XMIT_CHAR(cr_val);

                    int err = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *)(long)cr_val);
                    if (err == USLOSS_DEV_INVALID) {
                        USLOSS_Console("ERROR: Failed to write character %c to terminal %d\n", ch_to_write, unit);
                        release_mutex(__func__);
                        USLOSS_Halt(1);
                    }

                    release_mutex(__func__);

                } else {
                    // write the len out to the req
                    *req->lenOut = req->bufSize;

                    // pop the process off the queue and wake it up
                    term->write_queue = term->write_queue->next;
                    release_mutex(__func__);

                    unblockProc(req->pid);
                }
            }
        } 

        if (recv_status == USLOSS_DEV_ERROR) {
            // an error has occurred
            USLOSS_Console("ERROR: After retrieving terminal status, the receive status is USLOSS_DEV_ERROR!\n");
            USLOSS_Halt(1);
        }

        // turn on xmit interrupts here somewhere?


        // use macros to put together a control word to write to the control
        // register via USLOSS_DeviceOutput

    }

}


/* pause the current process for the specified number of seconds */
void kern_sleep(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // unpack arguments
    int secs = (int)(long)arg->arg1;
    int clock_cycles_to_wait = 10*secs; // since int fires every ~ 100ms

    // check for invalid argument
    if (secs < 0) {
        arg->arg4 = (void *)(long)-1;
        return;
    }

    // put the process into the sleep queue before sleeping
    pcb cur_proc = {
        .pid = getpid(),
        .wakeup_cycle = num_cycles_since_start + clock_cycles_to_wait,
        .next = NULL
    };

    // gain mutex before accessing shared variable
    gain_mutex(__func__);

    put_into_sleep_queue(&cur_proc);

    // release mutex before blocking
    release_mutex(__func__);

    // block
    blockMe();

    // regain mutex after blocking
    gain_mutex(__func__);

    // repack success return value
    arg->arg4 = (void *)(long)0;

    // release mutex
    release_mutex(__func__);
}

void kern_term_read(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    char *buf     =    (char *)arg->arg1;
    int   bufSize = (int)(long)arg->arg2;
    int   unit    = (int)(long)arg->arg3;
    int   lenOut  =                   -1;

    // check for invalid inputs
    if (!buf || bufSize <= 0 || !(0 <= unit && unit < 4)) {
        arg->arg4 = (void *)(long)-1;
        release_mutex(__func__);
        return;
    }

    // pack the arguments into an easily-sendable form
    rw_req req = {
        .pid     = getpid(),
        .buf     =      buf,
        .bufSize =  bufSize,
        .lenOut  =  &lenOut,
        .next    =     NULL
    };

    // retrieve a reference to the appropriate terminal
    Terminal *term = &terms[unit];

    // add the request to the queue
    put_into_term_queue(&req, &term->read_queue);

    // release mutex before blocking
    release_mutex(__func__);

    // block while waiting for request to be fulfilled
    blockMe();

    // regain mutex after blocking
    gain_mutex(__func__);

    // repack return values
    assert(lenOut != -1);
    arg->arg2 = (void *)(long)lenOut; // the number of chars read
    arg->arg4 = (void *)(long)     0;

    // release mutex here
    release_mutex(__func__);
}

void kern_term_write(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    gain_mutex(__func__);
    // unpack arguments
    char *buf     =    (char *)arg->arg1;
    int   bufSize = (int)(long)arg->arg2;
    int   unit    = (int)(long)arg->arg3;
    int   lenOut  =                   -1;

    // check for invalid inputs
    if (!buf || bufSize <= 0 || !(0 <= unit && unit < 4)) {
        arg->arg4 = (void *)(long)-1;
        release_mutex(__func__);
        return;
    }

    ENABLE_TERM_XMIT_INT(unit);

    // pack the arguments into an easily-sendable form
    rw_req req = {
        .pid     = getpid(),
        .buf     =      buf,
        .bufSize =  bufSize,
        .lenOut  =  &lenOut,
        .next    =     NULL
    };


    // retrieve a reference to the appropriate terminal
    Terminal *term = &terms[unit];

    release_mutex(__func__);

    // grab write resource
    MboxSend(term->write_mbox, NULL, 0);

    // gain mutex
    gain_mutex(__func__);

    // add the request to the queue
    put_into_term_queue(&req, &term->write_queue);

    // release mutex before blocking
    release_mutex(__func__);

    // block while waiting for request to be fulfilled
    blockMe();

    // release write resource
    MboxRecv(term->write_mbox, NULL, 0);

    gain_mutex(__func__);

    // repack return values
    assert(lenOut != -1);
    arg->arg2 = (void *)(long)bufSize; // the number of chars written
    arg->arg4 = (void *)(long)      0;

    DISABLE_TERM_XMIT_INT(unit);

    release_mutex(__func__);
}


void kern_disk_read(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    void *diskBuffer =            arg->arg1;
    int   sectors    = (int)(long)arg->arg2;
    int   track      = (int)(long)arg->arg3;
    int   first      = (int)(long)arg->arg4;
    int   unit       = (int)(long)arg->arg5;

    // repack return values
    int status, success;
    arg->arg1 = (void *)(long)            status;
    arg->arg4 = (void *)(long)(success ? 0 : -1);

    // release mutex
    release_mutex(__func__);

}

void kern_disk_write(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    void *diskBuffer =            arg->arg1;
    int   sectors    = (int)(long)arg->arg2;
    int   track      = (int)(long)arg->arg3;
    int   first      = (int)(long)arg->arg4;
    int   unit       = (int)(long)arg->arg5;

    // repack return values
    int status, success;
    arg->arg1 = (void *)(long)            status;
    arg->arg4 = (void *)(long)(success ? 0 : -1);

    // release mutex
    release_mutex(__func__);

}

void kern_disk_size(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    int unit = (int)(long)arg->arg1;

    // repack return values
    int sector, track, disk, success;
    arg->arg1 = (void *)(long)sector;
    arg->arg2 = (void *)(long)track;
    arg->arg3 = (void *)(long)disk;
    arg->arg4 = (void *)(long)(success ? 0 : -1);

    // release mutex
    release_mutex(__func__);
}

/* HELPER FUNCTIONS */

/* insert a process into the sleep queue */
void put_into_sleep_queue(pcb *proc) {

    // retrieve the cycle number at which the process should wakeup
    int wakeup_cycle = proc->wakeup_cycle;

    // iterate to the place in the queue at which the process should be inserted
    pcb *prev = NULL;
    pcb *cur = sleep_queue;
    while (cur) {
        int cur_wakeup_cycle = cur->wakeup_cycle;
        if (wakeup_cycle <= cur_wakeup_cycle) break;

        prev = cur;
        cur = cur->next;
    }

    // insert the process into the sleep queue
    if (prev) {
        pcb *temp = prev->next;
        prev->next = proc;
        proc->next = temp;
    } else {
        proc->next = cur;
        sleep_queue = proc;
    }
}

void put_into_term_queue(rw_req *req, rw_req **queue) {
    // add the request to the back of the queue
    rw_req *prev = NULL;
    rw_req *cur = *queue;
    while (cur) {
        USLOSS_Console("iterating through queue\n");
        prev = cur;
        cur = cur->next;
    }

    if (!prev) {
        *queue = req;
    } else {
        cur->next = req;
    }
}
