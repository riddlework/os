#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phase1.h"

/*
 * structs
 */

typedef struct pcb {

    struct pcb           *parent;         // parent process
    struct pcb           *first_child; 
    struct pcb           *next_sibling;
    struct pcb           *next_run;       // run queue
    struct pcb           *first_zap;      // head of zap
    struct pcb           *next_zap;

           int            pid;
           int            priority;
           int            status;         // filled in quit
           int            is_alive;       // 1 if alive, 0 if dead (usable)
           int            termination;    // flag for quit
           int            is_blocked;     // flag for blocking
           int            in_join;        // flag for blocking in join
                  
           USLOSS_Context context;
        
           char           *name;          // useful for debug
           int           (*func)(void *);
           void           *arg;
           char           *stack;
    
} pcb;

/*
 * function stubs
 */

int          get_next_pid         (                               );
pcb*         get_proc             (      int          pid         );
void         check_kernel_mode    (const char        *func        );
void         enable_interrupts    (                               );
unsigned int disable_interrupts   (                               );
void         restore_interrupts   (      unsigned int old_psr     );
unsigned int check_and_disable    (const char        *func        );
void         phase1_init          (      void                     );
void         init_main            (      void                     );
int          testcase_main_wrapper(                               );
int          spork                (      char        *name         ,
                                         int        (*func)(void *),
                                         void        *arg          ,
                                         int          stacksize    , 
                                         int          priority    );
void         funcWrapper          (                               );
int          join                 (      int         *status      );
void         quit                 (      int          status      );
void         zap                  (      int          pid         );
void         enqueue_proc         (      int          pid         );
void         dequeue_proc         (                               ); 
void         blockMe              (                               );
int          unblockProc          (      int          pid         );
void         dispatcher           (                               );
int          getpid               (                               );
void         dumpQueues           (                               );
void         dumpProcesses        (                               );

/*
 * global variables
 */

pcb   proc_table[MAXPROC];          // the process table
pcb*  cur_proc = NULL;              // a reference to the current process
char  init_stack[USLOSS_MIN_STACK]; // the stack for the init process
int   last_pid_created;             // the pid of the last process that was created in spork

// array of run-queue heads
pcb *queues[6];
int time_ofLastSwitch = 0;          // the system time of the last context switch


/*
 * functions
 */

// returns -1 if proc_table is full
int get_next_pid() {
    int next_pid = -1;

    for (int i = last_pid_created + 1; i % MAXPROC != last_pid_created % MAXPROC; i++) {
        pcb *proc = get_proc(i);
        if (!proc->is_alive) {
            next_pid = i;
            break;
        }
    } return next_pid;
}

pcb * get_proc(int pid) {
    return &(proc_table[pid % MAXPROC]);
}

void check_kernel_mode(const char *func) {
    unsigned int cur_psr = USLOSS_PsrGet();
    if (!(cur_psr & USLOSS_PSR_CURRENT_MODE)) {
        // not in kernel mode, halt simulation
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }
}

void enable_interrupts() {
    // enable interrupts
    int psr_status = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }
}

unsigned int disable_interrupts() {
    // retrieve current psr
    unsigned int old_psr = USLOSS_PsrGet();

    // flip the current interrupt bit
    int psr_status = USLOSS_PsrSet(old_psr & ~USLOSS_PSR_CURRENT_INT);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }

    // return previous psr
    return old_psr;
}

void restore_interrupts(unsigned int old_psr) {
    int psr_status = USLOSS_PsrSet(old_psr);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }
}

// check for kernel mode -- save and disable interrupts
unsigned int check_and_disable(const char *func) {
    check_kernel_mode(func);
    return disable_interrupts();
}

