#include "phase1.h"
#include "phase2.h"
#include "phase4.h"
#include "phase3_kernelInterfaces.h"
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
    int err = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE); \
    if (err == USLOSS_ERR_INVALID_PSR) { \
        USLOSS_Console("ERROR: Invalid PSR set while trying to change into user mode!\n"); \
        USLOSS_Halt(1); \
    } \
}

// enable interrupts
#define ENABLEINTS { \
    int err = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT); \
    if (err == USLOSS_ERR_INVALID_PSR) { \
        USLOSS_Console("ERROR: Invalid PSR set while trying to enable interrupts!\n"); \
        USLOSS_Halt(1); \
    } \
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

// macro for the size of a block on a sector of a track of the disk
// should be used for sizing bufs for rw operations on disk
#define BLOCKSZ 512   // the number of bytes in a sector
#define NUMSECTORS 16 // the number of sectors in a track

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

typedef enum {
    READ,
    WRITE
} Op;

typedef struct disk_req {
    int          pid;     // for unblocking
    Op            op;       // read/write
    void        *buf;   // to be read from or written to

    int       status; // for tracking whether an error occurred with the request
    int arg_validity;

    int  first_track;
    int   last_track;

    int first_sector;
    int  num_sectors;

    struct disk_req *next;
} disk_req;

typedef struct disk_state {
    int     rw_lock;
    int   cur_track;
    int  num_tracks;
    int is_blocked;
    int        pid;
    USLOSS_DeviceRequest cur_req;
    disk_req *queue;
} DiskState;

/* FUNCTION STUBS */
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase4_start_service_processes();
void put_into_sleep_queue(pcb *proc);
void put_into_term_queue(rw_req *req, rw_req **queue);
void put_into_disk_queue(disk_req *req, int unit);
void dump_disk_queue(int unit);
void dump_sleep_queue();
void dump_disk_state(int unit);

// system calls
void kern_sleep     (USLOSS_Sysargs *arg);
void kern_term_read (USLOSS_Sysargs *arg);
void kern_term_write(USLOSS_Sysargs *arg);
void kern_disk_read (USLOSS_Sysargs *arg);
void kern_disk_write(USLOSS_Sysargs *arg);
void kern_disk_size (USLOSS_Sysargs *arg);

// daemons
int sleepd(void *arg);
int termd (void *arg);
int diskd (void *arg);

// globals
int mutex;
pcb *sleep_queue;
int num_cycles_since_start;

// devices
Terminal        terms[4];
DiskState disk_states[2];


void gain_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO GAIN THE MUTEX!\n", func);*/
    /*MboxSend(mutex, NULL, 0);*/
    kernSemP(mutex);
    /*USLOSS_Console("%s HAS GAINED THE MUTEX!\n", func);*/
}

void release_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO RELEASE THE MUTEX!\n", func);*/
    /*MboxRecv(mutex, NULL, 0);*/
    kernSemV(mutex);
    /*USLOSS_Console("%s HAS RELEASED THE MUTEX!\n", func);*/
}

void phase4_init() {
    // require kernel mode
    CHECKMODE;

    // initialize the mutex using an mbox
    /*mutex = MboxCreate(1, 0);*/
    mutex = kernSemCreate(1, &mutex);

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

    // initialize disk states
    for (int i = 0; i < 2; i++) {
        DiskState *disk_state = &disk_states[i];
        memset(disk_state, 0, sizeof(DiskState));
        disk_state->rw_lock =  MboxCreate(1,0);  // initialize rw sem with mbox
        disk_state->num_tracks = -1;
    }

    // release the mutex
    release_mutex(__func__);
}

void phase4_start_service_processes() {
    // spork the sleep daemon
    spork("sleepd", sleepd, NULL, USLOSS_MIN_STACK, 1);

    // spork the four terminal daemons - one for each terminal
    for (int i = 0; i < 4; i++) spork("termd", termd, (void *)(long)i, USLOSS_MIN_STACK, 5);

    // spork the two disk daemons - one for each disk
    for (int i = 0; i < 2; i++) spork("diskd", diskd, (void *)(long)i, USLOSS_MIN_STACK, 5);
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

                    char ch_to_write = req->buf[req->cur_buf_idx++];

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
    }
}

/*
 * complete disk op if possible
 * 
 * returns:
 *  USLOSS_DEV_READY: the disk has accepted and is executing an op
 *  USLOSS_DEV_BUSY: the disk is busy with another operation, a NOP
 *  USLOSS_DEV_ERROR: the disk encountered an error
 */
