#include "defs.h"

// TO DO: add concurrent access to disks (spinlocks)

struct raid {
    int raid_initialized; // 0 or 1
    enum RAID_TYPE type; // RAID0, RAID1, RAID0_1, RAID4 or RAID5
    uint num_of_disks; // number of data disks in use

    uint block_size; // size of blocks

    uint max_blocks; // number of free blocks remaining
    uint remaining_blocks;

    uint dead_disk; // number of dead disk

    // for RAID 1
    // index of copy disks
    uint copy_disks[(DISKS-1)/2];
    // index of reserve disk that would jump in to serve when one dies
    uint reserve_disk;

    // for RAID 4
    uint parity_disk; // pointer to parity disk

    // for RAID 5
    uint stripe_size; // number of data disks and one parity disk 

    uint data_disks[DISKS-1]; // array of descriptors from 1 to DISKS-1 for indexing data disks
};

extern struct raid raid_metadata;

#define PARITY_DISK DISKS-1
#define STRIPE_SIZE DISKS/2
#define BLOCKSIZE 512