void phase1_init() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // initialize the process table to zeros
    for (int i=0; i < MAXPROC; i++) {
        memset(&(proc_table[i]), 0, sizeof(pcb));
    }

    // initialize the run-queue array
    for (int i = 0; i < 6; i++) queues[i] = NULL;

    // initialize process table entry for init process
    pcb * init_pcb = get_proc(1);

    init_pcb->name     = "init";
    init_pcb->pid      = 1;
    init_pcb->priority = 6;
    init_pcb->is_alive = 1;

    // update field
    last_pid_created = 1;

    // add init to run queue
    queues[5] = init_pcb;

    // initialize the context for init
    USLOSS_ContextInit(&(init_pcb->context), init_stack, USLOSS_MIN_STACK, NULL, init_main);

    restore_interrupts(old_psr);
}

void init_main() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // start service processes
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();

    // create testcase main process
    int testcase_main_pid = spork("testcase_main", testcase_main_wrapper, NULL, USLOSS_MIN_STACK, 3);

    // call the dispatcher to run init
    dispatcher();

    // loop over join
    int join_status = 0;
    while (join_status != -2) join(&join_status);

    // process has no children -- halt simulation
    USLOSS_Console("ERROR: Process being joined to has no children! Halting simulation.\n");
    USLOSS_Halt(1);

    restore_interrupts(old_psr);
}

int testcase_main_wrapper() {
    // check for kernel mode
    check_kernel_mode(__func__);

    // enable interrupts before function call
    enable_interrupts();

    // call the testcase main function
    int status = testcase_main();

    // disable interrupts, save old interrupt state
    unsigned int old_psr = disable_interrupts();

    // halt simulation since main function returned
    if (status != 0) USLOSS_Console("An ERROR was reported by a testcase! Halting simulation.\n");

    USLOSS_Halt(status);

    restore_interrupts(old_psr);
    return 0;
}

int spork(char *name, int(*func)(void *), void *arg, int stacksize, int priority) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // get next pid
    int child_pid    = get_next_pid();
    if (child_pid != -1) last_pid_created = child_pid;

    // error checking
    if (stacksize < USLOSS_MIN_STACK) return -2;
    if ((strlen(name) > MAXNAME)      ||
            (name      == NULL)          ||
            (func      == NULL)          ||
            (child_pid == -1)            ||
            (priority  < 1)              ||
            (priority  > 5))
        return -1;

    // retrieve parent, child process references
    pcb * parent_proc  = cur_proc;
    pcb * child_proc   = get_proc(child_pid);

    // allocate stack memory for process
    char * stack = malloc(stacksize);

    // fill in information on child process
    child_proc->parent   = parent_proc;
    child_proc->pid      = child_pid;
    child_proc->priority = priority;
    child_proc->is_alive = 1;
    child_proc->name     = name;
    child_proc->func     = func;
    child_proc->arg      = arg;
    child_proc->stack    = stack;

    // add child proc to list of children of parent proc
    child_proc->next_sibling = parent_proc->first_child;
    parent_proc->first_child = child_proc;

    // initialize a context for the child proc
    USLOSS_ContextInit(&child_proc->context, stack, stacksize, NULL, funcWrapper);

    // place the process at the end of its run queue
    enqueue_proc(child_pid);

    dispatcher();

    restore_interrupts(old_psr);
    return child_proc->pid;
}

// wrapper for main() function of process
void funcWrapper() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // retrieve main function and arguments of the current process
    int (*func)(void *) = cur_proc->func;
    void *arg           = cur_proc-> arg;

    // enable interrupts before function call
    enable_interrupts();
    
    // call the function, store its return value as process status
    int status = func(arg);
    
    // disable interrupts, save old interrupt state, check for kernel mode AGAIN
    old_psr = disable_interrupts();
    
    // restore interrupts before calling main function
    restore_interrupts(old_psr);

    // since the function has returned, call quit
    quit(status);
}

