#include "ext_2.h"
#include <stdio.h>

int main() {
    struct ext2_superblock sb;
    if (read_superblock("ext2.img", &sb))  {
        fprintf(stderr, "Failed to read superblock\n");
        return 1;
    }

    printf("Inodes Count: %u\n", sb.s_inodes_count);
    printf("Blocks Count: %u\n", sb.s_blocks_count);
    printf("Free Blocks: %u\n", sb.s_free_blocks_count);
    printf("Free Inodes: %u\n", sb.s_free_inodes_count);

    return 0;
}