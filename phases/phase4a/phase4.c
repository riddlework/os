#include "phase2.h"
#include "phase3_kernelInterfaces.h"
#include "phase4.h"
#include <usloss.h>

// a macro for checking that the program is currently running in kernel mode
#define CHECKMODE { \
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) { \
        USLOSS_Console("ERROR: Someone attempted to call a function while in user mode!\n"); \
        USLOSS_Halt(1);  \
    } \
}

// function stubs
/*void require_kernel_mode(const char *func);*/
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase4_start_service_processes();

void kern_sleep     (USLOSS_Sysargs *arg);
void kern_term_read (USLOSS_Sysargs *arg);
void kern_term_write(USLOSS_Sysargs *arg);
void kern_disk_read (USLOSS_Sysargs *arg);
void kern_disk_write(USLOSS_Sysargs *arg);
void kern_disk_size (USLOSS_Sysargs *arg);

// globals
int mutex;

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
    // need to start four daemons here?
    // start sleep daemon here

}

void kern_sleep(USLOSS_Sysargs *arg) {
    // check for kernel mode
    CHECKMODE;

    // gain mutex
    gain_mutex(__func__);

    // arg1 - seconds to sleep for

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
