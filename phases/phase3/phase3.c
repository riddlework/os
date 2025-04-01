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
void set_user_mode();

// syscall stubs
void Spawn_K(USLOSS_Sysargs *arg);
void Wait_K(USLOSS_Sysargs *arg);
void Terminate_K(USLOSS_Sysargs *arg);
void SemCreate_K(USLOSS_Sysargs *arg);
void SemP_K(USLOSS_Sysargs *arg);
void SemV_K(USLOSS_Sysargs *arg);
void GetTimeofDay_K(USLOSS_Sysargs *arg);
void GetPID_K(USLOSS_Sysargs *arg);

// struct definitions
typedef struct semaphore {
    int semaphore_id;
    int is_alive;
    int value;
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
    systemCallVec[SYS_SPAWN]        =        Spawn_K;
    systemCallVec[SYS_WAIT]         =         Wait_K;
    systemCallVec[SYS_TERMINATE]    =    Terminate_K;
    systemCallVec[SYS_SEMCREATE]    =    SemCreate_K;
    systemCallVec[SYS_SEMP]         =         SemP_K;
    systemCallVec[SYS_SEMV]         =         SemV_K;
    systemCallVec[SYS_GETTIMEOFDAY] = GetTimeofDay_K;
    systemCallVec[SYS_GETPID]       =       GetPID_K;

    // release mutual exclusion
    release_mutex();
}

void phase3_start_service_processes() {
}

void set_user_mode() {
    int setStatus = USLOSS_PsrSet((USLOSS_PsrGet()|1)-1);
}



int trampoline() {
    // Switch to user mode 
    set_user_mode();

    // TODO:
    // Recieve user main function info and args from some mbox
    // Call user main function and execute the users intended work
    // Gather and repack return values into arg
    // Send return values back to user process that executed the syscall
    // Call terminate to clean up the process

}

void Spawn_K(USLOSS_Sysargs *arg) {
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

    // TODO:
    // check for valid args (The system cannot be crashed by invalid arguments)
    // Create a mailbox to send process info 
    // Send user main proc info and args to the mailbox

    // spork the trampoline process
    int pid = spork(name, trampoline, param_to_pass, stack_size, priority);

    // Repack the spork return vals
    arg->arg1                   = (void *)          pid;
    arg->arg4                   = (void *)          0;
    
    // release mutex
    release_mutex();

}

void Wait_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // unpack arguments
    int *pid                    = (int *)           arg->arg1;
    int *status;

    // Call join to get return values
    int join_pid = join(status);

    arg->arg1 = (void *) join_pid;
    arg->arg2 = (void *) status;

    // if there are no children, then arg4 = -2. Otherwise 0.
    arg->arg4 = 0;
    if (join_pid == -2) {arg->arg4 = -2;}

    // release mutex
    release_mutex();
}

void Terminate_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // call join until it returns -2 then call quit with the provided status
    int *quit_status            = (int *)           arg->arg1; 
    int *status;
    int join_val = join(status);
    while (join_val != -2) {
        join_val = join(status);
    }
    quit(quit_status);
}

void SemCreate_K(USLOSS_Sysargs *arg) {
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

void SemP_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // TODO:
    // Unpack args to get semaphore id
    // get pointer to the semaphore
    // decrement the 'value' field

    // release mutex
    release_mutex();
}

void SemV_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // TODO:
    // Unpack args to get semaphore id
    // get pointer to the semaphore
    // increment the 'value' field

    // release mutex
    release_mutex();
}

void GetTimeofDay_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    // release mutex
    release_mutex();
}

void GetPID_K(USLOSS_Sysargs *arg) {
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

