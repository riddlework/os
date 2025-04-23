#include <stdlib.h>
#include <string.h>
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include <unistd.h>

// function stubs
void require_kernel_mode(const char *func);
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase3_init();
void phase3_start_service_processes();
void set_user_mode();
int  trampoline(void *);


// syscall stubs
void Spawn_K       (USLOSS_Sysargs *arg      );
void Wait_K        (USLOSS_Sysargs *arg      );
void Terminate_K   (USLOSS_Sysargs *arg      );
void SemCreate_K   (USLOSS_Sysargs *arg      );
void SemP_K        (USLOSS_Sysargs *arg      );
void SemV_K        (USLOSS_Sysargs *arg      );
void GetTimeofDay_K(USLOSS_Sysargs *arg      );
void GetPID_K      (USLOSS_Sysargs *arg      );
int  kernSemCreate (int             value     ,
                    int            *semaphore);
int  kernSemP      (int             semaphore);
int  kernSemV      (int             semaphore);

// tell the compiler that the definition for this can be found somewhere else
extern void Terminate(int status);

// struct definitions
typedef struct pcb {
    int pid;
    struct pcb *next;
} pcb;

typedef struct semaphore {
    /*int sem_id;*/
    int is_alive;
    int value;
    pcb *blocked_queue;
} Semaphore;

// globals
int mutex;
Semaphore sems[MAXSEMS];
static pcb shadow_proc_table[MAXPROC];

// verifies that the program is currently running in kernel mode and halts if not 
void require_kernel_mode(const char *func) {
    unsigned int cur_psr = USLOSS_PsrGet();
    if (!(cur_psr & USLOSS_PSR_CURRENT_MODE)) {
        // not in kernel mode, halt simulation
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }
}

void set_user_mode() {
    unsigned int cur_psr = USLOSS_PsrGet();

    // set user mode using the bit masks defined in usloss.h
    // basically turns off kernel mode
    int status = USLOSS_PsrSet(cur_psr & ~USLOSS_PSR_CURRENT_MODE);

    // TODO: perform error checking for status?
}

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

void phase3_init() {
    // require kernel mode
    require_kernel_mode(__func__);

    // create mutex
    mutex = MboxCreate(1, 0);

    // gain mutex
    gain_mutex(__func__);

    // initialize the semaphores to 0s
    for (int i = 0; i < MAXSEMS; i++) memset(&sems[i], 0, sizeof(Semaphore));

    // initialize the shadow proc table to 0s
    for (int i = 0; i < MAXPROC; i++) memset(&shadow_proc_table[i], 0, sizeof(pcb));

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
    release_mutex(__func__);
}

void phase3_start_service_processes() {
}

void Spawn_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    int   stack_size            = (int)(long)       arg->arg3;
    int   priority              = (int)(long)       arg->arg4;
    char *name                  = (char *)          arg->arg5;

    // create mbox and send args
    int mboxId = MboxCreate(2, sizeof(void *));
    MboxSend(mboxId, (void *)&(arg->arg1), sizeof(void *));
    MboxSend(mboxId, (void *)&(arg->arg2), sizeof(void *));

    // release mutex
    release_mutex(__func__);

    // spork the trampoline process
    int childPid = spork(name, trampoline, (void *)(long)mboxId, stack_size, priority);
    
    // repack the pid -- must cast to void *
    arg->arg1 = (void *)(long)childPid;
}

int trampoline(void *arg) {

    // gain mutex
    gain_mutex(__func__);
    
    // cast passed arg to int (the mbox id was passed)
    int mbox_id = (int)(long)arg;

    // instantiate pointers for start_func and param
    int (*start_func)(void *);
    void *param_to_pass;

    // recv user main func + param
    int recvVal = MboxRecv(mbox_id, &start_func, sizeof(void *));
    recvVal = MboxRecv(mbox_id, &param_to_pass, sizeof(void *));

    // release mbox here
    MboxRelease(mbox_id);

    // TODO: error checking for recv?

    // release mutex
    release_mutex(__func__);

    // Switch to user mode 
    set_user_mode();
    
    // Run user main
    int status = start_func(param_to_pass);
    
    Terminate(status);

    // compiler hint that this function never returns
    while(1);
}

void Wait_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // unpack arguments
    int status;

    // release mutex
    release_mutex(__func__);

    // Call join to get return values
    int pid_of_child_joined_to = join(&status);

    // gain mutex
    gain_mutex(__func__);

    arg->arg1 = (void *)(long)pid_of_child_joined_to;
    arg->arg2 = (void *)(long)status;

    // if there are no children, then arg4 = -2. Otherwise 0.
    arg->arg4 = 0;
    if (pid_of_child_joined_to == -2) arg->arg4 = (void *)(long)-2;
    
    // release mutex
    release_mutex(__func__);
}

