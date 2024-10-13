int MboxCreate(int numSlots, int slotSize) {

}

int MboxRelease(int mailboxID) {

}

int MboxSend(int mailboxID, void *message, int messageSize) {

}

int MboxRecv(int mailboxID, void *message, int maxMessageSize) {

}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {

}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {

}

void     waitDevice(int type, int unit, int *status) {

}

void wakeupByDevice(int type, int unit, int status) {

}

void (*systemCallVec[])(USLOSS_Sysargs *args) {
    
}