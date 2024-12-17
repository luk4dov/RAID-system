#include "types.h"
#include "param.h"
#include "riscv.h"
#include "virtio.h"
#include "raid.h"

// HELPERS
void cache_flush() { // if there were some writes, flush it to disks and sync cached data with data on disks
    for(int i = 1; i <= raid_metadata->num_of_disks; ++i) {
        if(i == raid_metadata->dead_disk) continue;
        if(raid_metadata->cache.dirty_blocks[i-1]) {
            write_block(i, raid_metadata->cache.stripe, raid_metadata->cache.cached_data_disks[i-1]);
        }
        raid_metadata->cache.dirty_blocks[i-1] = 0;
    }
    if(raid_metadata->cache.modified_parity) {
        write_block(PARITY_DISK, raid_metadata->cache.stripe, raid_metadata->cache.cached_data_disks[PARITY_DISK-1]);
        raid_metadata->cache.modified_parity = 0;
    }
    raid_metadata->cache.stripe = -1;
}

void cache(int blkn) { 
    // if it's not cached or if the disk is dead, cache it and do recovery if needed
    for(int i = 1; i <= raid_metadata->num_of_disks; ++i) {
        if(i == raid_metadata->dead_disk) continue; // don't try to read from dead disk
        read_block(i, blkn, raid_metadata->cache.cached_data_disks[i-1]);
        raid_metadata->cache.dirty_blocks[i-1] = 0;
    }
    // cache parity block
    read_block(raid_metadata->parity_disk, blkn, raid_metadata->cache.cached_data_disks[raid_metadata->num_of_disks]);
    raid_metadata->cache.modified_parity = 0;
    // mart the stripe that's cached
    raid_metadata->cache.stripe = blkn;
}

void raid_read_recovery(int diskn, uchar* paddr_data) { // recovery of dead disk's content
    for(int i = 0; i < BLOCKSIZE; ++i) {
        for(int j = 1; j <= raid_metadata->num_of_disks; ++j) { 
            if (diskn == j) continue;
            paddr_data[i] ^= raid_metadata->cache.cached_data_disks[j-1][i];
        }
        paddr_data[i] ^= raid_metadata->cache.cached_data_disks[PARITY_DISK][i];
    }
}

void raid_tocache_recovery(int diskn) { // if write hits the disk that's dead, recover it's data to cache
    for(int i = 0; i < BLOCKSIZE; ++i) {
        for(int j = 1; j <= raid_metadata->num_of_disks; ++j) {
            if(diskn == j) continue;
            raid_metadata->cache.cached_data_disks[diskn-1][i] ^= raid_metadata->cache.cached_data_disks[j-1][i];
        }
        raid_metadata->cache.cached_data_disks[diskn-1][i] ^= raid_metadata->cache.cached_data_disks[PARITY_DISK][i];
    }
}

void raid_write_updateparity_cache(int diskn, uchar* paddr_data) { // undo parity by xoring with old value, write new value, xor parity with new value
    for(int i = 0; i < BLOCKSIZE; ++i) {
        raid_metadata->cache.cached_data_disks[PARITY_DISK][i] ^= raid_metadata->cache.cached_data_disks[diskn-1][i];
        raid_metadata->cache.cached_data_disks[diskn-1][i] = paddr_data[i];
        raid_metadata->cache.cached_data_disks[PARITY_DISK][i] ^= raid_metadata->cache.cached_data_disks[diskn-1][i];
    }
    // mark modified disks for future write back
    raid_metadata->cache.modified_parity = 1;
    raid_metadata->cache.dirty_blocks[diskn-1] = 1;
}

// RAID 0
int read_raid0(int blkn, uchar* paddr_data) {
    // if disk is dead, return error code
    if(raid_metadata->dead_disk != -1) return -1;
    
    // get number of block and disk based on logical number of block
    int diskn = blkn % raid_metadata->num_of_disks+1;
    blkn = blkn / raid_metadata->num_of_disks+1;

    read_block(diskn, blkn, paddr_data);
    return 0;
}
int write_raid0(int blkn, uchar* paddr_data) {
    // if disk is dead, return error code
    if(raid_metadata->dead_disk != -1) return -1;
    
    // get number of block and disk based on logical number of block
    int diskn = blkn % raid_metadata->num_of_disks+1;
    blkn = blkn / raid_metadata->num_of_disks+1;

    write_block(diskn, blkn, paddr_data);
    return 0;
}

// RAID 1
int read_raid1(int blkn, uchar* paddr_data) {
    int diskn = blkn % raid_metadata->num_of_disks+1;
    // maybe for parallel read?
    // int copy_disk = raid_metadata->copy_disks[diskn];
    blkn = blkn / raid_metadata->num_of_disks+1;

    read_block(diskn, blkn, paddr_data);
    return 0;
}
int write_raid1(int blkn, uchar* paddr_data) {   
    int diskn = blkn % raid_metadata->num_of_disks+1;
    int copy_diskn = raid_metadata->copy_disks[diskn-1];
    blkn = blkn / raid_metadata->num_of_disks+1;

    write_block(diskn, blkn, paddr_data);
    write_block(copy_diskn, blkn, paddr_data);
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
    int diskn = blkn % raid_metadata->num_of_disks + 1;
    blkn = blkn / raid_metadata->num_of_disks + 1;

    // if disk is not dead and the stripe is cached, fetch the block
    if(raid_metadata->dead_disk != diskn && raid_metadata->cache.stripe == blkn) {
        memmove(paddr_data, raid_metadata->cache.cached_data_disks[diskn-1], BLOCKSIZE);
        return 0;
    }
    // if the disk is not dead, it's CACHE_MISS
    if(diskn != raid_metadata->dead_disk) {
        // if it's not in cache, flush the cache and fetch new stripe
        cache_flush();
        // if it's not cached or if the disk is dead, cache it and do recovery if needed
        cache(blkn);
    } 
    // the disk is dead but the stripe is cached, recover content of dead disk
    else {
        // recovery process
        raid_read_recovery(diskn, paddr_data);
    }
    // copy data
    memmove(paddr_data, raid_metadata->cache.cached_data_disks[diskn-1], BLOCKSIZE);
    return 0;
}
int write_raid4(int blkn, uchar* paddr_data) {
    int diskn = blkn % raid_metadata->num_of_disks + 1;
    blkn = blkn / raid_metadata->num_of_disks + 1;

    // check if its cached
    if(raid_metadata->cache.stripe == blkn) {
        // undo parity by xoring with old value and then xor it with new value
        raid_write_updateparity_cache(diskn, paddr_data);
        return 0;
    }
    // not in cache, flush the cache
    cache_flush(blkn);
    // cache it and recover(reconstruct) if some disk is dead 
    cache(blkn);
    // when data is cached, check if hit disk is dead and recover it's value in given stripe
    if(raid_metadata->dead_disk == diskn) {
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