// TODO: BLOCKING ?
// blocks the currently running process until one of its children has terminated
int join(int *status) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // error checking
    if (!status               ) return -3; // status is NULL
    if (!cur_proc->first_child) return -2; // process has no children to join

    int   pid_of_child_joined_to = -1;

    // set up pointers to loop through the list
    pcb * prev = NULL;
    pcb * cur  = cur_proc->first_child;

    while (1) {
        // loop over the children to see if any need their status collected
        while (cur) {
            if (cur->termination) {

                *status = cur->status;             // fill the status pointer with the status of the child
                free(cur->stack);             // free the stack memory
                pid_of_child_joined_to = cur->pid; // save the pid to return

                pcb * temp = cur;                  // save reference of pcb to be cleared

                // delete the child from the list of children
                if (prev == NULL) {
                    cur_proc->first_child = cur->next_sibling;
                } else {
                    prev->next_sibling = cur->next_sibling;
                } cur = cur->next_sibling;

                // clear the slot in the process table
                memset(temp, 0, sizeof(pcb));
                break;
                                                       
            } else {
                prev = cur;
                cur  = cur->next_sibling;
            }
        }

        if (pid_of_child_joined_to == -1) {
            cur_proc->in_join = 1; // set flag to indicate process has blocked in join
            blockMe();             // block if no children have terminated
        } else break;              // otherwise, return
    }
    
    restore_interrupts(old_psr);
    return pid_of_child_joined_to;
}

// terminates the process with the given pid
// process must call quit() on its own
void zap(int pid) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // retrieve a reference to the desired process
    pcb *proc_toZap = get_proc(pid);

    // error checking
    if (pid == getpid() ||             // trying to zap itself
            !proc_toZap->is_alive ||   // trying to zap a non-existent process
            proc_toZap->termination) { // trying to zap a terminated process
        USLOSS_Console("ERROR: zap() is trying to zap() an invalid process.");
        USLOSS_Halt(1);
    } else if (pid == 1) {             // trying to zap init
        USLOSS_Console("ERROR: zap() is trying to zap() init.");
        USLOSS_Halt(1);
    }

    // mark process for termination
    proc_toZap->termination = 1;

    // add self to zap queue
    if (!proc_toZap->first_zap) {
        proc_toZap->first_zap = cur_proc;
    } else {
        pcb *cur = proc_toZap->first_zap;
        while ((cur = cur->next_zap));
        cur->next_zap = cur_proc;
    }

    // block until process dies
    blockMe();
    
    restore_interrupts(old_psr);
}

// quits the current process, marking it for termination
void quit(int status) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // check if current process still has children
    if (cur_proc->first_child) {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n", cur_proc->pid);
        USLOSS_Halt(1);
    }

    // update the fields of the pcb
    cur_proc->status      = status;
    cur_proc->termination = 1;

    // if the parent is waiting in a join, unblock it
    if (cur_proc->parent->in_join) {
        cur_proc->parent->in_join = 0;
        unblockProc(cur_proc->parent->pid);
    }

    // clear the zappers
    while (cur_proc->first_zap) {
        unblockProc(cur_proc->first_zap->pid);
        cur_proc->first_zap = cur_proc->first_zap->next_zap;
    }

    // call the dispatcher to switch to some other process
    dispatcher();

    restore_interrupts(old_psr);

    // compiler hint that function doesn't return
    while(1);
}

// enqueue a process
void enqueue_proc(int pid) {
    // retrieve a reference to the desired process
    pcb *proc_toEnqueue = get_proc(pid);

    dumpProcesses();
    USLOSS_Console("pid: %d\n", pid);
    // enqueue the process if it is alive, not marked for termination, and not blocked
    if (proc_toEnqueue->is_alive &&
            !proc_toEnqueue->termination &&
            !proc_toEnqueue->is_blocked) {

        pcb *queue          = queues[proc_toEnqueue->priority - 1];
        pcb *cur            = queue;
        int  queue_num      = proc_toEnqueue->priority - 1;

        if (!queue) {
            queues[queue_num] = proc_toEnqueue;
        } else {
            while ((cur = cur->next_run));
            cur->next_run = proc_toEnqueue;
        }
    }
    dumpQueues();

}

