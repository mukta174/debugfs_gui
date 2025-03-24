#define _GNU_SOURCE
#include "ext_2.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ext2fs/ext2fs.h>
#include <string.h>

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

// Block iterator callback function
static int block_iter_callback(ext2_filsys fs, blk_t *blocknr, int blockcnt, void *priv_data) {
    struct block_iter_ctx *ctx = (struct block_iter_ctx *)priv_data;

    if (ctx->count < ctx->max_blocks) {
        ctx->blocks[ctx->count++] = *blocknr;
    }

    return 0;
}

// Search callback function
static int search_callback(struct ext2_dir_entry *dirent, int offset, int blocksize, char *buf, void *priv_data) {
    struct search_ctx *ctx = (struct search_ctx *)priv_data;
    struct ext2_inode inode;
    errcode_t ret;
    char name[EXT2_NAME_LEN + 1];

    if (dirent->inode == 0) {
        return 0;
    }

    int name_len = dirent->name_len & 0xFF;
    memcpy(name, dirent->name, name_len);
    name[name_len] = '\0';

    ret = ext2fs_read_inode(ctx->fs, dirent->inode, &inode);
    if (ret) {
        return 0;
    }

    if (strcasecmp(name, ctx->name) != 0) {
        ctx->current_pos += snprintf(ctx->json_buffer + ctx->current_pos, 
                            ctx->buffer_size - ctx->current_pos,
                            "{\"path\": \"%s\", \"inode\": %u, \"size\": %u}",
                            ctx->path, dirent->inode, inode.i_size);
        ctx->count++;
    }

    return 0;
}

// Get block allocation information
int ext2_get_block_map(ext2_context *ctx, ext2_ino_t ino, int *blocks, size_t max_blocks) {
    errcode_t ret;
    struct block_iter_ctx block_ctx = {
        .blocks = blocks,
        .max_blocks = max_blocks,
        .count = 0
    };
    
    ret = ext2fs_block_iterate(ctx->fs, ino, 0, NULL, block_iter_callback, &block_ctx);
    if (ret) {
        fprintf(stderr, "Error retrieving block map: %s\n", error_message(ret));
        return -1;
    }
    
    return block_ctx.count;
}

// Search for files by name
int ext2_search_file(ext2_context *ctx, const char *name, char *json_buffer, size_t buffer_size) {
    struct search_ctx search_ctx = {
        .name = name,
        .fs = ctx->fs,
        .json_buffer = json_buffer,
        .buffer_size = buffer_size,
        .current_pos = 0,
        .count = 0,
        .path_len = 0
    };
    
    // Start JSON array
    search_ctx.current_pos += snprintf(json_buffer, buffer_size, "[");

    // Start search from root directory
    search_ctx.path[0] = '/';
    search_ctx.path[1] = '\0';
    search_ctx.path_len = 1;
    
    ext2fs_dir_iterate(ctx->fs, EXT2_ROOT_INO, 0, NULL, search_callback, &search_ctx);
    
    // Close JSON array
    search_ctx.current_pos += snprintf(json_buffer + search_ctx.current_pos, 
                              buffer_size - search_ctx.current_pos, "]");
    
    return 0;
}

// Enhanced close function with cleanup
void ext2_close(ext2_context *ctx) {
    if (!ctx) return;
    
    if (ctx->fs) {
        ext2fs_close(ctx->fs);
    }
    
    printf("Filesystem closed successfully.\n");
}


