//header files
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
// change these to <> when done
#include "usloss.h"
#include "usyscall.h"

#include <stdlib.h>

// may want to include stdlib stuff?

// function stubs
int spork_trampoline(void *arg);
void kernelSpawn(USLOSS_Sysargs *args);
void kernelWait(USLOSS_Sysargs *args);
void kernelTerminate(USLOSS_Sysargs *args);
void kernelSemCreate(USLOSS_Sysargs *args);
void kernelSemP(USLOSS_Sysargs *args);
void kernelSemV(USLOSS_Sysargs *args);
void kernelGetTimeOfDay(USLOSS_Sysargs *args);
void kernelGetPID(USLOSS_Sysargs *args);


// global variables
int trampoline_Mbox;
int trampoline_lock;
int phase3init_lock;
int phase3ssp_lock;
int spawn_lock;
int wait_lock;
int terminate_lock;
int semcreate_lock;
int semp_lock;
int semv_lock;
int gettimeofday_lock;
int getpid_lock;

// lock helper functions
void gain_lock(int lock) { MboxSend(lock, NULL, 0); }

void release_lock(int lock) { MboxRecv(lock, NULL, 0); }

// phase functions
void phase3_init() {
    // gain lock
    gain_lock(phase3init_lock);

    // create global mbox
    trampoline_Mbox = MboxCreate(1, sizeof(int (*)(void *)));
    trampoline_lock = MboxCreate(1,0);
    phase3init_lock = MboxCreate(1,0);
    phase3ssp_lock = MboxCreate(1,0);
    spawn_lock = MboxCreate(1,0);
    wait_lock = MboxCreate(1,0);
    terminate_lock = MboxCreate(1,0);
    semcreate_lock = MboxCreate(1,0);
    semp_lock = MboxCreate(1,0);
    semv_lock = MboxCreate(1,0);
    gettimeofday_lock = MboxCreate(1,0);
    getpid_lock = MboxCreate(1,0);

    // fill systemCallVec
    systemCallVec[SYS_SPAWN] = kernelSpawn;
    systemCallVec[SYS_WAIT] = kernelWait;
    systemCallVec[SYS_TERMINATE] = kernelTerminate;
    systemCallVec[SYS_GETTIMEOFDAY] = kernelGetTimeOfDay;
    systemCallVec[SYS_GETPID] = kernelGetPID;
    systemCallVec[SYS_SEMCREATE] = kernelSemCreate;
    systemCallVec[SYS_SEMP] = kernelSemP;
    systemCallVec[SYS_SEMV] = kernelSemV;
    
    // release lock
    release_lock(phase3init_lock);
}

void phase3_start_service_processes() {
    // gain lock
    gain_lock(phase3ssp_lock);


    // release lock
    release_lock(phase3ssp_lock);
}


void kernelSpawn(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(spawn_lock);
    
    // unpack arguments
    int (*func)(void *) = (int (*)(void *)) args->arg1;
    void *arg = args->arg2;
    int stack_size = (int)(long) args->arg3;
    int priority = (int)(long) args->arg4;
    char *name = args->arg5;


    // call trampoline function
    MboxSend(trampoline_Mbox, (void *)func, sizeof(int (*)(void *)));
    int retval = spork(name, spork_trampoline, arg, stack_size, priority);

    args->arg1 = (void *)(long)retval;
    if (retval == -1) args->arg4 = (void *)(long) -1;
    else args->arg4 = 0;

    // release lock
    release_lock(spawn_lock);
}


int spork_trampoline(void *arg) {
    // gain lock
    gain_lock(trampoline_lock);

    // change the psr to change to user mode
    unsigned int old_psr = USLOSS_PsrGet();
    int psr_status = USLOSS_PsrSet(old_psr & 0b1110);
    
    // receive from the mailbox
    void *msg;
    MboxRecv(trampoline_Mbox, msg, sizeof(int (*)(void *)));

    int (*func)(void *) = (int (*)(void *)) msg;
    func(arg);

    // release lock
    release_lock(trampoline_lock);
}

void kernelWait(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(wait_lock);

    // call join
    int status;
    int retval = join(&status);

    // fill outputs
    args->arg1 = (void *)(long) retval;
    args->arg2 = (void *)(long) status;
    if (retval == -2) args->arg4 = (void *)(long) -2;
    else retval = 0;

    // release lock
    release_lock(wait_lock);
}

void kernelTerminate(USLOSS_Sysargs *args){
    // gain lock
    gain_lock(terminate_lock);

    // release lock
    release_lock(terminate_lock);
}

void kernelSemCreate(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(semcreate_lock);

    // release lock
    release_lock(semcreate_lock);
}

void kernelSemP(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(semp_lock);

    // release lock
    release_lock(semp_lock);
}

void kernelSemV(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(semv_lock);

    // release lock
    release_lock(semv_lock);
}

void kernelGetTimeOfDay(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(gettimeofday_lock);

    // release lock
    release_lock(gettimeofday_lock);
}

void kernelGetPID(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock(getpid_lock);

    // release lock
    release_lock(getpid_lock);
}

