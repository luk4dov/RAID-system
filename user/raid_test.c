#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main() {
    init_raid(RAID4);

    uint block_num, block_size, disk_num;
    info_raid(&block_num, &block_size, &disk_num);
    
    printf("\nINFO ABOUT RAID SYSTEM: %d, %d, %d\n", block_num, block_size, disk_num);

    uchar* addr_write = malloc(block_size);
    for(int j = 0; j < 10; ++j) {
        for(int i = 0; i < block_size; ++i) {
            addr_write[i] = j+1;
        }
        write_raid(j, addr_write);
    }
    free(addr_write);

    disk_fail_raid(3);

    uchar* addr_read = malloc(block_size);
    for(int j = 0; j < 10; ++j) {
        int sum = 0;
        read_raid(j, addr_read);
        for(int i = 0; i < block_size; ++i) {
            sum += addr_read[i];
        }
        printf("sum of block %d is: %d\n",j+1, sum);
    }
    free(addr_read);

    return 0;
}