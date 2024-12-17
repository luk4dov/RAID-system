#include "defs.h"

// TO DO: add concurrent access to disks (spinlocks)

#define PARITY_DISK DISKS
#define STRIPE_SIZE DISKS/2
#define BLOCKSIZE 512

struct raid {
    int raid_initialized; // 0 or 1
    enum RAID_TYPE type; // RAID0, RAID1, RAID0_1, RAID4 or RAID5
    uint num_of_disks; // number of data disks in use

    uint block_size; // size of blocks

    uint max_blocks; // number of free blocks remaining
    uint remaining_blocks;

    uint dead_disk; // pointer to dead disk
    uint num_dead_disks; // number of dead disks

    // for RAID 1
    // index of copy disks
    uint copy_disks[(DISKS-1)/2];
    // index of reserve disk that would jump in to serve when one dies
    uint reserve_disk;

    // for RAID0+1
    uint stripe_cursor; // pointer to the next disk in stripe
    // if the data spans across blocks, it has to be distrubuted evenly on disks

    // for RAID 4
    uint parity_disk; // pointer to parity disk

    // for RAID 5
    uint stripe_size; // number of data disks and one parity disk

    struct cached_blocks {
        uint stripe; // stripe that's cached
        uint parity_cached; // pointer to parity block in cache (if RAID system has parity block)
        uint dirty_blocks[DISKS]; // 0 or 1, if the write operation hit certain block
        uint modified_parity; // if parity block has been modified
        uchar *cached_data_disks[DISKS]; // cached data 
    } cache;
};

extern struct raid *raid_metadata;