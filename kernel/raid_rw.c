#include "types.h"
#include "param.h"
#include "riscv.h"
#include "virtio.h"
#include "raid.h"

// RAID 0
int read_raid0(int blkn, uchar* paddr_data) {
    // if disk is dead, return error code
    if(raid_metadata.dead_disk) return -1;
    
    // get number of block and disk based on logical number of block
    int diskn = raid_metadata.data_disks[blkn % raid_metadata.num_of_disks];
    blkn = blkn / raid_metadata.num_of_disks+1;

    read_block(diskn, blkn, paddr_data);
    return 0;
}
int write_raid0(int blkn, uchar* paddr_data) {
    // if disk is dead, return error code
    if(raid_metadata.dead_disk) return -1;
    
    // get number of block and disk based on logical number of block
    int diskn = raid_metadata.data_disks[blkn % raid_metadata.num_of_disks];
    blkn = blkn / raid_metadata.num_of_disks+1;

    write_block(diskn, blkn, paddr_data);
    return 0;
}

// RAID 1
int read_raid1(int blkn, uchar* paddr_data) {
    int diskn = raid_metadata.data_disks[blkn % raid_metadata.num_of_disks];
    // int copy_disk = raid_metadata.copy_disks[diskn];
    blkn = blkn / raid_metadata.num_of_disks+1;

    read_block(diskn, blkn, paddr_data);
    return 0;
}
int write_raid1(int blkn, uchar* paddr_data) {   
    int diskn = raid_metadata.data_disks[blkn % raid_metadata.num_of_disks];
    int copy_diskn = raid_metadata.copy_disks[diskn-1];
    blkn = blkn / raid_metadata.num_of_disks+1;

    write_block(diskn, blkn, paddr_data);
    write_block(copy_diskn, blkn, paddr_data);
    return 0;
}



int 
read_raid01(int blkn, uchar* paddr_data) 
{   
    return 0;
}

int
read_raid4(int blkn, uchar* paddr_data) 
{   
    return 0;
}

int
read_raid5(int blkn, uchar* paddr_data) 
{   
    return 0;
}

int
write_raid01(int blkn, uchar* paddr_data) 
{   
    return 0;
}

int
write_raid4(int blkn, uchar* paddr_data) 
{   
    return 0;
}

int
write_raid5(int blkn, uchar* paddr_data) 
{   
    return 0;
}