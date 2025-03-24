#ifndef EXT2_READER_H
#define EXT2_READER_H

#include <ext2fs/ext2fs.h>

typedef struct {
    ext2_filsys fs;
} ext2_context;

struct block_iter_ctx {
    int *blocks;
    size_t max_blocks;
    size_t count;
};

struct search_ctx {
    const char *name;
    ext2_filsys fs;
    char *json_buffer;
    size_t buffer_size;
    size_t current_pos;
    int count;
    char path[4096];
    int path_len;
};

// Function declarations
int ext2_init(ext2_context *ctx, const char *disk_image);
void ext2_get_superblock_info(ext2_context *ctx);
void ext2_list_directory(ext2_context *ctx, ext2_ino_t inode);
void ext2_get_inode_info(ext2_context *ctx, ext2_ino_t inode);
void ext2_close(ext2_context *ctx);
int ext2_get_block_map(ext2_context *ctx, ext2_ino_t ino, int *blocks, size_t max_blocks);
int ext2_search_file(ext2_context *ctx, const char *name, char *json_buffer, size_t buffer_size);
static int block_iter_callback(ext2_filsys fs, blk_t *blocknr, int blockcnt, void *priv_data);
static int search_callback(struct ext2_dir_entry *dirent, int offset, int blocksize, char *buf, void *priv_data);

#endif

