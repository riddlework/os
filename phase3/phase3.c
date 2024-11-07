//header files
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include <usloss.h>
#include <usyscall.h>

#include <stdlib.h>
#include <string.h>


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

// structs
typedef struct semaphore {
    int value;
    int is_alive;
} Semaphore;

// global variables
int trampoline_Mbox;
int lock;
Semaphore semaphores[MAXSEMS];

// lock helper functions
void gain_lock() { MboxSend(lock, NULL, 0); }

void release_lock() { MboxRecv(lock, NULL, 0); }

// phase functions
void phase3_init() {
    // create global mbox
    trampoline_Mbox = MboxCreate(1, sizeof(int (*)(void *)));
    lock = MboxCreate(1,0);

    // fill systemCallVec
    systemCallVec[SYS_SPAWN] = kernelSpawn;
    systemCallVec[SYS_WAIT] = kernelWait;
    systemCallVec[SYS_TERMINATE] = kernelTerminate;
    systemCallVec[SYS_GETTIMEOFDAY] = kernelGetTimeOfDay;
    systemCallVec[SYS_GETPID] = kernelGetPID;
    systemCallVec[SYS_SEMCREATE] = kernelSemCreate;
    systemCallVec[SYS_SEMP] = kernelSemP;
    systemCallVec[SYS_SEMV] = kernelSemV;
    
    // memset semaphore array to 0
    for (int i=0; i < MAXSEMS; i++) memset(&semaphores[i], 0, sizeof(Semaphore));
}

void phase3_start_service_processes() {
    // gain lock
    gain_lock();

    // release lock
    release_lock();
}


void kernelSpawn(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();
    
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
    release_lock();
}


int spork_trampoline(void *arg) {
    // gain lock
    gain_lock();

    // receive from the mailbox
    void *msg;
    MboxRecv(trampoline_Mbox, msg, sizeof(int (*)(void *)));

    int (*func)(void *) = (int (*)(void *)) msg;

    // release lock before going into user mode
    release_lock();

    // change the psr to change to user mode
    unsigned int old_psr = USLOSS_PsrGet();
    int psr_status = USLOSS_PsrSet(old_psr & 0b1110);
    

    // call func from user mode
    func(arg);
}

void kernelWait(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();

    // call join
    int status;
    int retval = join(&status);

    // fill outputs
    args->arg1 = (void *)(long) retval;
    args->arg2 = (void *)(long) status;
    if (retval == -2) args->arg4 = (void *)(long) -2;
    else retval = 0;

    // release lock
    release_lock();
}

void kernelTerminate(USLOSS_Sysargs *args){
    // gain lock
    gain_lock();

    // call join until it returns -2
    int status;
    int retval = 0;
    while (retval != -2) { retval = join(&status); }
    
    // call quit
    quit(status);

    // release lock
    release_lock();
}

void kernelSemCreate(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();

    int value = (int)(long) args->arg1;
    int sem_id = -1;
    for (int i=0; i < MAXSEMS; i++) {
        if (!semaphores[i].is_alive) {
            sem_id = i;
            semaphores[sem_id].value = value;
        }
    }

    args->arg1 = (void *)(long)sem_id;
    if (sem_id == -1) args->arg1 = (void *)(long) -1;
    else args->arg1 = 0;

    // release lock
    release_lock();
}

// decrements the semaphore
void kernelSemP(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();
    
    // unpack the argument
    int sem_id = (int)(long)args->arg1;

    // check for invalid id
    if (sem_id >= MAXSEMS || !semaphores[sem_id].is_alive) {
        args->arg4 = (void *)(long) -1;
    } else {
        // decrement the sempahore
        semaphores[sem_id].value--;
    }


    // release lock
    release_lock();
}

// increments the semaphore
void kernelSemV(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();

    // unpack the argument
    int sem_id = (int)(long)args->arg1;

    // check for invalid id
    if (sem_id >= MAXSEMS || !semaphores[sem_id].is_alive) {
        args->arg4 = (void *)(long) -1;
    } else {
        // decrement the sempahore
        semaphores[sem_id].value++;
    }

    // release lock
    release_lock();
}

void kernelGetTimeOfDay(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();

    args->arg1 = (void *)(long) currentTime();

    // release lock
    release_lock();
}

void kernelGetPID(USLOSS_Sysargs *args) {
    // gain lock
    gain_lock();

    args->arg1 = (void *)(long) getpid();

    // release lock
    release_lock();
}