void Terminate_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // call join until it returns -2 then call quit with the provided status
    int quit_status = (int)(long)arg->arg1;
    int status;
    int join_val;
    do {
        // release mutex before quitting
        release_mutex(__func__);

        join_val = join(&status);

        // gain mutex
        gain_mutex(__func__);
    } while (join_val != -2);

    // release mutex before quitting
    release_mutex(__func__);

    quit(quit_status);
}

void SemCreate_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // unpack argument
    int sem_value = (int)(long)arg->arg1;

    // release mutex
    release_mutex(__func__);

    int sem_id;
    int result = kernSemCreate(sem_value, &sem_id);

    // gain mutex
    gain_mutex(__func__);

    // pack return values
    arg->arg1 = (void *)(long)sem_id;  // sem_id
    arg->arg4 = (void *)(long)result;  // success

    // release mutex
    release_mutex(__func__);
}

int kernSemCreate(int value, int *semaphore) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // get next sem id
    int sem_id = -1;
    for (int i = 0; i < MAXSEMS; i++) {
        if (!sems[i].is_alive) { 
            sem_id = i;
            break;
        }
    }
    
    // check for valid arg
    if (value < 0 || sem_id == -1) {
        // release mutex before returning
        release_mutex(__func__);

        *semaphore = 0;
        return -1;
    }

    Semaphore *sem = &sems[sem_id];
    /*sem->sem_id = sem_id;*/
    sem->value = value;
    sem->is_alive = 1;

    // pack return values
    *semaphore = sem_id;

    // release mutex
    release_mutex(__func__);

    return 0;
}

void SemP_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // unpack arg
    int sem_id = (int)(long)arg->arg1;

    // release mutex
    release_mutex(__func__);

    int success = kernSemP(sem_id);

    // gain mutex
    gain_mutex(__func__);

    // pack return value
    arg->arg4 = (void *)(long)success;

    // release mutex
    release_mutex(__func__);
}

int kernSemP(int semaphore) {
    static int i = 0;
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // error checking
    if (semaphore < 0 || semaphore >= MAXSEMS) {
        // release mutex before returning
        release_mutex(__func__);

        return -1;
    }

    // gain a reference to the semaphore
    Semaphore *sem = &sems[semaphore];

    if (sem->value == 0) {
        // add self to sem's blocked queue and block
        int pid = getpid();
        pcb *to_queue = &shadow_proc_table[pid % MAXPROC];
        to_queue->pid = pid;

        pcb *cur = sem->blocked_queue;
        if (!cur) {
            sem->blocked_queue = to_queue;
        } else {
            while (cur->next) cur = cur->next;
            cur->next = to_queue;
        }

        // release the mutex before blocking
        release_mutex(__func__);

        blockMe();

        // gain back mutex
        gain_mutex(__func__);
    }

    // P (on the pizza)
    sem->value--;

    // release mutex
    release_mutex(__func__);

    return 0;
}

void SemV_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // unpack arg
    int sem_id = (int)(long)arg->arg1;

    // release mutex
    release_mutex(__func__);

    int success = kernSemV(sem_id);

    // gain mutex
    gain_mutex(__func__);

    // pack return value
    arg->arg4 = (void *)(long)success;

    // release mutex
    release_mutex(__func__);
}

int kernSemV(int semaphore) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // error checking
    if (semaphore < 0 || semaphore >= MAXSEMS) {
        // release mutex before returning
        release_mutex(__func__);

        return -1;
    }

    // gain a reference to the semaphore
    Semaphore *sem = &sems[semaphore];

    // V (on the pizza)
    sem->value++;

    // release mutex
    release_mutex(__func__);

    sem->value++;

    // unblock a proc that's blocked on the semaphore if one exists
    if (sem->blocked_queue) {
        int pid_toUnblock = sem->blocked_queue->pid;
        sem->blocked_queue = sem->blocked_queue->next;
        unblockProc(pid_toUnblock);
    }

    return 0;
}

void GetTimeofDay_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // store return value
    arg->arg1 = (void *)(long)currentTime();

    // release mutex
    release_mutex(__func__);
}

void GetPID_K(USLOSS_Sysargs *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // store return value
    arg->arg1 = (void *)(long)getpid();

    // release mutex
    release_mutex(__func__);
}
