#include "phase1.h"
#include "phase2.h"
#include "phase3_usermode.h"
#include "phase3_kernelInterfaces.h"
#include "phase4.h"
#include <usloss.h>
#include <stdlib.h>
#include <string.h>

#include "usyscall.h" // TODO: delete this later

// a macro for checking that the program is currently running in kernel mode
#define CHECKMODE { \
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) { \
        USLOSS_Console("ERROR: Someone attempted to call a function while in user mode!\n"); \
        USLOSS_Halt(1);  \
    } \
}

// change into user mode from kernel mode
#define SETUSERMODE { \
    USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE); \
}

// struct definitions
typedef struct pcb {
    int pid;
    int wakeup_cycle;
    struct pcb *next;
} pcb;

typedef struct arg_package {
    char *buf;
    int bufSize;
    int unit;
} arg_package;


/* FUNCTION STUBS */
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase4_start_service_processes();
void put_into_sleep_queue(pcb *proc);

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
pcb *sleep_head;
int num_cycles_since_start;

static pcb sleep_queue[MAXPROC];

void gain_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO GAIN THE MUTEX!\n", func);*/
    int fail = kernSemP(mutex);
    if (fail) USLOSS_Console("ERROR: P(mutex) failed!\n");
    /*USLOSS_Console("%s HAS GAINED THE MUTEX!\n", func);*/
}

void release_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO RELEASE THE MUTEX!\n", func);*/
    int fail = kernSemV(mutex);
    if (fail) USLOSS_Console("ERROR: V(mutex) failed!\n");
    /*USLOSS_Console("%s HAS RELEASED THE MUTEX!\n", func);*/
}

void phase4_init() {
    // require kernel mode
    CHECKMODE;

    // initialize the mutex using a semaphore
    int fail = kernSemCreate(1, &mutex);﻿
    if (fail) USLOSS_Console("ERROR: Failed to create semaphore!\n");

    // gain the mutex
    gain_mutex(__func__);
    
    // zero out the shadow process table
    for (int i = 0; i < MAXPROC; i++) memset(&sleep_queue[i], 0, sizeof(pcb));
    
    // load the system call vec
    systemCallVec[SYS_SLEEP]     =      kern_sleep;
    systemCallVec[SYS_TERMREAD]  =  kern_term_read;
    systemCallVec[SYS_TERMWRITE] = kern_term_write;
    systemCallVec[SYS_DISKREAD]  =  kern_disk_read; 
    systemCallVec[SYS_DISKWRITE] = kern_disk_write; 
    systemCallVec[SYS_DISKSIZE]  =  kern_disk_size; 


    // TODO:
    // definitely turn on the terminal read (recv) interrupt here
    // maybe turn on the terminal write (xmit) interrupt here -- or only when process is writing?

    // read the disk sizes here and save them as global varaibles
    // queuing and sequencing disk requests

    // release the mutex
    release_mutex(__func__);
}

void phase4_start_service_processes() {
    // spork the sleep daemon
    spork("sleepd", sleepd, NULL, USLOSS_MIN_STACK, 3);
    spork("termd", termd, NULL, USLOSS_MIN_STACK, 3);
}

/* DAEMONS */

/* sleep daemon */
int sleepd(void *arg) {

    while (1) {
        // call wait device to wait for a clock interrupt
        int status;
        waitDevice(USLOSS_CLOCK_INT, 0, &status);
        num_cycles_since_start++;

        // wakeup any cycles whose wakeup time has arrived/passed
        while (sleep_head && num_cycles_since_start >= sleep_head->wakeup_cycle) {
            
            // gain mutex here since the sleep queue is a shared 
            gain_mutex(__func__);
            int pid_toUnblock = sleep_head->pid;
            sleep_head = sleep_head->next;
            release_mutex(__func__);

            unblockProc(pid_toUnblock);
        }
    }
}

/* terminal daemon */
int termd(void *arg) {

    // waitDevice waits for a terminal interrupt (recv or xmit??)
    // and reads the status register
    // we want to write to the control register
    while (1) {
        int status;
        int unit = MboxRecv()
        waitDevice(USLOSS_TERMI_INT, )

    }

}


/* pause the current process for the specified number of seconds */
void kern_sleep(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    int secs = (int)(long)arg->arg1;
    int clock_cycles_to_wait = 10*secs; // since int fires every ~ 100ms

    // check for invalid argument
    if (secs < 0) {
        arg->arg4 = (void *)(long)-1;
        return;
    }

    // put the process into the sleep queue before sleeping
    int pid = getpid();
    pcb *cur_proc = &sleep_queue[pid % MAXPROC];
    cur_proc->pid = pid;
    cur_proc->wakeup_cycle = num_cycles_since_start + clock_cycles_to_wait;
    put_into_sleep_queue(cur_proc);

    // release mutex before blocking
    release_mutex(__func__);

    // block
    blockMe();

    // regain mutex after blocking
    gain_mutex(__func__);

    // zero out the slot so it can be reused
    memset(&sleep_queue[pid % MAXPROC], 1, sizeof(pcb));

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

    arg_package args = {
        .buf     =     buf,
        .bufSize = bufSize,
        .unit    =    unit
    };

    // send the arguments to the term daemon
    // TODO: Change the mbox number here
    MboxSend(0, &args, sizeof(arg_package));


    // recv the length back from the term daemon
    // TODO: change the mbox number here
    int lenOut;
    MboxRecv(0, &lenOut, sizeof(int));

    // repack return values
    arg->arg4 = (void *)(long)lenOut;

    // release mutex
    release_mutex(__func__);
}

void kern_term_write(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    char *buf     =    (char *)arg->arg1;
    int   bufSize = (int)(long)arg->arg2;
    int   unit    = (int)(long)arg->arg3;

    arg_package args = {
        .buf     =     buf,
        .bufSize = bufSize,
        .unit    =    unit
    };

    // send the arguments to the term daemon
    // TODO: Change the mbox number here
    MboxSend(0, &args, sizeof(arg_package));

    // recv the length back from the term daemon
    // TODO: change the mbox number here
    int lenOut;
    MboxRecv(0, &lenOut, sizeof(int));

    // repack return value
    arg->arg4 = (void *)(long)lenOut;

    // release mutex
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
    pcb *cur = sleep_head;
    while (cur) {
        int cur_wakeup_cycle = cur->wakeup_cycle;
        if (wakeup_cycle < cur_wakeup_cycle) break;

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
        sleep_head = proc;
    }
}
