#include "types.h"
#include "param.h"
#include "riscv.h"
#include "virtio.h"
#include "raid.h"

// HELPERS
void writeback() { // save current metadata
    for(int i = 1; i <= DISKS; ++i)
        if(raid_metadata->disk_state[i-1] == ALIVE)
            write_block(i, 0, (uchar*)(raid_metadata));
}

void cache(int stripe) { // cache given stripe of blocks
    for(int i = 1; i <= raid_metadata->num_of_disks; ++i) {
        if(raid_metadata->disk_state[i-1] == DEAD) continue; // don't try to read from dead disk
        read_block(i, stripe, raid_metadata->cache.cached_data_disks[i-1]);
    }
    // cache parity block
    if(raid_metadata->disk_state[raid_metadata->parity_disk] != DEAD) {
        read_block(raid_metadata->parity_disk+1, stripe, raid_metadata->cache.cached_data_disks[raid_metadata->parity_disk]);
    }
    raid_metadata->cache.stripe = stripe;
}

void raid_tocache_recovery(int diskn) { // if write hits the disk that's dead, recover it's data to cache
    for(int i = 0; i < BLOCKSIZE; ++i) {
        for(int j = 1; j <= raid_metadata->num_of_disks; ++j) {
            if(diskn == j) continue;
            raid_metadata->cache.cached_data_disks[diskn-1][i] ^= raid_metadata->cache.cached_data_disks[j-1][i];
        }
        raid_metadata->cache.cached_data_disks[diskn-1][i] ^= raid_metadata->cache.cached_data_disks[raid_metadata->parity_disk][i];
    }
}

void raid_write_updateparity_cache(int diskn, uchar* paddr_data) {
    // undo parity by xoring with old value, write new value, xor parity with new value
    for(int i = 0; i < BLOCKSIZE; ++i) {
        raid_metadata->cache.cached_data_disks[raid_metadata->parity_disk][i] ^= raid_metadata->cache.cached_data_disks[diskn-1][i];
        raid_metadata->cache.cached_data_disks[diskn-1][i] = paddr_data[i];
        raid_metadata->cache.cached_data_disks[raid_metadata->parity_disk][i] ^= raid_metadata->cache.cached_data_disks[diskn-1][i];
    }
    // immediately sync with disks
    if(raid_metadata->disk_state[raid_metadata->parity_disk] != DEAD) {
        write_block(raid_metadata->parity_disk+1, raid_metadata->cache.stripe, raid_metadata->cache.cached_data_disks[raid_metadata->parity_disk]); 
    }
    if(raid_metadata->disk_state[diskn-1] != DEAD) {
        write_block(diskn, raid_metadata->cache.stripe, raid_metadata->cache.cached_data_disks[diskn-1]);
    }
}

// RAID 0
int read_raid0(int blkn, uchar* paddr_data) {
    // if disk is dead, return error code
    if(raid_metadata->num_dead_disks > 0) return -1;
    
    // get number of block and disk based on logical number of block
    int diskn = blkn % raid_metadata->num_of_disks+1;
    blkn = blkn / raid_metadata->num_of_disks+1;

    read_block(diskn, blkn, paddr_data);
    return 0;
}
int write_raid0(int blkn, uchar* paddr_data) {
    // if disk is dead, return error code
    if(raid_metadata->num_dead_disks > 0) return -1;
    
    // get number of block and disk based on logical number of block
    int diskn = blkn % raid_metadata->num_of_disks+1;
    blkn = blkn / raid_metadata->num_of_disks+1;

    write_block(diskn, blkn, paddr_data);
    return 0;
}

