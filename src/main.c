#include "ext_2.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <disk image>\n", argv[0]);
        return 1;
    }

    ext2_context ctx;
    if (ext2_init(&ctx, argv[1]) != 0) {
        return 1;
    }

    ext2_get_superblock_info(&ctx);  // Print superblock information
    ext2_list_directory(&ctx, 2);    // List root directory (inode 2)
    ext2_get_inode_info(&ctx, 2);    // Get inode info for root directory

    ext2_close(&ctx);
    return 0;
}

