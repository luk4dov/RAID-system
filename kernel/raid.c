#include "types.h"
#include "param.h"
#include "riscv.h"
#include "virtio.h"
#include "raid.h"
#include "spinlock.h"
#include "proc.h"

struct raid *raid_metadata = {0};

int init_raid(enum RAID_TYPE raid) {
    if(raid_metadata == 0 || !raid_metadata->raid_initialized) {
        uchar* data = kalloc();
        read_block(1, 0, data);
        raid_metadata = (struct raid*)data;

        // cache is not valid 
        raid_metadata->cache.stripe = -1;
        raid_metadata->cache.parity_cached = -1;
        // raid_metadata->cache.modified_parity = 0;

        for(int i = 0; i < DISKS; ++i) {
            // raid_metadata->cache.dirty_blocks[i] = 0;
            raid_metadata->cache.cached_data_disks[i] = (uchar*)((uint64)raid_metadata + (BLOCKSIZE * (i+1)));
        }
    }
    raid_metadata->raid_initialized = 1;
    raid_metadata->type = raid;
    raid_metadata->block_size = BLOCKSIZE;
    raid_metadata->num_of_disks = DISKS;
    raid_metadata->reserve_disk = -1;
    // -1 because metadata is stored in first block of every disk
    raid_metadata->max_blocks = raid_metadata->num_of_disks * (DISK_SIZE/BLOCKSIZE - 1);
    // specific initialization for each raid system
    switch (raid) {
        case RAID0_1: {
            raid_metadata->stripe_cursor = -1;
            // no break, the initialization is the same as raid 1
        }
        case RAID1: {
            // define copy disks indexes, effective number of disks is DISKS/2
            raid_metadata->num_of_disks = DISKS/2;
            raid_metadata->reserve_disk = (DISKS % 2 == 0 ? -1 : DISKS);
            raid_metadata->max_dead_disks = raid_metadata->num_of_disks+1;
            if(raid_metadata->reserve_disk != -1) raid_metadata->max_dead_disks++;
            for(int i = 0; i < DISKS/2; ++i) {
                raid_metadata->data_disks[i] = i+1;
                raid_metadata->copy_disks[i] = i+1+DISKS/2;
            }
            raid_metadata->max_blocks = raid_metadata->num_of_disks * (DISK_SIZE/BLOCKSIZE - 1);
            break;
        }
        case RAID4: {
            // define parity disk and stripe size
            raid_metadata->max_dead_disks = 2;
            raid_metadata->stripe_size = DISKS-1;
            raid_metadata->parity_disk = PARITY_DISK-1;
            raid_metadata->num_of_disks--;
            break;
        }
        case RAID5: {
            raid_metadata->max_dead_disks = 2;
            // define stripe size and add 1 for parity block
            raid_metadata->stripe_size = STRIPE_SIZE+1;
            break;
        }
        default: {
            break;
        }
    }
    // save metadata on first block of every disk
    for(int i = 1; i <= DISKS; ++i) {
        raid_metadata->cache.cached_data_disks[i] = (uchar*)((uint64)raid_metadata + (BLOCKSIZE * (i+1))); 
        write_block(i, 0, (uchar*)(raid_metadata));
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

extern void writeback(); // sync metadata with first block on every disk

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
        // raid_metadata->cache.modified_parity = 0;

        for(int i = 0; i < DISKS; ++i) {
            // raid_metadata->cache.dirty_blocks[i] = 0;
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
    if(raid_metadata == 0 || diskn < 1 || diskn > DISKS)
        return -1;
    // check if disk was already dead
    if(raid_metadata->disk_state[diskn-1] == DEAD) return 0;
    // mark the disk as dead
    raid_metadata->disk_state[diskn-1] = DEAD;
    raid_metadata->num_dead_disks++;
    
    if(raid_metadata->type == RAID0 || raid_metadata->type == RAID4 || raid_metadata->type == RAID5) {
        writeback();
        return 0;
    }
    // reserved disk can only be used for RAID 1 and RAID 0+1
    // from now on, only types available are RAID 1 and RAID 0+1
    
    // if reserve disk exists and it's not used
    if(raid_metadata->reserve_disk != -1 && raid_metadata->reserve_disk != 0) {
        if(diskn <= raid_metadata->num_of_disks) { // if the primary is dead, replace if with copy
            raid_metadata->data_disks[diskn-1] = raid_metadata->copy_disks[diskn-1];
            raid_metadata->copy_disks[diskn-1] = raid_metadata->reserve_disk;
            raid_metadata->reserve_disk = 0; // it's used
            // here I should start copying content of new primary (previously copy) disk to new copy (reserved) disk
            // copy(primary, copy);
        } else { // copy disk died
            // find the primary that's the original of copy disk that died
            raid_metadata->copy_disks[DISKS-diskn] = raid_metadata->reserve_disk;
            raid_metadata->reserve_disk = 0;
            // copy content from primary to new copy disk(previously reserve)
            // copy(primary, copy);
        }
        writeback();
        return 0;
    }
    // there is no reserve disk for raid 0 and raid 0+1
    // check if the disk is primary or copy
    int index = -1; // index of primary 
    for(int i = 0; i < raid_metadata->num_of_disks; ++i)
        if (raid_metadata->data_disks[i]==diskn)
            index = i;
    if(index != -1) { // if primary died, use copy as new primary
        raid_metadata->data_disks[index] = raid_metadata->copy_disks[index];
        raid_metadata->copy_disks[index] = -1;
    } else {
        raid_metadata->copy_disks[diskn-1] = -1; 
    }
    writeback();
    return 0;
}

int disk_repaired_raid(int diskn) {
    if(raid_metadata == 0 || diskn < 1 || diskn > DISKS) {
        return -1;
    }
    // if disk isn't dead, no effect
    if(raid_metadata->disk_state[diskn-1] != DEAD) { 
        return 0;
    }
    // mark disk as alive
    raid_metadata->disk_state[diskn-1] = REPAIRED;
    raid_metadata->num_dead_disks--;
    writeback();
    return 0;
}