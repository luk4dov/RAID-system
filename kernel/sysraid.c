#include "types.h"
#include "riscv.h"
#include "defs.h"

uint64
sys_init_raid(void) 
{
    int raid;
    argint(0, &raid);
    return init_raid(raid);
}

uint64
sys_read_raid(void)
{
    int blkn;
    uint64 data;
    argint(0, &blkn);
    argaddr(1, &data);
    int ret = read_raid(blkn, (uchar*)data);
    if(ret == -1)
        printf("can't read\n");
    return ret;
}

uint64
sys_write_raid(void)
{
    int blkn;
    uint64 data;
    argint(0, &blkn);
    argaddr(1, &data);
    int ret = write_raid(blkn, (uchar*)data);
    if(ret == -1)
        printf("can't write\n");
    return ret;
}

uint64
sys_info_raid(void) 
{
    uint64 blkn, blks, diskn;
    argaddr(0, &blkn);
    argaddr(1, &blks);
    argaddr(2, &diskn);
    int ret = info_raid((uint*)blkn, (uint*)blks, (uint*)diskn);
    if(ret == -1) {
        printf("raid not initialized\n");
    }
    return ret;
}

uint64
sys_disk_fail_raid(void) {
    int diskn;
    argint(0, &diskn);
    int ret = disk_fail_raid(diskn);
    if(ret == -1) {
        printf("disk doesn't exist\n");
    }
    return ret;
} 

uint64
sys_disk_repaired_raid(void) {
    int diskn;
    argint(0, &diskn);
    int ret = disk_repaired_raid(diskn);
    if(ret == -1) {
        printf("disk doesn't exist\n");
    }
    return ret;
}

uint64
sys_destroy_raid(void) {
    return destroy_raid();
}