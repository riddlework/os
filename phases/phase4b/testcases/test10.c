#include <stdio.h>

#include <usloss.h>
#include <usyscall.h>

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4.h>
#include <phase4_usermode.h>



int start4(void *arg)
{
    int unit, sectorSize, trackSize, diskSize;
    char disk_buf_A[512];
    char disk_buf_B[512];
    int status;

    USLOSS_Console("start4(): started\n");

    unit = 0;
    DiskSize(unit, &sectorSize, &trackSize, &diskSize);

    USLOSS_Console("start4(): unit %d, sector size %d, track size %d, disk size %d\n", unit, sectorSize, trackSize, diskSize);

    unit = 1;
    DiskSize(unit, &sectorSize, &trackSize, &diskSize);

    USLOSS_Console("start4(): unit %d, sector size %d, track size %d, disk size %d\n", unit, sectorSize, trackSize, diskSize);
    USLOSS_Console("\n");

    USLOSS_Console("start4(): writing to disk 0, track 5, sector 0\n");
    sprintf(disk_buf_A, "A wonderful message to put on the disk...");
    DiskWrite(disk_buf_A, 0, 5, 0, 1, &status);
    USLOSS_Console("start4(): DiskWrite returned status = %d\n", status);
    USLOSS_Console("\n");

    USLOSS_Console("start4(): reading from disk 0, track 5, sector 0\n");
    DiskRead(disk_buf_B, 0, 5, 0, 1, &status);
    USLOSS_Console("start4(): DiskRead returned status = %d\n", status);
    USLOSS_Console("\n");

    USLOSS_Console("start4(): disk_buf_B contains:\n%s\n", disk_buf_B);
   
    USLOSS_Console("start4(): calling Terminate\n");
    Terminate(0);

    USLOSS_Console("start4(): should not see this message!\n");
    return 0;    // so that gcc won't complain
}

