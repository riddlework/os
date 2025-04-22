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

// function stubs
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase4_start_service_processes();

// system calls
void kern_sleep     (USLOSS_Sysargs *arg);
void kern_term_read (USLOSS_Sysargs *arg);
void kern_term_write(USLOSS_Sysargs *arg);
void kern_disk_read (USLOSS_Sysargs *arg);
void kern_disk_write(USLOSS_Sysargs *arg);
void kern_disk_size (USLOSS_Sysargs *arg);

// daemons
int sleepd(void *arg);

// struct definitions
typedef struct pcb {
    int pid;
    int is_alive;
    int wakeup_cycle;
    int mbox;
} pcb;

// globals
int mutex;
int sleep_mbox;

int num_cycles_since_start;

static pcb shadow_proc_table[MAXPROC];

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
    int fail = kernSemCreate(1, &mutex);
    if (fail) USLOSS_Console("ERROR: Failed to create semaphore!\n");

    // gain the mutex
    gain_mutex(__func__);
    
    // zero out the shadow process table
    for (int i = 0; i < MAXPROC; i++) memset(&shadow_proc_table[i], 0, sizeof(pcb));
    
    // load the system call vec
    systemCallVec[SYS_SLEEP]     =      kern_sleep;
    systemCallVec[SYS_TERMREAD]  =  kern_term_read;
    systemCallVec[SYS_TERMWRITE] = kern_term_write;
    systemCallVec[SYS_DISKREAD]  =  kern_disk_read; 
    systemCallVec[SYS_DISKWRITE] = kern_disk_write; 
    systemCallVec[SYS_DISKSIZE]  =  kern_disk_size; 

    // create sleep mbox
    /*sleep_mbox = MboxCreate()*/


    // TODO:
    // definitely turn on the terminal read (recv) interrupt here
    // maybe turn on the terminal write (xmit) interrupt here -- or only when process is writing?

    // read the disk sizes here and save them as global varaibles
    // queuing and sequencing disk requests

    // release the mutex
    release_mutex(__func__);
}

void phase4_start_service_processes() {
    // need to start four daemons here?
    // start sleep daemon here
    int pid = spork("sleepd", sleepd, NULL, USLOSS_MIN_STACK, 5);
    USLOSS_Console("phase4_start_service_processes(): Spork %d\n", pid);

}

int sleepd(void *arg) {
    // call waitDevice here?
    // waitDevice give syou the current status (time)
    // every 10 interrupts is one second

    while (1) {
        int status;
        waitDevice(USLOSS_CLOCK_INT, 0, &status);
        /*USLOSS_Console("WAIT DEVICE FILLED STATUS WITH: %d\n", status);*/
        num_cycles_since_start++;
        USLOSS_Console("time of day before for loop = %d\n", currentTime());
        USLOSS_Console("num_cycles_since_start before for loop = %d\n", num_cycles_since_start);
        for (int i = 0; i < MAXPROC; i++) {
            pcb *proc = &shadow_proc_table[i];
            if (proc->is_alive) {
                /*USLOSS_Console("wakeup_cycle %d\n", proc->wakeup_cycle);*/
                if (num_cycles_since_start >= proc->wakeup_cycle) {
                    MboxRecv(proc->mbox, NULL, 0);
                }

            }

        }
        USLOSS_Console("time of day after for loop = %d\n", currentTime());

    }

}

// pause the current process for the specified number of seconds
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


    int pid = getpid();
    /*USLOSS_Console("Process %d wants to sleep for %d seconds.\n", pid, secs);*/
    pcb *cur_proc = &shadow_proc_table[pid % MAXPROC];
    cur_proc->pid = pid;
    cur_proc->is_alive = 1;
    cur_proc->wakeup_cycle = num_cycles_since_start + clock_cycles_to_wait;
    cur_proc->mbox = MboxCreate(0,0);

    USLOSS_Console("Process %d: We are at CC# %d, so to sleep for %d seconds, we need to sleep for %d CCs and wake up at CC# %d\n", pid, num_cycles_since_start, secs, clock_cycles_to_wait, cur_proc->wakeup_cycle);
    

    // release mutex before blocking
    release_mutex(__func__);

    // block
    MboxSend(cur_proc->mbox, NULL, 0);

    // regain mutex
    gain_mutex(__func__);

    // release the mailbox and zero out the slot
    MboxRelease(cur_proc->mbox);
    memset(&shadow_proc_table[pid % MAXPROC], 1, sizeof(pcb));

    // repack return values
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

    // repack return values
    int lenOut;
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

    // repack return value
    int lenOut;
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
