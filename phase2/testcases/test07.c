
/* Creates a 5-slot mailbox. Creates XXp1 that sends five messages to the
 * mailbox, then terminates. Creates XXp2a,b,c each of which sends a
 * message to the mailbox and gets blocked since the box is full.
 * Creates XXp3 which receives all eight messages, unblocking XXp2a,b,c.
 */

#include <stdio.h>
#include <string.h>

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>

int XXp1(void *);
int XXp2(void *);
int XXp3(void *);
char buf[256];

int mbox_id;



int start2(void *arg)
{
   int kid_status, kidpid;

   USLOSS_Console("start2(): started\n");
   mbox_id = MboxCreate(5, 50);
   USLOSS_Console("start2(): MboxCreate returned id = %d\n", mbox_id);

   kidpid = spork("XXp1",  XXp1, NULL,    2 * USLOSS_MIN_STACK, 1);
   kidpid = spork("XXp2a", XXp2, "XXp2a", 2 * USLOSS_MIN_STACK, 1);
   kidpid = spork("XXp2b", XXp2, "XXp2b", 2 * USLOSS_MIN_STACK, 1);
   kidpid = spork("XXp2c", XXp2, "XXp2c", 2 * USLOSS_MIN_STACK, 1);
   kidpid = spork("XXp3",  XXp3, NULL,    2 * USLOSS_MIN_STACK, 2);

   kidpid = join(&kid_status);
   USLOSS_Console("start2(): joined with kid %d, status = %d\n", kidpid, kid_status);

   kidpid = join(&kid_status);
   USLOSS_Console("start2(): joined with kid %d, status = %d\n", kidpid, kid_status);

   kidpid = join(&kid_status);
   USLOSS_Console("start2(): joined with kid %d, status = %d\n", kidpid, kid_status);

   kidpid = join(&kid_status);
   USLOSS_Console("start2(): joined with kid %d, status = %d\n", kidpid, kid_status);

   kidpid = join(&kid_status);
   USLOSS_Console("start2(): joined with kid %d, status = %d\n", kidpid, kid_status);

   quit(0);
}


int XXp1(void *arg)
{
   int i, result;
   char buffer[20];

   USLOSS_Console("XXp1(): started\n");

   for (i = 0; i < 5; i++) {
      USLOSS_Console("XXp1(): sending message #%d to mailbox %d\n", i, mbox_id);
      sprintf(buffer, "hello there, #%d", i);
      result = MboxSend(mbox_id, buffer, strlen(buffer)+1);
      USLOSS_Console("XXp1(): after send of message #%d, result = %d\n", i, result);
   }

   quit(3);
}


int XXp2(void *arg)
{
   int result;
   char buffer[20];

   sprintf(buffer, "hello from %s", (char*)arg);
   USLOSS_Console("%s(): sending message '%s' to mailbox %d, msg_size = %lu\n", arg, buffer, mbox_id, strlen(buffer)+1);
   result = MboxSend(mbox_id, buffer, strlen(buffer)+1);
   USLOSS_Console("%s(): after send of message '%s', result = %d\n", (char*)arg, buffer, result);

   quit(4);
}


int XXp3(void *arg)
{
   char buffer[100];
   int i, result;

  /* BUGFIX: initialize buffers to predictable contents */
  memset(buffer, 'x', sizeof(buffer)-1);
  buffer[sizeof(buffer)-1] = '\0';

   USLOSS_Console("XXp3(): started\n");

   for (i = 0; i < 8; i++) {
      USLOSS_Console("XXp3(): receiving message #%d from mailbox %d\n", i, mbox_id);
      result = MboxRecv(mbox_id, buffer, 100);
      USLOSS_Console("XXp3(): after receipt of message #%d, result = %d   message = '%s'\n", i, result, buffer);
   }

   quit(5);
}

