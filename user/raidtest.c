#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

void dwrite(uint block_size) {
    uchar* addr_write = malloc(block_size);
    for(int j = 0; j < 20; ++j) {
        for(int i = 0; i < block_size; ++i) {
            addr_write[i] = j+1;
        }
        write_raid(j, addr_write);
    }
    free(addr_write);
}

void dread(uint block_size) {
    uchar* addr_read = malloc(block_size);
    uint sums[20];
    for(int j = 0; j < 20; ++j) {
        uint sum = 0;
        read_raid(j, addr_read); 
        for(int i = 0; i < block_size; ++i) {
            sum += addr_read[i];
        }
        sums[j] = sum;
    }
    for(int j = 0; j < 20; ++j) {
        printf("%d\n", sums[j]);
    }
    free(addr_read);
}


int main() {
    init_raid(RAID5);

    uint block_num, block_size, disk_num;
    info_raid(&block_num, &block_size, &disk_num);

    dwrite(block_size);
    dread(block_size);

    disk_fail_raid(2);
    dread(block_size);

    disk_fail_raid(3);
    disk_repaired_raid(2);
    dread(block_size);

    // destroy_raid();

    return 0;
}