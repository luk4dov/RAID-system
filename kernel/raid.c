#include "types.h"
#include "param.h"
#include "riscv.h"
#include "virtio.h"
#include "raid.h"
#include "spinlock.h"
#include "proc.h"

struct raid *raid_metadata = {0};

// OPTIMIZATION: add caching

int init_raid(enum RAID_TYPE raid) {
    if(raid_metadata == 0 || !raid_metadata->raid_initialized) {
        uchar* data = kalloc();
        read_block(1, 0, data);
        raid_metadata = (struct raid*)data;

        // cache is not valid 
        raid_metadata->cache.stripe = -1;
        raid_metadata->cache.parity_cached = -1;
        raid_metadata->cache.modified_parity = 0;

        for(int i = 0; i < DISKS; ++i) {
            raid_metadata->cache.dirty_blocks[i] = 0;
            raid_metadata->cache.cached_data_disks[i] = (uchar*)((uint64)raid_metadata + (BLOCKSIZE * (i+1))); 
        }
    }

    raid_metadata->raid_initialized = 1;
    raid_metadata->type = raid;
    raid_metadata->block_size = BLOCKSIZE;
    raid_metadata->num_of_disks = DISKS;
    raid_metadata->dead_disk = -1;
    // -1 because metadata is stored in first block of every disk
    raid_metadata->max_blocks = raid_metadata->num_of_disks * (DISK_SIZE/BLOCKSIZE - 1);
    switch (raid) {
        case RAID0_1: {
            raid_metadata->stripe_cursor = -1;
            // no break, the initialization is the same as raid 1
        }
        case RAID1: {
            // define copy disks indexes, effective number of disks is DISKS/2
            raid_metadata->num_of_disks = DISKS/2;
            raid_metadata->reserve_disk = (DISKS % 2 == 0 ? -1 : DISKS);
            for(int i = 0; i < DISKS/2; ++i) {
                raid_metadata->copy_disks[i] = i+1+DISKS/2;
            }
            raid_metadata->max_blocks = raid_metadata->num_of_disks * (DISK_SIZE/BLOCKSIZE - 1);
            break;
        }
        case RAID4: {
            // define parity disk and stripe size
            raid_metadata->stripe_size = DISKS-1;
            raid_metadata->parity_disk = PARITY_DISK;
            raid_metadata->num_of_disks--;
            break;
        }
        case RAID5: {
            // define stripe size and add 1 for parity block
            raid_metadata->stripe_size = STRIPE_SIZE+1;
            break;
        }
        default: {
            break;
        }
    }

    for(int i = 1; i <= DISKS; ++i) {
        raid_metadata->cache.cached_data_disks[i] = (uchar*)((uint64)raid_metadata + (BLOCKSIZE * (i+1))); 
        write_block(i, 0, (uchar*)(&raid_metadata));
    }
    return 0;
    
}

// read and write functions for different type of raid
extern int read_raid0(int, uchar*);
extern int read_raid1(int, uchar*);
extern int read_raid01(int, uchar*);
extern int read_raid4(int, uchar*);
extern int read_raid5(int, uchar*);
extern int write_raid0(int, uchar*);
extern int write_raid1(int, uchar*);
extern int write_raid01(int, uchar*);
extern int write_raid4(int, uchar*);
extern int write_raid5(int, uchar*);

static int (*raid_rw[])(int,uchar*) = {
    [0] read_raid0,
    [1] write_raid0,
    [2] read_raid1, 
    [3] write_raid1,
    [4] read_raid01, 
    [5] write_raid01, 
    [6] read_raid4,
    [7] write_raid4,
    [8] read_raid5,
    [9] write_raid5 
};

int read_raid(int blkn, uchar* vaddr_data) {
    if(blkn < 0 || blkn >= raid_metadata->max_blocks || vaddr_data == 0) return -1;

    // translate virtual to physical address
    uchar* paddr_data = (uchar*)(walkaddr(myproc()->pagetable, (uint64)vaddr_data) | 
                                 ((uint64)vaddr_data & OFFSET));

    // call read function based on raid type
    return raid_rw[raid_metadata->type << 1](blkn, paddr_data);
}

int write_raid(int blkn, uchar* vaddr_data) { // should also check if the metadata is in RAM
    if(blkn < 0 || blkn >= raid_metadata->max_blocks || vaddr_data == 0) return -1;
    
    // translate virtual to physical address
    uchar* paddr_data = (uchar*)(walkaddr(myproc()->pagetable, (uint64)vaddr_data) | 
                                 ((uint64)vaddr_data & OFFSET));

    // call write function based on raid type
    return raid_rw[(raid_metadata->type << 1) + 1](blkn, paddr_data);
}

int info_raid(uint *v_blkn, uint *v_blks, uint *v_disks) {    
    if(raid_metadata == 0 || !raid_metadata->raid_initialized) {
        uchar* data = kalloc();
        read_block(1, 0, data);

        raid_metadata = (struct raid*)data;
        
        raid_metadata->cache.stripe = -1;
        raid_metadata->cache.parity_cached = -1;
        raid_metadata->cache.modified_parity = 0;

        for(int i = 0; i < DISKS; ++i) {
            raid_metadata->cache.dirty_blocks[i] = 0;
            raid_metadata->cache.cached_data_disks[i] = (uchar*)((uint64)raid_metadata + (BLOCKSIZE * (i+1))); 
        }
    }
    if(!raid_metadata->raid_initialized) return -1;

    uint* p_blkn = (uint*)(walkaddr(myproc()->pagetable, (uint64)v_blkn) | ((uint64)v_blkn & OFFSET));
    uint* p_blks = (uint*)(walkaddr(myproc()->pagetable, (uint64)v_blks) | ((uint64)v_blks & OFFSET));
    uint* p_disks = (uint*)(walkaddr(myproc()->pagetable, (uint64)v_disks) | ((uint64)v_disks & OFFSET));
    *p_blkn = raid_metadata->max_blocks;
    *p_blks = raid_metadata->block_size;
    *p_disks = raid_metadata->num_of_disks;
    return 0;
}

int disk_fail_raid(int diskn) {
    // mark the disk as unusable
    raid_metadata->dead_disk = diskn;
    raid_metadata->num_dead_disks++;
    // try to use reserve disk, if there is one
    // if there is no reserve disk, ensure that raid system can still be used 
    return 0;
}