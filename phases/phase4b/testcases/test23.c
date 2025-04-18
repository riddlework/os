/* KILLER
 * Spawn 5 children to sleep
 * Spawn 2 children ... 1 to read 1 line from each terminal and one to write one line to each terminal
 * Spawn 4 children ....
 * 1 to write to disk0 1 to read from disk0
 * 1 to write to disk1 1 to read from disk1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <usloss.h>
#include <usyscall.h>

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4.h>
#include <phase4_usermode.h>



int ChildS(void *arg)
{
    int tod1,tod2;
    int my_num = (int)(long)arg;
    int seconds;

    seconds = (5 - my_num);

    USLOSS_Console("ChildS(%d): Sleeping for %d seconds\n", my_num, seconds);
    GetTimeofDay(&tod1);
    Sleep(seconds);
    GetTimeofDay(&tod2);
    USLOSS_Console("ChildS(%d): After sleeping %d seconds, diff in sys_clock is %d\n", my_num, seconds, tod2-tod1);

    Terminate(10 + my_num);
}



int ChildTR(void *arg)
{
    char buf[80] = "";
    int read_length;
    int i;

    USLOSS_Console("ChildTR(): start\n");

    for (i=0; i<4; i++)
    {
        int retval = TermRead(buf, 80, i, &read_length);
        if (retval < 0)
        {
            USLOSS_Console("ChildTR(): ERROR: ReadTerm\n");
            return -1;
        }

        USLOSS_Console("ChildTR(): terminal %d, read_length = %d\n", i, read_length);
        buf[read_length] = '\0';
        USLOSS_Console("ChildTR(): read from term%d: read '%s'", i, buf);
    }

    USLOSS_Console("ChildTR(): done\n");
    Terminate(0);
}



int ChildTW(void *arg)
{
    char buffer[MAXLINE];
    int  result, size;
    int  i;

    for (i=0; i<4; i++) {
       sprintf(buffer, "ChildTW(): A Something interesting to print to term %d ...\n", i);

       result = TermWrite(buffer, strlen(buffer), i, &size);
       if (result < 0 || size != strlen(buffer))
       {
           USLOSS_Console("\n ***** ChildTW(%d): got bad result or bad size! *****\n\n", i);
           USLOSS_Console("result = %d, size = %d, strlen(buffer) = %d\n", result, size, strlen(buffer));
       }
    }

    Terminate(1);
    USLOSS_Console("ChildTW(): should not see this message!\n");
}



int ChildDW0(void *arg)
{
    int status;
    char disk_buf_A[512];

    USLOSS_Console("ChildDW0(): writing to disk 0, track 5, sector 0\n");
    sprintf(disk_buf_A, "ChildDW0(): A wonderful message to put on the disk...");

    DiskWrite(disk_buf_A, 0, 5, 0, 1, &status);
    USLOSS_Console("ChildDW0(): DiskWrite0 returned status = %d\n", status);

    return 0;
}



int ChildDW1(void *arg)
{
    int status;
    char disk_buf_A[512];

    USLOSS_Console("ChildDW1(): writing to disk 1, track 5, sector 0\n");
    sprintf(disk_buf_A, "ChildDW1(): A wonderful message to put on the disk...");

    DiskWrite(disk_buf_A, 1, 5, 0, 1, &status);
    USLOSS_Console("ChildDW1(): DiskWrite1 returned status = %d\n", status);

    return 0;
}



int ChildDR0(void *arg)
{
    int status;
    char disk_buf_B[512];

    USLOSS_Console("ChildR0(): reading from disk 0, track 5, sector 0\n");
    DiskRead(disk_buf_B, 0, 5, 0, 1, &status);

    USLOSS_Console("ChildR0(): DiskRead returned status = %d\n", status);
    USLOSS_Console("ChildR0(): disk_buf_B contains: '%s'\n", disk_buf_B);

    return 0;
}



int ChildDR1(void *arg)
{
    int status;
    char disk_buf_B[512];

    USLOSS_Console("ChildR1(): reading from disk 1, track 5, sector 0\n");
    DiskRead(disk_buf_B, 1, 5, 0, 1, &status);

    USLOSS_Console("ChildR1(): DiskRead returned status = %d\n", status);
    USLOSS_Console("ChildR1(): disk_buf_B contains: '%s'\n", disk_buf_B);

    return 0;
}



extern int testcase_timeout;   // defined in the testcase common code

int start4(void *arg)
{
    int  pid, status, i;
    char name[] = "ChildS";

    testcase_timeout = 60;

    USLOSS_Console("start4(): Spawning 5 children to sleep\n");
    for (i = 0; i < 5; i++)
    {
        sprintf(name, "Child%d", i);
        status = Spawn(name, ChildS, (void*)(long)i, USLOSS_MIN_STACK,2, &pid);
    }

    USLOSS_Console("start4(): Spawning 2 children to termfuncs\n");
    status = Spawn("ChildTR", ChildTR, NULL, USLOSS_MIN_STACK,2, &pid);
    status = Spawn("ChildTW", ChildTW, NULL, USLOSS_MIN_STACK,2, &pid);

    USLOSS_Console("start4(): Spawning 4 children to diskfuncs\n");
    status = Spawn("ChildDW0", ChildDW0, NULL, USLOSS_MIN_STACK,2, &pid);
    status = Spawn("ChildDW1", ChildDW1, NULL, USLOSS_MIN_STACK,2, &pid);
    status = Spawn("ChildDR0", ChildDR0, NULL, USLOSS_MIN_STACK,4, &pid);
    status = Spawn("ChildDR1", ChildDR1, NULL, USLOSS_MIN_STACK,4, &pid);

    for (i = 0; i < 11; i++)
        Wait(&pid, &status);

    USLOSS_Console("start4(): done.\n");
    Terminate(0);
}

