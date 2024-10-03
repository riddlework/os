#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phase1.h"

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
    int status;
    USLOSS_Context context;
    int (* startFunc) (void *);
    void * arg;
    struct pcb * parent;
    node * children;
    char process_name[MAXNAME];
    int priority;
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
pcb * cur_proc;

// function stubs
void init_main();
void Temp_switchTo(int pid);
int testcase_main_trampoline();
void quit_phase_1a(int status, int switchToPid);

// returns a pointer to the PCB of the process with the given pid
pcb * get_proc(int pid) { return &proc_table[pid % MAXPROC]; }

// return the pid of the currently running process
int get_pid() { return cur_proc->pid; }


// find the next empty slot in the proc table
// assign a pid accordingly
// return -1 if the proc table is fulle
int get_next_pid() {
    int cur_pid = get_pid();
    int next_pid = -1;
    for (int i = cur_pid + 1; i % MAXPROC != cur_pid; i++) {
        pcb proc = proc_table[i & MAXPROC];
        if (!proc.is_alive) {
            next_pid = i;
            break;
        }
    } return next_pid;
}


// add to the head of a linked list
node * add_to_head(pcb * proc, node * list) {
    node * head;
    head->process = proc;
    head->next = list;
    return head;
}    


// initialize data structures such as the process table
void phase1_init(void) {

    // cannot block or context switch

    // store the current psr
    unsigned int old_psr = USLOSS_PsrGet();

    // check that we are running in kernel mode -- if not, halt
    if ((old_psr & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in phase1_init is being called from user mode\n");
        USLOSS_Halt(1);
    }

    // disable interrupts
    int psr_status = USLOSS_PsrSet(old_psr & ~USLOSS_PSR_CURRENT_INT); // might cause bugs?

    // initialize process table to zeros
    memset(proc_table, 0, sizeof(proc_table));

    // initialize process table entry for init process
    // init always has pid 1
    // never dies
    pcb * init = get_proc(1);
    init->pid = 1;
    init->priority = 6;
    init->is_alive = 1;
    cur_proc = init;

    // initialize a context for init
    // why should this be a char *?
    char * initStack = malloc(USLOSS_MIN_STACK);
    USLOSS_ContextInit(&init->context, initStack, USLOSS_MIN_STACK, NULL, init_main);

    // switch into init
    Temp_switchTo(1);

    // restore interrupts
    psr_status = USLOSS_PsrSet(old_psr);
}


// the main function of the init process
void init_main() {
    // TODO: disable interrupts and check that we're running in kernel mode
    
    // we have now been switched into the init process

    // ask other phases to create processes
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();
    
    // call spork once to create the testcase_main process
    int some_pid = spork("testcase_main", testcase_main_trampoline, NULL, USLOSS_MIN_STACK, 3);


    // init_main should call spork
    // spork calls context init and passes the trampoline as a function pointer
    // the trampoline then retrieves the appropriate function pointer from the pcb

    int join_return;
    while ((join_return = join(&cur_proc->status)) != -2) {
        USLOSS_Console("join returned a value of -2 -- terminating simulation");
        USLOSS_Halt(1);
    }
}


// a trampoline function for the testcase_main process
int testcase_main_trampoline() {

    // call testcase_main's main function
    int ret_val = testcase_main();

    // if it returns, terminate the simulation
    if (ret_val) USLOSS_Console("nonzero return value... some error detected... halting simulation...");
    USLOSS_Halt(ret_val);

}


// general purpose trampoline function
void trampoline() {

    // something about enabling interrupts before startFunc is called?
    

    // pull the function and argument pointers
    int (*startFunc)(void*) = cur_proc->startFunc;
    void *arg = cur_proc->arg;

    // call the processes main function
    int result = startFunc(arg);

    // if startFunc returns, call quit()
    quit_phase1a(cur_proc->status, 1); // TODO: Change the pid here?
}


// creates a new process, the child of the currently running process
// startFunc: the main() function for the child process
// arg: the argument to pass to startFunc
int spork(char * name, int (*startFunc)(void*), void * arg, int stack_size, int priority) {
    
    // store the current psr
    unsigned int psr_to_restore = USLOSS_PsrGet();

    // check that we are running in kernel mode -- otherwise, halt
    if ((psr_to_restore & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in spork is being called from user mode\n");
        USLOSS_Halt(1);
    }

    // disable interrupts
    int psr_status = USLOSS_PsrSet(psr_to_restore & ~USLOSS_PSR_CURRENT_INT); // might cause bugs?

    // get the child processes' pid, and thus its slot in the proc table
    int child_pid = get_next_pid();
    
    // idx of slot in proc table
    int slot = child_pid % MAXPROC;

    // return -1 if:
    // no empty slots in the process table
    // priority out of range
    // startFunc or name are NULL
    // name too long
    if (priority < 1 || priority > 5
            || !startFunc || !name
            || strlen(name) > MAXNAME
            || slot <0)
        return -1;

    // return -2 if:
    // stack_size < USLOSS_MIN_STACK
    if (stack_size < USLOSS_MIN_STACK) return -2;


    // fill slot in proc table
    pcb * child_proc = get_proc(child_pid);
    child_proc->pid = child_pid;
    strcpy(child_proc->process_name,name);
    child_proc->startFunc = startFunc;
    child_proc->arg = arg;
    child_proc->parent = cur_proc; // child of current process
    child_proc->priority = priority;

    // add new process to cur_proc's children
    cur_proc->children = add_to_head(child_proc,cur_proc->children);

    // initialize a context for the child process
    char * stack = malloc(stack_size);
    USLOSS_ContextInit(&child_proc->context, stack, stack_size, NULL, trampoline);

    // if the child process is a higher priority, run it immediately
    int cur_pid;
    if (child_proc->priority < cur_proc->priority) {
        pcb * old_proc = cur_proc;
        pcb * new_proc = child_proc;

        cur_proc = new_proc;
        cur_pid = cur_proc->pid;
        USLOSS_ContextSwitch(&old_proc->context, &new_proc->context);
    } 
    

    // restore interrupts
    psr_status = USLOSS_PsrSet(psr_to_restore);

    // return PID of child process
    return cur_pid;
}


// blocks current process until one of its children has terminated
// --> then delivers the status of the child (the parameter that the child
// passed to quit) back to the parent
int join(int * status) {
    // may block: yes
    // may context switch: yes
    
    // store the current psr
    unsigned int psr_to_restore = USLOSS_PsrGet();

    // check that we are running in kernel mode -- otherwise, halt
    if ((psr_to_restore & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in join is being called from user mode\n");
        USLOSS_Halt(1);
    }
    
    // disable interrupts
    int psr_status = USLOSS_PsrSet(psr_to_restore & ~USLOSS_PSR_CURRENT_INT); // might cause bugs?
                                                                          

    // fill status with the status of the process joined to

    // how to check if the children of the current process have already been joined?
    if (!status) return -3;
    else return 0;

    // restore interrupts
    psr_status = USLOSS_PsrSet(psr_to_restore);
}

void quit_phase_1a(int status, int switchToPid) {

    // store the current psr
    unsigned int psr_to_restore = USLOSS_PsrGet();

    // check that we are running in kernel mode -- otherwise, halt
    if ((psr_to_restore & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in quit_phase_1a is being called from user mode\n");
        USLOSS_Halt(1);
    }

    // terminatest the current process with a certain status value
    // if the parent process is already waiting in a join, the parent process will be awoken
    // it will continue to take up space in the process table until its status has been delivered
    // to its parent
    

    Temp_switchTo(switchToPid);

    // disable interrupts
    int psr_status = USLOSS_PsrSet(psr_to_restore & ~USLOSS_PSR_CURRENT_INT); // might cause bugs?
                                                                                   
    // stuff here

    // restore interrupts
    psr_status = USLOSS_PsrSet(psr_to_restore);
}


// prints human-readable debug data about the process table
void dumpProcesses(void) {

    // store the current psr
    unsigned int psr_to_restore = USLOSS_PsrGet();

    // check that we are running in kernel mode -- otherwise, halt
    if ((psr_to_restore & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in dumpPorcesses is being called from user mode\n");
        USLOSS_Halt(1);
    }

    // disable interrupts
    int psr_status = USLOSS_PsrSet(psr_to_restore & ~USLOSS_PSR_CURRENT_INT); // might cause bugs?
                                                                                   
    // stuff here

    // restore interrupts
    psr_status = USLOSS_PsrSet(psr_to_restore);
}


void TEMP_switchTo(int pid) {
    USLOSS_Console("Phase 1A TEMPORARY HACK: init() manually switching to PID %d.", pid);

    // store the current psr
    unsigned int psr_to_restore = USLOSS_PsrGet();

    // check that we are running in kernel mode -- otherwise, halt
    if ((psr_to_restore & USLOSS_PSR_CURRENT_MODE) == 0) {
        printf("Code in TEMP_switchTo is being called from user mode\n");
        USLOSS_Halt(1);
    }

    // retrieve the current process from the prob_table
    pcb * old_proc = cur_proc; 
    pcb * new_proc = get_proc(pid);
    pcb * cur_proc = new_proc;

    // perform context switch
    USLOSS_ContextSwitch(&old_proc->context,&new_proc->context);


    // disable interrupts
    int psr_status = USLOSS_PsrSet(psr_to_restore & ~USLOSS_PSR_CURRENT_INT); // might cause bugs?
                                                                                   
    // stuff here

    // restore interrupts
    psr_status = USLOSS_PsrSet(psr_to_restore);
}
