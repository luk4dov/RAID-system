#include "defs.h"


// TO DO: add concurrent access to disks (spinlocks)
// raid_metadata->stripe_cursor - MOZDA NE TREBA

#define PARITY_DISK DISKS
#define BLOCKSIZE 512

enum dstate { ALIVE, DEAD, REPAIRED };

struct raid {
    int raid_initialized; // 0 or 1
    enum RAID_TYPE type; // RAID0, RAID1, RAID0_1, RAID4 or RAID5
    uint num_of_disks; // number of data disks in use
    uint block_size; // size of blocks
    uint max_blocks; // number of free blocks remaining
    
    // raid0 can function even if it has all copies dead, so I need an array to store which disk is dead
    // if both primary and copy are dead, then there is no functioning
    uint num_dead_disks; // number of dead disks
    uint max_dead_disks; // max dead disk, different for various raid types
    enum dstate disk_state[DISKS]; // disk state

    // for RAID 1 and RAID 0+1
    // index of data disks;
    uint data_disks[DISKS/2];
    // index of copy disks
    uint copy_disks[DISKS/2];
    // index of reserve disk that would jump in to serve when one dies
    uint reserve_disk;

    // for RAID0+1
    uint stripe_cursor; // pointer to the next disk in stripe
    // if the data spans across blocks, it has to be distrubuted evenly on disks

    // for RAID 4
    uint parity_disk; // pointer to parity disk

    struct cached_blocks {
        uint stripe; // stripe that's cached
        uint parity_cached; // pointer to parity block in cache (if RAID system has parity block that's not fixed)
        uchar *cached_data_disks[DISKS]; // cached data 
    } cache;
};

extern struct raid *raid_metadata;