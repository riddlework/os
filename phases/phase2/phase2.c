

typedef struct mbox {

} Mbox;

void phase2_init() {

}

// TODO: necessary?
void phase2_start_service_processes() {
}

// returns id of mailbox, or -1 if no more mailboxes, or -1 if invalid args
int MboxCreate(int slots, int slot_size) {
    // TODO: error checking
    return 0;
}

// returns 0 if successful, -1 if invalid arg
int MboxRelease(int mbox_id) {
    return 0;
}

// return 0 if successful, -1 if invalid args
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    return 0;
}

// returns size of received msg if successful, -1 if invalid args
int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}

// returns 0 if successful, 1 if mailbox full, -1 if illegal args
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    return 0;
}

// returns 0 if successful, 1 if no msg available, -1 if illegal args
int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    return 0;
}
