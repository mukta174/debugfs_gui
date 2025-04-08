#include <ext2fs/ext2fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    ext2_filsys fs;
    char *device_path;
} ext2_context;

// Convert time_t to string
static void format_time(time_t t, char *buf, size_t bufsize) {
    struct tm *tm_info = localtime(&t);
    strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Get file type as string
static const char *get_file_type(unsigned int mode) {
    if (LINUX_S_ISREG(mode)) return "regular";
    if (LINUX_S_ISDIR(mode)) return "directory";
    if (LINUX_S_ISLNK(mode)) return "symlink";
    if (LINUX_S_ISBLK(mode)) return "block device";
    if (LINUX_S_ISCHR(mode)) return "character device";
    if (LINUX_S_ISFIFO(mode)) return "fifo";
    if (LINUX_S_ISSOCK(mode)) return "socket";
    return "unknown";
}

// Initialize ext2 filesystem reader
int ext2_init(ext2_context *ctx, const char *device) {
    errcode_t ret;
    
    ctx->device_path = strdup(device);
    
    ret = ext2fs_open(device, 0, 0, 0, unix_io_manager, &ctx->fs);
    if (ret) {
        fprintf(stderr, "Error opening filesystem: %s\n", error_message(ret));
        free(ctx->device_path);
        return -1;
    }
    
    return 0;
}

// Get superblock information
int ext2_get_superblock_info(ext2_context *ctx, char *json_buffer, size_t buffer_size) {
    struct ext2_super_block *sb = ctx->fs->super;
    char mtime_str[32], ctime_str[32], wtime_str[32];
    
    format_time(sb->s_mtime, mtime_str, sizeof(mtime_str));
    format_time(sb->s_mtime, ctime_str, sizeof(ctime_str));
    format_time(sb->s_wtime, wtime_str, sizeof(wtime_str));
    
    snprintf(json_buffer, buffer_size, 
        "{"
        "\"filesystem\": \"%s\","
        "\"blocks_count\": %u,"
        "\"free_blocks_count\": %u,"
        "\"free_blocks_percent\": %.2f,"
        "\"inodes_count\": %u,"
        "\"free_inodes_count\": %u,"
        "\"free_inodes_percent\": %.2f,"
        "\"first_data_block\": %u,"
        "\"block_size\": %u,"
        "\"blocks_per_group\": %u,"
        "\"inodes_per_group\": %u,"
        "\"magic\": \"0x%x\","
        "\"revision_level\": %u,"
        "\"mount_time\": \"%s\","
        "\"write_time\": \"%s\","
        "\"last_check\": \"%s\","
        "\"max_mount_count\": %u,"
        "\"mount_count\": %u,"
        "\"creator_os\": %u,"
        "\"volume_name\": \"%.*s\""
        "}",
        ctx->device_path,
        sb->s_blocks_count,
        sb->s_free_blocks_count,
        (float)sb->s_free_blocks_count / sb->s_blocks_count * 100.0,
        sb->s_inodes_count,
        sb->s_free_inodes_count,
        (float)sb->s_free_inodes_count / sb->s_inodes_count * 100.0,
        sb->s_first_data_block,
        EXT2_BLOCK_SIZE(sb),
        sb->s_blocks_per_group,
        sb->s_inodes_per_group,
        sb->s_magic,
        sb->s_rev_level,
        mtime_str,
        wtime_str,
        ctime_str,
        sb->s_max_mnt_count,
        sb->s_mnt_count,
        sb->s_creator_os,
        (int)sizeof(sb->s_volume_name), sb->s_volume_name
    );
    
    return 0;
}

// Directory iterator callback function
struct dir_context {
    int count;
    char *json_buffer;
    size_t buffer_size;
    size_t current_pos;
    ext2_filsys fs;
};

static int dir_iter_callback(struct ext2_dir_entry *dirent, int offset, 
                            int blocksize, char *buf, void *priv_data) {
    struct dir_context *ctx = (struct dir_context *)priv_data;
    struct ext2_inode inode;
    errcode_t ret;
    char name[EXT2_NAME_LEN + 1];
    char comma_str[2] = {',', '\0'};
    
    // Skip null entries and '.' and '..' if requested
    if (dirent->inode == 0) {
        return 0;
    }
    
    // Build null-terminated filename
    int name_len = dirent->name_len & 0xFF;
    memcpy(name, dirent->name, name_len);
    name[name_len] = '\0';
    
    // Get inode info
    ret = ext2fs_read_inode(ctx->fs, dirent->inode, &inode);
    if (ret) {
        return 0;
    }
    
    // Get file type
    const char *filetype = get_file_type(inode.i_mode);
    
    // Don't add comma before the first entry
    if (ctx->count > 0) {
        ctx->current_pos += snprintf(ctx->json_buffer + ctx->current_pos, 
                            ctx->buffer_size - ctx->current_pos, "%s", comma_str);
    }
    
    // Add entry to JSON array
    ctx->current_pos += snprintf(ctx->json_buffer + ctx->current_pos, 
                        ctx->buffer_size - ctx->current_pos,
                        "{"
                        "\"name\": \"%s\","
                        "\"inode\": %u,"
                        "\"size\": %u,"
                        "\"filetype\": \"%s\","
                        "\"mode\": %u,"
                        "\"uid\": %u,"
                        "\"gid\": %u,"
                        "\"links_count\": %u"
                        "}",
                        name,
                        dirent->inode,
                        inode.i_size,
                        filetype,
                        inode.i_mode & 0xFFF, // Permission bits
                        inode.i_uid,
                        inode.i_gid,
                        inode.i_links_count);
    
    ctx->count++;
    return 0;
}

// Get directory entries for a given inode
int ext2_list_directory(ext2_context *ctx, ext2_ino_t dir_ino, char *json_buffer, size_t buffer_size) {
    errcode_t ret;
    struct dir_context dir_ctx = {
        .count = 0,
        .json_buffer = json_buffer,
        .buffer_size = buffer_size,
        .current_pos = 0,
        .fs = ctx->fs
    };
    
    // Start JSON array
    dir_ctx.current_pos += snprintf(json_buffer, buffer_size, "[");
    
    // Iterate through directory entries
    ret = ext2fs_dir_iterate(ctx->fs, dir_ino, 0, NULL, dir_iter_callback, &dir_ctx);
    if (ret) {
        snprintf(json_buffer, buffer_size, "[]");
        return -1;
    }
    
    // Close JSON array
    dir_ctx.current_pos += snprintf(json_buffer + dir_ctx.current_pos, 
                           buffer_size - dir_ctx.current_pos, "]");
    
    return 0;
}

// Get file metadata by inode
int ext2_get_inode_info(ext2_context *ctx, ext2_ino_t ino, char *json_buffer, size_t buffer_size) {
    errcode_t ret;
    struct ext2_inode inode;
    char atime_str[32], ctime_str[32], mtime_str[32], dtime_str[32];
    
    ret = ext2fs_read_inode(ctx->fs, ino, &inode);
    if (ret) {
        snprintf(json_buffer, buffer_size, "{}");
        return -1;
    }
    
    format_time(inode.i_atime, atime_str, sizeof(atime_str));
    format_time(inode.i_ctime, ctime_str, sizeof(ctime_str));
    format_time(inode.i_mtime, mtime_str, sizeof(mtime_str));
    format_time(inode.i_dtime, dtime_str, sizeof(dtime_str));
    
    const char *filetype = get_file_type(inode.i_mode);
    
    snprintf(json_buffer, buffer_size,
        "{"
        "\"inode\": %u,"
        "\"mode\": \"0%o\","
        "\"owner\": %u,"
        "\"group\": %u,"
        "\"size\": %u,"
        "\"filetype\": \"%s\","
        "\"links_count\": %u,"
        "\"block_count\": %u,"
        "\"flags\": \"0x%x\","
        "\"access_time\": \"%s\","
        "\"change_time\": \"%s\","
        "\"modify_time\": \"%s\","
        "\"deletion_time\": \"%s\","
        "\"blocks\": ["
        ,
        ino,
        inode.i_mode & 0xFFF,
        inode.i_uid,
        inode.i_gid,
        inode.i_size,
        filetype,
        inode.i_links_count,
        inode.i_blocks,
        inode.i_flags,
        atime_str,
        ctime_str,
        mtime_str,
        dtime_str);
    
    // Add direct blocks
    size_t pos = strlen(json_buffer);
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        if (i > 0) {
            pos += snprintf(json_buffer + pos, buffer_size - pos, ", ");
        }
        pos += snprintf(json_buffer + pos, buffer_size - pos, "%u", inode.i_block[i]);
    }
    
    // Close blocks array and JSON object
    snprintf(json_buffer + pos, buffer_size - pos, "]}");
    
    return 0;
}

