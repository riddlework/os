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

// tell the compiler that the definition for this can be found somewhere else
extern void Terminate();

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
    unsigned int cur_psr = USLOSS_PsrGet();

    // set user mode using the bit masks defined in usloss.h
    // basically turns off kernel mode
    int status = USLOSS_PsrSet(cur_psr & USLOSS_PSR_CURRENT_MASK);
}



int trampoline(void *arg) {

    // cast passed arg to int (the mbox id was passed)
    int mbox_id = (int)(long) arg;

    // instantiate the recv out ptr
    USLOSS_Sysargs *sys_args;
    

    // recv user main func + param
    int recvVal = MboxRecv(mbox_id, (void *)sys_args, sizeof(void*));
    // unpack arguments
    int (*start_func)(void *) = (int (*)(void *)) sys_args->arg1;

    // don't have to cast this because it's already a void *
    void *param_to_pass = sys_args->arg2;

    // Switch to user mode 
    set_user_mode();
    
    // Run user main
    int result = start_func(param_to_pass);
    
    Terminate();
}

void Spawn_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex();

    int   stack_size            = (int)(long)       arg->arg3;
    int   priority              = (int)(long)       arg->arg4;
    char *name                  = (char *)          arg->arg5;

    // create mbox and send args
    int mboxId = MboxCreate(1, sizeof(void*));
    int sendVal = MboxSend(mboxId, arg, sizeof(void*));

    // spork the trampoline process
    int childPid = spork(name, trampoline, (void *)(long)mboxId, stack_size, priority);
    
    // repack the pid -- must cast to void *
    arg->arg1 = (void *)(long)childPid;

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

    arg->arg1 = (void *)(long)join_pid;
    arg->arg2 = (void *)status;

    
    // if there are no children, then arg4 = -2. Otherwise 0.
    arg->arg4 = 0;
    if (join_pid == -2) arg->arg4 = (void *)(long)-2;

    // release mutex
    release_mutex();
}

void Terminate_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // call join until it returns -2 then call quit with the provided status
    int quit_status = (int)(long)arg->arg1; 
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