int send_op_to_disk(int unit) {
    DiskState *disk_state = &disk_states[unit];
    disk_req *req_queue = disk_state->queue;

    req_queue->arg_validity = USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &(req_queue->status));

    // if invalid argument return
    if (req_queue->arg_validity == USLOSS_DEV_INVALID) {
        USLOSS_Console("ERROR: USLOSS_DeviceInput returned USLOSS_DEV_INVALID! Leaving %s.\n", __func__);
        return USLOSS_DEV_INVALID;
    }


    if (req_queue->status == USLOSS_DEV_READY) {
        // send the current request to the disk
        req_queue->arg_validity = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &disk_state->cur_req);
        /*USLOSS_Console("here\n");*/
        waitDevice(USLOSS_DISK_DEV, unit, &req_queue->status);
        /*USLOSS_Console("after blocking\n");*/

        // if invalid argument return
        if (req_queue->arg_validity == USLOSS_DEV_INVALID) {
            USLOSS_Console("ERROR: USLOSS_DeviceOutput returned USLOSS_DEV_INVALID! Leaving %s.\n", __func__);
            return USLOSS_DEV_INVALID;
        }

        if (req_queue->status == USLOSS_DEV_ERROR) {
            USLOSS_Console("ERROR: waitDevice filled status with USLOSS_DEV_ERROR! Leaving %s.\n", __func__);
            return USLOSS_DEV_ERROR;
        }
    }
    return req_queue->status;
}

/* disk daemon */
int diskd(void *arg) {
    int unit = (int)(long)arg;
    DiskState *disk_state = &disk_states[unit];

    /* process the queue of rw requests */
    while (1) {
        // process a request if there is one
        if (disk_state->queue) {
            // make the entirety of this section atomic
            gain_mutex(__func__);

            // fulfill request
            while (disk_state->queue->num_sectors != 0) {

                // build request to fulfill
                if (disk_state->cur_track != disk_state->queue->first_track) {
                    // seek to the required first track
                    disk_state->cur_req.opr = USLOSS_DISK_SEEK;
                    disk_state->cur_req.reg1 = (void *)(long)disk_state->queue->first_track;
                } else {
                    // read/write a block
                    disk_state->cur_req.reg1 = (void *)(long)disk_state->queue->first_sector;
                    disk_state->cur_req.reg2 = disk_state->queue->buf;
                    switch (disk_state->queue->op) {
                        case READ:
                            disk_state->cur_req.opr = USLOSS_DISK_READ;
                            break;
                        case WRITE:
                            disk_state->cur_req.opr = USLOSS_DISK_WRITE;
                            break;
                    }
                }

                int send_outcome = send_op_to_disk(unit);
                if (send_outcome == USLOSS_DEV_READY) {
                    switch (disk_state->cur_req.opr) {
                        // update current track
                        case USLOSS_DISK_SEEK:
                            disk_state->cur_track = disk_state->queue->first_track;
                            break;
                        // update parameters as necessary
                        case USLOSS_DISK_READ:
                        case USLOSS_DISK_WRITE:
                            disk_state->queue->num_sectors--;
                            disk_state->queue->first_sector = (disk_state->queue->first_sector + 1) % USLOSS_DISK_TRACK_SIZE;
                            disk_state->queue->buf += USLOSS_DISK_SECTOR_SIZE;
                            if (disk_state->queue->first_sector == 0) disk_state->queue->first_track++;
                            break;
                    }
                    // reset request slot
                    memset(&disk_state->cur_req, 0, sizeof(USLOSS_DeviceRequest));
                } else if (send_outcome == USLOSS_DEV_ERROR) {
                    // dequeue/unblock process if there was an error
                    USLOSS_Console("ERROR: send_op_to_disk returned USLOSS_DEV_ERROR! Breaking out of while loop and dequeueing process.\n");
                    break;
                }

                // do nothing if the disk is busy (want to send op again)
                
            }

            // dequeue the request that has completed
            int pid_toUnblock = disk_state->queue->pid;

            disk_state->queue = disk_state->queue->next;

            // release mutex before unblocking
            release_mutex(__func__);

            unblockProc(pid_toUnblock);

        } else {
            // block
            disk_state->pid = getpid();
            disk_state->is_blocked = 1;
            blockMe();
        }
    }
}


/* kernel-side syscalls */

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

