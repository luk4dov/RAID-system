#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

void dwrite(uint block_size) {
    uchar* addr_write = malloc(block_size);
    for(int j = 0; j < 12; ++j) {
        for(int i = 0; i < block_size; ++i) {
            addr_write[i] = j+1;
        }
        write_raid(j, addr_write);
    }
    free(addr_write);
}

void dread(uint block_size) {
    uchar* addr_read = malloc(block_size);
    int sums[12];
    for(int j = 0; j < 12; ++j) {
        int sum = 0;
        read_raid(j, addr_read);

        for(int i = 0; i < block_size; ++i) {
            sum += addr_read[i];
        }
        sums[j] = sum;
    }
    for(int j = 0; j < 12; ++j) {
        printf("%d\n", sums[j]);
    }
    free(sums);
    free(addr_read);
}


int main() {
    // init_raid(RAID4);

    uint block_num, block_size, disk_num;
    info_raid(&block_num, &block_size, &disk_num);

    // dwrite(block_size);

    disk_fail_raid(2);
    disk_fail_raid(3);
    disk_repaired_raid(2);
    disk_repaired_raid(3);


    dread(block_size);    

    return 0;
}