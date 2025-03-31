#include <stdlib.h>
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"

// function stubs
void require_kernel_mode(const char *func);
void gain_mutex();
void release_mutex();
void phase3_init();
void phase3_start_service_processes();

// syscall stubs
void Spawn(USLOSS_Sysargs *arg);
void Wait(USLOSS_Sysargs *arg);
void Terminate(USLOSS_Sysargs *arg);
void SemCreate(USLOSS_Sysargs *arg);
void SemP(USLOSS_Sysargs *arg);
void SemV(USLOSS_Sysargs *arg);
void GetTimeofDay(USLOSS_Sysargs *arg);
void GetPID(USLOSS_Sysargs *arg);

// struct definitions
typedef struct semaphore {
    int semaphore_id;
    int is_alive;
} Semaphore;

// globals
int mutex;
Semaphore semaphores[MAXSEMS];



// verifies that the program is currently running in kernel mode and halts if not 
void require_kernel_mode(const char *func) {
    unsigned int cur_psr = USLOSS_PsrGet();
    if (!(cur_psr & USLOSS_PSR_CURRENT_MODE)) {
        // not in kernel mode, halt simulation
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }
}

void gain_mutex() {
    MboxSend(mutex, NULL, 0);
}

void release_mutex() {
    MboxRecv(mutex, NULL, 0);
}

void phase3_init() {
    // require kernel mode
    require_kernel_mode(__func__);

    // create mutex
    mutex = MboxCreate(1, 0);

    // gain mutex
    gain_mutex();

    // fill system call vector
    systemCallVec[SYS_SPAWN]        =        Spawn;
    systemCallVec[SYS_WAIT]         =         Wait;
    systemCallVec[SYS_TERMINATE]    =    Terminate;
    systemCallVec[SYS_SEMCREATE]    =    SemCreate;
    systemCallVec[SYS_SEMP]         =         SemP;
    systemCallVec[SYS_SEMV]         =         SemV;
    systemCallVec[SYS_GETTIMEOFDAY] = GetTimeofDay;
    systemCallVec[SYS_GETPID]       =       GetPID;

    // release mutual exclusion
    release_mutex();
}

void phase3_start_service_proesses() {
}


void Spawn(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // unpack arguments
    int (*start_func)(void *)   = (int (*)(void *)) arg->arg1;
    void *param_to_pass         = (void *)          arg->arg2;
    int   stack_size            = (int)(long)       arg->arg3;
    int   priority              = (int)(long)       arg->arg4;
    char *name                  = (char *)          arg->arg5;

    int pid = spork(name, start_func, param_to_pass, stack_size, priority);

    // pack return values
    arg->arg1 = (void *) &pid;

    // arg4 should be -1 if illegal values were given as input, 0 otherwise
    // if the process returns the trampoline will automatically call Terminate()
    // from user mode on behalf of this process, passing as the argument
    // the value returned from the user-main function.

    // release mutex
    release_mutex();

}

void Wait(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // release mutex
    release_mutex();
}

void Terminate(USLOSS_Sysargs *arg) {
}

void SemCreate(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // unpack argument
    int sem_value = *((int *) arg->arg1);
    
    // check for valid arg
    if (sem_value > MAXSLOTS);
    int mbox_fail = MboxCreate(sem_value, 0);

    // release mutex
    release_mutex();
}

void SemP(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // release mutex
    release_mutex();
}

void SemV(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // release mutex
    release_mutex();
}

void GetTimeofDay(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // release mutex
    release_mutex();
}

void GetPID(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // release mutex
    release_mutex();
}

// user mode code must never crash the entire system
// must perform careful error checking, to ensure params passed to kernel are valid

// disable interrupts only in trampoline for user mode processes
// disable kernel mode before calling user main function

// semp
// semv