// lets try using spin locks... because why not
void kern_disk_size(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // unpack argument
    int unit = (int)(long)arg->arg1;

    // error check for invalid arguments
    if (!(unit == 0 || unit == 1)) {
        arg->arg4 = (void *)(long)-1;
        return;
    }

    DiskState *disk_state = &disk_states[unit];

    // perform operation if it hasn't been performed before and store the result
    if (disk_state->num_tracks == -1) {
        int status;
        // wait for the disk to become available before beginning an op
        do {
            USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &status);
            if (status == USLOSS_DEV_ERROR) USLOSS_Console("device input in disk size returned error code\n");
        } while (status != USLOSS_DEV_READY); // <await status = USLOSS_DEV_READY>

        // gain mutex
        gain_mutex(__func__);

        // construct device request
        USLOSS_DeviceRequest req = {
            .opr  = USLOSS_DISK_TRACKS,
            .reg1 = &disk_state->num_tracks,
            .reg2 = NULL
        };

        // send request to device
        USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &req);

        // release mutex
        release_mutex(__func__);

        // wait for request to complete
        waitDevice(USLOSS_DISK_DEV, unit, &status);
        if (status == USLOSS_DEV_ERROR) USLOSS_Console("wait device in disk size returned error code\n");
        
        // TODO: error check status?
    }

    // repack return values
    arg->arg1 = (void *)(long)USLOSS_DISK_SECTOR_SIZE; // no. of bytes   in a block always 512
    arg->arg2 = (void *)(long)USLOSS_DISK_TRACK_SIZE;  // no. of sectors in a track always  16
    arg->arg3 = (void *)(long)disk_state->num_tracks;  // no. of tracks  in a disk
    arg->arg4 = (void *)(long)0;

    // release mutex
    release_mutex(__func__);
}

void kern_disk_read(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    /*gain_mutex(__func__);*/

    // unpack arguments
    void *diskBuffer =            arg->arg1;
    int   sectors    = (int)(long)arg->arg2;
    int   track      = (int)(long)arg->arg3;
    int   first      = (int)(long)arg->arg4;
    int   unit       = (int)(long)arg->arg5;

    // request the number of tracks of the disk
    USLOSS_Sysargs sys_arg;
    sys_arg.arg1 = (void *)(long)unit;

    kern_disk_size(&sys_arg);

    // unpack the returned values
    int sector_sz = (int)(long)sys_arg.arg1; // no. of bytes in a sector
    int track_sz  = (int)(long)sys_arg.arg2; // no. of sectors in a track
    int disk_sz   = (int)(long)sys_arg.arg3; // no. of tracks in the disk
    int success   = (int)(long)sys_arg.arg4; // -1 invalid unit
    
    // error check unit
    if (success == -1) {
        arg->arg4 = (void *)(int)-1;
        return;
    }

    // error check the rest of the arguments
    if (
            diskBuffer == NULL ||
            !(0 <= track && track < disk_sz) ||
            !(0 <= first && first < track_sz) ||
            (sectors > track_sz * disk_sz)
       )
    {
        arg->arg4 = (void *)(long)-1;
        return;
    }

    // create request
    disk_req req = {
        .pid          = getpid(),
        .op           = READ,
        .buf          = diskBuffer,

        .first_track  = track,
        .last_track   = track + ((first + sectors) / track_sz),

        .first_sector = first,
        .num_sectors  = sectors,

        .next         = NULL
    };

    // add request to queue for disk daemon to process
    put_into_disk_queue(&req, unit);

    // block until request is fulfilled
    blockMe();

    // repack return values
    arg->arg1 = (void *)(long)req.status;
    arg->arg4 = (void *)(long)req.arg_validity;
}

void kern_disk_write(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // unpack arguments
    void *diskBuffer =            arg->arg1;
    int   sectors    = (int)(long)arg->arg2;
    int   track      = (int)(long)arg->arg3;
    int   first      = (int)(long)arg->arg4;
    int   unit       = (int)(long)arg->arg5;

    // request the number of tracks of the disk
    USLOSS_Sysargs sys_arg;
    sys_arg.arg1 = (void *)(long)unit;

    kern_disk_size(&sys_arg);

    // unpack the returned values
    int sector_sz = (int)(long)sys_arg.arg1; // no. of bytes in a sector
    int track_sz  = (int)(long)sys_arg.arg2; // no. of sectors in a track
    int disk_sz   = (int)(long)sys_arg.arg3; // no. of tracks in the disk
    int success   = (int)(long)sys_arg.arg4; // -1 invalid unit
    
    // error check unit
    if (success == -1) {
        arg->arg4 = (void *)(int)-1;
        return;
    }

    // error check the rest of the arguments
    if (
            diskBuffer == NULL ||
            !(0 <= track && track < disk_sz) ||
            !(0 <= first && first < track_sz) ||
            (sectors > track_sz * disk_sz)
       )
    {
        arg->arg4 = (void *)(long)-1;
        return;
    }

    // create request
    disk_req req = {
        .pid          = getpid(),
        .op           = WRITE,
        .buf          = diskBuffer,

        .first_track  = track,
        .last_track   = track + ((first + sectors) / track_sz),

        .first_sector = first,
        .num_sectors  = sectors,

        .next         = NULL
    };

    // add request to queue for disk daemon to process
    put_into_disk_queue(&req, unit);

    // block until request is fulfilled
    blockMe();

    // repack return values
    arg->arg1 = (void *)(long)req.status;
    arg->arg4 = (void *)(long)req.arg_validity;
}