// Block iterator callback function
struct block_iter_ctx {
    int *blocks;
    size_t max_blocks;
    size_t count;
};

static int block_iter_callback(ext2_filsys fs, blk_t *blocknr, int blockcnt, void *priv_data) {
    struct block_iter_ctx *ctx = (struct block_iter_ctx *)priv_data;
    
    if (ctx->count < ctx->max_blocks) {
        ctx->blocks[ctx->count++] = *blocknr;
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
        return -1;
    }
    
    return block_ctx.count;
}

// Search file structure
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

// Search callback function
static int search_callback(struct ext2_dir_entry *dirent, int offset,
                          int blocksize, char *buf, void *priv_data) {
    struct search_ctx *ctx = (struct search_ctx *)priv_data;
    struct ext2_inode inode;
    errcode_t ret;
    char name[EXT2_NAME_LEN + 1];
    int name_len = dirent->name_len & 0xFF;
    char comma_str[2] = {',', '\0'};
    
    if (dirent->inode == 0) {
        return 0;
    }
    
    // Build null-terminated filename
    memcpy(name, dirent->name, name_len);
    name[name_len] = '\0';
    
    // Skip "." and ".."
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    
    // Get inode info
    ret = ext2fs_read_inode(ctx->fs, dirent->inode, &inode);
    if (ret) {
        return 0;
    }
    
    // Save current path length
    int old_path_len = ctx->path_len;
    
    // Append slash and filename to path
    if (ctx->path_len > 0 && ctx->path[ctx->path_len-1] != '/') {
        ctx->path[ctx->path_len++] = '/';
    }
    strcpy(ctx->path + ctx->path_len, name);
    ctx->path_len += name_len;
    
    // Check if this file matches the search
    if (strcasestr(name, ctx->name) != NULL) {
        // Don't add comma before the first entry
        if (ctx->count > 0) {
            ctx->current_pos += snprintf(ctx->json_buffer + ctx->current_pos, 
                                ctx->buffer_size - ctx->current_pos, "%s", comma_str);
        }
        
        // Add entry to JSON array
        ctx->current_pos += snprintf(ctx->json_buffer + ctx->current_pos, 
                            ctx->buffer_size - ctx->current_pos,
                            "{"
                            "\"path\": \"%s\","
                            "\"inode\": %u,"
                            "\"size\": %u,"
                            "\"filetype\": \"%s\""
                            "}",
                            ctx->path,
                            dirent->inode,
                            inode.i_size,
                            get_file_type(inode.i_mode));
        
        ctx->count++;
    }
    
    // If this is a directory, recurse into it
    if (LINUX_S_ISDIR(inode.i_mode)) {
        ext2fs_dir_iterate(ctx->fs, dirent->inode, 0, NULL, search_callback, ctx);
    }
    
    // Restore path length
    ctx->path_len = old_path_len;
    ctx->path[ctx->path_len] = '\0';
    
    return 0;
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

// Clean up resources
void ext2_close(ext2_context *ctx) {
    ext2fs_close(ctx->fs);
    free(ctx->device_path);
}
