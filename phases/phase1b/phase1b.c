#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phase1.h"


// Process Control Block struct
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
           int            in_zap;         // flag for blocking in zap
                  
           USLOSS_Context context;
        
           char           *name;          // useful for debug
           int           (*func)(void *);
           void           *arg;
           char           *stack;
    
} pcb;



/*
 * function stubs
 */

int          scan_table           (      int          toCheck     );
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
void         dumpChildren         (                               );
void         dumpProcesses        (                               );


/*
 * global variables
 */

pcb   proc_table[MAXPROC];          // the process table
pcb*  cur_proc = &proc_table[0];    // a reference to the current process
char  init_stack[USLOSS_MIN_STACK]; // the stack for the init process
int   last_pid_created;             // the pid of the last process that was created in spork

// array of run-queue heads
pcb *queues[6];
int time_ofLastSwitch = 0;          // the system time of the last context switch


/*
 * functions
 */


// Scan the proc table to verify the presence of an arg pid
int scan_table(int toCheck) {
    //iterate over each item in the table 
    for (int i = 0; i < MAXPROC; i++) {
        if (proc_table[i].pid == toCheck) {return 1;}
    }
    return 0;
}

// Get the next available pid
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

// return a pointer to the current running process
pcb * get_proc(int pid) {
    return &(proc_table[pid % MAXPROC]);
}

// verifies that the program is currently running in kernel mode and halts if not 
void check_kernel_mode(const char *func) {
    unsigned int cur_psr = USLOSS_PsrGet();
    if (!(cur_psr & USLOSS_PSR_CURRENT_MODE)) {
        // not in kernel mode, halt simulation
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }
}

// updates psr to turn interrupts on
void enable_interrupts() {
    // enable interrupts
    int psr_status = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // halt simulation if psr_status is nonzero
    if (psr_status)  {
        USLOSS_Console("ERROR: Failed to disable interrupts!\n");
        USLOSS_Halt(1);
    }
}

// updates psr to turn interrupts off
// return the old status so that interrupts may be reset later
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

// updates psr status to reinstate old interrupt status
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

//sets up proc table
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

