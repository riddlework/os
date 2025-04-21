#include "phase2.h"
#include "phase3_kernelInterfaces.h"
#include "phase4.h"
#include <usloss.h>
#include <stdlib.h>

// function stubs
void require_kernel_mode(const char *func);
void gain_mutex(const char *func);
void release_mutex(const char *func);
void phase4_start_service_processes();

int kern_sleep(int seconds);
static void term_handler(int dev, void *arg);
int kern_term_read(char *buffer, int bufSize, int unit, int *lenOut);
int kern_term_write(char *buffer, int bufSize, int unit, int *lenOut);
static void disk_handler(int dev, void *arg);
int kern_disk_size(int unit, int *sector, int *track, int *disk);
int kern_disk_read(void *buffer, int unit, int track, int firstBlock, int blocks, int *statusOut);
int kern_disk_write(void *buffer, int unit, int track, int firstBlock, int blocks, int *statusOut);

// globals
int mutex;

// verifies that the program is currently running in kernel mode and halts if not 
void require_kernel_mode(const char *func) {
    unsigned int cur_psr = USLOSS_PsrGet();
    if (!(cur_psr & USLOSS_PSR_CURRENT_MODE)) {
        // not in kernel mode, halt simulation
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }
}

void gain_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO GAIN THE MUTEX!\n", func);*/
    int fail = kernSemP(mutex);
    if (fail) USLOSS_Console("ERROR: P(mutex) failed!\n");
    /*USLOSS_Console("%s HAS GAINED THE MUTEX!\n", func);*/
}

void release_mutex(const char *func) {
    /*USLOSS_Console("%s IS TRYING TO RELEASE THE MUTEX!\n", func);*/
    int fail = kernSemV(mutex);
    if (fail) USLOSS_Console("ERROR: V(mutex) failed!\n");
    /*USLOSS_Console("%s HAS RELEASED THE MUTEX!\n", func);*/
}

void phase4_init() {
    // require kernel mode
    require_kernel_mode(__func__);

    // initialize the mutex using a semaphore
    int fail = kernSemCreate(1, &mutex);
    if (fail) USLOSS_Console("ERROR: Failed to create semaphore!\n");

    // gain the mutex
    gain_mutex(__func__);
    
    // load the int vec
    USLOSS_IntVec[USLOSS_TERM_INT] = term_handler;
    USLOSS_IntVec[USLOSS_DISK_INT] = disk_handler;

    // release the mutex
    release_mutex(__func__);
}

void phase4_start_service_processes() {
    // need to start four daemons here?

}

int kern_sleep(int seconds) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);
}

static void term_handler(int dev, void *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // retreieve terminal number from arg
    int term_no = (int)(long) arg;

    // retrieve input status
    int status;
    int input_status = USLOSS_DeviceInput(dev, term_no, &status);

    // send status as payload for msg
    /*int mbox_id = term_mbox_ids[term_no];*/
    /*MboxCondSend(mbox_id, (void *) &status, sizeof(int));*/

    // release mutex
    release_mutex(__func__);
}

int kern_term_read(char *buffer, int bufSize, int unit, int *lenOut) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);
}

int kern_term_write(char *buffer, int bufSize, int unit, int *lenOut) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);
}

static void disk_handler(int dev, void *arg) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);

    // retrieve disk number from arg
    int disk_no = (int)(long) arg;

    // retrieve input status
    int status;
    int input_status = USLOSS_DeviceInput(dev, disk_no, &status);

    // send status as payload for msg
    /*int mbox_id = disk_mbox_ids[disk_no];*/
    /*MboxCondSend(mbox_id, (void *) &status, sizeof(int));*/

}

int kern_disk_size(int unit, int *sector, int *track, int *disk) {

    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);
}

int kern_disk_read(void *buffer, int unit, int track, int firstBlock, int blocks, int *statusOut) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);

}

int kern_disk_write(void *buffer, int unit, int track, int firstBlock, int blocks, int *statusOut) {
    // check for kernel mode
    require_kernel_mode(__func__);

    // gain mutex
    gain_mutex(__func__);

    // release mutex
    release_mutex(__func__);

}