// RAID 1
int read_raid1(int blkn, uchar* paddr_data) {
    int disk_index = blkn % raid_metadata->num_of_disks;
    
    int diskn = raid_metadata->data_disks[disk_index];
    int copy_diskn = raid_metadata->copy_disks[disk_index];
    blkn = blkn / raid_metadata->num_of_disks+1;

    if(raid_metadata->disk_state[diskn-1] != DEAD && diskn != -1) {
        read_block(diskn, blkn, paddr_data);
    } else if(raid_metadata->disk_state[copy_diskn-1] != DEAD && copy_diskn != -1) {
        read_block(copy_diskn, blkn, paddr_data);
    } else {
        return -1;
    }
    return 0;
}
int write_raid1(int blkn, uchar* paddr_data) {
    int disk_index = blkn % raid_metadata->num_of_disks;

    int diskn = raid_metadata->data_disks[disk_index];
    int copy_diskn = raid_metadata->copy_disks[disk_index];
    blkn = blkn / raid_metadata->num_of_disks+1;
    int flag = 0;

    if(!raid_metadata->disk_state[diskn-1] && diskn != -1) {
        write_block(diskn, blkn, paddr_data);
        flag = 1;
    }
    if(!raid_metadata->disk_state[copy_diskn-1] && copy_diskn != -1) {
        write_block(copy_diskn, blkn, paddr_data);
        flag = 1;
    }
    if(!flag) return -1;
    return 0;
}

// RAID 0+1
int read_raid01(int blkn, uchar* paddr_data) {   
    int diskn = blkn % raid_metadata->num_of_disks;
    // maybe for parallel read?
    // int copy_diskn = raid_metadata->copy_disks[diskn-1];
    blkn = blkn / raid_metadata->num_of_disks+1;

    read_block(diskn, blkn, paddr_data);
    return 0;
}
int write_raid01(int blkn, uchar* paddr_data) {   
    int diskn = blkn % raid_metadata->num_of_disks;
    int copy_diskn = raid_metadata->copy_disks[diskn-1];
    blkn = blkn / raid_metadata->num_of_disks + 1;

    write_block(diskn, blkn, paddr_data);
    write_block(copy_diskn, blkn, paddr_data);
    return 0;
}

// RAID 4
int read_raid4(int blkn, uchar* paddr_data) {
    if(raid_metadata->num_dead_disks >= raid_metadata->max_dead_disks) return -1;
    
    int diskn = blkn % raid_metadata->num_of_disks + 1;
    blkn = blkn / raid_metadata->num_of_disks + 1;

    // if disk is not dead and the stripe is cached, fetch the block
    if(raid_metadata->cache.stripe == blkn) {
        memmove(paddr_data, raid_metadata->cache.cached_data_disks[diskn-1], BLOCKSIZE);
        return 0;
    }
    // fetch its stripe to cache
    cache(blkn);
    // if the disk is dead, recover the data from it
    if(raid_metadata->disk_state[diskn-1] == DEAD) {
        raid_tocache_recovery(diskn);
    }
    // copy data
    memmove(paddr_data, raid_metadata->cache.cached_data_disks[diskn-1], BLOCKSIZE);
    return 0;
}
int write_raid4(int blkn, uchar* paddr_data) {
    if(raid_metadata->num_dead_disks >= raid_metadata->max_dead_disks) return -1;
    int diskn = blkn % raid_metadata->num_of_disks + 1;
    blkn = blkn / raid_metadata->num_of_disks + 1;

    // check if its cached
    if(raid_metadata->cache.stripe == blkn) {
        // undo parity by xoring with old value and then xor it with new value
        raid_write_updateparity_cache(diskn, paddr_data);
        return 0;
    }
    // cache it and recover(reconstruct) if some disk is dead 
    cache(blkn);
    // when data is cached, check if hit disk is dead and recover it's value in given stripe
    if(raid_metadata->disk_state[diskn-1] == DEAD) {
        raid_tocache_recovery(diskn);
    }
    // finally, do write by xoring parity with old value, write new data to cache and xor parity again
    raid_write_updateparity_cache(diskn, paddr_data);
    return 0;
}

// RAID 5
int read_raid5(int blkn, uchar* paddr_data) {   
    return 0;
}
int write_raid5(int blkn, uchar* paddr_data) {   
    return 0;
}