/* HELPER FUNCTIONS */

/* insert a process into the sleep queue */
void put_into_sleep_queue(pcb *proc) {

    // retrieve the cycle number at which the process should wakeup
    int wakeup_cycle = proc->wakeup_cycle;

    // iterate to the place in the queue at which the process should be inserted
    pcb *prev = NULL;
    pcb *cur  = sleep_queue;
    while (cur) {
        int cur_wakeup_cycle = cur->wakeup_cycle;
        if (wakeup_cycle <= cur_wakeup_cycle) break;

        prev = cur;
        cur  = cur->next;
    }

    // insert the process into the sleep queue
    if (prev) {
        pcb *temp  = prev->next;
        prev->next = proc;
        proc->next = temp;
    } else {
        proc->next  =  cur;
        sleep_queue = proc;
    }
}

void dump_sleep_queue() {
    dumpProcesses();
    pcb *cur = sleep_queue;
    while (cur) {
        USLOSS_Console(
                "pid = %d\n"
                "wakeup cycle = %d\n",
                cur->pid,
                cur->wakeup_cycle
                );
        cur = cur->next;
    }

}

void put_into_term_queue(rw_req *req, rw_req **queue) {
    // add the request to the back of the queue
    rw_req *prev = NULL;
    rw_req *cur = *queue;
    while (cur) {
        prev       = cur;
        cur  = cur->next;
    }

    if (!prev) {
        *queue = req;
    } else {
        cur->next = req;
    }
}

void put_into_disk_queue(disk_req *req, int unit) {

    DiskState *disk_state = &disk_states[unit];

    // gain the mutex before beginning
    gain_mutex(__func__);

    disk_req **queue =     &disk_state->queue;
    disk_req *cur    =                 *queue;
    disk_req *next   = cur ? cur->next : NULL;

    /* holy mother of god this took me three hours to figure out */
    while (
            cur  &&
            next &&
                !(
                  (
                    // gap between cur and next
                    // req fits in between
                    (cur->last_track <  next->first_track)  &&
                    (cur->last_track <=  req->first_track)  &&
                    (req->last_track <= next->first_track)
                  )
                  ||
                  (  
                    // cur, next touch
                    // req is (cur->last_track, next->first_track)
                    // which are equal
                    (cur->last_track == next->first_track) &&
                    (cur->last_track == req->first_track)  &&
                    (req->last_track == next->first_track)
                  )
                  ||
                  (  
                    // cur overlaps next
                    // and req fits after cur
                    (cur->last_track >  next->first_track) &&
                    (cur->last_track <=  req->first_track)
                  )
                  ||
                  (  
                    // cur overlaps next
                    // and req fits before next
                    (cur->last_track >  next->first_track)  &&
                    (cur->last_track >   req->first_track)  &&
                    (req->last_track <= next->first_track)
                  )
                 )
          )
    {
        cur  = next;
        next = next->next;
    }

    if (!cur) {
        *queue = req;
    } else {
        req->next = cur->next;
        cur->next = req;
    }

    // release the mutex
    release_mutex(__func__);

    // unblock disk if necessary
    if (disk_state->is_blocked) {
        // unblock
        disk_state->is_blocked = 0;
        unblockProc(disk_state->pid);
    }
}

void dump_disk_queue(int unit) {
    DiskState *disk_state = &disk_states[unit];
    disk_req *cur = disk_state->queue;
    USLOSS_Console("cur status = %d\n", cur != NULL);
    while (cur) {
        USLOSS_Console(
                "pid = %d\n"
                "op = %d\n"
                "buf = %s\n"
                "first_track = %d\n"
                "last_track = %d\n"
                "first_sector = %d\n"
                "num_sectors = %d\n\n",
                cur->pid,
                cur->op,
                (char *)cur->buf,
                cur->first_track,
                cur->last_track,
                cur->first_sector,
                cur->num_sectors
                );
        cur = cur->next;
    }

}

void dump_disk_state(int unit) {
    DiskState *disk_state = &disk_states[unit];

    USLOSS_Console(
            "rw_lock = %d\n"
            "cur_track = %d\n"
            "num_tracks = %d\n"
            "is_blocked = %d\n"
            "pid = %d\n"
            "cur_req.opr = %d\n"
            "cur_req.reg1 = %d\n"
            "cur_req.reg2 = %s\n"
            "queue ptr = %p\n",
            disk_state->rw_lock,
            disk_state->cur_track,
            disk_state->num_tracks,
            disk_state->is_blocked,
            disk_state->pid,
            disk_state->cur_req.opr,
            (int)(long)disk_state->cur_req.reg1,
            (char *)disk_state->cur_req.reg2,
            disk_state->queue
            );

}
