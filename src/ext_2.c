#include "ext_2.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ext2fs/ext2fs.h>

// Initialize the filesystem
int ext2_init(ext2_context *ctx, const char *disk_image) {
    errcode_t err = ext2fs_open(disk_image, 0, 0, 0, unix_io_manager, &ctx->fs);
    if (err) {
        fprintf(stderr, "Error: Could not open disk image %s\n", disk_image);
        return -1;
    }
    return 0;
}

// Read and print superblock information
void ext2_get_superblock_info(ext2_context *ctx) {
    struct ext2_super_block *sb = ctx->fs->super; 

    printf("Superblock Information:\n");
    printf("Total Blocks: %u\n", sb->s_blocks_count);
    printf("Free Blocks: %u\n", sb->s_free_blocks_count);
    printf("Block Size: %u\n", 1024 << sb->s_log_block_size);
    printf("Total Inodes: %u\n", sb->s_inodes_count);
    printf("Free Inodes: %u\n", sb->s_free_inodes_count);
}

void ext2_list_directory(ext2_context *ctx, ext2_ino_t inode) {
    struct ext2_inode inode_data;
    errcode_t err = ext2fs_read_inode(ctx->fs, inode, &inode_data);
    if (err) {
        printf("Error reading inode %u\n", inode);
        return;
    }

    char block[ctx->fs->blocksize];  // Buffer for directory block

    // Read directory block correctly
    err = ext2fs_read_dir_block(ctx->fs, inode_data.i_block[0], block);
    if (err) {
        printf("Error reading directory block\n");
        return;
    }

    struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)block;

    // Iterate through directory entries
    while ((char *)entry < (char *)block + ctx->fs->blocksize) {
        if (entry->inode != 0) {  // Ignore empty entries
            printf("File: %.*s (Inode: %u)\n", entry->name_len, entry->name, entry->inode);
        }
        entry = (struct ext2_dir_entry_2 *)((char *)entry + entry->rec_len);
    }
}

// Get inode information
void ext2_get_inode_info(ext2_context *ctx, ext2_ino_t inode) {
    struct ext2_inode inode_data;
    ext2fs_read_inode(ctx->fs, inode, &inode_data); 

    printf("File Size: %u bytes\n", inode_data.i_size);
    printf("Blocks: %u\n", inode_data.i_blocks);
}

// Close filesystem
void ext2_close(ext2_context *ctx) {
    ext2fs_close(ctx->fs);
}

