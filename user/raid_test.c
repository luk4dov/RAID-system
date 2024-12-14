#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main() {
    init_raid(RAID1);

    uint block_num, block_size, disk_num;
    info_raid(&block_num, &block_size, &disk_num);
    
    // printf("INFO ABOUT RAID SYSTEM: %d, %d, %d\n", block_num, block_size, disk_num);

    // uchar* addr_write = malloc(block_size);
    // static uchar letters[5] = {'a', 'b', 'c', 'd', 'e'};

    // if(addr_write != 0) {
    //     for(int i = 0; i < block_size; ++i) {
    //         *(addr_write+i) = letters[i%5];
    //     }
    //     write_raid(6, addr_write);
    // } else {
    //     printf("Couldn't allocate memory\n");
    // }

    // uchar* addr_read = malloc(block_size);
    // read_raid(6, addr_read);
    // for(int i = 0; i < block_size; ++i) {
    //     printf("%c", *(addr_read+i));
    // }

    // disk_fail_raid(3);

    return 0;
}