// function contents for the 'init' process
void init_main() {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // start service processes
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();

    // create testcase main process
    spork("testcase_main", testcase_main_wrapper, NULL, USLOSS_MIN_STACK, 3);

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

// self explanatory
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

// sporks (fork) a new child process from the currently running process
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

    // add child proc to list of children of parent proc by placing it at the front of the list
    child_proc->next_sibling = parent_proc->first_child;
    parent_proc->first_child = child_proc;

    // initialize a context for the child proc
    USLOSS_ContextInit(&child_proc->context, stack, stacksize, NULL, funcWrapper);

    // place the process at the end of its run queue
    enqueue_proc(child_pid);

    // call the dispatcher to see if the new child will run
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

    // repeat until some child joins
    while (1) {
        // loop over the children to see if any need their status collected
        cur = cur_proc->first_child;
        prev = NULL;
        while (cur) {
            
            // execute if a child is marked to terminate
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

// marks the process with the given pid for termination
// the marked process must call quit() on its own
void zap(int pid) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // retrieve a reference to the desired process
    pcb *proc_toZap = get_proc(pid);

    // error checking
    if (pid == cur_proc->pid) {  // trying to zap itself
        USLOSS_Console("ERROR: Attempt to zap() itself.\n");
        USLOSS_Halt(1);
    } else if (!scan_table(pid)) {  // trying to zap a process that is not in the table regardless of living status
        USLOSS_Console("ERROR: Attempt to zap() a non-existent process.\n");
        USLOSS_Halt(1);
    } else if (!proc_toZap->is_alive ||   // trying to zap a non-existent process
            proc_toZap->termination) { // trying to zap a terminated process
        USLOSS_Console("ERROR: Attempt to zap() a process that is already in the process of dying.\n");
        USLOSS_Halt(1);
    } else if (pid == 1) {             // trying to zap init
        USLOSS_Console("ERROR: Attempt to zap() init.\n");
        USLOSS_Halt(1);
    }

    // add self to zap queue
    if (!proc_toZap->first_zap) {
        proc_toZap->first_zap = cur_proc;
    } else {
        cur_proc->next_zap = proc_toZap->first_zap;
        proc_toZap->first_zap = cur_proc;
    }

    // block until process dies
    cur_proc->in_zap = 1;
    blockMe();
    cur_proc->in_zap = 0;
    
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
    cur_proc->status        = status;
    cur_proc->termination   = 1;

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

    // removes the current process from whichever priority queue is exists on
    dequeue_proc();

    // call the dispatcher to switch to some other process
    dispatcher();

    restore_interrupts(old_psr);

    // compiler hint that function doesn't return
    while(1);
}

// enqueue a process into one of the priority queues based on the priority filed in the proc pcb
void enqueue_proc(int pid) {
    // retrieve a reference to the desired process
    pcb *proc_toEnqueue = get_proc(pid);
    
    // enqueue the process if it is alive, not marked for termination, and not blocked
    if (proc_toEnqueue->is_alive &&
            !proc_toEnqueue->termination &&
            !proc_toEnqueue->is_blocked) {

        pcb *queue          = queues[proc_toEnqueue->priority - 1];     // get proper queue pointer
        pcb *cur            = queue;                                    // get pointer to head
        int  queue_num      = proc_toEnqueue->priority - 1;             // get queue row
        
        proc_toEnqueue->next_run = NULL;

        // if queue is empty then place proc at the head of the priority queue
        if (!queue) { 
            queues[queue_num] = proc_toEnqueue;
        // if the queue is not empty then place the new process at the end 
        } else {    
            while (cur->next_run) {
                cur = cur->next_run;
            }
            cur->next_run = proc_toEnqueue;
        }
    }
}

// dequeue the current process from whichever priority queue it resides on
void dequeue_proc() {
    pcb * queue = queues[cur_proc->priority - 1];
    int pid     = getpid();

    // set defaults
    pcb * prev      = NULL;                         
    pcb * cur       = queue;
    int   queue_num = cur_proc->priority - 1;

    // iterate until we find the current process and remove it from the queue corresponding to its priority
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

// blocks the currently running process
// can only be awoken by calling unblockProc
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

// wakes up a blocked process and reinstates it onto the priority queues
// the awoken process may or may not run depending on the decision of the dispatcher
int unblockProc(int pid) {
    // disable interrupts, save old interrupt state, check for kernel mode
    unsigned int old_psr = check_and_disable(__func__);

    // retrieve a reference to the desired process
    pcb * proc_toUnblock = get_proc(pid);
    //USLOSS_Console("Unblocking %s : %d", proc_toUnblock->name, proc_toUnblock->pid);

    // perform error checking
    if (!proc_toUnblock->is_blocked || !proc_toUnblock->is_alive) return -2;

    // mark the process as unblocked
    proc_toUnblock->is_blocked = 0;
    
    // place the process at the end of the appropriate run queue
    enqueue_proc(pid);

    // if the current running process has zappers, return so that they may be woken up too
    if (cur_proc->first_zap != NULL) {return 0;}

    // call the dispatcher to see if the awoken process needs to be switched to 
    dispatcher();

    restore_interrupts(old_psr);
    return 0;
}

// deciphers which process is at the head of the highest non-empty priority queue
// this process may or may not be the active process
// depending on the current process (and how long it has been active), a context switch may 
// turn on the chosen process, turn off the chosen process, or do nothing if the chosen process
// has not been running for its promised time
void dispatcher() {

    unsigned int old_psr = check_and_disable(__func__);

    // choose which process will run next
    pcb * proc_toRun = NULL;
    for (int i = 0; i < 6; i++) {
        if (queues[i]) {
            proc_toRun = queues[i];
            break;
        }
    }

    // if the same process is at the head of the first non-empty priority queue
    if (proc_toRun->pid == cur_proc->pid) {
        
        int cur_time = currentTime();
        int elapsed = (cur_time - time_ofLastSwitch)/1000;

        // if the current process is done, then requeue it and call the dispatcher again
        if (elapsed >= 80) {
            dequeue_proc();
            enqueue_proc(cur_proc->pid);
            dispatcher();
        }
        restore_interrupts(old_psr);
        return;
    }
    // if there is a new process that needs to run
    else if (proc_toRun->pid != cur_proc->pid) {
        dequeue_proc();
        enqueue_proc(cur_proc->pid);
        USLOSS_Context *old = &(cur_proc->context);
        USLOSS_Context *new = &(proc_toRun->context);

        // update current process global
        cur_proc = proc_toRun;
        time_ofLastSwitch = currentTime();
        // perform the context switch
        USLOSS_ContextSwitch(old, new);
    }

    restore_interrupts(old_psr);
}

// returns the pid of the current running process
int getpid() {
    return cur_proc->pid;
}

// prints out the contents of the priority queues
// (for debugging)
void dumpQueues() {
    for (int i=0; i < 6; i++) {
        pcb *cur = queues[i];
        USLOSS_Console("Queue %d: ", i+1);
        while (cur) {
            USLOSS_Console(" ( %s : %d ) ", cur->name, cur->pid);
            cur = cur->next_run;
        }
        USLOSS_Console("\n");
    }
}

// prints out the list of children that a process posesses
// (for debugging)
void dumpChildren() {
    pcb *cur = cur_proc->first_child;
    int i = 1;
    while (cur) {
        USLOSS_Console("child %d.     %s : %d\n", i, cur->name, cur->pid);
        cur = cur->next_sibling;
        i++;
    }
}

// prints out a list of all processes in the proc table
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
            else if (proc->is_blocked && proc->in_zap)  strcpy(status_to_print, "Blocked(waiting for zap target to quit)");
            else if (proc->is_blocked && proc->in_join)  strcpy(status_to_print, "Blocked(waiting for child to quit)");
            else if (proc->is_blocked)  strcpy(status_to_print, "Blocked(3)");
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