// dequeue the current process
void dequeue_proc() {
    pcb * queue = queues[cur_proc->priority - 1];
    int pid     = getpid();

    pcb * prev      = NULL;
    pcb * cur       = queue;
    int   queue_num = cur_proc->priority - 1;

    while (cur) {
        if (cur->pid == pid) {
            if (prev == NULL) queues[queue_num] = cur->next_run;
            else              prev->next_run    = cur->next_run;
            break;
        } else {
            prev = cur;
            cur  = cur->next_run;
        }
    }
}

void blockMe() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // mark current process as blocked
    cur_proc->is_blocked = 1;

    // remove it from the run-queue
    dequeue_proc();

    // call the dispatcher to decide what process to run next
    dispatcher();
    
    restore_interrupts(old_psr);
}

int unblockProc(int pid) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // retrieve a reference to the desired process
    pcb * proc_toUnblock = get_proc(pid);

    // perform error checking
    if (!proc_toUnblock->is_blocked || !proc_toUnblock->is_alive) return -2;

    // mark the process as unblocked
    proc_toUnblock->is_blocked = 0;

    // place the process at the end of the appropriate run queue
    enqueue_proc(pid);
    
    // call the dispatcher to decide what process to run next
    dispatcher();
    
    restore_interrupts(old_psr);
    return 0;
}

void dispatcher() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);
    
    // choose which process will run next
    pcb * proc_toRun = NULL;
    for (int i = 0; i < 6; i++) {
        if (queues[i]) {
            proc_toRun = queues[i];

            // dequeue the process
            queues[i] = queues[i]->next_run;

            // re-enqueue the process
            enqueue_proc(proc_toRun->pid);

            break;
        }
    }

    enable_interrupts();
    int cur_time = currentTime();
    disable_interrupts();

    if (!proc_toRun) {
        dumpQueues();
        dumpProcesses();
    }

    // TODO: what to do if process is NULL?

    // if the process that should run is not the current process, switch to it
    if (proc_toRun->pid != cur_proc->pid ||
            cur_time - time_ofLastSwitch >= 80) {
        time_ofLastSwitch = cur_time;

        USLOSS_Context * old = cur_proc ? &(cur_proc->context) : NULL;
        USLOSS_Context *new = &(proc_toRun->context);

        // update current proces global
        cur_proc = proc_toRun;

        // perform the context switch
        USLOSS_ContextSwitch(old, new);
    }

    restore_interrupts(old_psr);
}


int getpid() {
    return cur_proc->pid;
}

void dumpQueues() {
    for (int i=0; i < 6; i++) {
        pcb *cur = queues[i];
        USLOSS_Console("Queue %d: ", i+1);
        while (cur) {
            USLOSS_Console("%d ", cur->pid);
            cur = cur->next_run;
        }
        USLOSS_Console("\n");
    }
}

void dumpProcesses() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    USLOSS_Console(" PID  PPID  %-*s  PRIORITY  STATE\n", 16, "NAME");
    for (int i = 0; i < MAXPROC; i++) {
        pcb * proc = get_proc(i);
        if (proc->is_alive) {
            // retrieve the ppid
            int ppid = proc->parent      ? proc->parent->pid      : 0;

            // ascertain the status of the process
            char status_to_print[100];
            if      (proc == cur_proc)  strcpy(status_to_print, "Running");
            else if (proc->status == 0) strcpy(status_to_print, "Runnable");
            else                        snprintf(status_to_print, sizeof(status_to_print), "Terminated(%d)", proc->status);

            // print process information to console
            USLOSS_Console(" %*d  %*d  %-*s  %-*d  %s\n",
                    3, proc->pid,
                    4, ppid,
                    16, proc->name,
                    8, proc->priority,
                    status_to_print);
        }
    }
    restore_interrupts(old_psr);
}

