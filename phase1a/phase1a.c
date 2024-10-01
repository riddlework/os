#include <stdio.h>
#include <string.h>
#include "phase1.h"


// how do you create a process?
// how to tell if a slot in the process table is taken or not?
// how to keep track of slots in the process table?

// for every function, we must disable interrupts
    // store psr -- where to store? variable?
    // disable interrupts
    // do stuff
    // restore the state of the running process
// for every function, must check to confirm that the psr shows you are running
    // in kernel mode

// parent processes cannot end until all of their children have ended
// and the parent has collected all of their statues (using join())

// data structure to keep track of the state of a process while it is switched out?

// possible process statuses:
// 1. ready
// 2. running
// 3. blocked

typedef struct node node;

// process control block
typedef struct pcb {
    int pid;
    USLOSS_Context context;
    struct pcb * parent;
    node * children;
    int priority; // enforce limit? 1-5 inclusive?
    char process_name[MAXNAME];
    int is_alive;
} pcb;

// linked list
typedef struct node {
    pcb * process;
    struct node * next;
} node;


// process table
pcb proc_table[MAXPROC];

// current process
pcb * cur_proc = NULL;

// run queues -- no process will run if a higher priority process is ready to run
node * run_queue1;
node * run_queue2;
node * run_queue3;
node * run_queue4;
node * run_queue5;


// initialize data structures such as the process table
void phase1_init(void) {
    // cannot block or context switch

    // initialize process table to zeros
    memset(proc_table, 0, sizeof(proc_table));

    // initialize process table entry for init process
    // init always has pid 1
    // never dies
    pcb init = proc_table[0]; 
    init.pid = 1;
    init.priority = 6;
    init.is_alive = 1;

    // call spork once to create the testcase_main process

    // loop over join to clean up process table
    // keep calling it unless you receive -2 (no children), in which case
    // print out an error message and terminate the simulation

    // DO NOT perform context switch 
}

// creates a new process, the child of the currently running process
// startFunc: the main() function for the child process
// arg: the argument to pass to startFunc
int spork(char * name, int (* startFunc) (void *), void * arg, int stack_size, int priority) {
    // may block: no
    // may context switch: yes

    // return -1 if:
    // no empty slots in the process table
    // priority out of range
    // startFunc or name are NULL
    // name too long
    if (priority < 1 || priority > 5) return -1;
    if (!startFunc || !name) return -1;
    if (strlen(name) > MAXNAME) return -1;

    // linear search the process table until you find an empty slot
    // otherwise there are no empty slots left

    // return -2 if:
    // stack_size < USLOSS_MIN_STACK
    if (stack_size < USLOSS_MIN_STACK) return -2;


    // creates a child process of the current process
    // creates the entry in the process table and fills it in, then calls the dispatcher
    
    // if the child is higher priority than the parent, the child will run before spork() returns
        // when the child process runs, startFunc() will be called and passed the param specified as arg
        // if it ever returns, this terminates the process. same effect as calling quit
        // return value process status
    

    // STEPS:
    // find the next empty slot in the proc table using a linear search


    // when spork is called, allocate another slot in the table
        // use it to store information about the process (or return an error)

    // return PID of child process
    return cur_proc->pid;
}

// blocks current process until one of its children has terminated
// --> then delivers the status of the child (the parameter that the child
// passed to quit) back to the parent
int join(int * status) {
    // may block: yes
    // may context switch: yes

    // fill status with the status of the process joined to

    // how to check if the children of the current process have already been joined?
    if (!status) return -3;
    else return 0;

}

void quit_phase_1a(int status, int switchToPid) {
}


// this function never returns
// terminates the current process, with a certain "status" value
// if the parent process is already waiting in a join, then the parent will be
// awoken.
// it will continue to take up space in the process table until its status has been
// delivered to ist parent
void quit(int status) {
}

// returns the pid of the currently executing process
int getpid(void) { return cur_proc->pid; }

// prints human-readable debug data about the process table
void dumpProcesses(void) {
}

void TEMP_switchTo(int pid) {
}
