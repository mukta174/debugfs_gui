#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

struct ext2_superblock {
    uint32_t s_inodes_count;      // Total number of inodes
    uint32_t s_blocks_count;      // Total number of blocks
    uint32_t s_r_blocks_count;    // Number of reserved blocks
    uint32_t s_free_blocks_count; // Number of free blocks
    uint32_t s_free_inodes_count; // Number of free inodes
    uint32_t s_first_data_block;  // First data block
    // Add more fields as needed
};

// Function prototype
int read_superblock(const char *device, struct ext2_superblock *sb);

#endif // EXT2_H