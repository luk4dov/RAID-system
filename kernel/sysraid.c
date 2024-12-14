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
    return read_raid(blkn, (uchar*)data);
}

uint64
sys_write_raid(void)
{
    int blkn;
    uint64 data;
    argint(0, &blkn);
    argaddr(1, &data);
    return write_raid(blkn, (uchar*)data);
}

uint64
sys_info_raid(void) 
{
    uint64 blkn, blks, diskn;
    argaddr(0, &blkn);
    argaddr(1, &blks);
    argaddr(2, &diskn);
    return info_raid((uint*)blkn, (uint*)blks, (uint*)diskn);
}

uint64
sys_disk_fail_raid(void) {
    int diskn;
    argint(0, &diskn);
    return disk_fail_raid(diskn);
} 