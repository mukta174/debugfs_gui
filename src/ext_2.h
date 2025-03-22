#ifndef EXT2_READER_H
#define EXT2_READER_H

#include <ext2fs/ext2fs.h>

typedef struct {
    ext2_filsys fs;
} ext2_context;

// Function declarations
int ext2_init(ext2_context *ctx, const char *disk_image);
void ext2_get_superblock_info(ext2_context *ctx);
void ext2_list_directory(ext2_context *ctx, ext2_ino_t inode);
void ext2_get_inode_info(ext2_context *ctx, ext2_ino_t inode);
void ext2_close(ext2_context *ctx);

#endif

